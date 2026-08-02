#include "simulation/match_center.h"
#include "simulation/match_center_renderer.h"
#include "simulation/match_center_state.h"

#include <chrono>
#include <cstdlib>
#include <thread>

namespace match_center {
namespace {

int delayForSpeed(PlaybackSpeed speed, bool importantEvent) {
    switch (speed) {
        case PlaybackSpeed::Fast:
            return importantEvent ? 350 : 150;
        case PlaybackSpeed::Slow:
            return importantEvent ? 1800 : 1200;
        case PlaybackSpeed::Normal:
        default:
            return importantEvent ? 1000 : 600;
    }
}

bool isImportantEvent(MatchEventType type) {
    return type == MatchEventType::Goal ||
           type == MatchEventType::RedCard ||
           type == MatchEventType::Injury ||
           type == MatchEventType::Substitution ||
           type == MatchEventType::TacticalChange;
}

bool shouldDisplayEvent(const MatchEvent& event, bool showAllEvents) {
    if (showAllEvents) return true;

    return event.type == MatchEventType::Shot ||
           event.type == MatchEventType::BigChance ||
           event.type == MatchEventType::Goal ||
           event.type == MatchEventType::Miss ||
           event.type == MatchEventType::Save ||
           event.type == MatchEventType::YellowCard ||
           event.type == MatchEventType::RedCard ||
           event.type == MatchEventType::Injury ||
           event.type == MatchEventType::Corner ||
           event.type == MatchEventType::Counterattack ||
           event.type == MatchEventType::TacticalChange ||
           event.type == MatchEventType::Substitution;
}

void clearConsole() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

void pauseAfterEvent(PlaybackSpeed speed, MatchEventType type) {
    const int milliseconds =
        delayForSpeed(speed, isImportantEvent(type));

    if (milliseconds > 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(milliseconds));
    }
}

}  // namespace

void showMatchCenter(const Team& home,
                     const Team& away,
                     const MatchResult& result,
                     const PlaybackOptions& options) {
    LiveState state;
    updateLivePossession(state, result, 0);

    if (options.clearScreenBetweenEvents) {
        clearConsole();
    }

    drawMatchCenter(home, away, result, state, false);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            delayForSpeed(options.speed, false)));

    for (const MatchEvent& event : result.timeline.events) {
        applyEventImpact(state, event);
        updateLivePossession(state, result, state.minute);
        updateLiveMomentum(state, event, home, away);

        if (!shouldDisplayEvent(event, options.showAllEvents)) {
            continue;
        }

        if (options.clearScreenBetweenEvents) {
            clearConsole();
        }

        drawMatchCenter(home, away, result, state, false);
        pauseAfterEvent(options.speed, event.type);
    }

    state = makeFinalState(result);

    if (options.clearScreenBetweenEvents) {
        clearConsole();
    }

    drawMatchCenter(home, away, result, state, true);
}

}  // namespace match_center
