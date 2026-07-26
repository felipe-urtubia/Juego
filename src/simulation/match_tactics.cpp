#include "simulation/match_engine_internal.h"

#include "simulation/player_condition.h"
#include "utils.h"

#include <algorithm>
#include <cctype>

using namespace std;

namespace match_internal {

string compactToken(string value) {
    value = toLower(trim(value));
    value.erase(remove_if(value.begin(), value.end(), [](unsigned char ch) {
                    return std::isspace(ch) || ch == '-' || ch == '_';
                }),
                value.end());
    return value;
}

void applyRoleModifier(const Player& p, int& attack, int& defense) {
    string role = compactToken(p.role);
    string duty = compactToken(p.roleDuty);
    if (role == "stopper") {
        defense += 5;
        attack -= 1;
    } else if (role == "sweeperkeeper") {
        attack += 2;
        defense += 3;
    } else if (role == "ballplaying") {
        attack += 2;
        defense += 3;
    } else if (role == "carrilero") {
        attack += 4;
        defense -= 1;
    } else if (role == "enganche") {
        attack += 5;
        defense -= 3;
    } else if (role == "boxtobox") {
        attack += 2;
        defense += 2;
    } else if (role == "organizador") {
        attack += 4;
        defense += 1;
    } else if (role == "pivote") {
        defense += 5;
        attack -= 2;
    } else if (role == "interior") {
        attack += 4;
        defense += 1;
    } else if (role == "poacher") {
        attack += 5;
        defense -= 1;
    } else if (role == "falso9") {
        attack += 3;
        defense += 2;
    } else if (role == "pressing") {
        attack += 2;
        defense += 1;
    } else if (role == "objetivo") {
        attack += 4;
        defense += 1;
    }
    if (duty == "ataque") {
        attack += 3;
        defense -= 2;
    } else if (duty == "apoyo") {
        attack += 1;
        defense += 1;
    } else if (duty == "defensa") {
        defense += 3;
        attack -= 2;
    }
    attack = clampInt(attack, 1, 120);
    defense = clampInt(defense, 1, 120);
}

void applyIndividualInstructionModifier(const Player& p, const Team& team, int& attack, int& defense) {
    const string instruction = compactToken(p.individualInstruction);
    if (instruction == "arriesgarpase") {
        attack += 3;
        defense -= 1;
    } else if (instruction == "abrircampo") {
        attack += 2 + (team.width >= 4 ? 1 : 0);
    } else if (instruction == "cerrarpordentro") {
        defense += 3;
        attack -= 1;
    } else if (instruction == "atacarespalda") {
        attack += 4 + (team.tempo >= 4 || team.matchInstruction == "Juego directo" ? 1 : 0);
        defense -= 2;
    } else if (instruction == "conservarposicion") {
        defense += 2;
        attack -= 1;
    } else if (instruction == "marcarfuerte") {
        defense += 3;
        attack -= 1;
    } else if (instruction == "descansomedico") {
        attack -= 3;
        defense -= 3;
    }
    attack = clampInt(attack, 1, 125);
    defense = clampInt(defense, 1, 125);
}

void applyTraitModifier(const Player& p, int& attack, int& defense) {
    if (playerHasTrait(p, "Lider")) defense += 1;
    if (playerHasTrait(p, "Competidor")) {
        attack += 1;
        defense += 1;
    }
    if (playerHasTrait(p, "Pase riesgoso")) {
        attack += 3;
        defense -= 1;
    }
    if (playerHasTrait(p, "Llega al area")) attack += 2;
    if (playerHasTrait(p, "Presiona")) {
        attack += 1;
        defense += 2;
    }
    if (playerHasTrait(p, "Muralla")) defense += 3;
    if (playerHasTrait(p, "Cita grande")) {
        attack += 1;
        defense += 1;
    }
    if (playerHasTrait(p, "Versatil")) defense += 1;
    if (playerHasTrait(p, "Caliente")) {
        attack += 1;
        defense -= 1;
    }
    attack = clampInt(attack, 1, 125);
    defense = clampInt(defense, 1, 125);
}

void applyPlayerStateModifier(const Player& p, const Team& team, int& attack, int& defense) {
    attack = attack * clampInt(92 + p.currentForm / 4 + p.consistency / 8, 72, 128) / 100;
    defense = defense * clampInt(92 + p.currentForm / 5 + p.tacticalDiscipline / 5, 74, 130) / 100;
    attack += p.moraleMomentum / 4;
    defense += p.moraleMomentum / 5;
    attack -= p.fatigueLoad / 12;
    defense -= p.fatigueLoad / 14;
    attack += max(0, p.adaptation - 70) / 12;
    defense += max(0, p.discipline - 68) / 12;
    if (p.discipline <= 38) defense -= 1;
    if (p.injuryProneness >= 72 && p.fitness <= 70) {
        attack -= 1;
        defense -= 1;
    }
    if (p.preferredFoot == "Ambos") {
        if (normalizePosition(p.position) == "MED" || normalizePosition(p.position) == "DEL") attack += 2;
        else defense += 1;
    } else if (p.preferredFoot == "Izquierdo" && team.width >= 4) {
        attack += 1;
    }
    if (p.currentForm <= 35) {
        attack -= 2;
        defense -= 2;
    } else if (p.currentForm >= 78) {
        attack += 2;
        defense += 1;
    }
    if (p.tacticalDiscipline >= 75 && (team.tactics == "Defensive" || team.matchInstruction == "Bloque bajo")) {
        defense += 2;
    }
    if (!p.promisedPosition.empty() && normalizePosition(p.promisedPosition) == normalizePosition(p.position)) {
        attack += 1;
        defense += 1;
    }
    if (compactToken(p.roleDuty) == "ataque" && team.matchInstruction == "Presion final") attack += 1;
    if (compactToken(p.roleDuty) == "defensa" && (team.matchInstruction == "Bloque bajo" || team.tactics == "Defensive")) defense += 2;
    if (p.socialGroup == "Lideres" && team.morale >= 60) defense += 1;
    if (p.socialGroup == "Juveniles" && p.currentForm >= 65) attack += 1;
    attack = clampInt(attack, 1, 130);
    defense = clampInt(defense, 1, 130);
}

void applyMatchInstruction(const Team& team, int& attack, int& defense) {
    if (team.matchInstruction == "Laterales altos") {
        attack += 6;
        defense -= 3;
    } else if (team.matchInstruction == "Bloque bajo") {
        attack -= 4;
        defense += 8;
    } else if (team.matchInstruction == "Balon parado") {
        attack += 4;
        defense += 2;
    } else if (team.matchInstruction == "Presion final") {
        attack += 8;
        defense -= 2;
    } else if (team.matchInstruction == "Por bandas") {
        attack += 6;
        defense -= 1;
    } else if (team.matchInstruction == "Juego directo") {
        attack += 5;
        defense += 1;
    } else if (team.matchInstruction == "Contra-presion") {
        attack += 4;
        defense += 4;
    } else if (team.matchInstruction == "Pausar juego") {
        attack -= 3;
        defense += 6;
    }
}

void applyFormationBias(const Team& team, const vector<int>& xi, int& attack, int& defense) {
    int defenders = 0;
    int midfielders = 0;
    int forwards = 0;
    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        string pos = normalizePosition(team.players[idx].position);
        if (pos == "DEF") defenders++;
        else if (pos == "MED") midfielders++;
        else if (pos == "DEL") forwards++;
    }
    if (defenders >= 5) defense += 8;
    if (midfielders >= 4) {
        attack += 4;
        defense += 3;
    }
    if (forwards >= 3) {
        attack += 8;
        defense -= 4;
    }
    if (forwards <= 1) attack -= 5;
    if (team.formation.find("4-3-3") != string::npos || team.formation.find("3-4-3") != string::npos) attack += 3;
    if (team.formation.find("5-") == 0 || team.formation.find("4-5-1") != string::npos) defense += 4;
}

