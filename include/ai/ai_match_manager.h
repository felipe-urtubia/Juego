#pragma once

#include "engine/models.h"
#include "simulation/match_types.h"

#include <vector>

namespace ai_match_manager {

bool applyInMatchManagement(Team& team,
                            const Team& opponent,
                            std::vector<int>& xi,
                            std::vector<int>& participants,
                            const std::vector<std::string>& cautionedPlayers,
                            int minute,
                            int goalsFor,
                            int goalsAgainst,
                            int opponentAvailablePlayers,
                            int momentumScore,
                            MatchTimeline& timeline);

}  // namespace ai_match_manager
