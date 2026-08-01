#pragma once

#include "engine/models.h"
#include "simulation/match_center_state.h"

namespace match_center {

void drawMatchCenter(const Team& home,
                     const Team& away,
                     const MatchResult& result,
                     const LiveState& state,
                     bool finished);

}  // namespace match_center
