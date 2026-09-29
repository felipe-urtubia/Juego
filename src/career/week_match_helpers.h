#pragma once

#include "engine/models.h"
#include "simulation.h"

#include <utility>

namespace career_week_matches {

struct ScheduledTeamRef {
    TeamId id = kInvalidTeamId;
    Team* team = nullptr;
};

struct ScheduledMatchRef {
    ScheduledTeamRef home;
    ScheduledTeamRef away;

    bool valid() const {
        return home.team && away.team;
    }
};

ScheduledTeamRef scheduledTeamRef(Career& career, int index);

ScheduledMatchRef scheduledMatchRef(
    Career& career,
    const std::pair<int, int>& match);

void updateRivalMemoryForUserMatch(
    Career& career,
    const Team& home,
    const Team& away,
    const MatchResult& result);

}  // namespace career_week_matches
