#include "career/career_service.h"
#include "career/career_runtime.h"
#include "career/player_development.h"
#include "engine/facilities_system.h"
#include "simulation.h"
#include "utils.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

using namespace std;

namespace {

void maybeInvokeIdle() {
    if (IdleCallback callback = idleCallback()) {
        callback();
    }
}

}  // namespace
void CareerService::updatePlayerPhysicalState(
                              const vector<TeamId>& activeTeamIds,
                              const unordered_map<TeamId, vector<int>>& suspensionsBefore,
                              bool cupWeek) {
    Career& career = career_;
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
