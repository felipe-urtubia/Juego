#pragma once

#include "engine/models.h"
#include "simulation/match_momentum.h"

#include <string>

namespace match_center {

struct LiveState {
    int minute = 0;

    int homeGoals = 0;
    int awayGoals = 0;

    int homeShots = 0;
    int awayShots = 0;

    int homeShotsOnTarget = 0;
    int awayShotsOnTarget = 0;

    int homePossession = 50;
    int awayPossession = 50;

    MatchMomentum momentum;
    int momentumScore = 0;

    int homeCorners = 0;
    int awayCorners = 0;

    int homeFouls = 0;
    int awayFouls = 0;

    int homeYellowCards = 0;
    int awayYellowCards = 0;

    int homeRedCards = 0;
    int awayRedCards = 0;

    double homeExpectedGoals = 0.0;
    double awayExpectedGoals = 0.0;

    std::string lastEvent =
        "El partido está a punto de comenzar.";
};

std::string eventText(const MatchEvent& event);

void applyEventImpact(
    LiveState& state,
    const MatchEvent& event);

void updateLivePossession(
    LiveState& state,
    const MatchResult& result,
    int currentMinute);

void updateLiveMomentum(
    LiveState& state,
    const MatchEvent& event,
    const Team& home,
    const Team& away);

std::string momentumLabel(int momentumScore);

LiveState makeFinalState(
    const MatchResult& result);

}  // namespace match_center
