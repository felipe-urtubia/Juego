#include "ui/team_ui.h"

#include "career/app_services.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace std;
void retirePlayer(Team& team) {
    cout << "\n=== Retiro de Jugadores ===" << endl;
    vector<int> eligibleIndices;
    cout << "Jugadores elegibles para retiro (35-45 anos):" << endl;
    for (size_t i = 0; i < team.players.size(); ++i) {
        if (team.players[i].age >= 35 && team.players[i].age <= 45) {
            eligibleIndices.push_back(static_cast<int>(i));
            cout << eligibleIndices.size() << ". " << team.players[i].name << " (Edad: " << team.players[i].age << ")" << endl;
        }
    }
    if (eligibleIndices.empty()) {
        cout << "No hay jugadores elegibles para retiro." << endl;
        return;
    }
    int choice = readInt("Selecciona un jugador para retirar (0 para cancelar): ", 0, static_cast<int>(eligibleIndices.size()));
    if (choice >= 1 && choice <= static_cast<int>(eligibleIndices.size())) {
        int index = eligibleIndices[choice - 1];
        cout << team.players[index].name << " se ha retirado." << endl;
        team.players.erase(team.players.begin() + index);
    }
}

void viewTeam(Team& team) {
    cout << "\nEquipo: " << team.name << endl;
    cout << "Tacticas: " << team.tactics << ", Formacion: " << team.formation
         << ", Plan: " << team.trainingFocus << ", Instruccion: " << team.matchInstruction
         << ", Presupuesto: $" << team.budget << endl;
    cout << "Presion " << team.pressingIntensity << " | Linea " << team.defensiveLine
         << " | Tempo " << team.tempo << " | Amplitud " << team.width
         << " | Marcaje " << team.markingStyle << " | Rotacion " << team.rotationPolicy << endl;
    cout << "Capitan: " << (team.captain.empty() ? "No definido" : team.captain)
         << " | Penales: " << (team.penaltyTaker.empty() ? "No definido" : team.penaltyTaker)
         << " | Tiros libres: " << (team.freeKickTaker.empty() ? "No definido" : team.freeKickTaker)
         << " | Corners: " << (team.cornerTaker.empty() ? "No definido" : team.cornerTaker) << endl;
    cout << "Staff A/F/S/J/M: " << team.assistantCoach << "/" << team.fitnessCoach << "/" << team.scoutingChief
         << "/" << team.youthCoach << "/" << team.medicalTeam
         << " | Region juvenil: " << team.youthRegion << endl;
    cout << "Moral: " << team.morale << " | Temporada: " << team.wins << "G-" << team.draws << "E-" << team.losses << "P"
         << " | GF " << team.goalsFor << " / GA " << team.goalsAgainst << " | Pts " << team.points << endl;
    if (team.players.empty()) {
        cout << "No hay jugadores en el equipo." << endl;
        return;
    }
    int injuredCount = 0;
    int suspendedCount = 0;
    int lowFitCount = 0;
    int totalFit = 0;
    for (const auto& p : team.players) {
        if (p.injured) injuredCount++;
        if (p.matchesSuspended > 0) suspendedCount++;
        if (p.fitness < 60) lowFitCount++;
        totalFit += p.fitness;
    }
    int avgFit = team.players.empty() ? 0 : totalFit / static_cast<int>(team.players.size());
    if (injuredCount >= 3 || suspendedCount >= 2 || avgFit < 65) {
        cout << "[AVISO] Plantel con baja condicion: " << injuredCount << " lesionados, "
             << suspendedCount << " suspendidos, "
             << lowFitCount << " con condicion <60. Promedio: " << avgFit << endl;
    }

    auto xi = team.getStartingXIIndices();
    vector<bool> isXI(team.players.size(), false);
    for (int idx : xi) {
        if (idx >= 0 && idx < static_cast<int>(isXI.size())) isXI[idx] = true;
    }
    cout << "\nXI Titular:" << endl;
    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        const auto& p = team.players[idx];
        cout << "- " << p.name << " (" << p.position << ")"
             << (p.injured ? " [LES]" : "") << " - Habilidad: " << p.skill
             << (p.matchesSuspended > 0 ? " [SUSP]" : "") << ", Condicion: " << p.fitness << endl;
    }

    auto bench = team.getBenchIndices();
    if (!bench.empty()) {
        cout << "\nBanca:" << endl;
        for (int idx : bench) {
            if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
            const auto& p = team.players[idx];
            cout << "- " << p.name << " (" << p.position << ")"
                 << (p.injured ? " [LES]" : "")
                 << (p.matchesSuspended > 0 ? " [SUSP]" : "")
                 << " Hab " << p.skill << " Cond " << p.fitness << endl;
        }
    }

    cout << "\nPlantel (ordenado por posicion):" << endl;
    vector<string> order = {"ARQ", "DEF", "MED", "DEL", "N/A"};
    for (const auto& pos : order) {
        vector<int> idxs;
        for (size_t i = 0; i < team.players.size(); ++i) {
            string norm = normalizePosition(team.players[i].position);
            if (norm == pos || (pos == "N/A" && norm == "N/A")) idxs.push_back(static_cast<int>(i));
        }
        if (idxs.empty()) continue;
        sort(idxs.begin(), idxs.end(), [&](int a, int b) {
            if (team.players[a].skill != team.players[b].skill) return team.players[a].skill > team.players[b].skill;
            return team.players[a].fitness > team.players[b].fitness;
        });
        cout << pos << ":" << endl;
        for (int i : idxs) {
            const auto& p = team.players[i];
            cout << i + 1 << ". " << p.name << (isXI[i] ? " [XI]" : "")
                 << (p.injured ? " [LES]" : "") << " - Atq " << p.attack
                 << (p.matchesSuspended > 0 ? (string(" [SUSP ") + to_string(p.matchesSuspended) + "]") : "")
                 << ", Def " << p.defense << ", Res " << p.stamina
                 << ", Cond " << p.fitness << ", Hab " << p.skill << ", Pot " << p.potential
                 << ", Rol " << p.role << ", Edad " << p.age << ", Valor $" << p.value
                 << ", Salario $" << p.wage << ", Clausula $" << p.releaseClause
                 << ", Contrato " << p.contractWeeks << " sem"
                 << ", Pie " << p.preferredFoot
                 << ", Sec " << (p.secondaryPositions.empty() ? string("-") : joinStringValues(p.secondaryPositions, "/"))
                 << ", Forma " << playerFormLabel(p) << " (" << p.currentForm << ")"
                 << ", Fiabilidad " << playerReliabilityLabel(p)
                 << ", Partidos grandes " << p.bigMatches
                 << ", TA " << p.seasonYellowCards << ", TR " << p.seasonRedCards
                 << ", Fel " << p.happiness << ", Quim " << p.chemistry
                 << ", Lider " << p.leadership << ", Profesionalismo " << p.professionalism
                 << ", Disciplina tactica " << p.tacticalDiscipline
                 << ", Disciplina " << p.discipline
                 << ", Adaptacion " << p.adaptation
                 << ", Prop lesion " << p.injuryProneness
                 << ", Personalidad " << (p.personality.empty() ? string("Equilibrado") : p.personality)
                 << ", Instr " << p.individualInstruction
                 << ", Plan " << p.developmentPlan << ", Promesa " << p.promisedRole
                 << ", Rasgos " << joinStringValues(p.traits, ", ")
                 << (p.wantsToLeave ? ", Quiere salir" : "")
                 << (p.onLoan ? ", Prestamo desde " + p.parentClub + " (" + to_string(p.loanWeeksRemaining) + " sem)" : "")
                 << endl;
        }
    }
}

