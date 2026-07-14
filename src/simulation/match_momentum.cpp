#include "simulation/match_momentum.h"

#include <algorithm>

using namespace std;

void MatchMomentum::reset()
{
    homeMomentum = 0.0;
    awayMomentum = 0.0;

    homeConfidence = 0.5;
    awayConfidence = 0.5;

    homePressure = 0.0;
    awayPressure = 0.0;
}

void MatchMomentum::homeAttack()
{
    homeMomentum += 0.04;
    awayMomentum -= 0.02;

    homePressure += 0.02;
    homePressure = clamp(homePressure, 0.0, 1.0);
    homeMomentum = clamp(homeMomentum,-1.0,1.0);
    awayMomentum = clamp(awayMomentum,-1.0,1.0);
}

void MatchMomentum::awayAttack()
{
    awayMomentum += 0.04;
    homeMomentum -= 0.02;

    awayPressure += 0.02;
    awayPressure = clamp(awayPressure, 0.0, 1.0);

    homeMomentum = clamp(homeMomentum,-1.0,1.0);
    awayMomentum = clamp(awayMomentum,-1.0,1.0);
}

void MatchMomentum::homeGoal()
{
    homeConfidence += 0.15;
    awayConfidence -= 0.10;

    homeMomentum += 0.20;
    awayMomentum -= 0.20;

    homeConfidence = clamp(homeConfidence,0.0,1.0);
    awayConfidence = clamp(awayConfidence,0.0,1.0);
}

void MatchMomentum::awayGoal()
{
    awayConfidence += 0.15;
    homeConfidence -= 0.10;

    awayMomentum += 0.20;
    homeMomentum -= 0.20;

    homeConfidence = clamp(homeConfidence,0.0,1.0);
    awayConfidence = clamp(awayConfidence,0.0,1.0);
}

void MatchMomentum::decay()
{
    homeMomentum*=0.94;
    awayMomentum*=0.94;

    homePressure*=0.96;
    awayPressure*=0.96;
}

double MatchMomentum::homeBonus() const
{
    return homeMomentum*0.30
            +homeConfidence*0.20
            +homePressure*0.15;
}

double MatchMomentum::awayBonus() const
{
    return awayMomentum*0.30
            +awayConfidence*0.20
            +awayPressure*0.15;
}