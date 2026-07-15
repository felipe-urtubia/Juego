#include "simulation/match_context.h"

#include "simulation/fatigue_engine.h"
#include "simulation/match_engine_internal.h"
#include "simulation/morale_engine.h"
#include "simulation/player_condition.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

struct WeatherProfile {
    const char* label;
    double modifier;
};

WeatherProfile rollWeatherProfile() {
    const int roll = randInt(1, 100);
    if (roll <= 55) return {"Despejado", 1.00};
    if (roll <= 74) return {"Lluvia", 0.95};
    if (roll <= 84) return {"Viento", 0.97};
    if (roll <= 93) return {"Calor", 0.92};
    return {"Frio", 0.94};
}

TeamMatchSnapshot buildSnapshot(const Team& team,
                                const Team& opponent,
                                const vector<int>& xi,
                                bool keyMatch) {
    TeamMatchSnapshot snapshot;
    snapshot.tacticalProfile =
        tactics_engine::buildTacticalProfile(team);

    const vector<int> requestedXi =
        xi.empty() ? team.getStartingXIIndices() : xi;

    vector<bool> usedPlayers(team.players.size(), false);
    snapshot.xi.reserve(requestedXi.size());

    for (int idx : requestedXi) {
        if (idx < 0 ||
            idx >= static_cast<int>(team.players.size())) {
            continue;
        }

        const size_t playerIndex = static_cast<size_t>(idx);

        if (usedPlayers[playerIndex]) {
            continue;
        }

        usedPlayers[playerIndex] = true;
        snapshot.xi.push_back(idx);
    }

    if (snapshot.xi.empty()) {
        return snapshot;
    }

    int attack = 0;
    int defense = 0;
    int midfield = 0;
    int skill = 0;
    int stamina = 0;
    int form = 0;
    int morale = team.morale;
    int fatigue = 0;
    int chanceCreation = 0;
    int finishingQuality = 0;
    int pressResistance = 0;
    int defensiveShape = 0;
    int lineBreakThreat = 0;
    int pressingLoad = 0;
    int setPieceThreat = 0;

    for (int idx : snapshot.xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(idx)];
        int playerAttack = player.attack;
        int playerDefense = player.defense;
        const int readiness = player_condition::readinessScore(player, team);
        match_internal::applyRoleModifier(player, playerAttack, playerDefense);
        match_internal::applyIndividualInstructionModifier(player, team, playerAttack, playerDefense);
        match_internal::applyTraitModifier(player, playerAttack, playerDefense);
        match_internal::applyPlayerStateModifier(player, team, playerAttack, playerDefense);

        const string pos = normalizePosition(player.position);
        attack += playerAttack;
        defense += playerDefense;
        skill += player.skill + readiness / 8;
        stamina += player.fitness;
        form += player.currentForm;
        morale += player.happiness / 2 + player.chemistry / 3;
        fatigue += max(0, 100 - player.fitness);
        chanceCreation += player.attack / 2 + player.currentForm / 2 + player.chemistry / 3 + readiness / 9;
        pressResistance += player.tacticalDiscipline + player.consistency / 2 + player.chemistry / 2 + readiness / 8;
        defensiveShape += player.defense + player.tacticalDiscipline + player.chemistry / 2 + readiness / 10;
        lineBreakThreat += player.attack + player.currentForm / 2 + player.versatility / 3 + readiness / 10;
        setPieceThreat = max(setPieceThreat, player.setPieceSkill);
        pressingLoad += player.professionalism / 2 + player.stamina / 2 + player.currentForm / 3;

        if (pos == "ARQ") {
            snapshot.goalkeeperPower = max(snapshot.goalkeeperPower,
                                           playerDefense + player.skill / 2 + player.consistency / 3);
            midfield += player.tacticalDiscipline / 2;
            defensiveShape += player.defense / 2 + player.leadership / 3;
        } else if (pos == "DEF") {
            midfield += playerDefense / 3 + player.tacticalDiscipline / 2;
            defensiveShape += player.defense / 2 + player.tacticalDiscipline / 2;
            if (playerHasTrait(player, "Muralla")) defensiveShape += 8;
        } else if (pos == "MED") {
            midfield += (playerAttack + playerDefense) / 2 + player.tacticalDiscipline / 2 + player.chemistry / 3;
            chanceCreation += player.attack / 2 + player.skill / 3 + player.chemistry / 2;
            pressResistance += player.professionalism / 2 + player.chemistry / 2;
            if (playerHasTrait(player, "Pase riesgoso")) chanceCreation += 8;
            if (playerHasTrait(player, "Dos perfiles")) pressResistance += 4;
        } else if (pos == "DEL") {
            midfield += playerAttack / 4 + player.currentForm / 2;
            finishingQuality += player.attack + player.currentForm / 2 + player.bigMatches / 2 + player.consistency / 2;
            lineBreakThreat += player.attack / 2 + player.currentForm / 2;
            if (playerHasTrait(player, "Competidor")) finishingQuality += 8;
            if (playerHasTrait(player, "Llega al area")) lineBreakThreat += 6;
        }

        const string compactRole = match_internal::compactToken(player.role);
        const string compactDuty = match_internal::compactToken(player.roleDuty);
        const string compactInstruction = match_internal::compactToken(player.individualInstruction);
        if (compactRole == "poacher" || compactRole == "objetivo") finishingQuality += 5;
        if (compactRole == "organizador" || compactRole == "enganche") chanceCreation += 6;
        if (compactRole == "carrilero") lineBreakThreat += 5;
        if (compactDuty == "ataque") {
            lineBreakThreat += 8;
            chanceCreation += 4;
            defensiveShape -= 2;
        } else if (compactDuty == "apoyo") {
            chanceCreation += 2;
            pressResistance += 2;
        } else if (compactDuty == "defensa") {
            defensiveShape += 8;
            pressResistance += 2;
            lineBreakThreat -= 2;
            pressingLoad -= 1;
        }
        if (compactInstruction == "arriesgarpase") {
            chanceCreation += 6;
            pressResistance -= 2;
        } else if (compactInstruction == "abrircampo") {
            lineBreakThreat += 4;
            setPieceThreat += 2;
        } else if (compactInstruction == "cerrarpordentro") {
            defensiveShape += 6;
            lineBreakThreat -= 2;
        } else if (compactInstruction == "atacarespalda") {
            lineBreakThreat += 8;
            finishingQuality += 4;
        } else if (compactInstruction == "conservarposicion") {
            defensiveShape += 4;
            pressResistance += 3;
        } else if (compactInstruction == "marcarfuerte") {
            defensiveShape += 5;
            pressingLoad += 2;
        } else if (compactInstruction == "descansomedico") {
            pressingLoad -= 4;
            chanceCreation -= 2;
        }
    }

    match_internal::applyFormationBias(team, snapshot.xi, attack, defense);
    match_internal::applyMatchInstruction(team, attack, defense);

    if (!team.captain.empty()) {
        for (int idx : snapshot.xi) {
            if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
            const Player& player = team.players[static_cast<size_t>(idx)];
            if (player.name == team.captain) {
                defense += player.leadership / 3;
                midfield += player.leadership / 3;
                morale += player.leadership / 2;
                break;
            }
        }
    }

    snapshot.averageSkill = skill / max(1, static_cast<int>(snapshot.xi.size()));
    snapshot.averageStamina = stamina / max(1, static_cast<int>(snapshot.xi.size()));
    snapshot.collectiveForm = form / max(1, static_cast<int>(snapshot.xi.size()));
    snapshot.collectiveMorale = morale / max(1, static_cast<int>(snapshot.xi.size()) + 1);
    snapshot.collectiveFatigue = fatigue / max(1, static_cast<int>(snapshot.xi.size()));
    snapshot.attackPower = attack / max(1, static_cast<int>(snapshot.xi.size())) + snapshot.averageSkill / 2;
    const int goalkeeperBonus =
    max(0, snapshot.goalkeeperPower - 50) / 4;

    snapshot.defensePower =
        defense / max(1, static_cast<int>(snapshot.xi.size())) +
        goalkeeperBonus +
        snapshot.averageSkill / 3;
    snapshot.midfieldControl = midfield / max(1, static_cast<int>(snapshot.xi.size())) + snapshot.collectiveForm / 3;
    snapshot.moraleFactor = morale_engine::collectiveMoraleFactor(team, snapshot.xi, keyMatch);
    snapshot.fatigueFactor = fatigue_engine::collectiveFatigueFactor(team, snapshot.xi);
    snapshot.tacticalCompatibility = tactics_engine::tacticalCompatibility(team, snapshot.xi);
    snapshot.chanceCreation =
        chanceCreation / max(1, static_cast<int>(snapshot.xi.size())) + snapshot.midfieldControl / 3 + snapshot.collectiveForm / 4;
    snapshot.finishingQuality =
        max(42, finishingQuality / max(1, max(1, static_cast<int>(snapshot.xi.size()) / 3)) + snapshot.averageSkill / 4);
    snapshot.pressResistance =
        pressResistance / max(1, static_cast<int>(snapshot.xi.size())) + snapshot.midfieldControl / 4 + snapshot.collectiveMorale / 5;
    snapshot.defensiveShape =
        defensiveShape / max(1, static_cast<int>(snapshot.xi.size())) + snapshot.defensePower / 4 + snapshot.goalkeeperPower / 5;
    snapshot.lineBreakThreat =
        lineBreakThreat / max(1, static_cast<int>(snapshot.xi.size())) + snapshot.attackPower / 4 + snapshot.collectiveForm / 5;
    snapshot.pressingLoad =
        pressingLoad / max(1, static_cast<int>(snapshot.xi.size())) + team.pressingIntensity * 4 + team.tempo * 2;
    snapshot.setPieceThreat = max(setPieceThreat, 35) + team.width * 2;

    if (team.matchInstruction == "Por bandas") {
        snapshot.lineBreakThreat += 5;
        snapshot.chanceCreation += 4;
    } else if (team.matchInstruction == "Juego directo") {
        snapshot.lineBreakThreat += 8;
        snapshot.pressResistance -= 3;
    } else if (team.matchInstruction == "Balon parado") {
        snapshot.setPieceThreat += 10;
        snapshot.chanceCreation += 2;
    } else if (team.matchInstruction == "Bloque bajo") {
        snapshot.defensiveShape += 8;
        snapshot.pressingLoad -= 4;
    } else if (team.matchInstruction == "Pausar juego") {
        snapshot.pressResistance += 5;
        snapshot.pressingLoad -= 5;
    }

    if (team.tactics == "Pressing") {
        snapshot.pressingLoad += 8;
        snapshot.chanceCreation += 3;
        snapshot.defensiveShape -= 2;
    } else if (team.tactics == "Counter") {
        snapshot.lineBreakThreat += 7;
        snapshot.pressResistance += 2;
    } else if (team.tactics == "Defensive") {
        snapshot.defensiveShape += 7;
        snapshot.chanceCreation -= 3;
    } else if (team.tactics == "Offensive") {
        snapshot.chanceCreation += 6;
        snapshot.finishingQuality += 4;
        snapshot.defensiveShape -= 3;
    }

    const int analystBonus = max(0, team.performanceAnalyst - 55) / 4;
    const int goalkeepingBonus = max(0, team.goalkeepingCoach - 55) / 3;
    snapshot.goalkeeperPower += goalkeepingBonus;
    snapshot.pressResistance += analystBonus;
    snapshot.defensiveShape += analystBonus / 2;
    snapshot.tacticalCompatibility += analystBonus / 140.0;
    if (team.trainingFocus == "Ataque") {
        snapshot.chanceCreation += 5;
        snapshot.finishingQuality += 4;
    } else if (team.trainingFocus == "Defensa") {
        snapshot.defensiveShape += 6;
        snapshot.pressResistance += 3;
    } else if (team.trainingFocus == "Resistencia" || team.trainingFocus == "Recuperacion") {
        snapshot.pressingLoad -= 4;
        snapshot.fatigueFactor = min(1.08, snapshot.fatigueFactor + 0.03);
    } else if (team.trainingFocus == "Tactico" || team.trainingFocus == "Preparacion partido") {
        snapshot.pressResistance += 5;
        snapshot.defensiveShape += 3;
        snapshot.tacticalCompatibility += 0.05;
    }

    snapshot.chanceCreation = clampInt(snapshot.chanceCreation, 35, 130);
    snapshot.finishingQuality = clampInt(snapshot.finishingQuality, 38, 130);
    snapshot.pressResistance = clampInt(snapshot.pressResistance, 35, 125);
    snapshot.defensiveShape = clampInt(snapshot.defensiveShape, 35, 130);
    snapshot.lineBreakThreat = clampInt(snapshot.lineBreakThreat, 35, 130);
    snapshot.pressingLoad = clampInt(snapshot.pressingLoad, 28, 125);
    snapshot.setPieceThreat = clampInt(snapshot.setPieceThreat, 30, 125);

    if (team.tactics == "Counter" && opponent.defensiveLine >= 4) snapshot.attackPower += 5;
    if (team.tactics == "Defensive" && opponent.tactics == "Offensive") snapshot.defensePower += 4;
    return snapshot;
}

}  // namespace