void addPlayer(Team& team) {
    Player p;
    p.name = readLine("Nombre del jugador: ");
    p.position = normalizePosition(readLine("Posicion (ARQ/DEF/MED/DEL): "));
    if (p.position == "N/A") p.position = "MED";
    p.attack = readInt("Ataque (0-100): ", 0, 100);
    p.defense = readInt("Defensa (0-100): ", 0, 100);
    p.stamina = readInt("Resistencia (0-100): ", 0, 100);
    p.fitness = p.stamina;
    p.skill = readInt("Habilidad (0-100): ", 0, 100);
    p.potential = clampInt(p.skill + randInt(0, 5), p.skill, 95);
    p.age = readInt("Edad: ", 15, 50);
    p.value = readLongLong("Valor: ", 0, 1000000000000LL);
    p.wage = static_cast<long long>(p.skill) * 150 + randInt(0, 600);
    p.releaseClause = max(50000LL, p.value * 2);
    p.setPieceSkill = clampInt(p.skill + randInt(-8, 8), 20, 99);
    p.leadership = clampInt(35 + randInt(0, 45), 1, 99);
    p.professionalism = clampInt(40 + randInt(0, 45), 1, 99);
    p.ambition = clampInt(35 + randInt(0, 50), 1, 99);
    p.happiness = clampInt(55 + randInt(-10, 20), 1, 99);
    p.chemistry = clampInt(45 + randInt(0, 35), 1, 99);
    p.desiredStarts = (p.skill >= 70) ? 2 : 1;
    p.startsThisSeason = 0;
    p.wantsToLeave = false;
    p.onLoan = false;
    p.parentClub.clear();
    p.loanWeeksRemaining = 0;
    p.contractWeeks = randInt(52, 156);
    p.injured = false;
    p.injuryType = "";
    p.injuryWeeks = 0;
    p.injuryHistory = 0;
    p.yellowAccumulation = 0;
    p.seasonYellowCards = 0;
    p.seasonRedCards = 0;
    p.matchesSuspended = 0;
    p.goals = 0;
    p.assists = 0;
    p.matchesPlayed = 0;
    p.lastTrainedSeason = -1;
    p.lastTrainedWeek = -1;
    ensurePlayerProfile(p, true);
    team.addPlayer(p);
    cout << "Jugador agregado!" << endl;
}

