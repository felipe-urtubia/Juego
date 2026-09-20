#include "simulation/match_event_generator.h"

#include "career/career_runtime.h"
#include "simulation/match_engine_internal.h"
#include "simulation/match_event_resolver.h"
#include "simulation/player_condition.h"
#include "simulation/match_resolution.h"
#include "simulation/tactics_engine.h"
#include "utils/utils.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {

int pickPhysicalRiskPlayer(const Team& team, const vector<int>& activeXI) {
    int bestIndex = -1;
    int bestScore = -1;
    for (int idx : activeXI) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(idx)];
        int score = player_condition::workloadRisk(player, team) + player_condition::relapseRisk(player, team) / 2;
        score += max(0, player.age - 29) * 2;
        if (score > bestScore) {
            bestScore = score;
            bestIndex = idx;
        }
    }
    return bestIndex;
}

void removeActivePlayer(vector<int>& activeXI, int playerIndex) {
    activeXI.erase(remove(activeXI.begin(), activeXI.end(), playerIndex), activeXI.end());
}

string progressionDescription(const Team& team, bool directTransition) {
    if (team.matchInstruction == "Por bandas") return team.name + " cambia de ritmo y rompe por fuera";
    if (team.matchInstruction == "Laterales altos") return team.name + " empuja con los laterales y gana altura";
    if (team.matchInstruction == "Balon parado") return team.name + " fuerza una accion de pelota quieta";
    if (team.matchInstruction == "Pausar juego") return team.name + " cocina la jugada con paciencia";
    return directTransition ? team.name + " gana metros con una ruptura directa"
                            : team.name + " progresa entre lineas";
}

string buildUpDescription(const Team& team, bool directTransition) {
    if (team.matchInstruction == "Por bandas") return team.name + " ensancha el campo y busca el centro";
    if (team.matchInstruction == "Laterales altos") return team.name + " carga el area con sus laterales";
    if (team.matchInstruction == "Balon parado") return team.name + " inclina el juego para forzar una falta lateral";
    if (team.matchInstruction == "Pausar juego") return team.name + " pausa y vuelve a circular en campo rival";
    return directTransition ? team.name + " acelera la transicion"
                            : team.name + " madura el ataque en campo rival";
}

double instructionSetPieceBoost(const Team& team, const TeamMatchSnapshot& snapshot) {
    double boost = snapshot.setPieceThreat / 420.0;
    if (team.matchInstruction == "Balon parado") boost += 0.11;
    if (team.matchInstruction == "Laterales altos" || team.matchInstruction == "Por bandas") boost += 0.04;
    if (team.matchInstruction == "Pausar juego") boost -= 0.02;
    return clampValue(boost, 0.02, 0.34);
}

double instructionBigChanceBoost(const Team& team, const TeamMatchSnapshot& snapshot) {
    double boost = 0.0;
    if (team.matchInstruction == "Juego directo") boost += 0.06 + snapshot.tacticalProfile.directness * 0.04;
    if (team.matchInstruction == "Por bandas") boost += 0.04 + snapshot.tacticalProfile.width * 0.03;
    if (team.matchInstruction == "Laterales altos") boost += 0.03;
    if (team.matchInstruction == "Pausar juego") boost -= 0.03;
    return clampValue(boost, -0.04, 0.16);
}

