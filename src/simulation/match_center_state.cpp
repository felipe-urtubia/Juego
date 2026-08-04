#include "simulation/match_center_state.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace match_center {

std::string eventText(const MatchEvent& event) {
    std::ostringstream text;

    text << event.minute << "' ";

    if (!event.teamName.empty()) {
        text << '[' << event.teamName << "] ";
    }

    if (!event.description.empty()) {
        text << event.description;
    } else if (!event.playerName.empty()) {
        text << event.playerName;
    } else {
        text << matchEventTypeLabel(event.type);
    }

    return text.str();
}

void applyEventImpact(
    LiveState& state,
    const MatchEvent& event) {

    state.minute = std::max(
        state.minute,
        event.minute
    );

    state.homeGoals +=
        event.impact.homeGoalsDelta;

    state.awayGoals +=
        event.impact.awayGoalsDelta;

    state.homeShots +=
        event.impact.homeShotsDelta;

    state.awayShots +=
        event.impact.awayShotsDelta;

    state.homeShotsOnTarget +=
        event.impact.homeShotsOnTargetDelta;

    state.awayShotsOnTarget +=
        event.impact.awayShotsOnTargetDelta;

    state.homeCorners +=
        event.impact.homeCornersDelta;

    state.awayCorners +=
        event.impact.awayCornersDelta;

    state.homeFouls +=
        event.impact.homeFoulsDelta;

    state.awayFouls +=
        event.impact.awayFoulsDelta;

    state.homeYellowCards +=
        event.impact.homeYellowCardsDelta;

    state.awayYellowCards +=
        event.impact.awayYellowCardsDelta;

    state.homeRedCards +=
        event.impact.homeRedCardsDelta;

    state.awayRedCards +=
        event.impact.awayRedCardsDelta;

    state.homeExpectedGoals +=
        event.impact.homeExpectedGoalsDelta;

    state.awayExpectedGoals +=
        event.impact.awayExpectedGoalsDelta;

    state.lastEvent = eventText(event);
}

void updateLivePossession(
    LiveState& state,
    const MatchResult& result,
    int currentMinute) {

    if (result.timeline.phases.empty() || currentMinute <= 0) {
        state.homePossession = 50;
        state.awayPossession = 50;
        return;
    }

    double weightedHomePossession = 0.0;
    int coveredMinutes = 0;

    for (const MatchPhaseReport& phase : result.timeline.phases) {
        if (currentMinute < phase.minuteStart) {
            break;
        }

        const int phaseEnd =
            std::min(currentMinute, phase.minuteEnd);

        const int minutesInPhase =
            phaseEnd - phase.minuteStart + 1;

        if (minutesInPhase <= 0) {
            continue;
        }

        const int boundedHomeShare =
            std::clamp(phase.homePossessionShare, 0, 100);

        weightedHomePossession +=
            static_cast<double>(boundedHomeShare) *
            static_cast<double>(minutesInPhase);

        coveredMinutes += minutesInPhase;

        if (currentMinute <= phase.minuteEnd) {
            break;
        }
    }

    if (coveredMinutes <= 0) {
        state.homePossession = 50;
        state.awayPossession = 50;
        return;
    }

    state.homePossession = std::clamp(
        static_cast<int>(std::lround(
            weightedHomePossession /
            static_cast<double>(coveredMinutes)
        )),
        0,
        100
    );

    state.awayPossession =
        100 - state.homePossession;

    state.momentumScore = 0;
}


namespace {

bool eventBelongsToHome(const MatchEvent& event,
                        const Team& home,
                        const Team& away) {
    if (!event.teamName.empty()) {
        if (event.teamName == home.name) return true;
        if (event.teamName == away.name) return false;
    }

    const int homeImpact =
        event.impact.homeGoalsDelta +
        event.impact.homeShotsDelta +
        event.impact.homeShotsOnTargetDelta +
        event.impact.homeCornersDelta;

    const int awayImpact =
        event.impact.awayGoalsDelta +
        event.impact.awayShotsDelta +
        event.impact.awayShotsOnTargetDelta +
        event.impact.awayCornersDelta;

    return homeImpact >= awayImpact;
}

int calculateMomentumScore(const MatchMomentum& momentum) {
    const double combined =
        (momentum.homeMomentum - momentum.awayMomentum) * 55.0 +
        (momentum.homeConfidence - momentum.awayConfidence) * 25.0 +
        (momentum.homePressure - momentum.awayPressure) * 20.0;

    return std::clamp(
        static_cast<int>(std::lround(combined)),
        -100,
        100
    );
}

}  // namespace