static int trainingDeltaForStat(int stat) {
    int roll = randInt(1, 5);
    if (stat >= 90) return 1;
    if (stat >= 80) return min(roll, 2);
    if (stat >= 70) return min(roll, 3);
    return roll;
}

void trainPlayer(Team& team, int season, int week) {
    if (team.players.empty()) {
        cout << "No hay jugadores para entrenar." << endl;
        return;
    }
    cout << "Selecciona jugador para entrenar:" << endl;
    for (size_t i = 0; i < team.players.size(); ++i) {
        cout << i + 1 << ". " << team.players[i].name << (team.players[i].injured ? " (Lesionado)" : "") << endl;
    }
    int playerIndex = readInt("Elige jugador: ", 1, static_cast<int>(team.players.size()));
    Player& p = team.players[playerIndex - 1];
    if (p.injured) {
        cout << "No puedes entrenar a un jugador lesionado." << endl;
        return;
    }
    if (season >= 0 && week >= 0 && p.lastTrainedSeason == season && p.lastTrainedWeek == week) {
        cout << "Este jugador ya entreno esta semana." << endl;
        return;
    }
    cout << "Que quieres entrenar?" << endl;
    cout << "1. Ataque" << endl;
    cout << "2. Defensa" << endl;
    cout << "3. Resistencia" << endl;
    cout << "4. Habilidad" << endl;
    int trainChoice = readInt("Elige opcion: ", 1, 4);
    long long cost = 5000;
    if (team.budget < cost) {
        cout << "Presupuesto insuficiente para entrenar." << endl;
        return;
    }
    team.budget -= cost;
    int improvement = 1;
    switch (trainChoice) {
        case 1:
            improvement = trainingDeltaForStat(p.attack);
            p.attack = min(100, p.attack + improvement);
            cout << "Ataque mejorado a " << p.attack << endl;
            break;
        case 2:
            improvement = trainingDeltaForStat(p.defense);
            p.defense = min(100, p.defense + improvement);
            cout << "Defensa mejorada a " << p.defense << endl;
            break;
        case 3:
            improvement = trainingDeltaForStat(p.stamina);
            p.stamina = min(100, p.stamina + improvement);
            p.fitness = min(p.stamina, p.fitness + improvement);
            cout << "Resistencia mejorada a " << p.stamina << endl;
            break;
        case 4:
            improvement = trainingDeltaForStat(p.skill);
            p.skill = min(100, p.skill + improvement);
            if (p.skill > p.potential) p.potential = p.skill;
            cout << "Habilidad mejorada a " << p.skill << endl;
            break;
        default:
            break;
    }
    if (season >= 0 && week >= 0) {
        p.lastTrainedSeason = season;
        p.lastTrainedWeek = week;
    }
}

