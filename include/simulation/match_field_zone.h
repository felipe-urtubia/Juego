#pragma once

#include "simulation/match_types.h"

struct Team;

namespace match_field_zone {

MatchFieldZone ownThird(const Team& team, int sequenceIndex);
MatchFieldZone middleThird(const Team& team, int sequenceIndex);
MatchFieldZone finalThird(const Team& team, int sequenceIndex);

}  // namespace match_field_zone
