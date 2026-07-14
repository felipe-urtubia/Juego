#pragma once

namespace Football
{
    enum class Mentality
    {
        VeryDefensive,
        Defensive,
        Balanced,
        Attacking,
        VeryAttacking
    };

    enum class MatchInstruction
    {
        Balanced,
        Wings,
        Central,
        CounterAttack,
        Possession,
        LongBalls,
        SetPieces,
        HighPress,
        TimeWasting
    };

    enum class TrainingIntensity
    {
        Low,
        Normal,
        High
    };
}