void changeTactics(Team& team) {
    cout << "Tacticas actuales: " << team.tactics << endl;
    cout << "1. Defensive" << endl;
    cout << "2. Balanced" << endl;
    cout << "3. Offensive" << endl;
    cout << "4. Pressing" << endl;
    cout << "5. Counter" << endl;
    int tacticsChoice = readInt("Elige tactica: ", 1, 5);
    switch (tacticsChoice) {
        case 1: team.tactics = "Defensive"; break;
        case 2: team.tactics = "Balanced"; break;
        case 3: team.tactics = "Offensive"; break;
        case 4: team.tactics = "Pressing"; break;
        case 5: team.tactics = "Counter"; break;
        default: break;
    }
    cout << "Intensidad de presion actual: " << team.pressingIntensity << " (1-5)" << endl;
    team.pressingIntensity = readInt("Nueva intensidad de presion: ", 1, 5);
    cout << "Linea defensiva actual: " << team.defensiveLine << " (1-5)" << endl;
    team.defensiveLine = readInt("Nueva linea defensiva: ", 1, 5);
    cout << "Tempo actual: " << team.tempo << " (1-5)" << endl;
    team.tempo = readInt("Nuevo tempo: ", 1, 5);
    cout << "Amplitud actual: " << team.width << " (1-5)" << endl;
    team.width = readInt("Nueva amplitud: ", 1, 5);
    cout << "Marcaje actual: " << team.markingStyle << endl;
    cout << "1. Zonal" << endl;
    cout << "2. Hombre" << endl;
    int marking = readInt("Tipo de marcaje: ", 1, 2);
    team.markingStyle = (marking == 2) ? "Hombre" : "Zonal";
    cout << "Instruccion de partido actual: " << team.matchInstruction << endl;
    cout << "1. Equilibrado" << endl;
    cout << "2. Laterales altos" << endl;
    cout << "3. Bloque bajo" << endl;
    cout << "4. Balon parado" << endl;
    cout << "5. Presion final" << endl;
    cout << "6. Por bandas" << endl;
    cout << "7. Juego directo" << endl;
    cout << "8. Contra-presion" << endl;
    cout << "9. Pausar juego" << endl;
    int instruction = readInt("Nueva instruccion: ", 1, 9);
    if (instruction == 2) team.matchInstruction = "Laterales altos";
    else if (instruction == 3) team.matchInstruction = "Bloque bajo";
    else if (instruction == 4) team.matchInstruction = "Balon parado";
    else if (instruction == 5) team.matchInstruction = "Presion final";
    else if (instruction == 6) team.matchInstruction = "Por bandas";
    else if (instruction == 7) team.matchInstruction = "Juego directo";
    else if (instruction == 8) team.matchInstruction = "Contra-presion";
    else if (instruction == 9) team.matchInstruction = "Pausar juego";
    else team.matchInstruction = "Equilibrado";
    ensureTeamIdentity(team);
    cout << "Tacticas cambiadas a " << team.tactics << endl;
}

void setTransferPolicy(Team& team);