void applyStyleMatchup(const Team& home,
                       const Team& away,
                       int& homeAttack,
                       int& homeDefense,
                       int& awayAttack,
                       int& awayDefense) {
    if (home.tactics == "Pressing" && away.tactics == "Counter") {
        awayAttack += 7;
        homeDefense -= 4;
    }
    if (away.tactics == "Pressing" && home.tactics == "Counter") {
        homeAttack += 7;
        awayDefense -= 4;
    }
    if (home.tactics == "Offensive" && away.tactics == "Defensive") {
        awayDefense += 6;
        homeAttack -= 2;
    }
    if (away.tactics == "Offensive" && home.tactics == "Defensive") {
        homeDefense += 6;
        awayAttack -= 2;
    }
    if (home.markingStyle == "Hombre" && away.width >= 4) {
        awayAttack += 4;
        homeDefense -= 2;
    }
    if (away.markingStyle == "Hombre" && home.width >= 4) {
        homeAttack += 4;
        awayDefense -= 2;
    }
}

int pressingRecoveryBonus(const Team& team, const vector<int>& xi) {
    if (xi.empty()) return 0;
    int discipline = 0;
    int pressers = 0;
    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[idx];
        discipline += player.tacticalDiscipline;
        if (playerHasTrait(player, "Presiona") || compactToken(player.role) == "pressing") pressers++;
    }
    int avgDiscipline = discipline / static_cast<int>(xi.size());
    int bonus = max(0, team.pressingIntensity - 2) * 3 + max(0, avgDiscipline - 60) / 10 + pressers;
    if (team.tactics == "Pressing") bonus += 3;
    if (team.matchInstruction == "Contra-presion") bonus += 3;
    return clampInt(bonus, 0, 16);
}

