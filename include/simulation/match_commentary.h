#pragma once

#include "engine/models.h"
#include "simulation/match_center_state.h"

#include <string>

namespace match_commentary {

std::string buildCommentary(
    const MatchEvent& event,
    const match_center::LiveState& state,
    const Team& home,
    const Team& away);

}  // namespace match_commentary
