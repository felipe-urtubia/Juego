#include "simulation/match_center.h"
#include "simulation/match_commentary.h"
#include "simulation/match_center_renderer.h"
#include "simulation/match_center_state.h"
#include "utils/utils.h"

#include <iostream>
#include <iomanip>
#include <algorithm>

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

void drawInteractiveHeatMap(const std::string& teamName, const std::array<int, 9>& heatMap) {
    std::cout << teamName << "\n";
    std::cout << "              Izq  Centro  Der\n";
    std::cout << "Propio       " << heatMap[0] << "    " << heatMap[1] << "       " << heatMap[2] << "\n";
    std::cout << "Mediocampo   " << heatMap[3] << "    " << heatMap[4] << "       " << heatMap[5] << "\n";
    std::cout << "Ultimo tercio " << heatMap[6] << "    " << heatMap[7] << "       " << heatMap[8] << "\n";
}

void drawInteractivePlayerStats(
    const std::vector<player_rating_system::PlayerLiveRating>& players) {

    if (players.empty()) {
        return;
    }

    std::cout << "\nEstadisticas individuales destacadas\n";

    const std::size_t limit = std::min<std::size_t>(3, players.size());

    for (std::size_t i = 0; i < limit; ++i) {
        const auto& player = players[i];

        std::cout << std::left
                  << std::setw(24)
                  << player.playerName
                  << std::fixed
                  << std::setprecision(1)
                  << player.rating
                  << " | Tiros " << player.shots
                  << " | Al arco " << player.shotsOnTarget
                  << " | Goles " << player.goals
                  << " | xG " << std::setprecision(2) << player.expectedGoals
                  << " | Atajadas " << player.saves
                  << " | TA " << player.yellowCards
                  << " | TR " << player.redCards
                  << '\n';
    }
}

}  // namespace

