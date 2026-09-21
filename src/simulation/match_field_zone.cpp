#include "simulation/match_field_zone.h"

#include "engine/models.h"

namespace {

int normalizedIndex(int sequenceIndex) {
    const int value = sequenceIndex % 6;
    return value < 0 ? value + 6 : value;
}

int laneFor(const Team& team, int sequenceIndex) {
    const int index = normalizedIndex(sequenceIndex);

    if (team.matchInstruction == "Juego directo" || team.width <= 2) {
        return 1;
    }

    if (team.matchInstruction == "Por bandas" ||
        team.matchInstruction == "Laterales altos" ||
        team.width >= 4) {
        return index % 2 == 0 ? 0 : 2;
    }

    return index % 3;
}

MatchFieldZone zoneForThird(const Team& team, int sequenceIndex, int third) {
    const int lane = laneFor(team, sequenceIndex);

    if (third == 0) {
        if (lane == 0) return MatchFieldZone::OwnLeft;
        if (lane == 1) return MatchFieldZone::OwnCenter;
        return MatchFieldZone::OwnRight;
    }

    if (third == 1) {
        if (lane == 0) return MatchFieldZone::MiddleLeft;
        if (lane == 1) return MatchFieldZone::MiddleCenter;
        return MatchFieldZone::MiddleRight;
    }

    if (lane == 0) return MatchFieldZone::FinalLeft;
    if (lane == 1) return MatchFieldZone::FinalCenter;
    return MatchFieldZone::FinalRight;
}

}  // namespace

namespace match_field_zone {

MatchFieldZone ownThird(const Team& team, int sequenceIndex) {
    return zoneForThird(team, sequenceIndex, 0);
}

MatchFieldZone middleThird(const Team& team, int sequenceIndex) {
    return zoneForThird(team, sequenceIndex, 1);
}

MatchFieldZone finalThird(const Team& team, int sequenceIndex) {
    return zoneForThird(team, sequenceIndex, 2);
}

}  // namespace match_field_zone
