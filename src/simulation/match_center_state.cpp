#include "simulation/match_center_state.h"

#include <algorithm>
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

} // namespace match_center