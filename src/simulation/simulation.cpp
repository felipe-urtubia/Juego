#include "simulation.h"

#include "io/io.h"
#include "simulation/match_engine.h"
#include "simulation/match_engine_internal.h"
#include "simulation/match_postprocess.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <iostream>

using namespace std;

TeamStrength computeStrength(Team& team) {
    TeamStrength strength;
    strength.xi = team.getStartingXIIndices();
    if (strength.xi.empty()) return strength;

    int totalSkill = 0;
    int totalStamina = 0;
    int totalChemistry = 0;
    int totalProfessionalism = 0;
    int totalForm = 0;
    int totalDiscipline = 0;
    int validPlayers = 0;

    for (int idx : strength.xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(idx)];
        int attack = player.attack;
        int defense = player.defense;
        match_internal::applyRoleModifier(player, attack, defense);
        match_internal::applyTraitModifier(player, attack, defense);
        match_internal::applyPlayerStateModifier(player, team, attack, defense);
        strength.attack += attack;
        strength.defense += defense;
        totalSkill += player.skill;
        totalStamina += clampInt(player.fitness, 0, 100);
        totalChemistry += player.chemistry;
        totalProfessionalism += player.professionalism;
        totalForm += player.currentForm;
        totalDiscipline += player.tacticalDiscipline;
        validPlayers++;
    }

    if (validPlayers <= 0) return strength;
    match_internal::applyFormationBias(team, strength.xi, strength.attack, strength.defense);
    strength.avgSkill = totalSkill / validPlayers;
    strength.avgStamina = totalStamina / validPlayers;
    strength.attack += totalChemistry / validPlayers / 4;
    strength.defense += totalProfessionalism / validPlayers / 5;
    strength.attack += totalForm / validPlayers / 5;
    strength.defense += totalDiscipline / validPlayers / 5;
    return strength;
}

bool hasInjuredInXI(const Team& team, const vector<int>& xi) {
    for (int idx : xi) {
        if (idx >= 0 && idx < static_cast<int>(team.players.size()) &&
            team.players[static_cast<size_t>(idx)].injured) {
            return true;
        }
    }
    return false;
}

void applyTactics(const Team& team, int& attack, int& defense) {
    const string& tactics = team.tactics;
    if (tactics == "Defensive") {
        attack = attack * 89 / 100;
        defense = defense * 112 / 100;
    } else if (tactics == "Offensive") {
        attack = attack * 112 / 100;
        defense = defense * 89 / 100;
    } else if (tactics == "Pressing") {
        attack = attack * 106 / 100;
        defense = defense * 104 / 100;
    } else if (tactics == "Counter") {
        attack = attack * 108 / 100;
        defense = defense * 97 / 100;
    }

    attack += (team.tempo - 3) * 7;
    defense -= max(0, team.tempo - 3) * 3;
    defense += (team.defensiveLine - 3) * 4;
    attack += (team.width - 3) * 5;
    if (team.width <= 2) defense += 3;
    defense -= max(0, team.width - 3) * 3;
    attack += max(0, team.pressingIntensity - 3) * 4;
    defense += min(0, team.defensiveLine - 3) * 2;
    if (team.pressingIntensity >= 4 && team.defensiveLine >= 4) {
        attack += 4;
        defense -= 3;
    }
    if (team.defensiveLine <= 2 && team.tactics == "Counter") {
        defense += 4;
        attack += 2;
    }
    if (team.markingStyle == "Hombre") {
        defense += 4;
        attack -= 2;
        if (team.pressingIntensity >= 4) defense -= 1;
    }
    match_internal::applyMatchInstruction(team, attack, defense);
}

double calcLambda(int attack, int defense) {
    if (attack <= 0 || defense <= 0) return 0.2;
    double ratio = static_cast<double>(attack) / static_cast<double>(defense);
    double lambda = 1.2 * ratio;
    if (lambda < 0.2) lambda = 0.2;
    if (lambda > 3.5) lambda = 3.5;
    return lambda;
}

