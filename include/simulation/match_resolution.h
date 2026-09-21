#pragma once

#include "engine/models.h"
#include "simulation/match_types.h"

struct ChanceResolutionInput {
    int minute = 0;
    bool bigChance = false;
    bool attackingTeamIsHome = true;
    MatchFieldZone zone = MatchFieldZone::Unknown;
    double chanceQuality = 0.0;
    double attackingEdge = 0.0;
    double defensivePressure = 0.0;
    double finishingSupport = 0.0;
    double goalkeeperQuality = 0.0;
};

struct ChanceResolutionOutput {
    bool scored = false;
    bool onTarget = false;
    bool wonCorner = false;
    double expectedGoals = 0.0;
    int shooterIndex = -1;
    int assisterIndex = -1;
    MatchEvent attemptEvent;
    std::vector<MatchEvent> outcomeEvents;
};

namespace match_resolution {

double opportunityProbability(double effectiveAttack,
                              double effectiveDefense,
                              double possessionFactor,
                              double mentalityFactor,
                              double matchMomentFactor);
double progressionProbability(double buildUpQuality,
                              double pressResistance,
                              double opponentPressingLoad,
                              double possessionFactor,
                              double matchMomentFactor);
double attackConversionProbability(double lineBreakThreat,
                                   double opponentDefensiveShape,
                                   double tacticalRisk,
                                   double mentalityFactor,
                                   double fatigueFactor);
double shotCreationProbability(double chanceCreation,
                               double finishingQuality,
                               double opponentDefense,
                               double defensiveRisk,
                               double setPieceThreat,
                               double fatigueFactor);

}  // namespace match_resolution