void manageLineup(Career& career) {
    if (!career.myTeam) {
        cout << "No hay una carrera activa." << endl;
        return;
    }
    Team& team = *career.myTeam;
    if (team.players.empty()) {
        cout << "No hay jugadores disponibles." << endl;
        return;
    }
    auto selectNamedList = [&](vector<string>& target, int maxCount, bool excludePreferredXI) {
        vector<string> chosen;
        for (size_t i = 0; i < team.players.size(); ++i) {
            const Player& p = team.players[i];
            bool inPreferredXI = find(team.preferredXI.begin(), team.preferredXI.end(), p.name) != team.preferredXI.end();
            if (excludePreferredXI && inPreferredXI) continue;
            cout << i + 1 << ". " << p.name << " (" << p.position << ")"
                 << (p.injured ? " [LES]" : "")
                 << (p.matchesSuspended > 0 ? " [SUSP]" : "")
                 << " Hab " << p.skill << " Cond " << p.fitness << endl;
        }
        while (static_cast<int>(chosen.size()) < maxCount) {
            int idx = readInt("Jugador (0 para terminar): ", 0, static_cast<int>(team.players.size()));
            if (idx == 0) break;
          const Player& p =
    team.players[static_cast<size_t>(idx - 1)];

if (p.injured) {
    cout << "No puedes seleccionar a un jugador lesionado."
         << endl;
    continue;
}

if (p.matchesSuspended > 0) {
    cout << "No puedes seleccionar a un jugador suspendido."
         << endl;
    continue;
}

if (excludePreferredXI &&
    find(team.preferredXI.begin(),
         team.preferredXI.end(),
         p.name) != team.preferredXI.end()) {
    cout << "Ese jugador ya esta en los titulares preferidos."
         << endl;
    continue;
}

if (find(chosen.begin(), chosen.end(), p.name) !=
    chosen.end()) {
    cout << "Ese jugador ya fue elegido." << endl;
    continue;
}

chosen.push_back(p.name);

cout << "Seleccionado: " << p.name
     << " (" << chosen.size()
     << "/" << maxCount << ")" << endl;
        }
        target = chosen;
    };

    auto selectSinglePlayer = [&](const string& prompt, string& target) {
        for (size_t i = 0; i < team.players.size(); ++i) {
            cout << i + 1 << ". " << team.players[i].name << " (" << team.players[i].position << ")" << endl;
        }
        int idx = readInt(prompt, 1, static_cast<int>(team.players.size())) - 1;
        target = team.players[idx].name;
    };

    while (true) {
        cout << "\n=== Alineacion y Rotacion ===" << endl;
        cout << "Capitan: " << (team.captain.empty() ? "No definido" : team.captain) << endl;
        cout << "Penales: " << (team.penaltyTaker.empty() ? "No definido" : team.penaltyTaker) << endl;
        cout << "Tiros libres: " << (team.freeKickTaker.empty() ? "No definido" : team.freeKickTaker)
             << " | Corners: " << (team.cornerTaker.empty() ? "No definido" : team.cornerTaker) << endl;
        cout << "Rotacion: " << team.rotationPolicy << endl;
        cout << "Titulares preferidos: " << team.preferredXI.size() << "/11" << endl;
        cout << "Banca preferida: " << team.preferredBench.size() << "/7" << endl;
        cout << "1. Autoasignar mejor XI" << endl;
        cout << "2. Elegir titulares manualmente" << endl;
        cout << "3. Autoasignar banca" << endl;
        cout << "4. Elegir banca manualmente" << endl;
        cout << "5. Elegir capitan" << endl;
        cout << "6. Elegir lanzador de penales" << endl;
        cout << "7. Elegir lanzador de tiros libres" << endl;
        cout << "8. Elegir lanzador de corners" << endl;
        cout << "9. Politica de rotacion" << endl;
        cout << "10. Cambiar instruccion individual" << endl;
        cout << "11. Ver plan actual" << endl;
        cout << "12. Volver" << endl;
        int choice = readInt("Elige opcion: ", 1, 12);
        if (choice == 12) break;

        if (choice == 1) {
            team.preferredXI.clear();
            auto xi = team.getStartingXIIndices();
            for (int idx : xi) {
                if (idx >= 0 && idx < static_cast<int>(team.players.size())) {
                    team.preferredXI.push_back(team.players[idx].name);
                }
            }
            cout << "XI preferido actualizado automaticamente." << endl;
        } else if (choice == 2) {
            selectNamedList(team.preferredXI, 11, false);
            if (!team.preferredXI.empty()) {
                cout << "Titulares preferidos actualizados." << endl;
            }
        } else if (choice == 3) {
            team.preferredBench.clear();
            auto bench = team.getBenchIndices();
            for (int idx : bench) {
                if (idx >= 0 && idx < static_cast<int>(team.players.size())) {
                    team.preferredBench.push_back(team.players[idx].name);
                }
            }
            cout << "Banca preferida actualizada automaticamente." << endl;
        } else if (choice == 4) {
            selectNamedList(team.preferredBench, 7, true);
            cout << "Banca preferida actualizada." << endl;
        } else if (choice == 5) {
            selectSinglePlayer("Jugador: ", team.captain);
            cout << "Capitan actualizado." << endl;
        } else if (choice == 6) {
            selectSinglePlayer("Jugador: ", team.penaltyTaker);
            cout << "Lanzador de penales actualizado." << endl;
        } else if (choice == 7) {
            selectSinglePlayer("Jugador: ", team.freeKickTaker);
            cout << "Lanzador de tiros libres actualizado." << endl;
        } else if (choice == 8) {
            selectSinglePlayer("Jugador: ", team.cornerTaker);
            cout << "Lanzador de corners actualizado." << endl;
        } else if (choice == 9) {
            cout << "1. Titulares" << endl;
            cout << "2. Balanceado" << endl;
            cout << "3. Rotacion" << endl;
            int mode = readInt("Politica: ", 1, 3);
            if (mode == 1) team.rotationPolicy = "Titulares";
            else if (mode == 2) team.rotationPolicy = "Balanceado";
            else team.rotationPolicy = "Rotacion";
            cout << "Politica actualizada." << endl;
        } else if (choice == 10) {
            cout << "Jugadores e instruccion actual:" << endl;
            for (size_t i = 0; i < team.players.size(); ++i) {
                cout << i + 1 << ". " << team.players[i].name
                     << " (" << team.players[i].position << ")"
                     << " | Instr. " << team.players[i].individualInstruction << endl;
            }
            int idx = readInt("Jugador (0 para cancelar): ", 0, static_cast<int>(team.players.size()));
            if (idx == 0) continue;
            ServiceResult result = cyclePlayerInstructionService(career, team.players[static_cast<size_t>(idx - 1)].name);
            for (const auto& message : result.messages) {
                cout << message << endl;
            }
        } else if (choice == 7) {
            setTransferPolicy(team);
        } else if (choice == 11) {
            auto xi = team.getStartingXIIndices();
            cout << "XI actual:" << endl;
            for (int idx : xi) {
                if (idx >= 0 && idx < static_cast<int>(team.players.size())) {
                    cout << "- " << team.players[idx].name << " (" << team.players[idx].position << ")"
                         << " | Instr. " << team.players[idx].individualInstruction << endl;
                }
            }
            auto bench = team.getBenchIndices();
            if (!bench.empty()) {
                cout << "Banca actual:" << endl;
                for (int idx : bench) {
                    if (idx >= 0 && idx < static_cast<int>(team.players.size())) {
                        cout << "- " << team.players[idx].name << " (" << team.players[idx].position << ")"
                             << " | Instr. " << team.players[idx].individualInstruction << endl;
                    }
                }
            }
        }
    }
}