int directPlayBonus(const Team& attacking, const Team& defending, const vector<int>& xi) {
    bool direct = attacking.matchInstruction == "Juego directo" || attacking.tactics == "Counter" || attacking.tempo >= 4;
    if (!direct || defending.defensiveLine <= 3) return 0;
    int runners = 0;
    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(attacking.players.size())) continue;
        const Player& player = attacking.players[idx];
        if (normalizePosition(player.position) == "DEL" || playerHasTrait(player, "Competidor") ||
            compactToken(player.role) == "poacher" || compactToken(player.role) == "objetivo") {
            runners++;
        }
    }
    int bonus = (defending.defensiveLine - 3) * 4 + min(4, runners);
    return clampInt(bonus, 0, 15);
}

int crowdSupportBonus(const Team& home, const Team& away, bool neutralVenue) {
    if (neutralVenue) return 0;
    int bonus = home.fanBase / 8 + home.stadiumLevel * 2 + teamPrestigeScore(home) / 18;
    bonus -= away.fanBase / 12;
    if (areRivalClubs(home, away)) bonus += 3;
    return clampInt(bonus, 0, 12);
}

int clutchModifier(const Team& team, const vector<int>& xi, bool keyMatch) {
    if (xi.empty()) return 0;
    int nerve = team.morale;
    int leaders = 0;
    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[idx];
        nerve += player.bigMatches / 2;
        if (!team.captain.empty() && player.name == team.captain) nerve += player.leadership / 2;
        if (playerHasTrait(player, "Cita grande")) nerve += 6;
        if (playerHasTrait(player, "Caliente")) nerve -= 3;
        if (playerHasTrait(player, "Lider")) leaders++;
    }
    nerve += leaders * 2;
    if (keyMatch) nerve += 5;
    return clampInt((nerve / static_cast<int>(xi.size()) - 45) / 3, -4, 12);
}