match_engine::ManagerDecision askManagerDecision(
    const Team& controlledTeam,
    const match_engine::InteractiveMatchState& state) {

    using match_engine::ManagerDecision;
    using match_engine::ManagerDecisionType;

    std::cout << "\n=== MATCH CENTER ===\n";
    std::cout << "Minuto " << state.minute << "\n";
    std::cout << "Marcador: "
              << state.homeGoals << " - "
              << state.awayGoals << "\n";
    std::cout << "Tiros: "
              << state.homeShots << " - "
              << state.awayShots << "\n";
    std::cout << "Ataques peligrosos: "
              << state.homeDangerousAttacks << " - "
              << state.awayDangerousAttacks << "\n";
    std::cout << "Posesion: "
              << state.homePossession << "% - "
              << state.awayPossession << "%\n";
    std::cout << "\nMapa de calor por zonas\n";
    drawInteractiveHeatMap("LOCAL", state.homeHeatMap);
    std::cout << "\n";
    drawInteractiveHeatMap("VISITA", state.awayHeatMap);
    drawInteractivePlayerStats(state.playerStats);
    std::cout << "Tactica actual: "
              << state.currentTactics << "\n";
    std::cout << "Instruccion actual: "
              << state.currentInstruction << "\n";
    std::cout << "Cambios usados: "
              << state.substitutionsUsed << "/5\n";

    if (!state.recentEvents.empty()) {
        std::cout << "\nEventos recientes:\n";
        for (const std::string& event : state.recentEvents) {
            std::cout << "- " << event << "\n";
        }
    }

    std::cout << "\n";

    std::cout << "1. Continuar\n";
    std::cout << "2. Cambiar tactica\n";
    std::cout << "3. Cambiar instruccion\n";
    std::cout << "4. Hacer sustitucion\n";

    const int action =
        readInt("Elige una opcion: ", 1, 4);

    ManagerDecision decision;

    if (action == 1) {
        return decision;
    }

    if (action == 2) {
        static const std::vector<std::string> tactics = {
            "Defensive",
            "Balanced",
            "Offensive",
            "Pressing",
            "Counter"
        };

        std::cout << "\nTacticas:\n";
        for (size_t i = 0; i < tactics.size(); ++i) {
            std::cout << i + 1 << ". "
                      << tactics[i] << "\n";
        }

        const int choice =
            readInt(
                "Nueva tactica: ",
                1,
                static_cast<int>(tactics.size()));

        decision.type =
            ManagerDecisionType::ChangeTactics;

        decision.tactics =
            tactics[static_cast<size_t>(choice - 1)];

        return decision;
    }

    if (action == 3) {
        static const std::vector<std::string> instructions = {
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

        std::cout << "\nInstrucciones:\n";
        for (size_t i = 0; i < instructions.size(); ++i) {
            std::cout << i + 1 << ". "
                      << instructions[i] << "\n";
        }

        const int choice =
            readInt(
                "Nueva instruccion: ",
                1,
                static_cast<int>(instructions.size()));

        decision.type =
            ManagerDecisionType::ChangeInstruction;

        decision.instruction =
            instructions[static_cast<size_t>(choice - 1)];

        return decision;
    }

    if (state.substitutionsUsed >= 5 ||
        state.activeXi.empty() ||
        state.availableBench.empty()) {

        std::cout
            << "No hay sustituciones disponibles.\n";

        return decision;
    }

    std::cout << "\nJugadores en cancha:\n";

    for (size_t i = 0;
         i < state.activeXi.size();
         ++i) {

        const int playerIndex =
            state.activeXi[i];

        if (playerIndex < 0 ||
            playerIndex >=
                static_cast<int>(
                    controlledTeam.players.size())) {
            continue;
        }

        const Player& player =
            controlledTeam.players[
                static_cast<size_t>(playerIndex)];

        std::cout << i + 1 << ". "
                  << player.name
                  << " (" << player.position << ")\n";
    }

    const int outChoice =
        readInt(
            "Jugador que sale: ",
            1,
            static_cast<int>(
                state.activeXi.size()));

    std::cout << "\nSuplentes disponibles:\n";

    for (size_t i = 0;
         i < state.availableBench.size();
         ++i) {

        const int playerIndex =
            state.availableBench[i];

        if (playerIndex < 0 ||
            playerIndex >=
                static_cast<int>(
                    controlledTeam.players.size())) {
            continue;
        }

        const Player& player =
            controlledTeam.players[
                static_cast<size_t>(playerIndex)];

        std::cout << i + 1 << ". "
                  << player.name
                  << " (" << player.position << ")\n";
    }

    const int inChoice =
        readInt(
            "Jugador que entra: ",
            1,
            static_cast<int>(
                state.availableBench.size()));

    decision.type =
        ManagerDecisionType::Substitute;

    decision.playerOutIndex =
        state.activeXi[
            static_cast<size_t>(
                outChoice - 1)];

    decision.playerInIndex =
        state.availableBench[
            static_cast<size_t>(
                inChoice - 1)];

    return decision;
}

void showInteractiveFinalSummary(
    const Team& home,
    const Team& away,
    const MatchResult& result) {

    std::cout << "\n=== MATCH CENTER - FINAL ===\n";
    std::cout << home.name << " "
              << result.homeGoals << " - "
              << result.awayGoals << " "
              << away.name << "\n";
    std::cout << "Tiros: "
              << result.homeShots << " - "
              << result.awayShots << "\n";
    std::cout << "Ataques peligrosos: "
              << result.stats.homeDangerousAttacks << " - "
              << result.stats.awayDangerousAttacks << "\n";
    std::cout << "Posesion: "
              << result.homePossession << "% - "
              << result.awayPossession << "%\n";
    std::cout << "Cambios: "
              << result.homeSubstitutions << " - "
              << result.awaySubstitutions << "\n";

    std::vector<const MatchEvent*> finalEvents;

    for (const MatchEvent& event : result.timeline.events) {
        const bool importantEvent =
            event.type == MatchEventType::Goal ||
            event.type == MatchEventType::YellowCard ||
            event.type == MatchEventType::RedCard ||
            event.type == MatchEventType::Injury ||
            event.type == MatchEventType::Substitution ||
            event.type == MatchEventType::TacticalChange;

        if (event.minute > 75 && importantEvent) {
            finalEvents.push_back(&event);
        }
    }

    std::stable_sort(
        finalEvents.begin(),
        finalEvents.end(),
        [](const MatchEvent* left, const MatchEvent* right) {
            return left->minute < right->minute;
        });

    if (!finalEvents.empty()) {
        std::cout << "\nEventos finales:\n";

        for (const MatchEvent* event : finalEvents) {
            std::cout << "- "
                      << event->minute << "' "
                      << event->teamName << ": "
                      << event->description << "\n";
        }
    }

    std::cout << "\n";
}

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
        updateHeatMap(state, event, home, away);
        updateLivePossession(state, result, state.minute);
        updateLiveMomentum(state, event, home, away);
        applyLiveManagementEvent(state, event, home, away);
        state.playerRatings.applyEvent(event);
        state.lastEvent = match_commentary::buildCommentary(
            event,
            state,
            home,
            away);

        if (!shouldDisplayEvent(event, options.showAllEvents)) {
            continue;
        }

        if (options.clearScreenBetweenEvents) {
            clearConsole();
        }

        drawMatchCenter(home, away, result, state, false);
        pauseAfterEvent(options.speed, event.type);
    }

    const auto homeHeatMap = state.homeHeatMap;
    const auto awayHeatMap = state.awayHeatMap;

    state = makeFinalState(result);
    state.homeHeatMap = homeHeatMap;
    state.awayHeatMap = awayHeatMap;

    if (options.clearScreenBetweenEvents) {
        clearConsole();
    }

    drawMatchCenter(home, away, result, state, true);
}

}  // namespace match_center