void setTrainingPlan(Team& team) {
    cout << "\nPlan de entrenamiento actual: " << team.trainingFocus << endl;
    cout << "1. Balanceado" << endl;
    cout << "2. Resistencia" << endl;
    cout << "3. Ataque" << endl;
    cout << "4. Defensa" << endl;
    cout << "5. Tecnico" << endl;
    cout << "6. Tactico" << endl;
    cout << "7. Preparacion partido" << endl;
    cout << "8. Recuperacion" << endl;
    int choice = readInt("Elige plan: ", 1, 8);
    switch (choice) {
        case 1: team.trainingFocus = "Balanceado"; break;
        case 2: team.trainingFocus = "Resistencia"; break;
        case 3: team.trainingFocus = "Ataque"; break;
        case 4: team.trainingFocus = "Defensa"; break;
        case 5: team.trainingFocus = "Tecnico"; break;
        case 6: team.trainingFocus = "Tactico"; break;
        case 7: team.trainingFocus = "Preparacion partido"; break;
        case 8: team.trainingFocus = "Recuperacion"; break;
        default: break;
    }
    cout << "Plan actualizado a " << team.trainingFocus << endl;
}

void setClubIdentity(Team& team) {
    cout << "\nIdentidad de club actual: " << (team.clubStyle.empty() ? "No definida" : team.clubStyle) << endl;
    cout << "1. Control de posesion" << endl;
    cout << "2. Presion vertical" << endl;
    cout << "3. Ataque por bandas" << endl;
    cout << "4. Bloque ordenado" << endl;
    cout << "5. Transicion directa" << endl;
    cout << "6. Sin estilo concreto" << endl;
    int choice = readInt("Elige identidad de club: ", 1, 6);
    switch (choice) {
        case 1: team.clubStyle = "Control de posesion"; break;
        case 2: team.clubStyle = "Presion vertical"; break;
        case 3: team.clubStyle = "Ataque por bandas"; break;
        case 4: team.clubStyle = "Bloque ordenado"; break;
        case 5: team.clubStyle = "Transicion directa"; break;
        default: team.clubStyle.clear(); break;
    }
    cout << "Identidad de club actualizada a " << (team.clubStyle.empty() ? "sin estilo definido" : team.clubStyle) << endl;
}

