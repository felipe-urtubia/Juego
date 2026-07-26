#include "simulation/match_center.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace match_center {
namespace {

struct LiveState {
    int minute = 0;
    int homeGoals = 0;
    int awayGoals = 0;
    int homeShots = 0;
    int awayShots = 0;
    int homeShotsOnTarget = 0;
    int awayShotsOnTarget = 0;
    int homeCorners = 0;
    int awayCorners = 0;
    int homeFouls = 0;
    int awayFouls = 0;
    int homeYellowCards = 0;
    int awayYellowCards = 0;
    int homeRedCards = 0;
    int awayRedCards = 0;
    double homeExpectedGoals = 0.0;
    double awayExpectedGoals = 0.0;
    std::string lastEvent = "El partido está a punto de comenzar.";
};

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

std::string progressBar(int percentage, int width = 24) {
    percentage = std::clamp(percentage, 0, 100);
    const int filled = percentage * width / 100;

    std::string bar;
    bar.reserve(static_cast<std::size_t>(width));

    for (int i = 0; i < width; ++i) {
        bar += i < filled ? '#' : '.';
    }

    return bar;
}

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

void applyEventImpact(LiveState& state, const MatchEvent& event) {
    state.minute = std::max(state.minute, event.minute);
    state.homeGoals += event.impact.homeGoalsDelta;
    state.awayGoals += event.impact.awayGoalsDelta;
    state.homeShots += event.impact.homeShotsDelta;
    state.awayShots += event.impact.awayShotsDelta;
    state.homeShotsOnTarget += event.impact.homeShotsOnTargetDelta;
    state.awayShotsOnTarget += event.impact.awayShotsOnTargetDelta;
    state.homeCorners += event.impact.homeCornersDelta;
    state.awayCorners += event.impact.awayCornersDelta;
    state.homeFouls += event.impact.homeFoulsDelta;
    state.awayFouls += event.impact.awayFoulsDelta;
    state.homeYellowCards += event.impact.homeYellowCardsDelta;
    state.awayYellowCards += event.impact.awayYellowCardsDelta;
    state.homeRedCards += event.impact.homeRedCardsDelta;
    state.awayRedCards += event.impact.awayRedCardsDelta;
    state.homeExpectedGoals += event.impact.homeExpectedGoalsDelta;
    state.awayExpectedGoals += event.impact.awayExpectedGoalsDelta;
    state.lastEvent = eventText(event);
}

void drawMatchCenter(const Team& home,
                     const Team& away,
                     const MatchResult& result,
                     const LiveState& state,
                     bool finished) {
    const int homePossession = std::clamp(result.homePossession, 0, 100);
    const int awayPossession = std::clamp(result.awayPossession, 0, 100);

    std::cout << "============================================================\n";
    std::cout << "                       MATCH CENTER\n";
    std::cout << "============================================================\n\n";

    std::cout << home.name << ' ' << state.homeGoals
              << " - " << state.awayGoals << ' ' << away.name << '\n';

    if (finished) {
        std::cout << "                         FINAL\n";
    } else {
        std::cout << "                       MINUTO " << state.minute << "'\n";
    }

    if (!result.weather.empty()) {
        std::cout << "Clima: " << result.weather << '\n';
    }

    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "Posesion\n";
    std::cout << home.name << " [" << progressBar(homePossession) << "] "
              << homePossession << "%\n";
    std::cout << away.name << " [" << progressBar(awayPossession) << "] "
              << awayPossession << "%\n";

    std::cout << "\nEstadisticas\n";
    std::cout << std::left << std::setw(22) << "Tiros"
              << state.homeShots << " - " << state.awayShots << '\n';
    std::cout << std::left << std::setw(22) << "Tiros al arco"
              << state.homeShotsOnTarget << " - "
              << state.awayShotsOnTarget << '\n';
    std::cout << std::left << std::setw(22) << "Corners"
              << state.homeCorners << " - " << state.awayCorners << '\n';
    std::cout << std::left << std::setw(22) << "Faltas"
              << state.homeFouls << " - " << state.awayFouls << '\n';
    std::cout << std::left << std::setw(22) << "Tarjetas amarillas"
              << state.homeYellowCards << " - "
              << state.awayYellowCards << '\n';
    std::cout << std::left << std::setw(22) << "Tarjetas rojas"
              << state.homeRedCards << " - " << state.awayRedCards << '\n';
    std::cout << std::left << std::setw(22) << "xG"
              << std::fixed << std::setprecision(2)
              << state.homeExpectedGoals << " - "
              << state.awayExpectedGoals << '\n';

    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "Ultimo evento\n\n";
    std::cout << state.lastEvent << '\n';
    std::cout << "============================================================\n";
    std::cout.flush();
}

void pauseAfterEvent(PlaybackSpeed speed, MatchEventType type) {
    const int milliseconds = delayForSpeed(speed, isImportantEvent(type));
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

    if (options.clearScreenBetweenEvents) {
        clearConsole();
    }

    drawMatchCenter(home, away, result, state, false);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(delayForSpeed(options.speed, false)));

    for (const MatchEvent& event : result.timeline.events) {
        applyEventImpact(state, event);

        if (!shouldDisplayEvent(event, options.showAllEvents)) {
            continue;
        }

        if (options.clearScreenBetweenEvents) {
            clearConsole();
        }

        drawMatchCenter(home, away, result, state, false);
        pauseAfterEvent(options.speed, event.type);
    }

    state.minute = std::max(90, state.minute);
    state.homeGoals = result.homeGoals;
    state.awayGoals = result.awayGoals;
    state.homeShots = result.stats.homeShots;
    state.awayShots = result.stats.awayShots;
    state.homeShotsOnTarget = result.stats.homeShotsOnTarget;
    state.awayShotsOnTarget = result.stats.awayShotsOnTarget;
    state.homeCorners = result.stats.homeCorners;
    state.awayCorners = result.stats.awayCorners;
    state.homeFouls = result.stats.homeFouls;
    state.awayFouls = result.stats.awayFouls;
    state.homeYellowCards = result.stats.homeYellowCards;
    state.awayYellowCards = result.stats.awayYellowCards;
    state.homeRedCards = result.stats.homeRedCards;
    state.awayRedCards = result.stats.awayRedCards;
    state.homeExpectedGoals = result.stats.homeExpectedGoals;
    state.awayExpectedGoals = result.stats.awayExpectedGoals;
    state.lastEvent = "Final del partido.";

    if (options.clearScreenBetweenEvents) {
        clearConsole();
    }

    drawMatchCenter(home, away, result, state, true);
}

}  // namespace match_center