int bestBenchReplacement(const Team& team,
                         const vector<int>& activeXI,
                         const string& targetPos) {
    return bestBenchReplacement(team, activeXI, targetPos, 0, 0, 0, 11);
}

int bestBenchReplacement(const Team& team,
                         const vector<int>& activeXI,
                         const string& targetPos,
                         int minute,
                         int goalsFor,
                         int goalsAgainst,
                         int opponentAvailablePlayers) {
    const vector<int> bench = team.getBenchIndices(7);
    const int scoreDiff = goalsFor - goalsAgainst;
    const bool losing = scoreDiff < 0;
    const bool winning = scoreDiff > 0;
    const bool lateMatch = minute >= 70;
    const bool urgentAttack = losing && minute >= 65;
    const bool protectLead = winning && minute >= 70;
    const bool numericalAdvantage =
        opponentAvailablePlayers < 11 && scoreDiff <= 0;

    int bestIndex = -1;
    int bestScore = -100000;

    for (int idx : bench) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        if (find(activeXI.begin(), activeXI.end(), idx) != activeXI.end()) continue;

        const Player& player = team.players[static_cast<size_t>(idx)];
        if (player.injured || player.matchesSuspended > 0) continue;

        const string playerPos = normalizePosition(player.position);
        const string normalizedTarget = normalizePosition(targetPos);
        const string role = compactToken(player.role);
        const string duty = compactToken(player.roleDuty);

        int score = player.skill * 4;
        score += player.currentForm * 2;
        score += player.consistency;
        score += player.fitness * 2;
        score += player.attack + player.defense;
        score += positionFitScore(player, targetPos) * 4;
        score += player_condition::readinessScore(player, team) / 2;

        if (playerPos == normalizedTarget) score += 18;
        if (playerHasTrait(player, "Versatil")) score += 8;
        if (playerHasTrait(player, "Competidor")) score += 5;

        if (losing) {
            score += player.attack * (urgentAttack ? 3 : 2);

            if (playerPos == "DEL") score += urgentAttack ? 24 : 12;
            if (playerPos == "MED") score += urgentAttack ? 10 : 5;
            if (duty == "ataque") score += 14;

            if (role == "poacher" ||
                role == "enganche" ||
                role == "interior" ||
                role == "objetivo" ||
                role == "falso9") {
                score += 12;
            }

            if (playerHasTrait(player, "Llega al area")) score += 10;
            if (playerHasTrait(player, "Pase riesgoso")) score += 7;
        }

        if (winning) {
            score += player.defense * (protectLead ? 3 : 2);
            score += player.tacticalDiscipline * 2;
            score += player.stamina;

            if (playerPos == "DEF") score += protectLead ? 20 : 8;
            if (playerPos == "MED") score += protectLead ? 10 : 5;
            if (duty == "defensa") score += 14;

            if (role == "stopper" ||
                role == "pivote" ||
                role == "ballplaying") {
                score += 12;
            }

            if (playerHasTrait(player, "Muralla")) score += 10;
            if (playerHasTrait(player, "Lider")) score += 6;
        }

        if (scoreDiff == 0) {
            score += (player.attack + player.defense) * 2;

            if (lateMatch) {
                score += player.attack;
                if (duty == "ataque") score += 6;
                if (playerHasTrait(player, "Competidor")) score += 4;
            }
        }

        if (numericalAdvantage) {
            score += player.attack * 2;
            if (playerPos == "DEL" || playerPos == "MED") score += 10;
            if (duty == "ataque") score += 8;
        }

        if (lateMatch) {
            score += player.fitness * 2;
            score += player.stamina;
        }

        const int positionalFit = positionFitScore(player, targetPos);
        if (positionalFit < 35 && !playerHasTrait(player, "Versatil")) {
            score -= urgentAttack ? 25 : 45;
        }

        if (score > bestScore) {
            bestScore = score;
            bestIndex = idx;
        }
    }

    return bestIndex;
}

}  // namespace match_internal
