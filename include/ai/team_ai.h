#pragma once

#include "engine/models.h"

#include <vector>

namespace team_ai {

void adjustCpuTactics(Team& team, const Team& opponent, const Team* myTeam = nullptr);
bool applyInMatchCpuAdjustment(Team& team,
                               const Team& opponent,
                               int minute,
                               int goalsFor,
                               int goalsAgainst,
                               std::vector<std::string>* events = nullptr,
                               int availablePlayers = 11,
                               int cautionedPlayers = 0,
                               int opponentAvailablePlayers = 11,
                               int momentumScore = 0);

}  // namespace team_ai