bool replaceInjuredPlayer(Team& team,
                          vector<int>& activeXI,
                          vector<int>& participants,
                          int playerIndex,
                          int minute,
                          MatchTimeline& timeline) {
    if (playerIndex < 0 || playerIndex >= static_cast<int>(team.players.size())) return false;
    const string targetPos = normalizePosition(team.players[static_cast<size_t>(playerIndex)].position);
    const int replacement = match_internal::bestBenchReplacement(team, activeXI, targetPos);
    if (replacement < 0) {
        removeActivePlayer(activeXI, playerIndex);
        return false;
    }

    for (int& idx : activeXI) {
        if (idx == playerIndex) {
            idx = replacement;
            break;
        }
    }
    if (find(participants.begin(), participants.end(), replacement) == participants.end()) {
        participants.push_back(replacement);
    }

    MatchEvent sub;
    sub.minute = clampInt(minute + 1, 1, 90);
    sub.teamName = team.name;
    sub.playerName = team.players[static_cast<size_t>(replacement)].name;
    sub.type = MatchEventType::Substitution;
    sub.description = team.players[static_cast<size_t>(playerIndex)].name + " sale lesionado; entra " +
                      team.players[static_cast<size_t>(replacement)].name;
    timeline.events.push_back(sub);
    return true;
}

}  // namespace

