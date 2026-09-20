#pragma once

#include "engine/models.h"
#include "simulation/match_engine.h"

namespace match_center {

enum class PlaybackSpeed {
    Fast,
    Normal,
    Slow
};

struct PlaybackOptions {
    PlaybackSpeed speed = PlaybackSpeed::Normal;
    bool clearScreenBetweenEvents = true;
    bool showAllEvents = false;
};

void showMatchCenter(const Team& home,
                     const Team& away,
                     const MatchResult& result,
                     const PlaybackOptions& options = {});

match_engine::ManagerDecision askManagerDecision(
    const Team& controlledTeam,
    const match_engine::InteractiveMatchState& state);

void showInteractiveFinalSummary(
    const Team& home,
    const Team& away,
    const MatchResult& result);

}  // namespace match_center
