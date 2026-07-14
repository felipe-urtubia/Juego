#include "ai/team_ai.h"

#include "engine/team_personality.h"
#include "simulation/match_engine_internal.h"
#include "utils/utils.h"
#include "core/football_constants.h"

#include <algorithm>

using namespace std;

namespace {

int averageAvailableFitness(const Team& team) {
    int total = 0;
    int count = 0;
    for (const auto& player : team.players) {
        if (player.injured || player.matchesSuspended > 0) continue;
        total += player.fitness;
        count++;
    }
    return count > 0 ? total / count : 50;
}

int unavailableCount(const Team& team) {
    int unavailable = 0;
    for (const auto& player : team.players) {
        if (player.injured || player.matchesSuspended > 0) unavailable++;
    }
    return unavailable;
}

bool hasDirectThreat(const Team& team) {
    for (const auto& player : team.players) {
        if (normalizePosition(player.position) != "DEL") continue;
        if (playerHasTrait(player, "Competidor") || match_internal::compactToken(player.role) == "poacher" ||
            match_internal::compactToken(player.role) == "objetivo") {
            return true;
        }
    }
    return false;
}

bool applySetting(int& value, int target, int lo, int hi) {
    int clamped = clampInt(target, lo, hi);
    if (value == clamped) return false;
    value = clamped;
    return true;
}

bool applySetting(string& value, const string& target) {
    if (value == target) return false;
    value = target;
    return true;
}

}  // namespace

