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

}  // namespace

namespace match_engine {

MatchSimulationData simulate(const Team& home, const Team& away, bool keyMatch, bool neutralVenue) {
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

    for (size_t phaseIndex = 0; phaseIndex < kPhases.size(); ++phaseIndex) {
        momentum.decay();
        const int minuteStart = kPhases[phaseIndex].first;
        const int minuteEnd = kPhases[phaseIndex].second;

        MatchPhaseReport phase;
        phase.minuteStart = minuteStart;
        phase.minuteEnd = minuteEnd;

        const bool homeTacticalChange = ai_match_manager::applyInMatchManagement(homeState.team,
                                                                                 awayState.team,
                                                                                 homeState.xi,
                                                                                 homeState.participants,
                                                                                 homeState.cautionedPlayers,
                                                                                 minuteEnd,
                                                                                 stats.homeGoals,
                                                                                 stats.awayGoals,
                                                                                 static_cast<int>(awayState.xi.size()),
                                                                                 timeline);
        const bool awayTacticalChange = ai_match_manager::applyInMatchManagement(awayState.team,
                                                                                 homeState.team,
                                                                                 awayState.xi,
                                                                                 awayState.participants,
                                                                                 awayState.cautionedPlayers,
                                                                                 minuteEnd,
                                                                                 stats.awayGoals,
                                                                                 stats.homeGoals,
                                                                                 static_cast<int>(homeState.xi.size()),
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
                                                  phaseEval.homeAttack - phaseEval.awayDefense,
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
                                                  phaseEval.awayAttack - phaseEval.homeDefense,
                                                  phase.homeDefensiveRisk,
                                                  timeline,
                                                  stats,
                                                  awayState.goals);
        timeline.phases.back().homeShotsGenerated = stats.homeShots - homeShotsBefore;
        timeline.phases.back().awayShotsGenerated = stats.awayShots - awayShotsBefore;

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
