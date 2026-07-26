#include "ai/ai_match_manager.h"

#include "ai/team_ai.h"
#include "simulation/fatigue_engine.h"
#include "simulation/match_engine_internal.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

constexpr int MAX_SUBSTITUTIONS = 5;
constexpr int FIRST_PLANNED_SUBSTITUTION_MINUTE = 55;
constexpr int SECOND_SUBSTITUTION_WINDOW_MINUTE = 72;
constexpr int DEFAULT_SUBSTITUTION_NEED = 10;
constexpr int LATE_SUBSTITUTION_NEED = 7;
constexpr int MINUTES_BETWEEN_SUBSTITUTIONS = 8;
constexpr int EMERGENCY_RESERVE_UNTIL_MINUTE = 75;
constexpr int FINAL_PUSH_MINUTE = 82;

bool isCautioned(const vector<string>& cautionedPlayers, const string& name) {
    return find(cautionedPlayers.begin(), cautionedPlayers.end(), name) != cautionedPlayers.end();
}

int countTeamSubstitutions(const MatchTimeline& timeline, const string& teamName) {
    return static_cast<int>(count_if(
        timeline.events.begin(),
        timeline.events.end(),
        [&](const MatchEvent& event) {
            return event.type == MatchEventType::Substitution &&
                   event.teamName == teamName;
        }));
}

int lastTeamSubstitutionMinute(const MatchTimeline& timeline,
                               const string& teamName) {
    int lastMinute = -1000;

    for (const MatchEvent& event : timeline.events) {
        if (event.type == MatchEventType::Substitution &&
            event.teamName == teamName) {
            lastMinute = max(lastMinute, event.minute);
        }
    }

    return lastMinute;
}

bool substitutionCooldownReady(const MatchTimeline& timeline,
                               const string& teamName,
                               int minute,
                               bool emergency) {
    if (emergency) return true;

    return minute - lastTeamSubstitutionMinute(timeline, teamName) >=
           MINUTES_BETWEEN_SUBSTITUTIONS;
}

int contextualSubstitutionThreshold(int baseThreshold,
                                    int minute,
                                    int goalsFor,
                                    int goalsAgainst,
                                    int substitutionsUsed) {
    int threshold = baseThreshold;
    const int scoreDiff = goalsFor - goalsAgainst;

    if (scoreDiff < 0) {
        threshold -= minute >= 70 ? 4 : 2;
    } else if (scoreDiff > 0) {
        threshold += minute < 70 ? 3 : 1;
    }

    if (minute >= FINAL_PUSH_MINUTE) threshold -= 3;
    if (substitutionsUsed >= 3 && minute < EMERGENCY_RESERVE_UNTIL_MINUTE) {
        threshold += 4;
    }

    return max(1, threshold);
}

bool shouldReserveEmergencySubstitution(int minute,
                                        int substitutionsUsed,
                                        int goalsFor,
                                        int goalsAgainst) {
    if (minute >= EMERGENCY_RESERVE_UNTIL_MINUTE) return false;
    if (substitutionsUsed < MAX_SUBSTITUTIONS - 1) return false;

    // Solo se permite gastar el último cambio antes del minuto 75
    // cuando el equipo está perdiendo claramente.
    return goalsFor >= goalsAgainst - 1;
}

bool hasInjuredActivePlayer(const Team& team, const vector<int>& xi) {
    return any_of(
        xi.begin(),
        xi.end(),
        [&](int idx) {
            return idx >= 0 &&
                   idx < static_cast<int>(team.players.size()) &&
                   team.players[static_cast<size_t>(idx)].injured;
        });
}