namespace team_ai {

void adjustCpuTactics(Team& team, const Team& opponent, const Team* myTeam) {
    if (&team == myTeam) return;

    ensureTeamIdentity(team);
    ensureTeamIdentity(const_cast<Team&>(opponent));
    const TeamPersonalityProfile profile = buildTeamPersonalityProfile(team);

    int diff = team.getAverageSkill() - opponent.getAverageSkill();
    int avgFitness = averageAvailableFitness(team);
    int unavailable = unavailableCount(team);
    bool directThreat = hasDirectThreat(team);
    bool chaseBackSpace = opponent.defensiveLine >= 4 && directThreat;
    bool opponentAggressive = opponent.pressingIntensity >= 4 || opponent.tactics == "Pressing";

    if (avgFitness < 66 || unavailable >= 4) {
        team.rotationPolicy = std::string(Football::Rotation::Rotation);
    } else if (diff >= 6 && avgFitness >= 72) {
        team.rotationPolicy = std::string(Football::Rotation::Starters);
    } else {
        team.rotationPolicy = std::string(Football::Rotation::Balanced);
    }

    if (avgFitness < 60 || team.morale <= 35) {
        team.tactics = "Defensive";
    } else if (profile.pressBias >= 74 && avgFitness >= 68 && unavailable <= 3) {
        team.tactics = "Pressing";
    } else if (profile.blockBias >= 76 && (diff <= 3 || unavailable >= 3)) {
        team.tactics = "Defensive";
    } else if (profile.transitionBias >= 74 && (chaseBackSpace || opponentAggressive)) {
        team.tactics = diff >= 4 ? "Offensive" : "Counter";
    } else if (profile.widthBias >= 74 && diff >= 2 && avgFitness >= 66) {
        team.tactics = "Offensive";
    } else if (diff >= 7 && avgFitness >= 70) {
        team.tactics = "Offensive";
    } else if (opponentAggressive && chaseBackSpace) {
        team.tactics = "Counter";
    } else if (team.morale >= 72 && avgFitness >= 72) {
        team.tactics = "Pressing";
    } else {
        team.tactics = "Balanced";
    }

    if (team.tactics == "Offensive") {
        team.pressingIntensity = 4;
        team.defensiveLine = chaseBackSpace ? 3 : 4;
        team.tempo = 4;
        team.width = 4;
        team.markingStyle = "Zonal";
    } else if (team.tactics == "Defensive") {
        team.pressingIntensity = max(1, avgFitness < 58 ? 1 : 2);
        team.defensiveLine = 2;
        team.tempo = 2;
        team.width = 3;
        team.markingStyle = opponent.width >= 4 ? "Zonal" : "Hombre";
    } else if (team.tactics == "Counter") {
        team.pressingIntensity = 3;
        team.defensiveLine = 2;
        team.tempo = 4;
        team.width = 3;
        team.markingStyle = "Zonal";
    } else if (team.tactics == "Pressing") {
        team.pressingIntensity = 4;
        team.defensiveLine = 4;
        team.tempo = 4;
        team.width = 4;
        team.markingStyle = "Zonal";
    } else {
        team.pressingIntensity = 3;
        team.defensiveLine = 3;
        team.tempo = 3;
        team.width = opponent.markingStyle == "Hombre" ? 4 : 3;
        team.markingStyle = avgFitness < 62 ? "Zonal" : "Hombre";
    }

    if (chaseBackSpace) {
        team.matchInstruction = "Juego directo";
    } else if (team.tactics == "Offensive" && opponent.width <= 3) {
        team.matchInstruction = "Por bandas";
    } else if (team.tactics == "Defensive") {
        team.matchInstruction = "Bloque bajo";
    } else if (team.tactics == "Pressing" && avgFitness >= 68) {
        team.matchInstruction = "Contra-presion";
    } else if (avgFitness < 58) {
        team.matchInstruction = "Pausar juego";
    } else {
        team.matchInstruction = "Equilibrado";
    }

    if (profile.pressBias >= 72 && team.tactics == "Pressing") {
        team.pressingIntensity = max(team.pressingIntensity, 4);
        team.defensiveLine = max(team.defensiveLine, 4);
        if (avgFitness >= 68) team.matchInstruction = "Contra-presion";
    }
    if (profile.transitionBias >= 72 && team.tactics != "Defensive" && chaseBackSpace) {
        team.matchInstruction = "Juego directo";
        team.tempo = max(team.tempo, 4);
    }
    if (profile.widthBias >= 72 && team.tactics != "Defensive" && team.matchInstruction != "Juego directo") {
        team.width = max(team.width, 4);
        if (team.tactics != "Counter") team.matchInstruction = "Por bandas";
    }
    if (profile.blockBias >= 72 && team.tactics == "Defensive") {
        team.defensiveLine = min(team.defensiveLine, 2);
        team.markingStyle = "Zonal";
    }
    if (profile.controlBias >= 72 && team.tactics == "Balanced") {
        team.tempo = clampInt(team.tempo, 2, 3);
        team.width = clampInt(team.width, 2, 4);
        if (team.matchInstruction == "Equilibrado") team.markingStyle = "Zonal";
    }
}

bool applyInMatchCpuAdjustment(Team& team,
                               const Team& opponent,
                               int minute,
                               int goalsFor,
                               int goalsAgainst,
                               vector<string>* events,
                               int availablePlayers,
                               int cautionedPlayers,
                               int opponentAvailablePlayers) {
    bool changed = false;
    int scoreDiff = goalsFor - goalsAgainst;
    int avgFitness = averageAvailableFitness(team);
    int opponentFitness = averageAvailableFitness(opponent);
    const TeamPersonalityProfile profile = buildTeamPersonalityProfile(team);
    string note;

    if (scoreDiff <= -2 && minute >= 55) {
        if (profile.transitionBias >= profile.pressBias && profile.transitionBias >= 72) {
            changed |= applySetting(team.tactics, "Counter");
            changed |= applySetting(team.matchInstruction, "Juego directo");
            changed |= applySetting(team.tempo, team.tempo + 2, 1, 5);
            changed |= applySetting(team.width, 4, 1, 5);
            note = team.name + " acelera un plan vertical para remontar";
        } else {
            changed |= applySetting(team.tactics, profile.pressBias >= 72 ? "Pressing" : "Offensive");
            changed |= applySetting(team.matchInstruction, "Presion final");
            changed |= applySetting(team.pressingIntensity, team.pressingIntensity + 1, 1, 5);
            changed |= applySetting(team.defensiveLine, team.defensiveLine + 1, 1, 5);
            changed |= applySetting(team.tempo, team.tempo + 1, 1, 5);
            changed |= applySetting(team.width, 4, 1, 5);
            note = team.name + " cambia a un plan de urgencia para remontar";
        }
    } else if (scoreDiff == -1 && minute >= 65) {
        if (profile.transitionBias >= 72 && opponent.defensiveLine >= 4) {
            changed |= applySetting(team.tactics, "Counter");
            changed |= applySetting(team.matchInstruction, "Juego directo");
            changed |= applySetting(team.tempo, team.tempo + 1, 1, 5);
            changed |= applySetting(team.width, 4, 1, 5);
            note = team.name + " busca la espalda de la ultima linea rival";
        } else if (profile.widthBias >= 72) {
            changed |= applySetting(team.tactics, "Offensive");
            changed |= applySetting(team.matchInstruction, "Por bandas");
            changed |= applySetting(team.width, 5, 1, 5);
            changed |= applySetting(team.tempo, team.tempo + 1, 1, 5);
            note = team.name + " carga amplitud y centros para romper el partido";
        } else {
            changed |= applySetting(team.tactics, opponent.tactics == "Defensive" ? "Offensive" : "Pressing");
            changed |= applySetting(team.matchInstruction, opponent.defensiveLine >= 4 ? "Juego directo" : "Por bandas");
            changed |= applySetting(team.pressingIntensity, team.pressingIntensity + 1, 1, 5);
            changed |= applySetting(team.tempo, team.tempo + 1, 1, 5);
            changed |= applySetting(team.defensiveLine, team.defensiveLine + 1, 1, 5);
            note = team.name + " adelanta lineas y asume mas riesgo";
        }
    } else if (scoreDiff >= 1 && minute >= (profile.blockBias >= 72 ? 68 : 75)) {
        changed |= applySetting(team.tactics, "Defensive");
        changed |= applySetting(team.matchInstruction, avgFitness < 60 ? "Pausar juego" : "Bloque bajo");
        changed |= applySetting(team.pressingIntensity, team.pressingIntensity - 1, 1, 5);
        changed |= applySetting(team.tempo, team.tempo - 1, 1, 5);
        changed |= applySetting(team.defensiveLine, team.defensiveLine - 1, 1, 5);
        note = profile.blockBias >= 72
                   ? team.name + " protege la ventaja antes de tiempo con su sello conservador"
                   : team.name + " protege la ventaja con un bloque mas conservador";
    } else if (scoreDiff == 0 && minute >= 70 && team.getAverageSkill() >= opponent.getAverageSkill() + 4) {
        changed |= applySetting(team.matchInstruction, profile.transitionBias >= 72 ? "Juego directo" : "Por bandas");
        changed |= applySetting(team.tempo, team.tempo + 1, 1, 5);
        changed |= applySetting(team.width, profile.widthBias >= 72 ? 5 : 4, 1, 5);
        if (profile.pressBias >= 72 && avgFitness >= 62) {
            changed |= applySetting(team.tactics, "Pressing");
        }
        note = team.name + " busca romper el empate con un plan alineado a su identidad";
    }
    if (minute >= 65 &&
    scoreDiff == 0 &&
    opponentFitness <= 55 &&
    avgFitness >= opponentFitness + 8) {

    bool pressChanged = false;

    pressChanged |= applySetting(team.tactics, "Pressing");
    pressChanged |= applySetting(team.matchInstruction, "Contra-presion");
    pressChanged |= applySetting(team.pressingIntensity, 5, 1, 5);
    pressChanged |= applySetting(team.tempo, 5, 1, 5);

    if (pressChanged) {
        changed = true;
        note = team.name +
               " detecta el desgaste rival y aumenta la intensidad";
    }
}

    if (scoreDiff == 0 && minute >= 60 && opponentAvailablePlayers <= 10) {
        bool redAdvantageChanged = false;
        redAdvantageChanged |= applySetting(team.tactics, "Offensive");
        redAdvantageChanged |= applySetting(team.matchInstruction, team.width >= 4 ? "Por bandas" : "Juego directo");
        redAdvantageChanged |= applySetting(team.tempo, team.tempo + 1, 1, 5);
        redAdvantageChanged |= applySetting(team.defensiveLine, team.defensiveLine + 1, 1, 5);
        if (redAdvantageChanged) {
            changed = true;
            note = note.empty() ? team.name + " acelera al detectar superioridad numerica"
                                : note + "; " + team.name + " explota el hombre de mas";
        }
    }

    if (avgFitness < 54 && minute >= 65) {
        bool fatigueChanged = false;
        fatigueChanged |= applySetting(team.pressingIntensity, team.pressingIntensity - 1, 1, 5);
        fatigueChanged |= applySetting(team.tempo, team.tempo - 1, 1, 5);
        fatigueChanged |= applySetting(team.width, max(2, team.width - 1), 1, 5);
        if (fatigueChanged) {
            changed = true;
            if (note.empty()) note = team.name + " baja revoluciones por desgaste";
        }
    }

    if (avgFitness < 48 && minute >= 72) {
        bool survivalChanged = false;
        survivalChanged |= applySetting(team.tactics, scoreDiff >= 0 ? "Defensive" : "Counter");
        survivalChanged |= applySetting(team.matchInstruction, scoreDiff >= 0 ? "Pausar juego" : "Juego directo");
        survivalChanged |= applySetting(team.defensiveLine, team.defensiveLine - 1, 1, 5);
        survivalChanged |= applySetting(team.pressingIntensity, team.pressingIntensity - 1, 1, 5);
        if (survivalChanged) {
            changed = true;
            note = note.empty() ? team.name + " protege energia para el cierre"
                                : note + "; " + team.name + " administra el desgaste extremo";
        }
    }

    if (availablePlayers <= 10) {
        bool redCardChanged = false;
        redCardChanged |= applySetting(team.defensiveLine, team.defensiveLine - 1, 1, 5);
        redCardChanged |= applySetting(team.width, team.width - 1, 1, 5);
        if (scoreDiff >= 0) {
            redCardChanged |= applySetting(team.tactics, "Defensive");
            redCardChanged |= applySetting(team.matchInstruction, "Bloque bajo");
            redCardChanged |= applySetting(team.pressingIntensity, team.pressingIntensity - 1, 1, 5);
            redCardChanged |= applySetting(team.tempo, team.tempo - 1, 1, 5);
        } else {
            redCardChanged |= applySetting(team.tactics, "Counter");
            redCardChanged |= applySetting(team.matchInstruction, "Juego directo");
        }
        if (redCardChanged) {
            changed = true;
            note = note.empty() ? team.name + " recompone su estructura tras la expulsion"
                                : note + "; " + team.name + " protege zonas tras quedarse con diez";
        }
    }

    if (cautionedPlayers >= 3 && minute >= 45) {
        bool cautionChanged = false;
        cautionChanged |= applySetting(team.pressingIntensity, team.pressingIntensity - 1, 1, 5);
        cautionChanged |= applySetting(team.markingStyle, "Zonal");
        if (cautionChanged) {
            changed = true;
            note = note.empty() ? team.name + " baja agresividad para proteger a los amonestados"
                                : note + "; " + team.name + " reduce riesgos disciplinarios";
        }
    }

    if (minute >= 80 && scoreDiff <= -1 && avgFitness >= 58) {
        bool finalPush = false;
        finalPush |= applySetting(team.tactics, profile.transitionBias >= 74 ? "Counter" : "Offensive");
        finalPush |= applySetting(team.matchInstruction, profile.transitionBias >= 74 ? "Juego directo" : "Presion final");
        finalPush |= applySetting(team.pressingIntensity, 5, 1, 5);
        finalPush |= applySetting(team.tempo, 5, 1, 5);
        finalPush |= applySetting(team.width, 5, 1, 5);
        if (finalPush) {
            changed = true;
            note = note.empty() ? team.name + " va con todo en el tramo final"
                                : note + "; " + team.name + " activa un asedio final";
        }
    }

    if (changed && events && !note.empty()) {
        events->push_back(to_string(minute) + "' Ajuste tactico: " + note);
    }
    return changed;
}

}  // namespace team_ai
