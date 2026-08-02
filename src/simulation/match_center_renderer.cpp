#include "simulation/match_center_renderer.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

namespace match_center {
namespace {

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


std::string momentumBar(int score, int width = 41) {
    score = std::clamp(score, -100, 100);

    const int center = width / 2;
    const int magnitude = std::abs(score) * center / 100;

    std::string bar(static_cast<std::size_t>(width), '.');
    bar[static_cast<std::size_t>(center)] = '|';

    if (score > 0) {
        for (int i = 1; i <= magnitude; ++i) {
            bar[static_cast<std::size_t>(center - i)] = '#';
        }
    } else if (score < 0) {
        for (int i = 1; i <= magnitude; ++i) {
            bar[static_cast<std::size_t>(center + i)] = '#';
        }
    }

    return bar;
}

}  // namespace

void drawMatchCenter(const Team& home,
                     const Team& away,
                     const MatchResult& result,
                     const LiveState& state,
                     bool finished) {
    const int homePossession =
        std::clamp(state.homePossession, 0, 100);

    const int awayPossession =
        100 - homePossession;

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
    std::cout << "Posesion en vivo\n";
    std::cout << home.name << " [" << progressBar(homePossession) << "] "
              << homePossession << "%\n";
    std::cout << away.name << " [" << progressBar(awayPossession) << "] "
              << awayPossession << "%\n";

    std::cout << "\nMomentum\n";
    std::cout << "LOCAL [" << momentumBar(state.momentumScore)
              << "] VISITA\n";
    std::cout << momentumLabel(state.momentumScore)
              << " (" << state.momentumScore << ")\n";

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

}  // namespace match_center
