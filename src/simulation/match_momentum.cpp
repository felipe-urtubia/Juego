#include "simulation/match_momentum.h"

#include <algorithm>

using namespace std;

namespace {

constexpr double kNeutralConfidence = 0.5;

double clampMomentum(double value)
{
    return clamp(value, -1.0, 1.0);
}

double clampConfidence(double value)
{
    return clamp(value, 0.0, 1.0);
}

double clampPressure(double value)
{
    return clamp(value, 0.0, 1.0);
}

} // namespace

void MatchMomentum::reset()
{
    homeMomentum = 0.0;
    awayMomentum = 0.0;

    homeConfidence = kNeutralConfidence;
    awayConfidence = kNeutralConfidence;

    homePressure = 0.0;
    awayPressure = 0.0;
}

void MatchMomentum::homeAttack()
{
    homeMomentum += 0.04;
    awayMomentum -= 0.02;

    homePressure += 0.02;
    awayPressure -= 0.01;

    homeMomentum = clampMomentum(homeMomentum);
    awayMomentum = clampMomentum(awayMomentum);

    homePressure = clampPressure(homePressure);
    awayPressure = clampPressure(awayPressure);
}

void MatchMomentum::awayAttack()
{
    awayMomentum += 0.04;
    homeMomentum -= 0.02;

    awayPressure += 0.02;
    homePressure -= 0.01;

    homeMomentum = clampMomentum(homeMomentum);
    awayMomentum = clampMomentum(awayMomentum);

    homePressure = clampPressure(homePressure);
    awayPressure = clampPressure(awayPressure);
}

void MatchMomentum::homeGoal()
{
    homeConfidence += 0.15;
    awayConfidence -= 0.10;

    homeMomentum += 0.20;
    awayMomentum -= 0.20;

    homeConfidence = clampConfidence(homeConfidence);
    awayConfidence = clampConfidence(awayConfidence);

    homeMomentum = clampMomentum(homeMomentum);
    awayMomentum = clampMomentum(awayMomentum);
}

void MatchMomentum::awayGoal()
{
    awayConfidence += 0.15;
    homeConfidence -= 0.10;

    awayMomentum += 0.20;
    homeMomentum -= 0.20;

    homeConfidence = clampConfidence(homeConfidence);
    awayConfidence = clampConfidence(awayConfidence);

    homeMomentum = clampMomentum(homeMomentum);
    awayMomentum = clampMomentum(awayMomentum);
}

void MatchMomentum::decay()
{
    homeMomentum *= 0.94;
    awayMomentum *= 0.94;

    homePressure *= 0.96;
    awayPressure *= 0.96;

    homeConfidence +=
        (kNeutralConfidence - homeConfidence) * 0.04;

    awayConfidence +=
        (kNeutralConfidence - awayConfidence) * 0.04;

    homeMomentum = clampMomentum(homeMomentum);
    awayMomentum = clampMomentum(awayMomentum);

    homePressure = clampPressure(homePressure);
    awayPressure = clampPressure(awayPressure);

    homeConfidence = clampConfidence(homeConfidence);
    awayConfidence = clampConfidence(awayConfidence);
}

double MatchMomentum::homeBonus() const
{
    return homeMomentum * 0.30
           + (homeConfidence - kNeutralConfidence) * 0.20
           + homePressure * 0.15;
}

double MatchMomentum::awayBonus() const
{
    return awayMomentum * 0.30
           + (awayConfidence - kNeutralConfidence) * 0.20
           + awayPressure * 0.15;
}