namespace match_event_generator {

void playPhaseSequences(Team& attacking,
                        Team& defending,
                        const vector<int>& attackingXI,
                        const vector<int>& defendingXI,
                        const TeamMatchSnapshot& attackingSnapshot,
                        const TeamMatchSnapshot& defendingSnapshot,
                        bool attackingIsHome,
                        int minuteStart,
                        int minuteEnd,
                        int possessionChains,
                        int progressionCount,
                        int attackCount,
                        int chanceCount,
                        double attackingEdge,
                        double defensiveRisk,
                        MatchTimeline& timeline,
                        MatchStats& stats,
                        vector<GoalContribution>& goals) {
    if (attackCount <= 0 && chanceCount <= 0) return;

    const double transitionThreat = tactics_engine::transitionThreatWeight(attackingSnapshot.tacticalProfile);
    const double attackingSecurity = tactics_engine::defensiveSecurityWeight(attackingSnapshot.tacticalProfile);
    const double progressionSuccess = clampValue(0.28 + attackingSnapshot.pressResistance / 210.0 +
                                                     attackingSnapshot.chanceCreation / 260.0 +
                                                     attackingSnapshot.tacticalProfile.width * 0.06 +
                                                     possessionChains * 0.010 + attackingEdge * 0.0015 -
                                                     defendingSnapshot.pressingLoad / 420.0 +
                                                     defensiveRisk * 0.08,
                                                 0.16, 0.82);
    const double setPieceBoost = instructionSetPieceBoost(attacking, attackingSnapshot);
    const double bigChanceBoost = instructionBigChanceBoost(attacking, attackingSnapshot);
    const double baseChanceReachProbability = clampValue(
        0.08 + static_cast<double>(chanceCount + 1) / static_cast<double>(max(1, attackCount + 3)) +
            attackingEdge * 0.0025 + transitionThreat * 0.10 + defensiveRisk * 0.08 + attackingSecurity * 0.03 +
            attackingSnapshot.finishingQuality / 420.0 - defendingSnapshot.defensiveShape / 620.0,
        0.06, 0.66);

    for (int i = 0; i < attackCount; ++i) {
        if (i % 5 == 0) {
            if (IdleCallback cb = idleCallback()) {
                cb();
            }
        }
        const int minute = randInt(minuteStart, minuteEnd);
        const double sequenceChanceReachProbability =
            clampValue(baseChanceReachProbability + (i < chanceCount ? 0.10 : -0.04), 0.06, 0.76);
        const bool directTransition = attacking.tactics == "Counter" ||
                                      attacking.matchInstruction == "Juego directo" ||
                                      rand01() <= clampValue(0.18 + transitionThreat * 0.45, 0.10, 0.62);

        if (i < progressionCount && rand01() <= progressionSuccess) {
            MatchEvent progression;
            progression.minute = max(minuteStart, minute - 2);
            progression.teamName = attacking.name;
            progression.type = MatchEventType::Progression;
            progression.description = progressionDescription(attacking, directTransition);
            match_stats::pushEvent(timeline, stats, progression);
        }

        MatchEvent buildUp;
        buildUp.minute = max(minuteStart, minute - 1);
        buildUp.teamName = attacking.name;
        buildUp.type = directTransition ? MatchEventType::Counterattack : MatchEventType::AttackBuildUp;
        buildUp.description = buildUpDescription(attacking, directTransition);
        if (i < chanceCount) {
            if (attackingIsHome) buildUp.impact.homeDangerousAttacksDelta = 1;
            else buildUp.impact.awayDangerousAttacksDelta = 1;
        }
        match_stats::pushEvent(timeline, stats, buildUp);

        if (rand01() > sequenceChanceReachProbability) {
            MatchEvent interruption;
            interruption.minute = minute;
            interruption.teamName = rand01() <= 0.55 ? defending.name : attacking.name;
            const double interruptionRoll = rand01();
            if (interruptionRoll <= 0.38) {
                interruption.type = MatchEventType::Offside;
                interruption.description = attacking.name + " rompe mal la linea y queda adelantado";
            } else if (interruptionRoll <= 0.72) {
                interruption.type = MatchEventType::Foul;
                interruption.description = defending.name + " frena la jugada antes de que llegue al area";
            } else {
                interruption.type = MatchEventType::Progression;
                interruption.description = defending.name + " recupera y obliga a reiniciar la posesion";
            }
            match_stats::pushEvent(timeline, stats, interruption);

            const double setPieceFollowUp =
                clampValue(0.04 + attackingSnapshot.setPieceThreat / 260.0 +
                               max(0.0, attackingEdge) * 0.003 +
                               setPieceBoost +
                               (interruption.type == MatchEventType::Foul ? 0.06 : 0.0),
                           0.04,
                           0.26);
            if (rand01() <= setPieceFollowUp) {
                MatchEvent setPiece;
                setPiece.minute = clampInt(minute + 1, minuteStart, minuteEnd);
                setPiece.teamName = attacking.name;
                setPiece.type = MatchEventType::Corner;
                setPiece.description = attacking.matchInstruction == "Balon parado"
                                           ? attacking.name + " activa una rutina preparada a balon parado"
                                           : attacking.name + " aprovecha la accion para cargar el area a balon parado";
                if (attackingIsHome) setPiece.impact.homeCornersDelta = 1;
                else setPiece.impact.awayCornersDelta = 1;
                match_stats::pushEvent(timeline, stats, setPiece);

                ChanceResolutionInput setPieceInput;
                setPieceInput.minute = clampInt(setPiece.minute + 1, minuteStart, minuteEnd);
                setPieceInput.bigChance = false;
                setPieceInput.attackingTeamIsHome = attackingIsHome;
                setPieceInput.chanceQuality =
                    clampValue(0.07 + attackingSnapshot.setPieceThreat / 520.0 +
                                   setPieceBoost * 0.45 +
                                   attackingSnapshot.finishingQuality / 780.0 -
                                   defendingSnapshot.goalkeeperPower / 1200.0,
                               0.06,
                               0.24);
                setPieceInput.attackingEdge = attackingEdge / 2.0;
                setPieceInput.defensivePressure = max(0.10, defendingSnapshot.defensePower / 220.0);
                setPieceInput.finishingSupport = attackingSnapshot.finishingQuality / 120.0;
                setPieceInput.goalkeeperQuality = defendingSnapshot.goalkeeperPower / 100.0;

                ChanceResolutionOutput setPieceOutput =
                    match_event_resolver::resolveChance(attacking, defending, attackingXI, defendingXI, setPieceInput);
                match_stats::pushEvent(timeline, stats, setPieceOutput.attemptEvent);
                for (const MatchEvent& event : setPieceOutput.outcomeEvents) {
                    match_stats::pushEvent(timeline, stats, event);
                }
                if (setPieceOutput.scored && setPieceOutput.shooterIndex >= 0 &&
                    setPieceOutput.shooterIndex < static_cast<int>(attacking.players.size())) {
                    GoalContribution contribution;
                    contribution.minute = setPieceInput.minute;
                    contribution.scorerName = attacking.players[static_cast<size_t>(setPieceOutput.shooterIndex)].name;
                    if (setPieceOutput.assisterIndex >= 0 &&
                        setPieceOutput.assisterIndex < static_cast<int>(attacking.players.size()) &&
                        setPieceOutput.assisterIndex != setPieceOutput.shooterIndex) {
                        contribution.assisterName =
                            attacking.players[static_cast<size_t>(setPieceOutput.assisterIndex)].name;
                    }
                    goals.push_back(contribution);
                }
            }
            continue;
        }

        const bool bigChance =
            rand01() <= clampValue(0.10 + max(0.0, attackingEdge) * 0.018 + transitionThreat * 0.12 -
                                       defensiveRisk * 0.05 + bigChanceBoost,
                                   0.10, 0.42);

        ChanceResolutionInput input;
        input.minute = minute;
        input.bigChance = bigChance;
        input.attackingTeamIsHome = attackingIsHome;
        input.chanceQuality = clampValue(0.06 + attackingEdge * 0.010 + bigChance * 0.18 +
                                             transitionThreat * 0.12 -
                                             tactics_engine::defensiveSecurityWeight(defendingSnapshot.tacticalProfile) * 0.05 +
                                             bigChanceBoost * 0.22 +
                                             attackingSnapshot.finishingQuality / 500.0 -
                                             defendingSnapshot.goalkeeperPower / 950.0,
                                         0.05, 0.52);
        input.attackingEdge = attackingEdge;
        input.defensivePressure = max(0.14, defendingSnapshot.defensePower / 185.0 + defendingSnapshot.pressingLoad / 520.0);
        input.finishingSupport = attackingSnapshot.finishingQuality / 100.0;
        input.goalkeeperQuality = defendingSnapshot.goalkeeperPower / 100.0;

        ChanceResolutionOutput output =
            match_event_resolver::resolveChance(attacking, defending, attackingXI, defendingXI, input);
        match_stats::pushEvent(timeline, stats, output.attemptEvent);
        for (const MatchEvent& event : output.outcomeEvents) {
            match_stats::pushEvent(timeline, stats, event);
        }
        if (bigChance) {
            if (attackingIsHome) stats.homeBigChances++;
            else stats.awayBigChances++;
        }
        if (output.scored && output.shooterIndex >= 0 && output.shooterIndex < static_cast<int>(attacking.players.size())) {
            GoalContribution contribution;
            contribution.minute = minute;
            contribution.scorerName = attacking.players[static_cast<size_t>(output.shooterIndex)].name;
            if (output.assisterIndex >= 0 && output.assisterIndex < static_cast<int>(attacking.players.size()) &&
                output.assisterIndex != output.shooterIndex) {
                contribution.assisterName = attacking.players[static_cast<size_t>(output.assisterIndex)].name;
            }
            goals.push_back(contribution);
        }
    }
}

void registerDiscipline(Team& team,
                        vector<int>& activeXI,
                        bool teamIsHome,
                        double intensity,
                        MatchTimeline& timeline,
                        MatchStats& stats,
                        vector<string>& cautionedPlayers,
                        vector<string>& sentOffPlayers,
                        vector<string>& yellowCardPlayers,
                        vector<string>& redCardPlayers) {
    MatchEvent foulEvent;
    foulEvent.minute = randInt(timeline.phases.back().minuteStart, timeline.phases.back().minuteEnd);
    foulEvent.teamName = team.name;
    foulEvent.type = MatchEventType::Foul;
    foulEvent.description = team.name + " corta el ritmo con una falta tactica";
    int fouls = clampInt(static_cast<int>(round(intensity * (1.0 + team.pressingIntensity * 0.22))) + randInt(0, 2), 1, 5);
    if (teamIsHome) foulEvent.impact.homeFoulsDelta = fouls;
    else foulEvent.impact.awayFoulsDelta = fouls;
    match_stats::pushEvent(timeline, stats, foulEvent);

    if (activeXI.empty()) return;
    double cautionProbability = clampValue(0.10 + intensity * 0.08 + max(0, team.pressingIntensity - 3) * 0.04, 0.05, 0.34);
    if (rand01() > cautionProbability) return;

    const int idx = activeXI[static_cast<size_t>(randInt(0, static_cast<int>(activeXI.size()) - 1))];
    if (idx < 0 || idx >= static_cast<int>(team.players.size())) return;
    const string playerName = team.players[static_cast<size_t>(idx)].name;
    const bool alreadyBooked = find(cautionedPlayers.begin(), cautionedPlayers.end(), playerName) != cautionedPlayers.end();
    const bool straightRed = rand01() <= clampValue(0.02 + max(0, team.pressingIntensity - 3) * 0.02 + intensity * 0.02, 0.02, 0.10);

    if (alreadyBooked || straightRed) {
        sentOffPlayers.push_back(playerName);
        redCardPlayers.push_back(playerName);
        removeActivePlayer(activeXI, idx);

        MatchEvent red;
        red.minute = randInt(timeline.phases.back().minuteStart, timeline.phases.back().minuteEnd);
        red.teamName = team.name;
        red.playerName = playerName;
        red.type = MatchEventType::RedCard;
        red.description = straightRed ? playerName + " ve la roja directa por una entrada temeraria"
                                      : playerName + " es expulsado por doble amarilla";
        if (teamIsHome) red.impact.homeRedCardsDelta = 1;
        else red.impact.awayRedCardsDelta = 1;
        match_stats::pushEvent(timeline, stats, red);
        return;
    }

    cautionedPlayers.push_back(playerName);
    yellowCardPlayers.push_back(playerName);
    MatchEvent yellow;
    yellow.minute = randInt(timeline.phases.back().minuteStart, timeline.phases.back().minuteEnd);
    yellow.teamName = team.name;
    yellow.playerName = playerName;
    yellow.type = MatchEventType::YellowCard;
    yellow.description = playerName + " ve la amarilla por llegar tarde";
    if (teamIsHome) yellow.impact.homeYellowCardsDelta = 1;
    else yellow.impact.awayYellowCardsDelta = 1;
    match_stats::pushEvent(timeline, stats, yellow);
}

void maybeInjure(Team& team,
                 vector<int>& activeXI,
                 vector<int>& participants,
                 double injuryRisk,
                 int minuteStart,
                 int minuteEnd,
                 MatchTimeline& timeline,
                 vector<string>& injuredPlayers) {
    if (activeXI.empty()) return;
    const int idx = pickPhysicalRiskPlayer(team, activeXI);
    if (idx < 0 || idx >= static_cast<int>(team.players.size())) return;
    const Player& player = team.players[static_cast<size_t>(idx)];
    const double triggerChance =
        clampValue(injuryRisk +
                       player_condition::workloadRisk(player, team) / 350.0 +
                       player_condition::relapseRisk(player, team) / 520.0,
                   0.04,
                   0.36);
    if (rand01() > triggerChance) return;
    const string playerName = team.players[static_cast<size_t>(idx)].name;
    if (find(injuredPlayers.begin(), injuredPlayers.end(), playerName) != injuredPlayers.end()) return;

    injuredPlayers.push_back(playerName);
    MatchEvent event;
    event.minute = randInt(minuteStart, minuteEnd);
    event.teamName = team.name;
    event.playerName = playerName;
    event.type = MatchEventType::Injury;
    event.description = playerName + " acusa un problema fisico por el desgaste";
    timeline.events.push_back(event);
    replaceInjuredPlayer(team, activeXI, participants, idx, event.minute, timeline);
}

}  // namespace match_event_generator
