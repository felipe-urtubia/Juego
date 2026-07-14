#pragma once

struct MatchMomentum
{
    double homeMomentum = 0.0;
    double awayMomentum = 0.0;

    double homeConfidence = 0.5;
    double awayConfidence = 0.5;

    double homePressure = 0.0;
    double awayPressure = 0.0;

    void reset();

    void homeAttack();

    void awayAttack();

    void homeGoal();

    void awayGoal();

    void decay();

    double homeBonus() const;

    double awayBonus() const;
};