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

using namespace std;

namespace {

void pushUniqueLimited(vector<string>& lines, const string& line, size_t limit = 3) {
    if (line.empty()) return;
    if (find(lines.begin(), lines.end(), line) != lines.end()) return;
    if (lines.size() >= limit) return;
    lines.push_back(line);
}

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

struct ScheduledMatchRef {
    ScheduledTeamRef home;
    ScheduledTeamRef away;

    bool valid() const {
        return home.team && away.team;
    }
};

ScheduledTeamRef scheduledTeamRef(Career& career, int index) {
    TeamRepository teams(career);
    const TeamId id = teams.getActiveTeamIdAt(index);
    return {id, teams.getTeamById(id)};
}

ScheduledMatchRef scheduledMatchRef(Career& career, const pair<int, int>& match) {
    return {scheduledTeamRef(career, match.first), scheduledTeamRef(career, match.second)};
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

void updateRivalMemoryForUserMatch(Career& career, const Team& home, const Team& away, const MatchResult& result) {
    if (!career.myTeam) return;
    const bool playerHome = (&home == career.myTeam);
    const bool playerAway = (&away == career.myTeam);
    if (!playerHome && !playerAway) return;

    const Team& rivalTeam = playerHome ? away : home;
    const Team& userTeam = playerHome ? home : away;
    const int rivalGoals = playerHome ? result.awayGoals : result.homeGoals;
    const int userGoals = playerHome ? result.homeGoals : result.awayGoals;

    RivalAI& rivalAI = career.rivalAIMap[rivalTeam.name];
    if (rivalAI.personality.teamName.empty()) {
        rivalAI = createRivalAI(rivalTeam);
    }

    auto memoryIt = find_if(rivalAI.memoryBank.begin(), rivalAI.memoryBank.end(), [&](const RivalMemory& memory) {
        return memory.opponentName == userTeam.name;
    });
    if (memoryIt == rivalAI.memoryBank.end()) {
        rivalAI.memoryBank.push_back(RivalMemory{});
        memoryIt = rivalAI.memoryBank.end() - 1;
        memoryIt->opponentName = userTeam.name;
    }

    RivalMemory& memory = *memoryIt;
    const int previousOutcome = memory.lastMatchOutcome;
    const int newOutcome = rivalGoals > userGoals ? 1 : (rivalGoals < userGoals ? -1 : 0);
    memory.matchesPlayed++;
    if (newOutcome > 0) memory.wins++;
    else if (newOutcome < 0) memory.losses++;
    else memory.draws++;
    memory.lastMatchOutcome = newOutcome;
    if (newOutcome != 0 && previousOutcome == newOutcome) {
        memory.consecutiveVsThisTeam = clampInt(memory.consecutiveVsThisTeam + 1, 1, 12);
    } else if (newOutcome != 0) {
        memory.consecutiveVsThisTeam = 1;
    } else {
        memory.consecutiveVsThisTeam = 0;
    }

    if (newOutcome >= 0) {
        pushUniqueLimited(memory.favoredFormations, rivalTeam.formation);
        pushUniqueLimited(memory.favoredTactics, rivalTeam.tactics);
    }

    if (rivalTeam.matchInstruction == "Juego directo") {
        memory.commonPlayPattern = "vertical";
    } else if (rivalTeam.matchInstruction == "Por bandas") {
        memory.commonPlayPattern = "bandas";
    } else if (rivalTeam.tactics == "Pressing") {
        memory.commonPlayPattern = "presion";
    } else {
        memory.commonPlayPattern = "equilibrado";
    }

    if (userGoals >= 2 || (playerHome ? result.stats.homeExpectedGoals : result.stats.awayExpectedGoals) >= 1.5) {
        pushUniqueLimited(memory.identifiedWeaknesses, "defensive_fragility");
    }
    if (rivalGoals >= 2 || (playerHome ? result.stats.awayExpectedGoals : result.stats.homeExpectedGoals) >= 1.5) {
        pushUniqueLimited(memory.identifiedStrengths, "sharp_attack");
    }
    if ((playerHome ? result.homePossession : result.awayPossession) >= 57) {
        pushUniqueLimited(memory.identifiedWeaknesses, "midfield_control");
    }
}

int teamRank(const LeagueTable& table, const Team* team) {
    for (size_t i = 0; i < table.teams.size(); ++i) {
        if (table.teams[i] == team) return static_cast<int>(i) + 1;
    }
    return -1;
}

bool isKeyMatch(const LeagueTable& table, const Team* home, const Team* away) {
    int homeRank = teamRank(table, home);
    int awayRank = teamRank(table, away);
    if (homeRank <= 0 || awayRank <= 0) return false;
    if (homeRank <= 3 || awayRank <= 3) return true;
    return abs(homeRank - awayRank) <= 2;
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

void applyWeeklyFinances(Career& career, const unordered_map<TeamId, int>& pointsBefore) {
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

void generateManagerCareerEvents(Career& career) {
    if (!career.myTeam) return;
    int rank = career.currentCompetitiveRank();
    int field = max(1, career.currentCompetitiveFieldSize());
    if (rank > 0 && rank <= max(2, field / 4) && randInt(1, 100) <= 18) {
        career.addNews("Entrevista: la prensa describe a " + career.managerName + " como un DT " +
                       managerStyleLabel(*career.myTeam) + ".");
    }
    int youthContributors = 0;
    for (const auto& player : career.myTeam->players) {
        if (player.age <= 21 && player.matchesPlayed >= 4) youthContributors++;
    }
    if (youthContributors >= 2 && randInt(1, 100) <= 16) {
        career.addNews("Perfil de manager: la prensa valora la apuesta juvenil de " + career.managerName + ".");
    }
    if (career.managerReputation >= 58 && rank > 0 && rank <= career.boardExpectedFinish &&
        randInt(1, 100) <= 12) {
        vector<Team*> jobs = buildJobMarket(career, false);
        if (!jobs.empty()) {
            career.addNews("Rumor de banquillo: " + jobs.front()->name + " sigue a " + career.managerName + ".");
        }
    }
}

void weeklyDashboard(const Career& career) {
    if (!career.myTeam) return;
    emitUiMessage("");
    for (const string& line : formatCareerReportLines(buildWeeklyDashboardReport(career))) {
        emitUiMessage(line);
    }
    emitUiMessage("");
    emitUiMessage("--- CENTRO SEMANAL DE DECISIONES ---");
    for (const string& line : buildWeeklyDecisionOptions(career)) {
        emitUiMessage(line);
    }
    
    // === Mostrar Sistemas de Gameplay ===
    emitUiMessage("");
    emitUiMessage("--- SISTEMAS DE JUGABILIDAD ---");
    
    // Reportes de vestuario
    auto dressingRoomBrief = gameplay_reports::getDressingRoomBrief(career.dressingRoomDynamics);
    for (const auto& line : dressingRoomBrief) {
        emitUiMessage(line);
    }
    
    // Alertas del manager
    auto managerAlerts = gameplay_reports::getManagerCriticalAlerts(career.managerStress);
    if (!managerAlerts.empty()) {
        emitUiMessage("");
        for (const auto& alert : managerAlerts) {
            emitUiMessage(alert);
        }
    }
    
    // Alertas de deuda
    auto debtAlerts = gameplay_reports::getDebtAlerts(career.debtStatus);
    if (!debtAlerts.empty()) {
        emitUiMessage("");
        for (const auto& alert : debtAlerts) {
            emitUiMessage(alert);
        }
    }
    
    // Rivalidades destacadas
    auto rivalryHighlights = gameplay_reports::getRivalryHighlights(career.rivalryDynamics, career.myTeam->name);
    if (!rivalryHighlights.empty()) {
        emitUiMessage("");
        for (const auto& highlight : rivalryHighlights) {
            emitUiMessage(highlight);
        }
    }
    
    // Sugerencias de instalaciones
    auto facilitySuggestions = gameplay_reports::getFacilitySuggestions(career.infrastructure, career.myTeam->budget);
    if (!facilitySuggestions.empty()) {
        emitUiMessage("");
        for (const auto& suggestion : facilitySuggestions) {
            emitUiMessage(suggestion);
        }
    }
}

void applyClubEvent(Career& career) {
    if (!career.myTeam || randInt(1, 100) > 15) return;
    int event = randInt(1, 6);
    if (event == 1) {
        long long bonus = 50000 + randInt(0, 30000);
        career.myTeam->budget += bonus;
        career.addNews("Nuevo patrocinio para " + career.myTeam->name + " por $" + to_string(bonus) + ".");
        emitUiMessage("[Evento] Patrocinio sorpresa: +" + to_string(bonus));
        return;
    }
    if (event == 2) {
        career.myTeam->morale = clampInt(career.myTeam->morale - 5, 0, 100);
        career.addNews("La hinchada presiona a " + career.myTeam->name + " tras los ultimos resultados.");
        emitUiMessage("[Evento] Protesta de hinchas: moral -5.");
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::MoraleAlert,
            "Protesta de hinchas",
            "Baja moral en el equipo. La hinchada presiona tras los ultimos resultados (-5 moral)."
        );
        return;
    }
    if (event == 3) {
        if (career.myTeam->players.empty()) return;
        int index = randInt(0, static_cast<int>(career.myTeam->players.size()) - 1);
        Player& player = career.myTeam->players[static_cast<size_t>(index)];
        player.injured = true;
        player.injuryType = "Leve";
        player.injuryWeeks = randInt(1, 2);
        player.injuryHistory++;
        career.addNews(player.name + " sufre una lesion leve en entrenamiento.");
        emitUiMessage("[Evento] Accidente en entrenamiento: " + player.name +
                      " fuera " + to_string(player.injuryWeeks) + " semanas.");
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::CriticalInjury,
            "Lesión en entrenamiento",
            player.name + " sufre una lesión leve. Baja estimada: " + to_string(player.injuryWeeks) + " semanas."
        );
        return;
    }
    if (event == 4) {
        for (auto& player : career.myTeam->players) {
            if (player.age > 29 || player.startsThisSeason > 1) continue;
            player.happiness = clampInt(player.happiness - 5, 1, 99);
            player.unhappinessWeeks = clampInt(player.unhappinessWeeks + 1, 0, 52);
            player.socialGroup = "Frustrados";
            career.addNews("Vestuario: " + player.name + " pide una conversacion por falta de minutos.");
            emitUiMessage("[Evento] Vestuario tenso: " + player.name + " reclama mas protagonismo.");
            career_events::EventNotificationSystem::recordEvent(
                career_events::EventType::ManagerAlert,
                "Jugador frustrado",
                player.name + " reclama más minutos. ¡Atiende al vestuario!"
            );
            return;
        }
    }
    if (event == 5) {
        const Player* best = nullptr;
        for (const auto& player : career.myTeam->players) {
            if (player.injured) continue;
            if (!best || player.skill > best->skill) best = &player;
        }
        if (best) {
            int idx = static_cast<int>(best - &career.myTeam->players[0]);
            Player& player = career.myTeam->players[static_cast<size_t>(idx)];
            player.wantsToLeave = player.ambition >= 60;
            player.happiness = clampInt(player.happiness - 3, 1, 99);
            career.addNews("Mercado: un club grande empieza a seguir a " + player.name + ".");
            emitUiMessage("[Evento] Mercado: aumenta el interes externo por " + player.name + ".");
            career_events::EventNotificationSystem::recordEvent(
                career_events::EventType::PlayerOffered,
                "Interés de mercado",
                "Un club grande sigue a " + player.name + ". ¡Prepárate para una posible oferta!"
            );
            return;
        }
    }

    int maxSquad = getCompetitionConfig(career.myTeam->division).maxSquadSize;
    if (maxSquad > 0 && static_cast<int>(career.myTeam->players.size()) >= maxSquad) return;
    int minSkill = 0;
    int maxSkill = 0;
    getDivisionSkillRange(career.myTeam->division, minSkill, maxSkill);
    int youthBoost = max(0, career.myTeam->youthFacilityLevel - 1);
    Player youth = makeRandomPlayer("MED", minSkill + youthBoost, maxSkill + youthBoost, 16, 18);
    youth.potential = clampInt(youth.skill + randInt(8 + youthBoost, 15 + youthBoost), youth.skill, 99);
    career.myTeam->addPlayer(youth);
    career.addNews("La cantera promociona a " + youth.name + " en " + career.myTeam->name + ".");
    emitUiMessage("[Evento] Cantera: se unio " + youth.name + " (pot " + to_string(youth.potential) + ").");
}

struct TeamTableSnapshot {
    int points;
    int goalsFor;
    int goalsAgainst;
    int awayGoals;
    int wins;
    int draws;
    int losses;
    int yellowCards;
    int redCards;
    vector<HeadToHeadRecord> headToHead;
};

TeamTableSnapshot captureTableState(const Team& team) {
    return {team.points, team.goalsFor, team.goalsAgainst, team.awayGoals, team.wins, team.draws,
            team.losses, team.yellowCards, team.redCards, team.headToHead};
}

void restoreTableState(Team& team, const TeamTableSnapshot& snapshot) {
    team.points = snapshot.points;
    team.goalsFor = snapshot.goalsFor;
    team.goalsAgainst = snapshot.goalsAgainst;
    team.awayGoals = snapshot.awayGoals;
    team.wins = snapshot.wins;
    team.draws = snapshot.draws;
    team.losses = snapshot.losses;
    team.yellowCards = snapshot.yellowCards;
    team.redCards = snapshot.redCards;
    team.headToHead = snapshot.headToHead;
}

void maybeInvokeIdle() {
    if (IdleCallback callback = idleCallback()) {
        callback();
    }
}

void storeMatchAnalysis(Career& career,
                        const Team& home,
                        const Team& away,
                        const MatchResult& result,
                        bool cupMatch) {
    career_match_analysis::storeMatchAnalysis(career, home, away, result, cupMatch);
}

void simulateSeasonCupRound(Career& career) {
    if (!career.cupActive) return;
    vector<Team*> alive;
    for (const auto& name : career.cupRemainingTeams) {
        Team* team = career.findTeamByName(name);
        if (team && team->division == career.activeDivision) alive.push_back(team);
    }
    if (alive.size() <= 1) {
        career.cupActive = false;
        if (!alive.empty()) {
            career.cupChampion = alive.front()->name;
            career.addNews("Copa de temporada: " + career.cupChampion + " se consagra campeon.");
        }
        return;
    }

    career.cupRound++;
    emitUiMessage("");
    emitUiMessage("--- Copa de temporada: ronda " + to_string(career.cupRound) + " ---");
    vector<string> nextRound;
    if (alive.size() % 2 == 1) {
        Team* bye = alive.back();
        nextRound.push_back(bye->name);
        alive.pop_back();
        emitUiMessage("Pase libre: " + bye->name);
    }

    for (size_t i = 0; i < alive.size(); i += 2) {
        maybeInvokeIdle();
        Team* home = alive[i];
        Team* away = alive[i + 1];
        TeamTableSnapshot homeSnap = captureTableState(*home);
        TeamTableSnapshot awaySnap = captureTableState(*away);
        bool verbose = (home == career.myTeam || away == career.myTeam);
        emitUiMessage(home->name + " vs " + away->name);
        MatchResult result = verbose ? playMatch(&career, *home, *away, true, true, true)
                                     : playMatch(*home, *away, false, true, true);
        restoreTableState(*home, homeSnap);
        restoreTableState(*away, awaySnap);
        storeMatchAnalysis(career, *home, *away, result, true);
        updateRivalMemoryForUserMatch(career, *home, *away, result);

        Team* winner = home;
        if (result.awayGoals > result.homeGoals) {
            winner = away;
        } else if (result.homeGoals == result.awayGoals) {
            winner = (teamPenaltyStrength(*home) >= teamPenaltyStrength(*away)) ? home : away;
            emitUiMessage("Gana por penales: " + winner->name);
        }
        nextRound.push_back(winner->name);
    }

    career.cupRemainingTeams = nextRound;
    if (career.cupRemainingTeams.size() == 1) {
        career.cupActive = false;
        career.cupChampion = career.cupRemainingTeams.front();
        career.addNews("Copa de temporada: " + career.cupChampion + " se consagra campeon.");
        emitUiMessage("Campeon de la copa: " + career.cupChampion);
    }
}

void updateSquadDynamics(Career& career, int pointsDelta) {
    DressingRoomSnapshot snapshot = dressing_room_service::applyWeeklyUpdate(career, pointsDelta);
    if (!snapshot.summary.empty()) {
        emitUiMessage("[Vestuario] " + snapshot.summary);
    }
    for (size_t i = 0; i < snapshot.alerts.size() && i < 2; ++i) {
        emitUiMessage("[Vestuario] " + snapshot.alerts[i]);
    }
}

void runMonthlyDevelopment(Career& career) {
    if (career.currentWeek <= 0 || career.currentWeek % 4 != 0) return;
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        Team* team = career.getActiveTeamAt(i);
        if (!team) continue;
        const development::MonthlyDevelopmentSummary summary =
            development::runMonthlyDevelopmentCycle(*team, career.currentWeek);
        if (team == career.myTeam && summary.improvedPlayers > 0) {
            career.addNews("Informe juvenil: " + to_string(summary.improvedPlayers) +
                           " jugador(es) joven(es) mejoran este mes.");
            if (summary.acceleratedProspects > 0) {
                career.addNews("Informe de cantera: " + to_string(summary.acceleratedProspects) +
                               " prospecto(s) muestran una aceleracion especial.");
            }
        }
        if (team == career.myTeam && summary.newYouthPlayers > 0) {
            career.addNews("La cantera suma " + to_string(summary.newYouthPlayers) +
                           " nuevo(s) prospecto(s) desde la region " + team->youthRegion + ".");
        }
    }
}

void progressScoutingAssignments(Career& career) {
    if (!career.myTeam || career.scoutingAssignments.empty()) return;

    Team& team = *career.myTeam;
    for (size_t i = 0; i < career.scoutingAssignments.size();) {
        ScoutingAssignment& assignment = career.scoutingAssignments[i];
        assignment.weeksRemaining = max(0, assignment.weeksRemaining - 1);
        assignment.knowledgeLevel = clampInt(assignment.knowledgeLevel + team.scoutingChief / 5 + max(0, team.performanceAnalyst - 50) / 6, 0, 100);

        if (assignment.weeksRemaining <= 0) {
            const ScoutingSessionResult session = runScoutingSessionService(career, assignment.region, assignment.focusPosition);
            if (session.service.ok) {
                const string lead = session.candidates.empty()
                    ? string("sin candidato destacado")
                    : session.candidates.front().playerName + " (" + session.candidates.front().clubName + ")";
                career.addInboxItem("Asignacion cerrada en " + assignment.region + " | foco " + assignment.focusPosition +
                                    " | radar " + lead + ".", "Scouting");
            } else {
                career.addInboxItem("Asignacion pausada en " + assignment.region + ": " + session.service.messages.front(), "Scouting");
            }
            career.scoutingAssignments.erase(career.scoutingAssignments.begin() + static_cast<long long>(i));
            continue;
        }

        if (assignment.weeksRemaining == 1 || assignment.knowledgeLevel >= 72) {
            career.addInboxItem("Seguimiento en curso: " + assignment.region + " | foco " + assignment.focusPosition +
                                " | conocimiento " + to_string(assignment.knowledgeLevel) + "%.", "Scouting");
        }
        ++i;
    }
}

void updateShortlistAlerts(Career& career) {
    if (!career.myTeam || career.scoutingShortlist.empty() || career.currentWeek % 4 != 0) return;

    vector<string> active;
    for (const auto& item : career.scoutingShortlist) {
        auto parts = splitByDelimiter(item, '|');
        if (parts.size() < 2) continue;
        Team* seller = career.findTeamByName(parts[0]);
        if (!seller) continue;
        int index = team_mgmt::playerIndexByName(*seller, parts[1]);
        if (index < 0) continue;
        const Player& player = seller->players[static_cast<size_t>(index)];
        active.push_back(item);
        if (player.contractWeeks <= 12) {
            career.addNews("Alerta de shortlist: " + player.name + " entra en ventana de precontrato con " +
                           seller->name + ".");
        } else if (player.value <= player.releaseClause * 60 / 100) {
            career.addNews("Alerta de shortlist: " + player.name + " mantiene un costo accesible en " +
                           seller->name + ".");
        }
    }
    career.scoutingShortlist = active;
}

void dispatchWeeklyStaffBriefing(Career& career) {
    if (!career.myTeam) return;
    const auto recommendations = staff_service::buildStaffRecommendations(career, 4);
    if (recommendations.empty()) return;

    for (size_t i = 0; i < recommendations.size() && i < 2; ++i) {
        const auto& recommendation = recommendations[i];
        career.addInboxItem(recommendation.staffRole + " | " + recommendation.severity + " | " +
                                recommendation.summary + " | Accion: " + recommendation.suggestedAction,
                            "Staff");
    }

    const auto& headline = recommendations.front();
    if (headline.urgency >= 48) {
        career.addNews("Mesa del staff: " + headline.staffRole + " avisa que " + headline.summary +
                       " Accion sugerida: " + headline.suggestedAction);
    }
    emitUiMessage("[Staff] " + headline.staffRole + " | " + headline.summary);
}

const Player* leadingForward(const Team& team) {
    const Player* best = nullptr;
    for (const auto& player : team.players) {
        if (normalizePosition(player.position) != "DEL") continue;
        if (!best || player.skill > best->skill) best = &player;
    }
    return best;
}

void addSquadAlerts(Career& career) {
    if (!career.myTeam) return;
    int defenseFitness = averageFitnessForLine(*career.myTeam, "DEF");
    int midfieldFitness = averageFitnessForLine(*career.myTeam, "MED");
    int attackFitness = averageFitnessForLine(*career.myTeam, "DEL");
    if (defenseFitness < 58 || midfieldFitness < 58 || attackFitness < 58) {
        string line = (defenseFitness <= midfieldFitness && defenseFitness <= attackFitness)
                          ? "la linea defensiva"
                          : (midfieldFitness <= attackFitness ? "el mediocampo" : "el frente de ataque");
        career.addNews("Alerta fisica: " + line + " llega exigida a la proxima fecha.");
    }
    const Player* forward = leadingForward(*career.myTeam);
    if (forward && forward->matchesPlayed >= 5 && forward->goals == 0) {
        career.addNews("Alerta ofensiva: " + forward->name + " ya suma " +
                       to_string(forward->matchesPlayed) + " partido(s) sin marcar.");
    }
    int promiseWarnings = 0;
    for (const auto& player : career.myTeam->players) {
        if (promiseAtRisk(player, career.currentWeek)) promiseWarnings++;
    }
    if (promiseWarnings >= 2) {
        career.addNews("Alerta de vestuario: hay " + to_string(promiseWarnings) +
                       " promesa(s) contractuales bajo revision.");
    }
}

void generateWeeklyNarratives(Career& career, int myTeamPointsDelta) {
    if (!career.myTeam) return;
    int rank = career.currentCompetitiveRank();
    int field = max(1, career.currentCompetitiveFieldSize());
    if (rank > 0 && rank <= max(2, field / 4)) {
        career.addNews("La prensa destaca a " + career.myTeam->name + " por su presencia en la zona alta.");
    } else if (rank >= max(2, field - field / 4)) {
        career.addNews("La prensa pone a " + career.myTeam->name + " en la pelea por evitar el fondo.");
    }

    if (myTeamPointsDelta == 3 && career.myTeam->morale >= 65) {
        career.addNews("El vestuario de " + career.myTeam->name + " atraviesa un momento de confianza.");
    } else if (myTeamPointsDelta == 0 && career.boardConfidence <= 30) {
        career.addNews("Crece la tension institucional alrededor de " + career.myTeam->name + ".");
    }

    int promiseAlerts = 0;
    int leaders = 0;
    for (const auto& player : career.myTeam->players) {
        if (player.promisedRole == "Titular" && player.startsThisSeason + 2 < max(2, career.currentWeek * 2 / 3)) {
            promiseAlerts++;
        }
        if (player.promisedRole == "Rotacion" && player.startsThisSeason + 1 < max(1, career.currentWeek / 3)) {
            promiseAlerts++;
        }
        if ((player.leadership >= 72 || playerHasTrait(player, "Lider")) && player.happiness >= 55) {
            leaders++;
        }
    }
    if (promiseAlerts > 0) {
        career.addNews("Se acumulan " + to_string(promiseAlerts) + " promesa(s) de rol bajo presion en el plantel.");
    }
    if (career.myTeam->fanBase >= 65 && myTeamPointsDelta == 3) {
        career.addNews("La aficion responde con entusiasmo y empuja la recaudacion del club.");
    } else if (career.myTeam->fanBase >= 45 && myTeamPointsDelta == 0) {
        career.addNews("La prensa cuestiona la falta de resultados recientes de " + career.myTeam->name + ".");
    }
    if (leaders >= 3 && career.myTeam->morale >= 60) {
        career.addNews("Los lideres del vestuario sostienen un ambiente competitivo en " + career.myTeam->name + ".");
    }
    if (career.myTeam->youthIdentity == "Cantera estructurada") {
        int youthMinutes = 0;
        for (const auto& player : career.myTeam->players) {
            if (player.age <= 20 && player.matchesPlayed > 0) youthMinutes++;
        }
        if (youthMinutes >= 2) {
            career.addNews("La identidad de cantera de " + career.myTeam->name + " gana peso esta semana.");
        }
    }

    const Team* opponent = nextOpponent(career);
    if (opponent) {
        career.addNews("Informe previo: " + buildOpponentReport(career) + ".");
        if (areRivalClubs(*career.myTeam, *opponent)) {
            career.addNews("La semana queda marcada por un clasico ante " + opponent->name + ".");
        }
    }
    if (teamPrestigeScore(*career.myTeam) >= 68 && myTeamPointsDelta == 0) {
        career.addNews("La exigencia institucional aprieta: el entorno de " + career.myTeam->name + " esperaba mas.");
    }

    for (const auto& player : career.myTeam->players) {
        if (player.contractWeeks > 0 && player.contractWeeks <= 4) {
            career.addNews("Contrato al limite: " + player.name + " entra en sus ultimas " +
                           to_string(player.contractWeeks) + " semana(s).");
            break;
        }
    }
    addSquadAlerts(career);
}

void processIncomingOffers(Career& career) {
    if (!career.myTeam || career.myTeam->players.size() <= 18) return;
    bool squadUnrest = false;
    for (const auto& player : career.myTeam->players) {
        if (player.wantsToLeave) {
            squadUnrest = true;
            break;
        }
    }
    if (randInt(1, 100) > (squadUnrest ? 50 : 32)) return;

    vector<int> candidates;
    for (size_t i = 0; i < career.myTeam->players.size(); ++i) {
        const Player& player = career.myTeam->players[i];
        if (!player.injured && player.contractWeeks > 8) {
            candidates.push_back(static_cast<int>(i));
            if (player.wantsToLeave) candidates.push_back(static_cast<int>(i));
        }
    }
    if (candidates.empty()) return;

    int index = candidates[static_cast<size_t>(randInt(0, static_cast<int>(candidates.size()) - 1))];
    Player& player = career.myTeam->players[static_cast<size_t>(index)];
    Team* bidder = nullptr;
    int bidderNeed = -100000;
    for (auto& club : career.allTeams) {
        if (&club == career.myTeam) continue;
        if (club.players.size() >= 26) continue;
        if (club.budget < player.value * 9 / 10) continue;
        int need = positionFitScore(player, transfer_market::weakestSquadPosition(club)) +
                   teamPrestigeScore(club) - teamPrestigeScore(*career.myTeam) / 2;
        if (areRivalClubs(club, *career.myTeam)) need += 8;
        if (need > bidderNeed) {
            bidderNeed = need;
            bidder = &club;
        }
    }
    if (!bidder) return;

    ensureTeamIdentity(*career.myTeam);
    ensureTeamIdentity(*bidder);
    long long maxOffer = max(player.value, player.value * (105 + randInt(0, 40)) / 100);
    if (areRivalClubs(*bidder, *career.myTeam)) maxOffer = maxOffer * 112 / 100;
    maxOffer = max(maxOffer, static_cast<long long>(player.value * (100 + teamPrestigeScore(*bidder) / 10) / 100));
    long long offer = max(player.value * 9 / 10, maxOffer * 85 / 100);

    emitUiMessage("");
    emitUiMessage("Oferta recibida por " + player.name + " desde " + bidder->name + ": $" + to_string(offer) +
                  " (tope estimado del mercado: $" + to_string(maxOffer) + ")" +
                  (areRivalClubs(*bidder, *career.myTeam) ? " [rival directo]" : ""));

    int choice = (offer >= maxOffer || (player.wantsToLeave && offer >= player.value)) ? 1 : 3;
    long long counter = 0;
    if (incomingOfferDecisionCallback()) {
        IncomingOfferDecision decision = incomingOfferDecisionCallback()(career, player, offer, maxOffer);
        if (decision.action >= 1 && decision.action <= 3) {
            choice = decision.action;
            counter = decision.counterOffer;
        }
    }

    if (choice == 1) {
        career.myTeam->budget += offer;
        bidder->budget = max(0LL, bidder->budget - offer);
        Player moved = player;
        moved.wantsToLeave = false;
        moved.onLoan = false;
        moved.parentClub.clear();
        moved.loanWeeksRemaining = 0;
        bidder->addPlayer(moved);
        emitUiMessage("Transferencia aceptada. " + player.name + " vendido a " + bidder->name + ".");
        career.addNews(player.name + " es vendido a " + bidder->name + " por $" + to_string(offer) + ".");
        team_mgmt::detachPlayerFromSelections(*career.myTeam, player.name);
        team_mgmt::applyDepartureShock(*career.myTeam, player);
        career.myTeam->players.erase(career.myTeam->players.begin() + index);
        return;
    }

    if (choice == 2) {
        if (counter <= maxOffer && bidder->budget >= counter) {
            career.myTeam->budget += counter;
            bidder->budget = max(0LL, bidder->budget - counter);
            Player moved = player;
            moved.wantsToLeave = false;
            moved.onLoan = false;
            moved.parentClub.clear();
            moved.loanWeeksRemaining = 0;
            bidder->addPlayer(moved);
            emitUiMessage("Contraoferta aceptada. " + player.name + " vendido a " + bidder->name + " por $" +
                          to_string(counter));
            career.addNews(player.name + " es vendido a " + bidder->name + " tras contraoferta por $" +
                           to_string(counter) + ".");
            team_mgmt::detachPlayerFromSelections(*career.myTeam, player.name);
            team_mgmt::applyDepartureShock(*career.myTeam, player);
            career.myTeam->players.erase(career.myTeam->players.begin() + index);
        } else {
            emitUiMessage("La contraoferta fue rechazada.");
        }
        return;
    }

    emitUiMessage("Oferta rechazada.");
}

bool shouldAutoRenewContract(const Team& team, const Player& player, long long demandedWage) {
    if (team.budget < demandedWage * 6) return false;
    if (player.wantsToLeave && player.happiness < 45) return false;
    return player.skill >= team.getAverageSkill() - 5 || team.players.size() <= 18;
}

void updateContracts(Career& career) {
    for (auto& teamRef : career.allTeams) {
        Team* team = &teamRef;
        ensureTeamIdentity(*team);
        for (size_t i = 0; i < team->players.size();) {
            Player& player = team->players[i];
            if (player.contractWeeks > 0) player.contractWeeks--;
            if (player.contractWeeks > 0) {
                ++i;
                continue;
            }

            if (team == career.myTeam) {
                long long demandedWage = max(player.wage, wageDemandFor(player));
                if (player.wantsToLeave) demandedWage = demandedWage * 120 / 100;
                if (promiseAtRisk(player, career.currentWeek)) demandedWage = demandedWage * 108 / 100;
                if (player.skill >= team->getAverageSkill()) demandedWage = demandedWage * 110 / 100;
                int demandedWeeks = randInt(78, 182);
                long long demandedClause = max(player.value * 2,
                                               demandedWage * (player.skill >= team->getAverageSkill() ? 48 : 40));
                emitUiMessage("");
                emitUiMessage("Contrato expirado: " + player.name);
                emitUiMessage("Demanda renovar por " + to_string(demandedWeeks) +
                              " semanas | Salario $" + to_string(demandedWage) +
                              " | Clausula $" + to_string(demandedClause));
                if (player.wantsToLeave) {
                    emitUiMessage(player.name + " esta inquieto por su rol en el club y exige mejores condiciones.");
                }
                if (promiseAtRisk(player, career.currentWeek)) {
                    emitUiMessage("Advertencia: " + player.name + " siente que su promesa de rol fue incumplida.");
                }

                bool renew = shouldAutoRenewContract(*team, player, demandedWage);
                if (contractRenewalDecisionCallback()) {
                    renew = contractRenewalDecisionCallback()(career, *team, player, demandedWage, demandedWeeks,
                                                              demandedClause);
                }

                if (renew) {
                    if (team->budget < demandedWage * 6) {
                        emitUiMessage("No hay margen salarial suficiente. " + player.name + " deja el club.");
                        career.addNews(player.name + " deja el club tras no acordar renovacion.");
                        team_mgmt::detachPlayerFromSelections(*team, player.name);
                        team_mgmt::applyDepartureShock(*team, player);
                        team->players.erase(team->players.begin() + static_cast<long long>(i));
                    } else {
                        player.contractWeeks = demandedWeeks;
                        player.wage = demandedWage;
                        player.releaseClause = demandedClause;
                        player.wantsToLeave = false;
                        player.happiness = clampInt(player.happiness + 6, 1, 99);
                        emitUiMessage("Renovado. Nuevo salario $" + to_string(player.wage));
                        career.addNews(player.name + " renueva contrato en " + team->name + ".");
                        ++i;
                    }
                } else {
                    emitUiMessage(player.name + " deja el club.");
                    career.addNews(player.name + " deja el club al finalizar su contrato.");
                    team_mgmt::detachPlayerFromSelections(*team, player.name);
                    team_mgmt::applyDepartureShock(*team, player);
                    team->players.erase(team->players.begin() + static_cast<long long>(i));
                }
            } else {
                if (team->budget > player.wage * 8 &&
                    randInt(1, 100) <= (promiseAtRisk(player, career.currentWeek) ? 45 : 70)) {
                    player.contractWeeks = randInt(52, 156);
                    player.wage = static_cast<long long>(player.wage * (1.05 + randInt(0, 15) / 100.0));
                    player.releaseClause = max(player.value * 2, player.wage * 45);
                    ++i;
                } else {
                    team_mgmt::detachPlayerFromSelections(*team, player.name);
                    team->players.erase(team->players.begin() + static_cast<long long>(i));
                }
            }
        }
    }
}

void updateManagerReputation(Career& career) {
    if (!career.myTeam) return;
    int rank = career.currentCompetitiveRank();
    if (rank > 0) {
        if (rank <= max(1, career.boardExpectedFinish - 1)) {
            career.managerReputation = clampInt(career.managerReputation + 2, 1, 100);
        } else if (rank > career.boardExpectedFinish + 2) {
            career.managerReputation = clampInt(career.managerReputation - 1, 1, 100);
        }
    }
    if ((career.myTeam->tactics == "Pressing" || career.myTeam->matchInstruction == "Juego directo") &&
        career.myTeam->goalsFor >= max(4, career.currentWeek * 2)) {
        career.managerReputation = clampInt(career.managerReputation + 1, 1, 100);
    }
    int promiseWarnings = 0;
    int youthContributors = 0;
    for (const auto& player : career.myTeam->players) {
        if (promiseAtRisk(player, career.currentWeek)) promiseWarnings++;
        if (player.age <= 21 && player.matchesPlayed >= 4) youthContributors++;
    }
    if (youthContributors >= 2) {
        career.managerReputation = clampInt(career.managerReputation + 1, 1, 100);
    }
    DressingRoomSnapshot dressing = dressing_room_service::buildSnapshot(*career.myTeam, career.currentWeek);
    if (dressing.socialTension <= 2 && career.myTeam->morale >= 68) {
        career.managerReputation = clampInt(career.managerReputation + 1, 1, 100);
    } else if (dressing.socialTension >= 5) {
        career.managerReputation = clampInt(career.managerReputation - 1, 1, 100);
    }
    if (career.boardConfidence <= 25 && promiseWarnings >= 2) {
        career.managerReputation = clampInt(career.managerReputation - 1, 1, 100);
    }
}

void handleManagerStatus(Career& career) {
    if (!career.myTeam) return;
    if (career.boardConfidence >= 20 && career.boardWarningWeeks < 6) return;
    emitUiMessage("");
    emitUiMessage("[Directiva] " + career.myTeam->name + " decide despedirte.");
    career.addNews(career.managerName + " fue despedido de " + career.myTeam->name + ".");
    career.managerReputation = clampInt(career.managerReputation - 8, 10, 100);

    vector<Team*> jobs = buildJobMarket(career, true);
    if (jobs.empty()) {
        for (auto& team : career.allTeams) {
            if (&team != career.myTeam) jobs.push_back(&team);
        }
    }
    if (jobs.empty()) return;

    emitUiMessage("Debes elegir nuevo club:");
    for (size_t i = 0; i < jobs.size(); ++i) {
        emitUiMessage(to_string(i + 1) + ". " + jobs[i]->name + " (" + divisionDisplay(jobs[i]->division) + ")");
    }

    int choice = 1;
    if (managerJobSelectionCallback()) {
        int selected = managerJobSelectionCallback()(career, jobs);
        if (selected >= 0 && selected < static_cast<int>(jobs.size())) {
            choice = selected + 1;
        }
    }
    takeManagerJob(career, jobs[static_cast<size_t>(choice - 1)], "Llega tras un despido reciente.");
}

vector<vector<pair<int, int>>> buildRoundRobinIndexSchedule(int teamCount, bool doubleRound) {
    vector<vector<pair<int, int>>> out;
    if (teamCount < 2) return out;
    vector<int> indices;
    indices.reserve(teamCount + 1);
    for (int i = 0; i < teamCount; ++i) indices.push_back(i);
    if (indices.size() % 2 == 1) indices.push_back(-1);

    int size = static_cast<int>(indices.size());
    int rounds = size - 1;
    for (int round = 0; round < rounds; ++round) {
        vector<pair<int, int>> matches;
        for (int i = 0; i < size / 2; ++i) {
            int a = indices[static_cast<size_t>(i)];
            int b = indices[static_cast<size_t>(size - 1 - i)];
            if (a == -1 || b == -1) continue;
            if (round % 2 == 0) matches.push_back({a, b});
            else matches.push_back({b, a});
        }
        out.push_back(matches);
        int last = indices.back();
        for (int i = size - 1; i > 1; --i) indices[static_cast<size_t>(i)] = indices[static_cast<size_t>(i - 1)];
        indices[1] = last;
    }

    if (doubleRound) {
        int base = static_cast<int>(out.size());
        for (int i = 0; i < base; ++i) {
            vector<pair<int, int>> reverseMatches;
            for (const auto& match : out[static_cast<size_t>(i)]) {
                reverseMatches.push_back({match.second, match.first});
            }
            out.push_back(reverseMatches);
        }
    }
    return out;
}

void simulateBackgroundDivisionWeek(Career& career, const string& divisionId) {
    if (divisionId.empty() || divisionId == career.activeDivision) return;
    vector<Team*> teams = career.getDivisionTeams(divisionId);
    if (teams.size() < 2) return;
    sort(teams.begin(), teams.end(), [](Team* left, Team* right) {
        if (left->tiebreakerSeed != right->tiebreakerSeed) return left->tiebreakerSeed < right->tiebreakerSeed;
        return left->name < right->name;
    });
    auto schedule = buildRoundRobinIndexSchedule(static_cast<int>(teams.size()), true);
    int round = career.currentWeek - 1;
    if (round < 0 || round >= static_cast<int>(schedule.size())) return;

    int headlineMargin = -1;
    string headline;
    for (const auto& match : schedule[static_cast<size_t>(round)]) {
        maybeInvokeIdle();
        Team* home = teams[static_cast<size_t>(match.first)];
        Team* away = teams[static_cast<size_t>(match.second)];
        MatchResult result = playMatch(*home, *away, false, false);
        int margin = abs(result.homeGoals - result.awayGoals);
        if (margin > headlineMargin) {
            headlineMargin = margin;
            headline = home->name + " " + to_string(result.homeGoals) + "-" + to_string(result.awayGoals) + " " +
                       away->name;
        }
    }

    LeagueTable table;
    table.title = divisionDisplay(divisionId);
    table.ruleId = divisionId;
    for (Team* team : teams) table.addTeam(team);
    table.sortTable();
    Team* leader = table.teams.empty() ? nullptr : table.teams.front();
    Team* bottom = table.teams.empty() ? nullptr : table.teams.back();

    if (!headline.empty() && (career.currentWeek % 4 == 0 || headlineMargin >= 3)) {
        string news = "[Mundo] " + divisionDisplay(divisionId) + ": " + headline;
        if (leader) news += " | Lider " + leader->name + " (" + to_string(leader->points) + " pts)";
        career.addNews(news);
    }
    if (leader && randInt(1, 100) <= world_state_service::worldRuleValue("background_leader_story_chance", 14)) {
        career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + leader->name +
                       " instala una historia de temporada con estilo " + leader->clubStyle + ".");
    }
    if (bottom && randInt(1, 100) <= world_state_service::worldRuleValue("background_pressure_story_chance", 10)) {
        career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": crece la presion en " + bottom->name +
                       " por su mala racha.");
    }
    if (bottom && randInt(1, 100) <= world_state_service::worldRuleValue("background_manager_review_chance", 8)) {
        ensureTeamIdentity(*bottom);
        bottom->morale = clampInt(bottom->morale - 3, 0, 100);
        bottom->jobSecurity = clampInt(bottom->jobSecurity - 6, 5, 92);
        career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + bottom->name +
                       " entra en revision de banquillo tras otra semana bajo presion con " + bottom->headCoachName + ".");
    }
    if (leader && leader->youthFacilityLevel + leader->youthCoach >= 130 && randInt(1, 100) <= world_state_service::worldRuleValue("background_youth_promotion_chance", 12)) {
        const int maxSquad = getCompetitionConfig(divisionId).maxSquadSize;
        if (maxSquad <= 0 || static_cast<int>(leader->players.size()) < maxSquad) {
            const string youthPosition = leader->goalsAgainst > leader->goalsFor ? "DEF" : "MED";
            Player promoted = makeRandomPlayer(youthPosition,
                                               max(40, leader->getAverageSkill() - 18),
                                               max(leader->getAverageSkill() - 4, 45),
                                               17,
                                               19);
            promoted.potential = clampInt(promoted.skill + randInt(8, 16), promoted.skill, 95);
            promoted.contractWeeks = 156;
            promoted.wage = max(2500LL, promoted.wage / 2);
            leader->addPlayer(promoted);
            career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + leader->name +
                           " promociona al juvenil " + promoted.name + ".");
        }
    }
    if (leader && randInt(1, 100) <= world_state_service::worldRuleValue("background_injury_story_chance", 8)) {
        Player* keyPlayer = nullptr;
        for (auto& player : leader->players) {
            if (player.injured) continue;
            if (!keyPlayer || player.skill > keyPlayer->skill) keyPlayer = &player;
        }
        if (keyPlayer) {
            keyPlayer->injured = true;
            keyPlayer->injuryType = randInt(0, 1) == 0 ? "Sobrecarga" : "Muscular";
            keyPlayer->injuryWeeks = randInt(1, 3);
            keyPlayer->injuryHistory++;
            career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + leader->name +
                           " pierde por lesion a " + keyPlayer->name + " durante " +
                           to_string(keyPlayer->injuryWeeks) + " semana(s).");
        }
    }
}