int contextualSubstitutionNeed(const Team& team,
                               int playerIndex,
                               bool cautioned,
                               int minute,
                               int goalsFor,
                               int goalsAgainst) {
    if (playerIndex < 0 ||
        playerIndex >= static_cast<int>(team.players.size())) {
        return 0;
    }

    const Player& player = team.players[static_cast<size_t>(playerIndex)];
    int need = fatigue_engine::substitutionNeedScore(
        team,
        playerIndex,
        cautioned);

    if (player.injured) return need + 1000;

    const int scoreDiff = goalsFor - goalsAgainst;
    const string position = normalizePosition(player.position);
    const string duty = match_internal::compactToken(player.roleDuty);
    const bool lateMatch = minute >= 70;

    // Forma baja: representa un rendimiento deficiente durante el periodo.
    need += max(0, 52 - player.currentForm) / 2;

    if (player.currentForm <= 35) need += 7;
    if (player.currentForm >= 78) need -= 5;
    if (player.consistency <= 40) need += 3;

    // La fatiga tiene más importancia en el cierre del partido.
    if (lateMatch) {
        need += max(0, 68 - player.fitness);
        need += max(0, 55 - player.stamina) / 2;
    }

    // Perdiendo: se reemplaza antes a quien ofrece poco impacto ofensivo.
    if (scoreDiff < 0) {
        if (position == "DEL") {
            need += max(0, 62 - player.attack) / 3;
            if (player.attack >= 76 || player.currentForm >= 72) need -= 7;
        } else if (position == "MED") {
            need += max(0, 58 - player.attack) / 4;
            if (duty == "defensa") need += minute >= 65 ? 6 : 2;
        } else if (position == "DEF" && minute >= 70) {
            need += 3;
        }

        if (duty == "ataque" && player.attack >= 70) need -= 4;
        if (playerHasTrait(player, "Llega al area")) need -= 3;
        if (playerHasTrait(player, "Pase riesgoso")) need -= 2;
    }

    // Ganando: se reemplaza antes a quien pueda comprometer la ventaja.
    if (scoreDiff > 0) {
        need += max(0, 58 - player.tacticalDiscipline) / 3;

        if (position == "DEF") {
            need += max(0, 62 - player.defense) / 3;
            if (player.defense >= 76 &&
                player.tacticalDiscipline >= 68) {
                need -= 7;
            }
        } else if (position == "MED") {
            need += max(0, 55 - player.defense) / 4;
        } else if (position == "DEL" && minute >= 72) {
            need += 4;
        }

        if (duty == "defensa") need -= 3;
        if (playerHasTrait(player, "Muralla")) need -= 4;
        if (playerHasTrait(player, "Lider")) need -= 3;
    }

    // Con empate tardío se busca refrescar jugadores poco influyentes.
    if (scoreDiff == 0 && minute >= 72) {
        const int balancedImpact = (player.attack + player.defense) / 2;
        need += max(0, 58 - balancedImpact) / 3;
        if (player.currentForm >= 75) need -= 4;
    }

    // Una segunda amarilla es especialmente probable con presión o marcaje.
    if (cautioned &&
        (team.pressingIntensity >= 4 ||
         team.markingStyle == "Hombre")) {
        need += lateMatch ? 8 : 4;
    }

    // Se intenta conservar al capitán salvo lesión o riesgo evidente.
    if (!team.captain.empty() &&
        player.name == team.captain &&
        player.fitness >= 48 &&
        !cautioned) {
        need -= 6;
    }

    return max(0, need);
}

bool applyBenchSubstitution(Team& team,
                            vector<int>& xi,
                            vector<int>& participants,
                            const vector<string>& cautionedPlayers,
                            int minute,
                            int minimumNeed,
                            int goalsFor,
                            int goalsAgainst,
                            int opponentAvailablePlayers,
                            MatchTimeline& timeline) {
    const bool emergency = hasInjuredActivePlayer(team, xi);

    if (xi.empty() ||
        countTeamSubstitutions(timeline, team.name) >= MAX_SUBSTITUTIONS ||
        !substitutionCooldownReady(
            timeline,
            team.name,
            minute,
            emergency)) {
        return false;
    }

    int playerOut = -1;
    int outSlot = -1;
    int highestNeed = 0;

    for (size_t slot = 0; slot < xi.size(); ++slot) {
        const int idx = xi[slot];
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;

        const Player& player = team.players[static_cast<size_t>(idx)];
        const int need = contextualSubstitutionNeed(
            team,
            idx,
            isCautioned(cautionedPlayers, player.name),
            minute,
            goalsFor,
            goalsAgainst);

        if (need > highestNeed) {
            highestNeed = need;
            playerOut = idx;
            outSlot = static_cast<int>(slot);
        }
    }

    if (playerOut < 0 || highestNeed < minimumNeed) return false;

    const string targetPos =
        normalizePosition(team.players[static_cast<size_t>(playerOut)].position);
    const int playerIn = match_internal::bestBenchReplacement(
        team,
        xi,
        targetPos,
        minute,
        goalsFor,
        goalsAgainst,
        opponentAvailablePlayers);

    if (playerIn < 0) return false;

    xi[static_cast<size_t>(outSlot)] = playerIn;

    if (find(participants.begin(), participants.end(), playerIn) ==
        participants.end()) {
        participants.push_back(playerIn);
    }

    const Player& outgoing = team.players[static_cast<size_t>(playerOut)];
    const Player& incoming = team.players[static_cast<size_t>(playerIn)];

    MatchEvent event;
    event.minute = minute;
    event.teamName = team.name;
    event.playerName = incoming.name;
    event.type = MatchEventType::Substitution;

    if (outgoing.injured) {
        event.description =
            outgoing.name + " sale lesionado; entra " + incoming.name;
    } else if (isCautioned(cautionedPlayers, outgoing.name) &&
               outgoing.fitness < 62) {
        event.description =
            outgoing.name + " deja el campo para evitar riesgos; entra " +
            incoming.name;
    } else if (outgoing.fitness < 56) {
        event.description =
            outgoing.name + " sale por desgaste; entra " + incoming.name;
    } else if (goalsFor < goalsAgainst) {
        event.description =
            team.name + " busca mayor impacto con " + incoming.name +
            " en lugar de " + outgoing.name;
    } else if (goalsFor > goalsAgainst) {
        event.description =
            team.name + " protege la ventaja: entra " + incoming.name +
            " por " + outgoing.name;
    } else {
        event.description =
            outgoing.name + " deja el campo por " + incoming.name;
    }

    timeline.events.push_back(event);
    return true;
}

