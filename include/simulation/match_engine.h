#pragma once

#include "engine/models.h"
#include "simulation/player_rating_system.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace match_engine {

struct MatchSimulationData {
    MatchResult result;
    std::vector<int> homeParticipants;
    std::vector<int> awayParticipants;
    std::vector<std::string> homeYellowCardPlayers;
    std::vector<std::string> awayYellowCardPlayers;
    std::vector<std::string> homeRedCardPlayers;
    std::vector<std::string> awayRedCardPlayers;
    std::vector<std::string> homeInjuredPlayers;
    std::vector<std::string> awayInjuredPlayers;
    std::vector<GoalContribution> homeGoals;
    std::vector<GoalContribution> awayGoals;
};

enum class ManagerDecisionType {
    Continue,
    ChangeTactics,
    ChangeInstruction,
    Substitute
};

struct ManagerDecision {
    ManagerDecisionType type = ManagerDecisionType::Continue;
    std::string tactics;
    std::string instruction;
    int playerOutIndex = -1;
    int playerInIndex = -1;
};

struct InteractiveMatchState {
    int minute = 0;
    bool userIsHome = true;

    int homeGoals = 0;
    int awayGoals = 0;
    int homeShots = 0;
    int awayShots = 0;
    int homeDangerousAttacks = 0;
    int awayDangerousAttacks = 0;
    int homePossession = 50;
    int awayPossession = 50;

    std::array<int, 9> homeHeatMap{};
    std::array<int, 9> awayHeatMap{};

    int substitutionsUsed = 0;

    std::string currentTactics;
    std::string currentInstruction;

    std::vector<int> activeXi;
    std::vector<int> availableBench;
    std::vector<std::string> recentEvents;
    std::vector<std::string> timelineEvents;
    std::vector<player_rating_system::PlayerLiveRating> playerStats;
};

using ManagerDecisionCallback =
    std::function<ManagerDecision(const InteractiveMatchState&)>;

MatchSimulationData simulateInteractive(
    const Team& home,
    const Team& away,
    bool userControlsHome,
    const ManagerDecisionCallback& decisionCallback,
    bool keyMatch = false,
    bool neutralVenue = false);

MatchSimulationData simulateInteractive(
    const Team& home,
    const Team& away,
    const Career* career,
    bool userControlsHome,
    const ManagerDecisionCallback& decisionCallback,
    bool keyMatch = false,
    bool neutralVenue = false);

MatchSimulationData simulate(const Team& home, const Team& away, bool keyMatch = false, bool neutralVenue = false);
MatchSimulationData simulate(const Team& home, const Team& away, const Career* career, bool keyMatch = false, bool neutralVenue = false);

}  // namespace match_engine
