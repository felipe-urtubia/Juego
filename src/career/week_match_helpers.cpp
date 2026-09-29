#include "week_match_helpers.h"
#include "career/career_modules.h"
#include "engine/rival_ai.h"
#include "utils.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace std;

namespace career_week_matches {
namespace {
void pushUniqueLimited(vector<string>& lines, const string& line, size_t limit = 3) {
    if (line.empty()) return;
    if (find(lines.begin(), lines.end(), line) != lines.end()) return;
    if (lines.size() >= limit) return;
    lines.push_back(line);
}

}  // namespace

ScheduledTeamRef scheduledTeamRef(Career& career, int index) {
    TeamRepository teams(career);
    const TeamId id = teams.getActiveTeamIdAt(index);
    return {id, teams.getTeamById(id)};
}

ScheduledMatchRef scheduledMatchRef(Career& career, const pair<int, int>& match) {
    return {scheduledTeamRef(career, match.first), scheduledTeamRef(career, match.second)};
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

}  // namespace career_week_matches
