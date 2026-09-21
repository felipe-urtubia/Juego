#pragma once

#include <string>
#include <vector>

enum class MatchFieldZone {
    Unknown,
    OwnLeft,
    OwnCenter,
    OwnRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    FinalLeft,
    FinalCenter,
    FinalRight
};
enum class MatchEventType {
    PossessionPhase,
    Progression,
    AttackBuildUp,
    Shot,
    BigChance,
    Goal,
    Miss,
    Save,
    Foul,
    YellowCard,
    RedCard,
    Injury,
    Corner,
    Offside,
    Counterattack,
    TacticalChange,
    Substitution
};

inline const char* matchEventTypeLabel(MatchEventType type) {
    switch (type) {
        case MatchEventType::PossessionPhase: return "possession_phase";
        case MatchEventType::Progression: return "progression";
        case MatchEventType::AttackBuildUp: return "attack_build_up";
        case MatchEventType::Shot: return "shot";
        case MatchEventType::BigChance: return "big_chance";
        case MatchEventType::Goal: return "goal";
        case MatchEventType::Miss: return "miss";
        case MatchEventType::Save: return "save";
        case MatchEventType::Foul: return "foul";
        case MatchEventType::YellowCard: return "yellow_card";
        case MatchEventType::RedCard: return "red_card";
        case MatchEventType::Injury: return "injury";
        case MatchEventType::Corner: return "corner";
        case MatchEventType::Offside: return "offside";
        case MatchEventType::Counterattack: return "counterattack";
        case MatchEventType::TacticalChange: return "tactical_change";
        case MatchEventType::Substitution: return "substitution";
    }
    return "unknown";
}

struct MatchEventImpact {
    int homeGoalsDelta = 0;
    int awayGoalsDelta = 0;
    int homeShotsDelta = 0;
    int awayShotsDelta = 0;
    int homeShotsOnTargetDelta = 0;
    int awayShotsOnTargetDelta = 0;
    int homeCornersDelta = 0;
    int awayCornersDelta = 0;
    int homeDangerousAttacksDelta = 0;
    int awayDangerousAttacksDelta = 0;
    int homeFoulsDelta = 0;
    int awayFoulsDelta = 0;
    int homeYellowCardsDelta = 0;
    int awayYellowCardsDelta = 0;
    int homeRedCardsDelta = 0;
    int awayRedCardsDelta = 0;
    double homeExpectedGoalsDelta = 0.0;
    double awayExpectedGoalsDelta = 0.0;
};

struct MatchEvent {
    int minute = 0;
    std::string teamName;
    std::string playerName;
    MatchEventType type = MatchEventType::PossessionPhase;
    std::string description;
    MatchEventImpact impact;
    MatchFieldZone zone = MatchFieldZone::Unknown;
};

struct MatchPhaseReport {
    int minuteStart = 0;
    int minuteEnd = 0;
    std::string dominantTeam;
    int homePlayersAvailable = 11;
    int awayPlayersAvailable = 11;
    int homePossessionShare = 50;
    int awayPossessionShare = 50;
    int homePossessionChains = 0;
    int awayPossessionChains = 0;
    int homeProgressions = 0;
    int awayProgressions = 0;
    int homeAttacks = 0;
    int awayAttacks = 0;
    int homeShotsGenerated = 0;
    int awayShotsGenerated = 0;
    double intensity = 1.0;
    double homeUrgency = 0.0;
    double awayUrgency = 0.0;
    double homeChanceProbability = 0.0;
    double awayChanceProbability = 0.0;
    double homeDefensiveRisk = 0.0;
    double awayDefensiveRisk = 0.0;
    double injuryRisk = 0.0;
    int homeFatigueGain = 0;
    int awayFatigueGain = 0;
    bool homeTacticalChange = false;
    bool awayTacticalChange = false;
};

struct MatchTimeline {
    std::vector<MatchEvent> events;
    std::vector<MatchPhaseReport> phases;
};

struct MatchStats {
    int homeGoals = 0;
    int awayGoals = 0;
    int homeShots = 0;
    int awayShots = 0;
    int homeShotsOnTarget = 0;
    int awayShotsOnTarget = 0;
    int homePossession = 50;
    int awayPossession = 50;
    int homeFouls = 0;
    int awayFouls = 0;
    int homeYellowCards = 0;
    int awayYellowCards = 0;
    int homeRedCards = 0;
    int awayRedCards = 0;
    int homeCorners = 0;
    int awayCorners = 0;
    int homeDangerousAttacks = 0;
    int awayDangerousAttacks = 0;
    int homeBigChances = 0;
    int awayBigChances = 0;
    double homeExpectedGoals = 0.0;
    double awayExpectedGoals = 0.0;
};

struct TacticalImpactSummary {
    std::string homeSummary;
    std::string awaySummary;
    double homeControlScore = 0.0;
    double awayControlScore = 0.0;
    double homePressingLoad = 0.0;
    double awayPressingLoad = 0.0;
    double homeTransitionThreat = 0.0;
    double awayTransitionThreat = 0.0;
};

struct FatigueImpactSummary {
    int homeFatigueLoad = 0;
    int awayFatigueLoad = 0;
    bool homeLateDrop = false;
    bool awayLateDrop = false;
    std::string summary;
};

struct MatchExplanation {
    std::string likelyReason;
    std::string tacticalStory;
    std::string fatigueStory;
    std::string disciplineStory;
    std::string chanceStory;
};

struct MatchReport {
    std::vector<std::string> phaseSummaries;
    MatchExplanation explanation;
    TacticalImpactSummary tacticalImpact;
    FatigueImpactSummary fatigueImpact;
    std::string playerOfTheMatch;
    int playerOfTheMatchScore = 0;
    std::vector<std::string> playerRatingLines;
    std::string postMatchImpact;
};

struct MatchContext {
    double teamStrengthHome = 0.0;
    double teamStrengthAway = 0.0;
    double attackPowerHome = 0.0;
    double attackPowerAway = 0.0;
    double defensePowerHome = 0.0;
    double defensePowerAway = 0.0;
    double midfieldControlHome = 0.0;
    double midfieldControlAway = 0.0;
    double moraleFactorHome = 1.0;
    double moraleFactorAway = 1.0;
    double fatigueFactorHome = 1.0;
    double fatigueFactorAway = 1.0;
    double tacticalAdvantageHome = 0.0;
    double tacticalAdvantageAway = 0.0;
    double weatherModifier = 1.0;
    double homeAdvantage = 1.0;
    int randomnessSeed = 0;
    std::string weather;
};

struct GoalContribution {
    int minute = 0;
    std::string scorerName;
    std::string assisterName;
};
