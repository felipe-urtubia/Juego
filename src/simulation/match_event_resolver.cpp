#include "simulation/match_event_resolver.h"

#include "simulation/match_engine_internal.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

int weightedCandidate(const Team& team,
                      const vector<int>& xi,
                      bool attackersOnly,
                      bool creatorsOnly) {
    vector<pair<int, int>> weighted;
    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(idx)];
        const string pos = normalizePosition(player.position);
        const string role = match_internal::compactToken(player.role);
        if (attackersOnly && pos == "ARQ") continue;
        if (creatorsOnly) {
            if (!(pos == "MED" || role == "enganche" || role == "organizador" || role == "interior")) continue;
        } else if (attackersOnly && pos == "DEF" && role != "carrilero") {
            continue;
        }
        int weight = player.attack * 4 + player.currentForm * 2 + player.skill * 2 + player.consistency;
        if (pos == "DEL") weight += 40;
        if (role == "poacher" || role == "objetivo") weight += 25;
        if (role == "enganche" || role == "organizador") weight += creatorsOnly ? 35 : 10;
        if (playerHasTrait(player, "Competidor")) weight += 12;
        if (playerHasTrait(player, "Llega al area")) weight += 8;
        if (player.injured) weight /= 2;
        weighted.push_back({idx, max(1, weight)});
    }
    if (weighted.empty()) return xi.empty() ? -1 : xi.front();

    int total = 0;
    for (const auto& entry : weighted) total += entry.second;
    int pick = randInt(1, max(1, total));
    for (const auto& entry : weighted) {
        pick -= entry.second;
        if (pick <= 0) return entry.first;
    }
    return weighted.back().first;
}

int goalkeeperIndex(const Team& team, const vector<int>& xi) {
    int bestIdx = -1;
    int bestScore = -1;
    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(idx)];
        int score = player.defense * 3 + player.skill * 2 + player.currentForm + player.consistency;
        if (normalizePosition(player.position) == "ARQ") score += 60;
        if (score > bestScore) {
            bestIdx = idx;
            bestScore = score;
        }
    }
    return bestIdx;
}

double clampProbability(double value) {
    return clampValue(value, 0.02, 0.66);
}

}  // namespace