namespace match_context {

MatchSetup buildMatchSetup(const Team& home, const Team& away, bool keyMatch, bool neutralVenue) {
    MatchSetup setup;
    const WeatherProfile weather = rollWeatherProfile();
    setup.home = buildSnapshot(home, away, home.getStartingXIIndices(), keyMatch);
    setup.away = buildSnapshot(away, home, away.getStartingXIIndices(), keyMatch);

    setup.home.tacticalAdvantage = tactics_engine::tacticalAdvantage(home,
                                                                     away,
                                                                     setup.home.tacticalProfile,
                                                                     setup.away.tacticalProfile,
                                                                     setup.home.xi);
    setup.away.tacticalAdvantage = tactics_engine::tacticalAdvantage(away,
                                                                     home,
                                                                     setup.away.tacticalProfile,
                                                                     setup.home.tacticalProfile,
                                                                     setup.away.xi);

    const double rawCrowdImpact =
        home.fanBase / 240.0 +
        home.stadiumLevel * 0.01 -
        away.fanBase / 400.0;

    const double crowdImpact =
        neutralVenue ? 0.0
                 : std::clamp(rawCrowdImpact, -0.03, 0.10);

    setup.context.teamStrengthHome = (setup.home.attackPower + setup.home.defensePower + setup.home.midfieldControl) / 3.0;
    setup.context.teamStrengthAway = (setup.away.attackPower + setup.away.defensePower + setup.away.midfieldControl) / 3.0;
    setup.context.attackPowerHome = setup.home.attackPower;
    setup.context.attackPowerAway = setup.away.attackPower;
    setup.context.defensePowerHome = setup.home.defensePower;
    setup.context.defensePowerAway = setup.away.defensePower;
    setup.context.midfieldControlHome = setup.home.midfieldControl;
    setup.context.midfieldControlAway = setup.away.midfieldControl;
    setup.context.moraleFactorHome = setup.home.moraleFactor;
    setup.context.moraleFactorAway = setup.away.moraleFactor;
    setup.context.fatigueFactorHome = setup.home.fatigueFactor;
    setup.context.fatigueFactorAway = setup.away.fatigueFactor;
    setup.context.tacticalAdvantageHome = setup.home.tacticalAdvantage;
    setup.context.tacticalAdvantageAway = setup.away.tacticalAdvantage;
    setup.context.weatherModifier = weather.modifier;
    setup.context.homeAdvantage = neutralVenue ? 1.0 : 1.03 + crowdImpact;
    setup.context.randomnessSeed = randInt(100000, 999999);
    setup.context.weather = weather.label;
    return setup;
}

TeamMatchSnapshot rebuildSnapshot(const Team& team,
                                  const Team& opponent,
                                  const vector<int>& xi,
                                  bool keyMatch) {
    return buildSnapshot(team, opponent, xi, keyMatch);
}

}  // namespace match_context