void setYouthIdentity(Team& team) {
    cout << "\nPolitica juvenil actual: " << (team.youthIdentity.empty() ? "No definida" : team.youthIdentity) << endl;
    cout << "1. Cantera estructurada" << endl;
    cout << "2. Desarrollo mixto" << endl;
    cout << "3. Talento local" << endl;
    cout << "4. Sin politica especifica" << endl;
    int choice = readInt("Elige politica juvenil: ", 1, 4);
    switch (choice) {
        case 1: team.youthIdentity = "Cantera estructurada"; break;
        case 2: team.youthIdentity = "Desarrollo mixto"; break;
        case 3: team.youthIdentity = "Talento local"; break;
        default: team.youthIdentity.clear(); break;
    }
    cout << "Politica juvenil actualizada a " << (team.youthIdentity.empty() ? "sin politica definida" : team.youthIdentity) << endl;
}

void setTransferPolicy(Team& team) {
    cout << "\nPolitica de mercado actual: " << (team.transferPolicy.empty() ? "Mixta" : team.transferPolicy) << endl;
    cout << "1. Cantera y valor futuro" << endl;
    cout << "2. Vender antes de comprar" << endl;
    cout << "3. Mixta" << endl;
    cout << "4. Sin politica concreta" << endl;
    int choice = readInt("Elige politica de mercado: ", 1, 4);
    switch (choice) {
        case 1: team.transferPolicy = "Cantera y valor futuro"; break;
        case 2: team.transferPolicy = "Vender antes de comprar"; break;
        case 3: team.transferPolicy = "Mixta"; break;
        default: team.transferPolicy.clear(); break;
    }
    cout << "Politica de mercado actualizada a " << (team.transferPolicy.empty() ? "sin politica definida" : team.transferPolicy) << endl;
}

