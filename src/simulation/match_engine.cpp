#include "simulation/match_engine.h"

#include "career/career_runtime.h"
#include "ai/ai_match_manager.h"
#include "engine/rival_ai.h"
#include "engine/rivalry_system.h"
#include "simulation/fatigue_engine.h"
#include "simulation/match_context.h"
#include "simulation/match_event_generator.h"
#include "simulation/match_phase.h"
#include "simulation/match_report.h"
#include "simulation/match_resolution.h"
#include "simulation/match_stats.h"
#include "simulation/tactics_engine.h"
#include "utils/utils.h"
#include "simulation/match_momentum.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace std;

namespace {

struct TeamRuntimeState {
    Team team;
    vector<int> xi;
    vector<int> participants;
    vector<string> cautionedPlayers;
    vector<string> sentOffPlayers;
    vector<string> injuredPlayers;
    vector<GoalContribution> goals;
};

const vector<pair<int, int>> kPhases = {{1, 15}, {16, 30}, {31, 45}, {46, 60}, {61, 75}, {76, 90}};

string formatDouble2(double value) {
    ostringstream out;
    out.setf(ios::fixed);
    out.precision(2);
    out << value;
    return out.str();
}

int momentumScoreForTeam(
    const MatchMomentum& momentum,
    bool homeTeam) {

    const double momentumDifference =
        homeTeam
            ? momentum.homeMomentum - momentum.awayMomentum
            : momentum.awayMomentum - momentum.homeMomentum;

    const double confidenceDifference =
        homeTeam
            ? momentum.homeConfidence - momentum.awayConfidence
            : momentum.awayConfidence - momentum.homeConfidence;

    const double pressureDifference =
        homeTeam
            ? momentum.homePressure - momentum.awayPressure
            : momentum.awayPressure - momentum.homePressure;

    const double combined =
        momentumDifference * 55.0 +
        confidenceDifference * 25.0 +
        pressureDifference * 20.0;

    return clampInt(
        static_cast<int>(std::round(combined)),
        -100,
        100);
}

}  // namespace