void emitSeasonTransitionSummary(const SeasonTransitionSummary& summary) {
    emitUiMessage("");
    emitUiMessage("--- Cierre de Temporada ---");
    for (const string& line : summary.lines) {
        emitUiMessage(line);
    }
}


}  // namespace

void checkAchievements(Career& career) {
    if (!career.myTeam) return;
    if (career.myTeam->wins >= 10 &&
        find(career.achievements.begin(), career.achievements.end(), "10 Victorias") == career.achievements.end()) {
        career.achievements.push_back("10 Victorias");
        emitUiMessage("Logro desbloqueado: 10 Victorias!");
    }
}

// === PHASE 2: SEPARATION OF CONCERNS (Internal Helpers) ===
// These functions extract logical sections of simulateCareerWeek to improve readability
// and reduce cyclomatic complexity of the main function

namespace {

// Process all matches scheduled for this week and return points delta for player team
void processWeekMatches(Career& career, const vector<pair<int, int>>& matches,
                        const unordered_map<TeamId, int>& pointsBefore,
                        int& outMyTeamPointsDelta) {
    LeagueTable northTable;
    LeagueTable southTable;
    bool useGroups = career.usesGroupFormat();
    if (useGroups) {
        northTable = buildCompetitionGroupTable(career, true);
        southTable = buildCompetitionGroupTable(career, false);
    }

    const TeamId managedTeamId = career.getTeamIdFor(career.myTeam);
    for (const auto& match : matches) {
        maybeInvokeIdle();
        const ScheduledMatchRef fixture = scheduledMatchRef(career, match);
        Team* home = fixture.home.team;
        Team* away = fixture.away.team;
        if (!fixture.valid()) {
            emitUiMessage("[Calendario] Partido omitido por indice de equipo invalido.");
            continue;
        }
        const bool userControlledMatch =
            fixture.home.id == managedTeamId || fixture.away.id == managedTeamId;

        const WeekSimulationPresentation presentation =
            weekSimulationPresentation();

        const bool verbose =
            userControlledMatch &&
            presentation == WeekSimulationPresentation::Detailed;

        const bool useMatchCenter =
            userControlledMatch &&
            presentation == WeekSimulationPresentation::MatchCenter;

        team_ai::adjustCpuTactics(*home, *away, career.myTeam);
        team_ai::adjustCpuTactics(*away, *home, career.myTeam);

        bool key = false;
        if (useGroups) {
            int homeGroup = competitionGroupForTeam(career, home);
            int awayGroup = competitionGroupForTeam(career, away);
            if (homeGroup == awayGroup && homeGroup == 0) {
                key = isKeyMatch(northTable, home, away);
            } else if (homeGroup == awayGroup && homeGroup == 1) {
                key = isKeyMatch(southTable, home, away);
            } else {
                key = isKeyMatch(career.leagueTable, home, away);
            }
        } else {
            key = isKeyMatch(career.leagueTable, home, away);
        }
        if (verbose && key) emitUiMessage("[Aviso] Partido clave de la semana.");

        MatchResult result =
            userControlledMatch
                ? playMatch(&career, *home, *away, verbose, key)
                : playMatch(*home, *away, verbose, key);

        if (useMatchCenter) {
            match_center::PlaybackOptions options;
            options.speed = match_center::PlaybackSpeed::Normal;
            options.clearScreenBetweenEvents = true;
            options.showAllEvents = false;

            match_center::showMatchCenter(
                *home,
                *away,
                result,
                options);
        }

        storeMatchAnalysis(career, *home, *away, result, false);
        updateRivalMemoryForUserMatch(career, *home, *away, result);
        if (userControlledMatch) {
            if (RivalryRecord* rivalryRec = getRivalryRecord(career.rivalryDynamics, home->name, away->name)) {
                updateRivalryRecord(*rivalryRec, result.homeGoals, result.awayGoals);
                rivalryRec->lastMeetingWeek = career.currentWeek;
                if (rivalryRec->intensity >= 70) {
                    career.managerStress.pressureIntensity =
                        min(100, career.managerStress.pressureIntensity + 3);
                }
            }
        }
    }

    // Calculate points delta for player team
    if (managedTeamId != kInvalidTeamId) {
        Team* managedTeam = career.getTeamById(managedTeamId);
        const auto pointsIt = pointsBefore.find(managedTeamId);
        if (managedTeam && pointsIt != pointsBefore.end()) {
            outMyTeamPointsDelta = managedTeam->points - pointsIt->second;
        }
    }
}

// Update suspensions, injuries, fitness and training for all teams
void updateTeamPhysicalStates(Career& career,
                              const vector<TeamId>& activeTeamIds,
                              const unordered_map<TeamId, vector<int>>& suspensionsBefore,
                              bool cupWeek) {
    maybeInvokeIdle();
    for (const TeamId teamId : activeTeamIds) {
        const auto snapshotIt = suspensionsBefore.find(teamId);
        if (snapshotIt == suspensionsBefore.end()) continue;
        Team* team = career.getTeamById(teamId);
        if (!team) continue;  // Safety check
        const auto& snapshot = snapshotIt->second;
        size_t limit = min(snapshot.size(), team->players.size());
        for (size_t j = 0; j < limit; ++j) {
            if (snapshot[j] > 0 && team->players[j].matchesSuspended > 0) {
                team->players[j].matchesSuspended--;
            }
        }
        maybeInvokeIdle();
    }

    maybeInvokeIdle();
    for (auto& team : career.allTeams) {
        maybeInvokeIdle();
        healInjuries(team, false);
        recoverFitness(team, 7);
        if (career.myTeam == &team) {
            const InfrastructureModifiers mods = getModifiersFromFacilities(career.infrastructure.levels);
            for (auto& player : team.players) {
                if (player.injured && mods.injuryRecoverySpeed >= 1.2f && randInt(1, 100) <= 35) {
                    player.injuryWeeks = max(0, player.injuryWeeks - 1);
                    if (player.injuryWeeks == 0) {
                        player.injured = false;
                        player.injuryType.clear();
                    }
                }
                if (mods.playerHappiness > 0) {
                    player.happiness = clampInt(player.happiness + static_cast<int>(mods.playerHappiness) / 3, 1, 99);
                }
            }
            team.trainingFacilityLevel = max(team.trainingFacilityLevel, career.infrastructure.levels.trainingGround);
            team.youthFacilityLevel = max(team.youthFacilityLevel, career.infrastructure.levels.youthAcademy);
            team.stadiumLevel = max(team.stadiumLevel, career.infrastructure.levels.stadium);
        }
        maybeInvokeIdle();
        const bool congestedTraining = cupWeek ? team.division == career.activeDivision 
                                                : (career.currentWeek % 5 == 0 && team.division != career.activeDivision);
        player_dev::applyWeeklyTrainingPlan(team, congestedTraining);
        maybeInvokeIdle();
    }
}

void processTransfersPhase(Career& career) {
    maybeInvokeIdle();
    updateContracts(career);
    maybeInvokeIdle();
    processIncomingOffers(career);
    maybeInvokeIdle();
    transfer_market::processCpuTransfers(career);
    maybeInvokeIdle();
    transfer_market::processLoanReturns(career);
}

void applyFinancesPhase(Career& career, const unordered_map<TeamId, int>& pointsBefore) {
    maybeInvokeIdle();
    applyWeeklyFinances(career, pointsBefore);
    maybeInvokeIdle();
    career.leagueTable.sortTable();
    maybeInvokeIdle();
}

// Update manager-specific game state (reputation, objectives, etc.)
void updateManagerGameState(Career& career, int myTeamPointsDelta) {
    maybeInvokeIdle();
    if (career.myTeam) {
        if (career.boardMonthlyObjective.find("puntos") != string::npos) {
            career.boardMonthlyProgress += myTeamPointsDelta;
        }
        updateSquadDynamics(career, myTeamPointsDelta);
        maybeInvokeIdle();
        WorldPulseSummary worldPulse = world_state_service::processWeeklyWorldState(career);
        for (size_t i = 0; i < worldPulse.headlines.size() && i < 2; ++i) {
            emitUiMessage("[Mundo] " + worldPulse.headlines[i]);
        }
    }

    runMonthlyDevelopment(career);
    maybeInvokeIdle();
    progressScoutingAssignments(career);
    maybeInvokeIdle();
    updateShortlistAlerts(career);
    maybeInvokeIdle();
    career.updateDynamicObjectiveStatus();
    maybeInvokeIdle();
    career.updateBoardConfidence();
    maybeInvokeIdle();
    updateManagerReputation(career);
}

// Generate game narrative, events and communications for the week
void generateWeeklyNarrative(Career& career, int myTeamPointsDelta) {
    if (career.myTeam) {
        if (myTeamPointsDelta == 3) {
            career.addNews(career.myTeam->name + " gana en la fecha " + to_string(career.currentWeek) + ".");
        } else if (myTeamPointsDelta == 1) {
            career.addNews(career.myTeam->name + " empata en la fecha " + to_string(career.currentWeek) + ".");
        } else {
            career.addNews(career.myTeam->name + " pierde en la fecha " + to_string(career.currentWeek) + ".");
        }
        if (career.boardWarningWeeks >= 4) {
            career.addNews("La directiva aumenta la presion sobre " + career.myTeam->name + ".");
        }
        generateManagerCareerEvents(career);
        generateWeeklyNarratives(career, myTeamPointsDelta);
    }
}

struct WeekSimulationSnapshots {
    vector<TeamId> activeTeamIds;
    unordered_map<TeamId, vector<int>> suspensionsBefore;
    unordered_map<TeamId, int> pointsBefore;
};

WeekSimulationSnapshots captureWeekSnapshots(const Career& career) {
    WeekSimulationSnapshots snapshots;
    const int activeTeamCount = career.getActiveTeamCount();
    snapshots.activeTeamIds.reserve(static_cast<size_t>(activeTeamCount));
    snapshots.suspensionsBefore.reserve(static_cast<size_t>(activeTeamCount));
    snapshots.pointsBefore.reserve(static_cast<size_t>(activeTeamCount));

    for (int i = 0; i < activeTeamCount; ++i) {
        const TeamId teamId = safeActiveTeamIdAt(career, i);
        const Team* team = career.getTeamById(teamId);
        if (!team) {
            continue;
        }

        snapshots.activeTeamIds.push_back(teamId);
        vector<int> suspensions;
        suspensions.reserve(team->players.size());
        for (const auto& player : team->players) suspensions.push_back(player.matchesSuspended);
        snapshots.suspensionsBefore.emplace(teamId, std::move(suspensions));
        snapshots.pointsBefore.emplace(teamId, team->points);
    }

    return snapshots;
}

void simulateMatchesPhase(Career& career,
                          const vector<pair<int, int>>& matches,
                          const unordered_map<TeamId, int>& pointsBefore,
                          int& myTeamPointsDelta,
                          bool& cupWeek) {
    processWeekMatches(career, matches, pointsBefore, myTeamPointsDelta);
    maybeInvokeIdle();

    for (const auto& division : career.divisions) {
        simulateBackgroundDivisionWeek(career, division.id);
        maybeInvokeIdle();
    }

    cupWeek = career.cupActive &&
              (career.currentWeek == 1 || career.currentWeek % 4 == 0 ||
               career.currentWeek == static_cast<int>(career.schedule.size()));
    if (cupWeek) {
        simulateSeasonCupRound(career);
        maybeInvokeIdle();
    }
}

void updateFitnessPhase(Career& career,
                        const vector<TeamId>& activeTeamIds,
                        const unordered_map<TeamId, vector<int>>& suspensionsBefore,
                        bool cupWeek) {
    updateTeamPhysicalStates(career, activeTeamIds, suspensionsBefore, cupWeek);
    maybeInvokeIdle();
}

void updateGameplaySystemsPhase(Career& career, const vector<pair<int, int>>& matches, int myTeamPointsDelta) {
    if (!career.myTeam) return;

    StressEvent stressEvent;
    if (myTeamPointsDelta >= 3) {
        stressEvent.type = "win";
        stressEvent.stressImpact = -10;
        stressEvent.description = "Victoria conseguida";
    } else if (myTeamPointsDelta == 1) {
        stressEvent.type = "draw";
        stressEvent.stressImpact = -3;
        stressEvent.description = "Empate";
    } else {
        stressEvent.type = "loss";
        stressEvent.stressImpact = +15;
        stressEvent.description = "Derrota sufrida";
    }
    updateManagerStress(career.managerStress, stressEvent);

    const bool hadWin = myTeamPointsDelta >= 3;
    const bool isKeyWeek = career.currentWeek % 4 == 0;
    updateCliqueDynamics(career.dressingRoomDynamics, hadWin, isKeyWeek);

    career.debtStatus = calculateDebtStatus(
        career.myTeam->budget,
        career.myTeam->debt,
        max(1LL, career.myTeam->sponsorWeekly + static_cast<long long>(career.myTeam->fanBase) * 2500LL));
    applyFinancialSanctions(career.debtStatus);

    const TeamId managedTeamId = career.getTeamIdFor(career.myTeam);
    for (const auto& match : matches) {
        const ScheduledMatchRef fixture = scheduledMatchRef(career, match);
        Team* home = fixture.home.team;
        Team* away = fixture.away.team;
        if (!fixture.valid()) continue;

        if (fixture.home.id == managedTeamId) {
            RivalryRecord* rivalryRec = getRivalryRecord(career.rivalryDynamics, home->name, away->name);
            if (rivalryRec) rivalryRec->lastMeetingWeek = career.currentWeek;
            const int intensity = getRivalryIntensity(career.rivalryDynamics, home->name, away->name);
            if (intensity > 70) {
                career.managerStress.pressureIntensity = min(100, career.managerStress.pressureIntensity + 3);
            }
        } else if (fixture.away.id == managedTeamId) {
            RivalryRecord* rivalryRec = getRivalryRecord(career.rivalryDynamics, away->name, home->name);
            if (rivalryRec) rivalryRec->lastMeetingWeek = career.currentWeek;
        }
    }
}

void generateNewsPhase(Career& career, const vector<pair<int, int>>& matches, int myTeamPointsDelta) {
    updateManagerGameState(career, myTeamPointsDelta);
    maybeInvokeIdle();

    updateGameplaySystemsPhase(career, matches, myTeamPointsDelta);

    if (career.myTeam) ensureTeamIdentity(*career.myTeam);
    dispatchWeeklyStaffBriefing(career);
    maybeInvokeIdle();
    weeklyDashboard(career);
    maybeInvokeIdle();
    applyClubEvent(career);
    maybeInvokeIdle();

    generateWeeklyNarrative(career, myTeamPointsDelta);
    maybeInvokeIdle();
}

void advanceCalendarPhase(Career& career) {
    handleManagerStatus(career);
    career.currentWeek++;
    checkAchievements(career);
    career.syncActiveHumanManager();

    int milestoneWeek = -999;
    if (career_events::checkCareerMilestone(career, milestoneWeek)) {
        string description = career_events::GetMilestoneDescription(milestoneWeek, career);
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::CareerMilestone,
            "Hito alcanzado",
            description);
        emitUiMessage("[Hito] " + description);
    }