void editTeam(Team& team) {
    while (true) {
        cout << "\n=== Editor Rapido de Equipo ===" << endl;
        cout << "1. Renombrar equipo" << endl;
        cout << "2. Cambiar formacion" << endl;
        cout << "3. Editar jugador" << endl;
        cout << "4. Eliminar jugador" << endl;
        cout << "5. Ajustar identidad de club" << endl;
        cout << "6. Ajustar politica juvenil" << endl;
        cout << "7. Ajustar politica de mercado" << endl;
        cout << "8. Volver" << endl;
        int choice = readInt("Elige una opcion: ", 1, 8);
        if (choice == 8) break;

        if (choice == 1) {
            string name = readLine("Nuevo nombre del equipo: ");
            if (!name.empty()) team.name = name;
            cout << "Nombre actualizado." << endl;
        } else if (choice == 2) {
            string form = readLine("Nueva formacion (ej: 4-4-2): ");
            if (!form.empty()) team.formation = form;
            cout << "Formacion actualizada." << endl;
        } else if (choice == 3) {
            if (team.players.empty()) {
                cout << "No hay jugadores para editar." << endl;
                continue;
            }
            cout << "Selecciona jugador:" << endl;
            for (size_t i = 0; i < team.players.size(); ++i) {
                cout << i + 1 << ". " << team.players[i].name << " (" << team.players[i].position << ")" << endl;
            }
            int idx = readInt("Jugador: ", 1, static_cast<int>(team.players.size())) - 1;
            Player& p = team.players[idx];
            cout << "1. Nombre" << endl;
            cout << "2. Posicion" << endl;
            cout << "3. Ataque" << endl;
            cout << "4. Defensa" << endl;
            cout << "5. Resistencia" << endl;
            cout << "6. Habilidad" << endl;
            cout << "7. Edad" << endl;
            cout << "8. Valor" << endl;
            cout << "9. Condicion" << endl;
            cout << "10. Potencial" << endl;
            cout << "11. Rol" << endl;
            cout << "12. Salario" << endl;
            cout << "13. Contrato (semanas)" << endl;
            cout << "14. Volver" << endl;
            int field = readInt("Campo a editar: ", 1, 14);
            if (field == 14) continue;
            switch (field) {
                case 1: {
                    string v = readLine("Nuevo nombre: ");
                    if (!v.empty()) p.name = v;
                    break;
                }
                case 2: {
                    string v = readLine("Nueva posicion: ");
                    string pos = normalizePosition(v);
                    p.position = (pos == "N/A") ? "MED" : pos;
                    break;
                }
                case 3:
                    p.attack = readInt("Nuevo ataque (0-100): ", 0, 100);
                    break;
                case 4:
                    p.defense = readInt("Nueva defensa (0-100): ", 0, 100);
                    break;
                case 5:
                    p.stamina = readInt("Nueva resistencia (0-100): ", 0, 100);
                    if (p.fitness > p.stamina) p.fitness = p.stamina;
                    break;
                case 6:
                    p.skill = readInt("Nueva habilidad (0-100): ", 0, 100);
                    if (p.potential < p.skill) p.potential = p.skill;
                    break;
                case 7:
                    p.age = readInt("Nueva edad: ", 15, 50);
                    break;
                case 8:
                    p.value = readLongLong("Nuevo valor: ", 0, 1000000000000LL);
                    break;
                case 9:
                    p.fitness = readInt("Nueva condicion (0-100): ", 0, 100);
                    if (p.fitness > p.stamina) p.fitness = p.stamina;
                    break;
                case 10:
                    p.potential = readInt("Nuevo potencial (0-100): ", 0, 100);
                    if (p.potential < p.skill) p.potential = p.skill;
                    break;
                case 11: {
                    string v = readLine("Nuevo rol: ");
                    if (!v.empty()) p.role = v;
                    break;
                }
                case 12:
                    p.wage = readLongLong("Nuevo salario: ", 0, 1000000000000LL);
                    break;
                case 13:
                    p.contractWeeks = readInt("Nuevo contrato (semanas): ", 0, 520);
                    break;
                default:
                    break;
            }
            cout << "Jugador actualizado." << endl;
        } else if (choice == 4) {
            if (team.players.size() <= 11) {
                cout << "No puedes dejar al equipo con menos de 11 jugadores." << endl;
                continue;
            }
            cout << "Selecciona jugador para eliminar:" << endl;
            for (size_t i = 0; i < team.players.size(); ++i) {
                cout << i + 1 << ". " << team.players[i].name << endl;
            }
            int idx = readInt("Jugador: ", 1, static_cast<int>(team.players.size())) - 1;
            cout << team.players[idx].name << " eliminado." << endl;
            team.players.erase(team.players.begin() + idx);
        } else if (choice == 5) {
            setClubIdentity(team);
        } else if (choice == 6) {
            setYouthIdentity(team);
        } else if (choice == 7) {
            setTransferPolicy(team);
        }
    }
}

void displayStatistics(Team& team) {
    cout << "\n--- Estadisticas del Equipo ---" << endl;
    cout << "Moral: " << team.morale << endl;
    cout << "Victorias: " << team.wins << endl;
    cout << "Empates: " << team.draws << endl;
    cout << "Derrotas: " << team.losses << endl;
    cout << "Goles a Favor: " << team.goalsFor << endl;
    cout << "Goles en Contra: " << team.goalsAgainst << endl;
    cout << "Goles de Visita: " << team.awayGoals << endl;
    cout << "Tarjetas Amarillas: " << team.yellowCards << endl;
    cout << "Tarjetas Rojas: " << team.redCards << endl;
    cout << "Puntos: " << team.points << endl;
    cout << "\n--- Estadisticas de Jugadores ---" << endl;
    for (const auto& p : team.players) {
        cout << p.name << ": Goles " << p.goals
             << ", Asistencias " << p.assists
             << ", Partidos " << p.matchesPlayed
             << ", TA " << p.seasonYellowCards
             << ", TR " << p.seasonRedCards
             << ", Susp " << p.matchesSuspended
             << ", Tit " << p.startsThisSeason
             << ", Fel " << p.happiness
             << ", Quim " << p.chemistry
             << (p.wantsToLeave ? ", Quiere salir" : "")
             << endl;
    }
}