namespace match_engine {

MatchSimulationData simulateCore(
    const Team& home,
    const Team& away,
    bool keyMatch,
    bool neutralVenue,
    bool interactive,
    bool userControlsHome,
    const ManagerDecisionCallback* decisionCallback) {
    MatchSimulationData data;
    const MatchSetup setup = match_context::buildMatchSetup(home, away, keyMatch, neutralVenue);

    TeamRuntimeState homeState{home, setup.home.xi, setup.home.xi, {}, {}, {}, {}};
    TeamRuntimeState awayState{away, setup.away.xi, setup.away.xi, {}, {}, {}, {}};
    MatchStats stats;
    MatchTimeline timeline;
    timeline.events.reserve(96);
    timeline.phases.reserve(kPhases.size());
    homeState.participants.reserve(16);
    awayState.participants.reserve(16);
    homeState.goals.reserve(6);
    awayState.goals.reserve(6);
    int homePossAccumulator = 0;
    MatchMomentum momentum;
    momentum.reset();
    size_t interactiveEventCursor = 0;

    for (size_t phaseIndex = 0; phaseIndex < kPhases.size(); ++phaseIndex) {
        momentum.decay();
        const int minuteStart = kPhases[phaseIndex].first;
        const int minuteEnd = kPhases[phaseIndex].second;

        MatchPhaseReport phase;
        phase.minuteStart = minuteStart;
        phase.minuteEnd = minuteEnd;

        const int homeMomentumScore =
            momentumScoreForTeam(momentum, true);

        const int awayMomentumScore =
            momentumScoreForTeam(momentum, false);

        const bool humanControlsHome =
            interactive && userControlsHome;
        const bool humanControlsAway =
            interactive && !userControlsHome;

        const bool homeTacticalChange =
            humanControlsHome
                ? false
                : ai_match_manager::applyInMatchManagement(
                      homeState.team,
                      awayState.team,
                      homeState.xi,
                      homeState.participants,
                      homeState.cautionedPlayers,
                      minuteEnd,
                      stats.homeGoals,
                      stats.awayGoals,
                      static_cast<int>(awayState.xi.size()),
                      homeMomentumScore,
                      timeline);

        const bool awayTacticalChange =
            humanControlsAway
                ? false
                : ai_match_manager::applyInMatchManagement(
                      awayState.team,
                      homeState.team,
                      awayState.xi,
                      awayState.participants,
                      awayState.cautionedPlayers,
                      minuteEnd,
                      stats.awayGoals,
                      stats.homeGoals,
                      static_cast<int>(homeState.xi.size()),
                      awayMomentumScore,
                      timeline);

        const TeamMatchSnapshot homeSnapshot = match_context::rebuildSnapshot(homeState.team, awayState.team, homeState.xi, keyMatch);
        const TeamMatchSnapshot awaySnapshot = match_context::rebuildSnapshot(awayState.team, homeState.team, awayState.xi, keyMatch);
        const MatchPhaseEvaluation phaseEval = match_phase::evaluatePhase(setup,
                                                                          homeState.team,
                                                                          awayState.team,
                                                                          homeSnapshot,
                                                                          awaySnapshot,
                                                                          static_cast<int>(phaseIndex),
                                                                          minuteStart,
                                                                          minuteEnd,
                                                                          stats.homeGoals,
                                                                          stats.awayGoals,
                                                                          static_cast<int>(homeState.xi.size()),
                                                                          static_cast<int>(awayState.xi.size()));
        phase = phaseEval.report;
        phase.homeTacticalChange = homeTacticalChange;
        phase.awayTacticalChange = awayTacticalChange;
        timeline.phases.push_back(phase);

        MatchEvent controlEvent;
        controlEvent.minute = minuteStart;
        controlEvent.teamName = phase.dominantTeam;
        controlEvent.type = MatchEventType::PossessionPhase;
        controlEvent.description = phase.dominantTeam + " domina el tramo " + to_string(minuteStart) + "-" + to_string(minuteEnd);
        match_stats::pushEvent(timeline, stats, controlEvent);

        homePossAccumulator += phase.homePossessionShare;

        const int homeShotsBefore = stats.homeShots;
        const int awayShotsBefore = stats.awayShots;
        const int homeGoalsBefore = stats.homeGoals;
    const int awayGoalsBefore = stats.awayGoals;
        match_event_generator::playPhaseSequences(homeState.team,
                                                  awayState.team,
                                                  homeState.xi,
                                                  awayState.xi,
                                                  homeSnapshot,
                                                  awaySnapshot,
                                                  true,
                                                  minuteStart,
                                                  minuteEnd,
                                                  phaseEval.homePossessionChains,
                                                  phaseEval.homeProgressions,
                                                  phaseEval.homeAttacks,
                                                  phaseEval.homeChanceCount,
                                                  phaseEval.homeAttack - phaseEval.awayDefense + momentum.homeBonus() * 10.0,
                                                  phase.awayDefensiveRisk,
                                                  timeline,
                                                  stats,
                                                  homeState.goals);
        match_event_generator::playPhaseSequences(awayState.team,
                                                  homeState.team,
                                                  awayState.xi,
                                                  homeState.xi,
                                                  awaySnapshot,
                                                  homeSnapshot,
                                                  false,
                                                  minuteStart,
                                                  minuteEnd,
                                                  phaseEval.awayPossessionChains,
                                                  phaseEval.awayProgressions,
                                                  phaseEval.awayAttacks,
                                                  phaseEval.awayChanceCount,
                                                  phaseEval.awayAttack - phaseEval.homeDefense + momentum.awayBonus() * 10.0,
                                                  phase.homeDefensiveRisk,
                                                  timeline,
                                                  stats,
                                                  awayState.goals);
        timeline.phases.back().homeShotsGenerated = stats.homeShots - homeShotsBefore;
        timeline.phases.back().awayShotsGenerated = stats.awayShots - awayShotsBefore;
        const int homeShotsGenerated = stats.homeShots - homeShotsBefore;
const int awayShotsGenerated = stats.awayShots - awayShotsBefore;

for (int i = 0; i < homeShotsGenerated; ++i) {
    momentum.homeAttack();
}

for (int i = 0; i < awayShotsGenerated; ++i) {
    momentum.awayAttack();
}

if (stats.homeGoals > homeGoalsBefore) {
    momentum.homeGoal();
}

if (stats.awayGoals > awayGoalsBefore) {
    momentum.awayGoal();
}

        match_event_generator::registerDiscipline(homeState.team,
                                                  homeState.xi,
                                                  true,
                                                  phase.intensity,
                                                  timeline,
                                                  stats,
                                                  homeState.cautionedPlayers,
                                                  homeState.sentOffPlayers,
                                                  data.homeYellowCardPlayers,
                                                  data.homeRedCardPlayers);
        match_event_generator::registerDiscipline(awayState.team,
                                                  awayState.xi,
                                                  false,
                                                  phase.intensity,
                                                  timeline,
                                                  stats,
                                                  awayState.cautionedPlayers,
                                                  awayState.sentOffPlayers,
                                                  data.awayYellowCardPlayers,
                                                  data.awayRedCardPlayers);

        match_event_generator::maybeInjure(homeState.team,
                                           homeState.xi,
                                           homeState.participants,
                                           phase.injuryRisk,
                                           minuteStart,
                                           minuteEnd,
                                           timeline,
                                           data.homeInjuredPlayers);
        match_event_generator::maybeInjure(awayState.team,
                                           awayState.xi,
                                           awayState.participants,
                                           phase.injuryRisk,
                                           minuteStart,
                                           minuteEnd,
                                           timeline,
                                           data.awayInjuredPlayers);
        fatigue_engine::applyPhaseFatigue(homeState.team, homeState.xi, static_cast<int>(phaseIndex));
        fatigue_engine::applyPhaseFatigue(awayState.team, awayState.xi, static_cast<int>(phaseIndex));

        if (interactive &&
            decisionCallback &&
            *decisionCallback &&
            minuteEnd < 90) {

            TeamRuntimeState& userState =
                userControlsHome ? homeState : awayState;

            InteractiveMatchState interactiveState;
            interactiveState.minute = minuteEnd;
            interactiveState.userIsHome = userControlsHome;
            interactiveState.homeGoals = stats.homeGoals;
            interactiveState.awayGoals = stats.awayGoals;
            interactiveState.homeShots = stats.homeShots;
            interactiveState.awayShots = stats.awayShots;

            const int phasesPlayed =
                static_cast<int>(phaseIndex) + 1;

            interactiveState.homePossession =
                phasesPlayed > 0
                    ? clampInt(
                          static_cast<int>(round(
                              homePossAccumulator /
                              static_cast<double>(phasesPlayed))),
                          0,
                          100)
                    : 50;

            interactiveState.awayPossession =
                100 - interactiveState.homePossession;

            interactiveState.currentTactics =
                userState.team.tactics;

            interactiveState.currentInstruction =
                userState.team.matchInstruction;

            interactiveState.activeXi =
                userState.xi;

            interactiveState.substitutionsUsed =
                match_stats::countSubstitutions(
                    timeline,
                    userState.team.name);

            for (int i = 0;
                 i < static_cast<int>(userState.team.players.size());
                 ++i) {
                if (find(
                        userState.xi.begin(),
                        userState.xi.end(),
                        i) == userState.xi.end() &&
                    find(
                        userState.participants.begin(),
                        userState.participants.end(),
                        i) == userState.participants.end()) {
                    interactiveState.availableBench.push_back(i);
                }
            }

            vector<const MatchEvent*> recentImportantEvents;

            for (size_t eventIndex = interactiveEventCursor;
                 eventIndex < timeline.events.size();
                 ++eventIndex) {
                const MatchEvent& event = timeline.events[eventIndex];
                const bool importantEvent =
                    event.type == MatchEventType::Goal ||
                    event.type == MatchEventType::YellowCard ||
                    event.type == MatchEventType::RedCard ||
                    event.type == MatchEventType::Injury ||
                    event.type == MatchEventType::Substitution ||
                    event.type == MatchEventType::TacticalChange;

                if (importantEvent) {
                    recentImportantEvents.push_back(&event);
                }
            }

            stable_sort(
                recentImportantEvents.begin(),
                recentImportantEvents.end(),
                [](const MatchEvent* left, const MatchEvent* right) {
                    return left->minute < right->minute;
                });

            for (const MatchEvent* event : recentImportantEvents) {
                interactiveState.recentEvents.push_back(
                    to_string(event->minute) + "' " +
                    event->teamName + ": " +
                    event->description);
            }

            interactiveEventCursor = timeline.events.size();

            const ManagerDecision decision =
                (*decisionCallback)(interactiveState);

            if (decision.type ==
                    ManagerDecisionType::ChangeTactics) {

                static const vector<string> validTactics = {
                    "Defensive",
                    "Balanced",
                    "Offensive",
                    "Pressing",
                    "Counter"
                };

                if (find(
                        validTactics.begin(),
                        validTactics.end(),
                        decision.tactics) !=
                    validTactics.end()) {

                    userState.team.tactics =
                        decision.tactics;

                    MatchEvent event;
                    event.minute = minuteEnd;
                    event.teamName = userState.team.name;
                    event.type =
                        MatchEventType::TacticalChange;
                    event.description =
                        userState.team.name +
                        " cambia la mentalidad a " +
                        decision.tactics;

                    timeline.events.push_back(event);
                }
            } else if (decision.type ==
                       ManagerDecisionType::ChangeInstruction) {

                static const vector<string> validInstructions = {
                    "Equilibrado",
                    "Laterales altos",
                    "Bloque bajo",
                    "Balon parado",
                    "Presion final",
                    "Por bandas",
                    "Juego directo",
                    "Contra-presion",
                    "Pausar juego"
                };

                if (find(
                        validInstructions.begin(),
                        validInstructions.end(),
                        decision.instruction) !=
                    validInstructions.end()) {

                    userState.team.matchInstruction =
                        decision.instruction;

                    MatchEvent event;
                    event.minute = minuteEnd;
                    event.teamName = userState.team.name;
                    event.type =
                        MatchEventType::TacticalChange;
                    event.description =
                        userState.team.name +
                        " cambia la instruccion a " +
                        decision.instruction;

                    timeline.events.push_back(event);
                }
            } else if (decision.type ==
                       ManagerDecisionType::Substitute) {

                const int substitutionsUsed =
                    match_stats::countSubstitutions(
                        timeline,
                        userState.team.name);

                const auto outIt =
                    find(
                        userState.xi.begin(),
                        userState.xi.end(),
                        decision.playerOutIndex);

                const bool validIncomingIndex =
                    decision.playerInIndex >= 0 &&
                    decision.playerInIndex <
                        static_cast<int>(
                            userState.team.players.size());

                const bool incomingAlreadyActive =
                    validIncomingIndex &&
                    find(
                        userState.xi.begin(),
                        userState.xi.end(),
                        decision.playerInIndex) !=
                        userState.xi.end();

                const bool incomingAlreadyParticipated =
                    validIncomingIndex &&
                    find(
                        userState.participants.begin(),
                        userState.participants.end(),
                        decision.playerInIndex) !=
                        userState.participants.end();

                if (substitutionsUsed < 5 &&
                    outIt != userState.xi.end() &&
                    validIncomingIndex &&
                    !incomingAlreadyActive &&
                    !incomingAlreadyParticipated) {

                    const int outgoingIndex = *outIt;

                    *outIt =
                        decision.playerInIndex;

                    userState.participants.push_back(
                        decision.playerInIndex);

                    const Player& outgoing =
                        userState.team.players[
                            static_cast<size_t>(
                                outgoingIndex)];

                    const Player& incoming =
                        userState.team.players[
                            static_cast<size_t>(
                                decision.playerInIndex)];

                    MatchEvent event;
                    event.minute = minuteEnd;
                    event.teamName =
                        userState.team.name;
                    event.playerName =
                        incoming.name;
                    event.type =
                        MatchEventType::Substitution;
                    event.description =
                        outgoing.name +
                        " sale; entra " +
                        incoming.name;

                    timeline.events.push_back(event);
                }
            }
        }

        if (IdleCallback cb = idleCallback()) {
            cb();
        }
    }

    stats.homePossession = clampInt(static_cast<int>(round(homePossAccumulator / static_cast<double>(kPhases.size()))), 30, 70);
    stats.awayPossession = 100 - stats.homePossession;

    data.homeParticipants = homeState.participants;
    data.awayParticipants = awayState.participants;
    data.homeGoals = homeState.goals;
    data.awayGoals = awayState.goals;

    MatchResult result;
    result.homeGoals = stats.homeGoals;
    result.awayGoals = stats.awayGoals;
    result.homeShots = stats.homeShots;
    result.awayShots = stats.awayShots;
    result.homePossession = stats.homePossession;
    result.awayPossession = stats.awayPossession;
    result.homeSubstitutions = match_stats::countSubstitutions(timeline, home.name);
    result.awaySubstitutions = match_stats::countSubstitutions(timeline, away.name);
    result.homeCorners = stats.homeCorners;
    result.awayCorners = stats.awayCorners;
    result.weather = setup.context.weather;
    result.context = setup.context;
    result.stats = stats;
    result.timeline = timeline;
    result.report = match_report::buildReport(setup, homeState.team, awayState.team, timeline, stats);
    result.events = match_stats::buildLegacyTimeline(timeline);
    result.reportLines.push_back("--- Partido ---");
    result.reportLines.push_back(home.name + " vs " + away.name);
    result.reportLines.push_back("Clima: " + setup.context.weather + " | xG " + formatDouble2(stats.homeExpectedGoals) +
                                 " - " + formatDouble2(stats.awayExpectedGoals));
    result.reportLines.push_back("Resultado Final: " + home.name + " " + to_string(stats.homeGoals) + " - " +
                                 to_string(stats.awayGoals) + " " + away.name);
    result.reportLines.push_back("Estadisticas: Tiros " + to_string(stats.homeShots) + "-" + to_string(stats.awayShots) +
                                 ", Tiros al arco " + to_string(stats.homeShotsOnTarget) + "-" +
                                 to_string(stats.awayShotsOnTarget) +
                                 ", Posesion " + to_string(stats.homePossession) + "%-" +
                                 to_string(stats.awayPossession) + "%" +
                                 ", Corners " + to_string(stats.homeCorners) + "-" + to_string(stats.awayCorners));
    match_report::appendSummaryLines(result.report, result.reportLines);
    if (setup.context.fatigueFactorHome < 0.92 || setup.context.fatigueFactorAway < 0.92) {
        result.warnings.push_back("El desgaste acumulado tuvo impacto directo en el rendimiento.");
    }
    result.verdict = stats.homeGoals > stats.awayGoals ? "Victoria local"
                     : stats.homeGoals < stats.awayGoals ? "Victoria visitante"
                                                         : "Empate";
    data.result = std::move(result);
    return data;
}


MatchSimulationData simulate(
    const Team& home,
    const Team& away,
    bool keyMatch,
    bool neutralVenue) {

    return simulateCore(
        home,
        away,
        keyMatch,
        neutralVenue,
        false,
        true,
        nullptr);
}

MatchSimulationData simulateInteractive(
    const Team& home,
    const Team& away,
    bool userControlsHome,
    const ManagerDecisionCallback& decisionCallback,
    bool keyMatch,
    bool neutralVenue) {

    return simulateCore(
        home,
        away,
        keyMatch,
        neutralVenue,
        true,
        userControlsHome,
        &decisionCallback);
}

MatchSimulationData simulateInteractive(
    const Team& home,
    const Team& away,
    const Career* career,
    bool userControlsHome,
    const ManagerDecisionCallback& decisionCallback,
    bool keyMatch,
    bool neutralVenue) {

    if (!career) {
        return simulateInteractive(
            home,
            away,
            userControlsHome,
            decisionCallback,
            keyMatch,
            neutralVenue);
    }

    Team homeModified = home;
    Team awayModified = away;

    const bool playerInHome =
        career->myTeam &&
        homeModified.name == career->myTeam->name;

    const bool playerInAway =
        career->myTeam &&
        awayModified.name == career->myTeam->name;

    if (playerInHome || playerInAway) {
        Team& opponentModified =
            playerInHome ? awayModified : homeModified;

        const auto it =
            career->rivalAIMap.find(opponentModified.name);

        if (it != career->rivalAIMap.end()) {
            const RivalAI& rivalAI = it->second;

            const string playerFormation =
                career->myTeam->formation.empty()
                    ? "4-3-3"
                    : career->myTeam->formation;

            const string playerTactics =
                career->myTeam->tactics.empty()
                    ? "Balanced"
                    : career->myTeam->tactics;

            const string rivalTactics =
                toLower(
                    rivalAI.decideTactics(
                        playerFormation,
                        playerTactics));

            if (rivalTactics.find("aggressive") != string::npos) {
                opponentModified.tactics = "Pressing";
                opponentModified.matchInstruction = "Contra-presion";
                opponentModified.tempo =
                    min(5, opponentModified.tempo + 1);
                opponentModified.pressingIntensity =
                    min(5, opponentModified.pressingIntensity + 1);
                opponentModified.defensiveLine =
                    min(5, opponentModified.defensiveLine + 1);

            } else if (
                rivalTactics.find("defensive") != string::npos) {

                opponentModified.tactics = "Defensive";
                opponentModified.matchInstruction = "Bloque bajo";
                opponentModified.tempo =
                    max(1, opponentModified.tempo - 1);
                opponentModified.defensiveLine =
                    max(1, opponentModified.defensiveLine - 1);

            } else if (
                rivalTactics.find("counter") != string::npos) {

                opponentModified.tactics = "Counter";
                opponentModified.matchInstruction = "Juego directo";
                opponentModified.tempo =
                    min(5, opponentModified.tempo + 1);
                opponentModified.defensiveLine =
                    max(1, opponentModified.defensiveLine - 1);

            } else if (
                rivalTactics.find("possession") != string::npos) {

                opponentModified.tactics = "Balanced";
                opponentModified.matchInstruction = "Equilibrado";
                opponentModified.tempo =
                    clampInt(opponentModified.tempo, 2, 3);
                opponentModified.width =
                    clampInt(opponentModified.width, 2, 4);
                opponentModified.markingStyle = "Zonal";
            }

            if (rivalAI.personality.unpredictability > 65) {
                opponentModified.width =
                    clampInt(
                        opponentModified.width +
                            randInt(-1, 1),
                        1,
                        5);

                opponentModified.defensiveLine =
                    clampInt(
                        opponentModified.defensiveLine +
                            randInt(-1, 1),
                        1,
                        5);
            }
        }
    }

    Team* playerTeamModified = nullptr;

    if (
        career->myTeam &&
        homeModified.name == career->myTeam->name) {

        playerTeamModified = &homeModified;

    } else if (
        career->myTeam &&
        awayModified.name == career->myTeam->name) {

        playerTeamModified = &awayModified;
    }

    if (playerTeamModified) {
        if (career->managerStress.stressLevel > 75) {
            playerTeamModified->defensiveLine =
                max(
                    1,
                    playerTeamModified->defensiveLine - 1);

            playerTeamModified->pressingIntensity =
                max(
                    1,
                    playerTeamModified->pressingIntensity - 1);

        } else if (career->managerStress.stressLevel < 30) {
            playerTeamModified->pressingIntensity =
                min(
                    5,
                    playerTeamModified->pressingIntensity + 1);
        }
    }

    return simulateCore(
        homeModified,
        awayModified,
        keyMatch,
        neutralVenue,
        true,
        userControlsHome,
        &decisionCallback);
}

// Overload that integrates rival AI tactics
MatchSimulationData simulate(const Team& home, const Team& away, const Career* career, bool keyMatch, bool neutralVenue) {
    // If no career context, use basic simulation
    if (!career) {
        return simulate(home, away, keyMatch, neutralVenue);
    }
    
    Team homeModified = home;
    Team awayModified = away;
    const bool playerInHome = career->myTeam && homeModified.name == career->myTeam->name;
    const bool playerInAway = career->myTeam && awayModified.name == career->myTeam->name;

    if (playerInHome || playerInAway) {
        Team& opponentModified = playerInHome ? awayModified : homeModified;
        const auto it = career->rivalAIMap.find(opponentModified.name);
        if (it != career->rivalAIMap.end()) {
            const RivalAI& rivalAI = it->second;
            const string playerFormation = career->myTeam->formation.empty() ? "4-3-3" : career->myTeam->formation;
            const string playerTactics = career->myTeam->tactics.empty() ? "Balanced" : career->myTeam->tactics;
            
            // Decide rival tactics based on player's team
            const string rivalTactics = toLower(rivalAI.decideTactics(playerFormation, playerTactics));
            
            if (rivalTactics.find("aggressive") != string::npos) {
                opponentModified.tactics = "Pressing";
                opponentModified.matchInstruction = "Contra-presion";
                opponentModified.tempo = min(5, opponentModified.tempo + 1);
                opponentModified.pressingIntensity = min(5, opponentModified.pressingIntensity + 1);
                opponentModified.defensiveLine = min(5, opponentModified.defensiveLine + 1);
            } else if (rivalTactics.find("defensive") != string::npos) {
                opponentModified.tactics = "Defensive";
                opponentModified.matchInstruction = "Bloque bajo";
                opponentModified.tempo = max(1, opponentModified.tempo - 1);
                opponentModified.defensiveLine = max(1, opponentModified.defensiveLine - 1);
            } else if (rivalTactics.find("counter") != string::npos) {
                opponentModified.tactics = "Counter";
                opponentModified.matchInstruction = "Juego directo";
                opponentModified.tempo = min(5, opponentModified.tempo + 1);
                opponentModified.defensiveLine = max(1, opponentModified.defensiveLine - 1);
            } else if (rivalTactics.find("possession") != string::npos) {
                opponentModified.tactics = "Balanced";
                opponentModified.matchInstruction = "Equilibrado";
                opponentModified.tempo = clampInt(opponentModified.tempo, 2, 3);
                opponentModified.width = clampInt(opponentModified.width, 2, 4);
                opponentModified.markingStyle = "Zonal";
            }
            
            // Apply unpredictability
            if (rivalAI.personality.unpredictability > 65) {
                opponentModified.width = clampInt(opponentModified.width + randInt(-1, 1), 1, 5);
                opponentModified.defensiveLine = clampInt(opponentModified.defensiveLine + randInt(-1, 1), 1, 5);
            }
        }
    }
    
    // Adjust the player's team tactics if we have manager stress context
    Team* playerTeamModified = nullptr;
    if (career->myTeam && homeModified.name == career->myTeam->name) playerTeamModified = &homeModified;
    else if (career->myTeam && awayModified.name == career->myTeam->name) playerTeamModified = &awayModified;

    if (playerTeamModified) {
        if (career->managerStress.stressLevel > 75) {
            playerTeamModified->defensiveLine = max(1, playerTeamModified->defensiveLine - 1);
            playerTeamModified->pressingIntensity = max(1, playerTeamModified->pressingIntensity - 1);
        } else if (career->managerStress.stressLevel < 30) {
            playerTeamModified->pressingIntensity = min(5, playerTeamModified->pressingIntensity + 1);
        }
    }
    
    // Run the modified match simulation
    return simulate(homeModified, awayModified, keyMatch, neutralVenue);
}

}  // namespace match_engine