namespace match_event_resolver {

ChanceResolutionOutput resolveChance(const Team& attacking,
                                     const Team& defending,
                                     const vector<int>& attackingXI,
                                     const vector<int>& defendingXI,
                                     const ChanceResolutionInput& input) {
    ChanceResolutionOutput output;
    output.shooterIndex = weightedCandidate(attacking, attackingXI, true, false);
    output.assisterIndex = weightedCandidate(attacking, attackingXI, false, true);

    const Player* shooter = (output.shooterIndex >= 0 && output.shooterIndex < static_cast<int>(attacking.players.size()))
                                ? &attacking.players[static_cast<size_t>(output.shooterIndex)]
                                : nullptr;
    const Player* assister = (output.assisterIndex >= 0 && output.assisterIndex < static_cast<int>(attacking.players.size()) &&
                              output.assisterIndex != output.shooterIndex)
                                 ? &attacking.players[static_cast<size_t>(output.assisterIndex)]
                                 : nullptr;
    const int keeperIdx = goalkeeperIndex(defending, defendingXI);
    const Player* keeper = (keeperIdx >= 0 && keeperIdx < static_cast<int>(defending.players.size()))
                               ? &defending.players[static_cast<size_t>(keeperIdx)]
                               : nullptr;

    const string attackerName = shooter ? shooter->name : attacking.name;
    const double quality = clampValue(input.chanceQuality, 0.05, input.bigChance ? 0.54 : 0.34);
    output.expectedGoals = quality;

    const double finishing = shooter ? (shooter->attack * 0.44 + shooter->skill * 0.18 + shooter->currentForm * 0.16 +
                                        shooter->consistency * 0.10 + shooter->bigMatches * 0.12)
                                     : 55.0;
    const double goalkeeper = keeper ? (keeper->defense * 0.52 + keeper->skill * 0.28 + keeper->currentForm * 0.12 +
                                        keeper->consistency * 0.08)
                                     : 48.0;
    const double fatiguePenalty = shooter ? max(0, 60 - shooter->fitness) / 150.0 : 0.0;
    const double onTargetProbability =
        clampProbability(0.20 + quality * 0.52 + finishing / 360.0 + input.finishingSupport * 0.10 -
                         input.defensivePressure * 0.10 - input.goalkeeperQuality * 0.05 - fatiguePenalty);
    const double goalProbability =
        clampProbability(0.02 + quality * 0.40 + finishing / 410.0 + input.finishingSupport * 0.08 -
                         goalkeeper / 255.0 - input.defensivePressure * 0.12 -
                         input.goalkeeperQuality * 0.05 - fatiguePenalty + (input.bigChance ? 0.06 : 0.0));

    output.attemptEvent.minute = input.minute;
    output.attemptEvent.teamName = attacking.name;
    output.attemptEvent.playerName = attackerName;
    output.attemptEvent.type = input.bigChance ? MatchEventType::BigChance : MatchEventType::Shot;
    output.attemptEvent.zone = input.zone;
    output.attemptEvent.description = attackerName +
                                      (input.bigChance ? " queda frente al arco." : " encuentra linea de tiro.");
    output.attemptEvent.impact.homeExpectedGoalsDelta = input.attackingTeamIsHome ? quality : 0.0;
    output.attemptEvent.impact.awayExpectedGoalsDelta = input.attackingTeamIsHome ? 0.0 : quality;
    output.attemptEvent.impact.homeShotsDelta = input.attackingTeamIsHome ? 1 : 0;
    output.attemptEvent.impact.awayShotsDelta = input.attackingTeamIsHome ? 0 : 1;

    output.onTarget = rand01() <= onTargetProbability;
    if (output.onTarget) {
        if (input.attackingTeamIsHome) output.attemptEvent.impact.homeShotsOnTargetDelta = 1;
        else output.attemptEvent.impact.awayShotsOnTargetDelta = 1;
    }

    MatchEvent resolution;
    resolution.minute = input.minute;
    resolution.teamName = attacking.name;
    resolution.playerName = attackerName;
    resolution.zone = input.zone;

    if (output.onTarget && rand01() <= goalProbability) {
        output.scored = true;
        resolution.type = MatchEventType::Goal;
        resolution.description = attackerName + " define con precision";
        if (assister) resolution.description += " tras pase de " + assister->name;
        if (input.attackingTeamIsHome) resolution.impact.homeGoalsDelta = 1;
        else resolution.impact.awayGoalsDelta = 1;
    } else if (output.onTarget) {
        resolution.type = MatchEventType::Save;
        resolution.teamName = defending.name;
        resolution.playerName = keeper ? keeper->name : attackerName;
        resolution.description = keeper ? keeper->name + " responde y evita el gol" : "el portero contiene el remate";
    } else {
        resolution.type = MatchEventType::Miss;
        resolution.description = attackerName + " no logra direccionar bien el remate";
    }
    output.outcomeEvents.push_back(resolution);

    const bool cornerLikely = !output.scored &&
                              (rand01() <= (input.bigChance ? 0.28 : 0.18) + max(0.0, input.attackingEdge) * 0.01);
    if (cornerLikely) {
        output.wonCorner = true;
        MatchEvent corner;
        corner.minute = input.minute + 1;
        corner.teamName = attacking.name;
        corner.type = MatchEventType::Corner;
        corner.zone = input.zone;
        corner.description = attacking.name + " fuerza un corner";
        if (input.attackingTeamIsHome) corner.impact.homeCornersDelta = 1;
        else corner.impact.awayCornersDelta = 1;
        output.outcomeEvents.push_back(corner);
    }

    return output;
}

}  // namespace match_event_resolver
