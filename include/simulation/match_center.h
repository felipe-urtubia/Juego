#pragma once

#include "engine/models.h"

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

}  // namespace match_center