void updateLiveMomentum(LiveState& state,
                        const MatchEvent& event,
                        const Team& home,
                        const Team& away) {
    state.momentum.decay();

    const bool homeEvent = eventBelongsToHome(event, home, away);

    if (event.type == MatchEventType::Goal) {
        if (homeEvent) state.momentum.homeGoal();
        else state.momentum.awayGoal();
    } else {
        int impulses = 0;

        switch (event.type) {
            case MatchEventType::BigChance:
                impulses = 3;
                break;
            case MatchEventType::Shot:
            case MatchEventType::Save:
            case MatchEventType::Counterattack:
                impulses = 2;
                break;
            case MatchEventType::Miss:
            case MatchEventType::Corner:
            case MatchEventType::AttackBuildUp:
            case MatchEventType::Progression:
                impulses = 1;
                break;
            case MatchEventType::RedCard:
                impulses = 3;
                break;
            case MatchEventType::YellowCard:
            case MatchEventType::Foul:
                impulses = 1;
                break;
            default:
                break;
        }

        const bool reverseImpact =
            event.type == MatchEventType::RedCard ||
            event.type == MatchEventType::YellowCard ||
            event.type == MatchEventType::Foul;

        const bool boostHome = reverseImpact ? !homeEvent : homeEvent;

        for (int i = 0; i < impulses; ++i) {
            if (boostHome) state.momentum.homeAttack();
            else state.momentum.awayAttack();
        }
    }

    state.momentumScore = calculateMomentumScore(state.momentum);
}

std::string momentumLabel(int momentumScore) {
    if (momentumScore >= 60) return "Dominio total del local";
    if (momentumScore >= 30) return "El local controla el partido";
    if (momentumScore >= 10) return "Ligera iniciativa local";
    if (momentumScore <= -60) return "Dominio total del visitante";
    if (momentumScore <= -30) return "El visitante controla el partido";
    if (momentumScore <= -10) return "Ligera iniciativa visitante";
    return "Partido equilibrado";
}



void applyLiveManagementEvent(
    LiveState& state,
    const MatchEvent& event,
    const Team& home,
    const Team& away) {

    bool isHomeTeam = false;
    bool teamResolved = false;

    if (event.teamName == home.name) {
        isHomeTeam = true;
        teamResolved = true;
    } else if (event.teamName == away.name) {
        isHomeTeam = false;
        teamResolved = true;
    }

    if (!teamResolved) {
        return;
    }

    if (event.type == MatchEventType::Substitution) {
        if (isHomeTeam) {
            state.homeSubstitutions++;
        } else {
            state.awaySubstitutions++;
        }
    }

    if (event.type == MatchEventType::TacticalChange) {
        if (isHomeTeam) {
            state.homeTacticalChanges++;
        } else {
            state.awayTacticalChanges++;
        }
    }
}

LiveState makeFinalState(
    const MatchResult& result) {

    LiveState state;

    state.minute = 90;

    if (!result.timeline.events.empty()) {
        state.minute = std::max(
            90,
            result.timeline.events.back().minute
        );
    }

    state.homeGoals =
        result.homeGoals;

    state.awayGoals =
        result.awayGoals;

    state.homeShots =
        result.stats.homeShots;

    state.awayShots =
        result.stats.awayShots;

    state.homeShotsOnTarget =
        result.stats.homeShotsOnTarget;

    state.awayShotsOnTarget =
        result.stats.awayShotsOnTarget;

    state.homePossession =
        std::clamp(result.homePossession, 0, 100);

    state.awayPossession =
        100 - state.homePossession;

    state.momentumScore = 0;

    state.homeCorners =
        result.stats.homeCorners;

    state.awayCorners =
        result.stats.awayCorners;

    state.homeFouls =
        result.stats.homeFouls;

    state.awayFouls =
        result.stats.awayFouls;

    state.homeYellowCards =
        result.stats.homeYellowCards;

    state.awayYellowCards =
        result.stats.awayYellowCards;

    state.homeRedCards =
        result.stats.homeRedCards;

    state.awayRedCards =
        result.stats.awayRedCards;

    state.homeExpectedGoals =
        result.stats.homeExpectedGoals;

    state.awayExpectedGoals =
        result.stats.awayExpectedGoals;

    state.lastEvent =
        "Final del partido.";

    return state;
}

}  // namespace match_center
