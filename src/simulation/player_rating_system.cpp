#include "simulation/player_rating_system.h"

#include <algorithm>

namespace player_rating_system {
namespace {

double eventDelta(MatchEventType type) {
    switch (type) {
        case MatchEventType::Goal:
            return 0.90;

        case MatchEventType::BigChance:
            return 0.35;

        case MatchEventType::Shot:
            return 0.18;

        case MatchEventType::Save:
            return 0.30;

        case MatchEventType::Counterattack:
            return 0.20;

        case MatchEventType::Progression:
        case MatchEventType::AttackBuildUp:
            return 0.08;

        case MatchEventType::Corner:
            return 0.05;

        case MatchEventType::Miss:
            return -0.12;

        case MatchEventType::Offside:
            return -0.08;

        case MatchEventType::Foul:
            return -0.10;

        case MatchEventType::YellowCard:
            return -0.30;

        case MatchEventType::RedCard:
            return -1.40;

        case MatchEventType::Injury:
            return -0.15;

        case MatchEventType::Substitution:
        case MatchEventType::TacticalChange:
        case MatchEventType::PossessionPhase:
            return 0.0;
    }

    return 0.0;
}

}  // namespace

double clampRating(double rating) {
    return std::clamp(rating, 1.0, 10.0);
}

void LiveRatings::applyEvent(const MatchEvent& event) {
    if (event.playerName.empty()) {
        return;
    }

    PlayerLiveRating& player =
        ratings_[event.playerName];

    if (player.playerName.empty()) {
        player.playerName = event.playerName;
        player.teamName = event.teamName;
        player.rating = 6.5;
    }

    player.rating = clampRating(
        player.rating + eventDelta(event.type));

    player.events++;
}

double LiveRatings::ratingFor(
    const std::string& playerName) const {

    const auto it = ratings_.find(playerName);

    if (it == ratings_.end()) {
        return 6.5;
    }

    return it->second.rating;
}

std::vector<PlayerLiveRating> LiveRatings::topPlayers(
    std::size_t limit) const {

    std::vector<PlayerLiveRating> players;

    for (const auto& entry : ratings_) {
        players.push_back(entry.second);
    }

    std::sort(
        players.begin(),
        players.end(),
        [](const PlayerLiveRating& left,
           const PlayerLiveRating& right) {
            if (left.rating != right.rating) {
                return left.rating > right.rating;
            }

            return left.playerName < right.playerName;
        });

    if (players.size() > limit) {
        players.resize(limit);
    }

    return players;
}

std::vector<PlayerLiveRating> LiveRatings::bottomPlayers(
    std::size_t limit) const {

    std::vector<PlayerLiveRating> players;

    for (const auto& entry : ratings_) {
        players.push_back(entry.second);
    }

    std::sort(
        players.begin(),
        players.end(),
        [](const PlayerLiveRating& left,
           const PlayerLiveRating& right) {
            if (left.rating != right.rating) {
                return left.rating < right.rating;
            }

            return left.playerName < right.playerName;
        });

    if (players.size() > limit) {
        players.resize(limit);
    }

    return players;
}

bool LiveRatings::empty() const {
    return ratings_.empty();
}

}  // namespace player_rating_system