int samplePoisson(double lambda) {
    const double threshold = exp(-lambda);
    int k = 0;
    double probability = 1.0;
    do {
        k++;
        probability *= rand01();
    } while (probability > threshold);
    return k - 1;
}

void assignGoalsAndAssists(Team& team,
                           int goals,
                           const vector<int>& xi,
                           const string& teamName,
                           vector<string>* events) {
    match_internal::assignGoalsAndAssists(team, goals, xi, teamName, events);
}

MatchResult simulateMatch(Team& home, Team& away, bool keyMatch, bool neutralVenue) {
    ensureMinimumSquad(home, 11);
    ensureMinimumSquad(away, 11);
    ensureTeamIdentity(home);
    ensureTeamIdentity(away);

    const vector<int> homeStartXI = home.getStartingXIIndices();
    const vector<int> awayStartXI = away.getStartingXIIndices();
    for (int idx : homeStartXI) {
        if (idx >= 0 && idx < static_cast<int>(home.players.size())) {
            home.players[static_cast<size_t>(idx)].startsThisSeason++;
        }
    }
    for (int idx : awayStartXI) {
        if (idx >= 0 && idx < static_cast<int>(away.players.size())) {
            away.players[static_cast<size_t>(idx)].startsThisSeason++;
        }
    }

    const match_engine::MatchSimulationData simulation = match_engine::simulate(home, away, keyMatch, neutralVenue);
    match_postprocess::applySimulationOutcome(home, away, simulation, homeStartXI, awayStartXI, keyMatch);
    return simulation.result;
}

MatchResult playMatch(Team& home, Team& away, bool verbose, bool keyMatch, bool neutralVenue) {
    MatchResult result = simulateMatch(home, away, keyMatch, neutralVenue);
    if (!verbose) return result;

    cout << endl;
    for (const auto& warning : result.warnings) {
        cout << "[AVISO] " << warning << endl;
    }
    for (const auto& line : result.reportLines) {
        cout << line << endl;
    }
    if (!result.events.empty()) {
        cout << "\nEventos:" << endl;
        for (const auto& event : result.events) {
            cout << "- " << event << endl;
        }
    }
    return result;
}

// Overload with Career context for rival AI integration
MatchResult simulateMatch(Career* career, Team& home, Team& away, bool keyMatch, bool neutralVenue) {
    ensureMinimumSquad(home, 11);
    ensureMinimumSquad(away, 11);
    ensureTeamIdentity(home);
    ensureTeamIdentity(away);

    const vector<int> homeStartXI = home.getStartingXIIndices();
    const vector<int> awayStartXI = away.getStartingXIIndices();
    for (int idx : homeStartXI) {
        if (idx >= 0 && idx < static_cast<int>(home.players.size())) {
            home.players[static_cast<size_t>(idx)].startsThisSeason++;
        }
    }
    for (int idx : awayStartXI) {
        if (idx >= 0 && idx < static_cast<int>(away.players.size())) {
            away.players[static_cast<size_t>(idx)].startsThisSeason++;
        }
    }

    const match_engine::MatchSimulationData simulation = match_engine::simulate(home, away, career, keyMatch, neutralVenue);
    match_postprocess::applySimulationOutcome(home, away, simulation, homeStartXI, awayStartXI, keyMatch);
    return simulation.result;
}

// Overload with Career context for rival AI integration
MatchResult playMatch(Career* career, Team& home, Team& away, bool verbose, bool keyMatch, bool neutralVenue) {
    MatchResult result = simulateMatch(career, home, away, keyMatch, neutralVenue);
    if (!verbose) return result;

    cout << endl;
    for (const auto& warning : result.warnings) {
        cout << "[AVISO] " << warning << endl;
    }
    for (const auto& line : result.reportLines) {
        cout << line << endl;
    }
    if (!result.events.empty()) {
        cout << "\nEventos:" << endl;
        for (const auto& event : result.events) {
            cout << "- " << event << endl;
        }
    }
    return result;
}
