#pragma once

#include "engine/models.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace player_rating_system {

struct PlayerLiveRating {
    std::string playerName;
    std::string teamName;
    double rating = 6.5;
    int events = 0;
};

class LiveRatings {
public:
    void applyEvent(const MatchEvent& event);

    double ratingFor(const std::string& playerName) const;

    std::vector<PlayerLiveRating> topPlayers(
        std::size_t limit = 5) const;

    std::vector<PlayerLiveRating> bottomPlayers(
        std::size_t limit = 5) const;

    bool empty() const;

private:
    std::unordered_map<std::string, PlayerLiveRating> ratings_;
};

double clampRating(double rating);

}  // namespace player_rating_system