int averageActiveFitness(const Team& team, const vector<int>& xi) {
    int total = 0;
    int count = 0;

    for (int idx : xi) {
        if (idx < 0 || idx >= static_cast<int>(team.players.size())) continue;
        total += team.players[static_cast<size_t>(idx)].fitness;
        count++;
    }

    return count > 0 ? total / count : 50;
}

}  // namespace

namespace ai_match_manager {

bool applyInMatchManagement(Team& team,
                            const Team& opponent,
                            vector<int>& xi,
                            vector<int>& participants,
                            const vector<string>& cautionedPlayers,
                            int minute,
                            int goalsFor,
                            int goalsAgainst,
                            int opponentAvailablePlayers,
                            MatchTimeline& timeline) {
    bool changed = false;
    vector<string> notes;

    if (team_ai::applyInMatchCpuAdjustment(
            team,
            opponent,
            minute,
            goalsFor,
            goalsAgainst,
            &notes,
            static_cast<int>(xi.size()),
            static_cast<int>(cautionedPlayers.size()),
            opponentAvailablePlayers)) {
        changed = true;

        for (const string& note : notes) {
            MatchEvent event;
            event.minute = minute;
            event.teamName = team.name;
            event.type = MatchEventType::TacticalChange;
            event.description = note;
            timeline.events.push_back(event);
        }
    }

    int substitutionsUsed = countTeamSubstitutions(timeline, team.name);

    const int plannedThreshold = contextualSubstitutionThreshold(
        DEFAULT_SUBSTITUTION_NEED,
        minute,
        goalsFor,
        goalsAgainst,
        substitutionsUsed);

    const int lateThreshold = contextualSubstitutionThreshold(
        LATE_SUBSTITUTION_NEED,
        minute,
        goalsFor,
        goalsAgainst,
        substitutionsUsed);

    const bool reserveEmergencySub =
        shouldReserveEmergencySubstitution(
            minute,
            substitutionsUsed,
            goalsFor,
            goalsAgainst);

    // Las lesiones tienen prioridad y pueden forzar un cambio antes
    // de la ventana habitual de sustituciones.
    if (substitutionsUsed < MAX_SUBSTITUTIONS &&
        hasInjuredActivePlayer(team, xi) &&
        applyBenchSubstitution(
            team,
            xi,
            participants,
            cautionedPlayers,
            minute,
            1,
            goalsFor,
            goalsAgainst,
            opponentAvailablePlayers,
            timeline)) {
        changed = true;
        substitutionsUsed++;
    }

    // Primer cambio planificado: cansancio, tarjeta o riesgo físico.
    if (substitutionsUsed < MAX_SUBSTITUTIONS &&
        !reserveEmergencySub &&
        minute >= FIRST_PLANNED_SUBSTITUTION_MINUTE &&
        applyBenchSubstitution(
            team,
            xi,
            participants,
            cautionedPlayers,
            minute,
            plannedThreshold,
            goalsFor,
            goalsAgainst,
            opponentAvailablePlayers,
            timeline)) {
        changed = true;
        substitutionsUsed++;
    }

    // Segunda evaluación en el tramo final. Se reduce el umbral para
    // permitir refrescar el equipo cuando el desgaste colectivo es alto.
    if (substitutionsUsed < MAX_SUBSTITUTIONS &&
        !reserveEmergencySub &&
        minute >= SECOND_SUBSTITUTION_WINDOW_MINUTE &&
        (averageActiveFitness(team, xi) < 56 ||
         minute >= FINAL_PUSH_MINUTE) &&
        applyBenchSubstitution(
            team,
            xi,
            participants,
            cautionedPlayers,
            minute,
            lateThreshold,
            goalsFor,
            goalsAgainst,
            opponentAvailablePlayers,
            timeline)) {
        changed = true;
    }

    return changed;
}

}  // namespace ai_match_manager