    if (career.currentWeek > static_cast<int>(career.schedule.size())) {
        emitSeasonTransitionSummary(endSeason(career));
    }
}

}  // anonymous namespace
// === END SEPARATION OF CONCERNS ===

// simulateCareerWeek is intentionally organized into discrete phases to keep weekly simulation
// behavior predictable and easy to maintain. Each phase has a single responsibility:
// 1. simulateMatchesPhase: resuelve los partidos de la jornada.
// 2. updateFitnessPhase: actualiza forma, lesiones y suspensiones.
// 3. processTransfersPhase: ejecuta ofertas y movimientos pendientes.
// 4. applyFinancesPhase: aplica ingresos y gastos semanales.
// 5. generateNewsPhase: crea noticias y eventos derivados de la semana.
// 6. advanceCalendarPhase: avanza la semana y maneja transicion de temporada.
//
// Esto es una mejora de claridad; no altera la lógica de simulacion ya existente.
void simulateCareerWeek(Career& career) {
    if (career.getActiveTeamCount() == 0 || career.schedule.empty()) {
        emitUiMessage("No hay calendario disponible.");
        return;
    }
    if (career.currentWeek < 1) {
        emitUiMessage("Semana inválida detectada; reiniciando a semana 1.");
        career.currentWeek = 1;
    }
    if (career.currentWeek > static_cast<int>(career.schedule.size())) {
        emitSeasonTransitionSummary(endSeason(career));
        return;
    }

    emitUiMessage("");
    emitUiMessage("Simulando semana " + to_string(career.currentWeek) + "...");
    career.leagueTable.sortTable();

    WeekSimulationSnapshots snapshots = captureWeekSnapshots(career);
    const auto& matches = career.schedule[static_cast<size_t>(career.currentWeek - 1)];
    int myTeamPointsDelta = 0;
    bool cupWeek = career.cupActive &&
                   (career.currentWeek == 1 || career.currentWeek % 4 == 0 ||
                    career.currentWeek == static_cast<int>(career.schedule.size()));

    simulateMatchesPhase(career, matches, snapshots.pointsBefore, myTeamPointsDelta, cupWeek);
    updateFitnessPhase(career, snapshots.activeTeamIds, snapshots.suspensionsBefore, cupWeek);
    processTransfersPhase(career);
    applyFinancesPhase(career, snapshots.pointsBefore);
    generateNewsPhase(career, matches, myTeamPointsDelta);
    advanceCalendarPhase(career);
}

