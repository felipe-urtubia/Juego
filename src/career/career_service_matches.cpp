#include "career/career_service.h"
#include "week_match_helpers.h"
#include "ai/team_ai.h"
#include "career/career_reports.h"
#include "career/career_runtime.h"
#include "career/match_analysis_store.h"
#include "engine/rivalry_system.h"
#include "simulation.h"
#include "simulation/match_center.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

namespace {

void maybeInvokeIdle() {
    if (IdleCallback callback = idleCallback()) {
        callback();
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

}  // namespace

void CareerService::simulateWeekMatches(const vector<pair<int, int>>& matches,
    const unordered_map<TeamId, int>& pointsBefore,
    int& outMyTeamPointsDelta) {
    Career& career = career_;
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
        const career_week_matches::ScheduledMatchRef fixture = career_week_matches::scheduledMatchRef(career, match);
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

        MatchResult result;

        if (useMatchCenter) {
            const bool userControlsHome =
                fixture.home.id == managedTeamId;

            Team& controlledTeam =
                userControlsHome ? *home : *away;

            match_engine::InteractiveMatchState lastInteractiveState;

            result = simulateInteractiveMatch(
                &career,
                *home,
                *away,
                userControlsHome,
                [&](const match_engine::InteractiveMatchState& state) {
                    lastInteractiveState = state;

                    LiveMatchStateCallback stateCallback =
                        liveMatchStateCallback();

                    if (stateCallback) {
                        stateCallback(
                            home->name,
                            away->name,
                            state);
                    }

                    if (LiveMatchDecisionCallback decisionCallback =
                            liveMatchDecisionCallback()) {

                        match_engine::ManagerDecision decision =
                            decisionCallback(state);

                        if ((decision.changeTactics ||
                             decision.type ==
                                 match_engine::ManagerDecisionType::ChangeTactics) &&
                            !decision.tactics.empty()) {

                            lastInteractiveState.currentTactics =
                                decision.tactics;
                        }

                        if ((decision.changeInstruction ||
                             decision.type ==
                                 match_engine::ManagerDecisionType::ChangeInstruction) &&
                            !decision.instruction.empty()) {

                            lastInteractiveState.currentInstruction =
                                decision.instruction;
                        }

                        if (decision.type ==
                                match_engine::ManagerDecisionType::Substitute &&
                            lastInteractiveState.substitutionsUsed < 5) {

                            auto outgoingIt = std::find(
                                lastInteractiveState.activeXi.begin(),
                                lastInteractiveState.activeXi.end(),
                                decision.playerOutIndex);

                            auto incomingIt = std::find(
                                lastInteractiveState.availableBench.begin(),
                                lastInteractiveState.availableBench.end(),
                                decision.playerInIndex);

                            if (outgoingIt !=
                                    lastInteractiveState.activeXi.end() &&
                                incomingIt !=
                                    lastInteractiveState.availableBench.end()) {

                                *outgoingIt = decision.playerInIndex;

                                lastInteractiveState.availableBench.erase(
                                    incomingIt);

                                ++lastInteractiveState.substitutionsUsed;
                            }
                        }

                        return decision;
                    }

                    if (stateCallback) {
                        return match_engine::ManagerDecision{};
                    }

                    return match_center::askManagerDecision(
                        controlledTeam,
                        state);
                },
                key,
                false);

            if (LiveMatchStateCallback callback =
                    liveMatchStateCallback()) {
                match_engine::InteractiveMatchState finalState =
                    lastInteractiveState;
                finalState.minute = 90;
                finalState.userIsHome = userControlsHome;
                finalState.homeGoals = result.homeGoals;
                finalState.awayGoals = result.awayGoals;
                finalState.homeShots = result.homeShots;
                finalState.awayShots = result.awayShots;
                finalState.homePossession = result.homePossession;
                finalState.awayPossession = result.awayPossession;
                finalState.timelineEventsDetailed =
                    result.timeline.events;

                callback(
                    home->name,
                    away->name,
                    finalState);
            } else {
                match_center::showInteractiveFinalSummary(
                    *home,
                    *away,
                    result);
            }
        } else {
            result =
                userControlledMatch
                    ? playMatch(
                          &career,
                          *home,
                          *away,
                          verbose,
                          key)
                    : playMatch(
                          *home,
                          *away,
                          verbose,
                          key);
        }

        career_match_analysis::storeMatchAnalysis(career, *home, *away, result, false);
        career_week_matches::updateRivalMemoryForUserMatch(career, *home, *away, result);
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
