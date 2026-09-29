#include "career/week_simulation.h"

#include "career/app_services.h"
#include "career/career_modules.h"
#include "career/gameplay_reports.h"
#include "ai/team_ai.h"
#include "career/match_analysis_store.h"
#include "career/dressing_room_service.h"
#include "career/career_reports.h"
#include "career/career_runtime.h"
#include "career/season_transition.h"
#include "career/career_support.h"
#include "career/player_development.h"
#include "career/staff_service.h"
#include "career/team_management.h"
#include "career/world_state_service.h"
#include "career/game_events_system.h"
#include "competition.h"
#include "development/monthly_development.h"
#include "simulation.h"
#include "simulation/match_center.h"
#include "transfers/negotiation_system.h"
#include "transfers/transfer_market.h"
#include "engine/social_system.h"
#include "engine/rival_ai.h"
#include "engine/rivalry_system.h"
#include "engine/manager_stress.h"
#include "engine/debt_system.h"
#include "engine/facilities_system.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "career/career_service.h"

using namespace std;

namespace {

TeamId safeActiveTeamIdAt(const Career& career, size_t index) {
    if (index > static_cast<size_t>(numeric_limits<int>::max())) return kInvalidTeamId;
    return career.getActiveTeamIdAt(static_cast<int>(index));
}

// Safety helpers for vector access
bool isValidWeekIndex(const Career& career, int week) {
    return week >= 1 && week <= static_cast<int>(career.schedule.size());
}

struct ScheduledTeamRef {
    TeamId id = kInvalidTeamId;
    Team* team = nullptr;
};

ScheduledTeamRef scheduledTeamRef(Career& career, int index) {
    TeamRepository teams(career);
    const TeamId id = teams.getActiveTeamIdAt(index);
    return {id, teams.getTeamById(id)};
}

FacilityLevel facilityLevelsForTeam(const Team& team) {
    FacilityLevel levels;
    levels.trainingGround = clampInt(team.trainingFacilityLevel, 1, 5);
    levels.youthAcademy = clampInt(team.youthFacilityLevel, 1, 5);
    levels.medical = clampInt(1 + team.medicalTeam / 25, 1, 5);
    levels.stadium = clampInt(team.stadiumLevel, 1, 5);
    levels.facilities = clampInt(1 + team.assistantCoach / 30, 1, 5);
    return levels;
}

long long divisionBaseIncome(const string& division) {
    return getCompetitionConfig(division).baseIncome;
}

int divisionWageFactor(const string& division) {
    return getCompetitionConfig(division).wageFactor;
}

long long weeklyWage(const Team& team) {
    long long total = 0;
    for (const auto& player : team.players) total += player.wage;
    return total * divisionWageFactor(team.division) / 100;
}


}  // namespace

void CareerService::processWeeklyFinances(const unordered_map<TeamId, int>& pointsBefore) {
    Career& career = career_;
    unordered_map<TeamId, int> homeGames;
    if (isValidWeekIndex(career, career.currentWeek)) {
        for (const auto& match : career.schedule[static_cast<size_t>(career.currentWeek - 1)]) {
            const ScheduledTeamRef home = scheduledTeamRef(career, match.first);
            if (home.id != kInvalidTeamId && home.team) {
                homeGames[home.id]++;
            }
        }
    }

    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        const TeamId teamId = safeActiveTeamIdAt(career, i);
        Team* team = career.getTeamById(teamId);
        const auto pointsIt = pointsBefore.find(teamId);
        if (!team || pointsIt == pointsBefore.end()) continue;
        const FacilityLevel levels = (team == career.myTeam)
                                         ? career.infrastructure.levels
                                         : facilityLevelsForTeam(*team);
        const InfrastructureModifiers infraMods = getModifiersFromFacilities(levels);
        int pointsDelta = team->points - pointsIt->second;
        if (pointsDelta >= 3) {
            team->fanBase = clampInt(team->fanBase + 1, 10, 99);
        } else if (pointsDelta == 0 && team->fanBase > 12 && randInt(1, 100) <= 30) {
            team->fanBase--;
        }

        long long baseTicketIncome =
            static_cast<long long>(homeGames[teamId]) * (team->fanBase * 2500LL + team->stadiumLevel * 7000LL);
        long long ticketIncome = static_cast<long long>(baseTicketIncome * infraMods.ticketRevenue);
        long long seasonTickets = (career.currentWeek % 4 == 1) ? team->fanBase * 900LL : 0LL;
        long long merchandising = static_cast<long long>(team->fanBase) * 350LL +
                                  static_cast<long long>(teamPrestigeScore(*team)) * 180LL +
                                  static_cast<long long>(max(0, team->goalsFor - team->goalsAgainst)) * 120LL;
        long long sponsorActivation = (pointsDelta >= 3 ? 3500LL : 0LL) + (team->fanBase >= 60 ? 2000LL : 0LL);
        if (team == career.myTeam && career.boardMonthlyTarget > 0 &&
            career.boardMonthlyProgress >= career.boardMonthlyTarget) {
            sponsorActivation += 4500LL;
        }
        long long sponsor = team->sponsorWeekly + max(0, pointsDelta) * 800LL + sponsorActivation;
        long long performanceBonus = pointsDelta * 4000LL;
        long long solidarity = randInt(0, 3000);
        long long income = divisionBaseIncome(team->division) + sponsor + ticketIncome + seasonTickets +
                           merchandising + performanceBonus + solidarity;
        long long wages = weeklyWage(*team);
        long long debtPayment = min(team->debt, max(0LL, income / 8));
        team->debt -= debtPayment;
        long long debtInterest = max(0LL, team->debt / 250);
        long long infrastructure =
            (levels.trainingGround + levels.youthAcademy + levels.medical + levels.stadium + levels.facilities - 5) * 1250LL;
        long long net = income - wages - debtPayment - debtInterest - infrastructure;
        const long long budgetAfter = team->budget + net;
        if (budgetAfter < 0) {
            team->debt += -budgetAfter;
            team->budget = 0;
        } else {
            team->budget = budgetAfter;
        }

        if (career.currentWeek % 8 == 0 && pointsDelta >= 3) {
            team->sponsorWeekly += max(500LL, team->fanBase * 30LL);
        }

        if (career.myTeam == team) {
            career.debtStatus = calculateDebtStatus(team->budget, team->debt, max(1LL, income));
            applyFinancialSanctions(career.debtStatus);
            ostringstream out;
            out << "Finanzas semanales: +" << income << " (entradas " << ticketIncome << ", abonos "
                << seasonTickets << ", merch " << merchandising << ", sponsor " << sponsor << ")"
                << " / -" << wages << " salarios"
                << " / -" << debtPayment << " deuda"
                << " / -" << debtInterest << " interes"
                << " / -" << infrastructure << " infraestructura"
                << " = " << net
                << " | deuda " << team->debt
                << " | severidad " << career.debtStatus.debtSeverity << "/100";
            emitUiMessage(out.str());
            if (career.debtStatus.inDefaultRisk && career.currentWeek % 4 == 0 && team->points > 0) {
                team->points = max(0, team->points - 1);
                career.addNews("Sancion financiera: la crisis de deuda descuenta 1 punto a " + team->name + ".");
                emitUiMessage("[Deuda] Riesgo de embargo: se descuenta 1 punto por incumplimiento financiero.");
            }
        }
    }
}
