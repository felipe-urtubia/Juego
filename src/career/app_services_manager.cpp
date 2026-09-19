#include "career/app_services.h"

#include "career/dressing_room_service.h"
#include "career/team_management.h"
#include "development/training_impact_system.h"
#include "engine/models.h"
#include "transfers/negotiation_system.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

ServiceResult failure(const string& message) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(message);
    return result;
}

string nextDevelopmentPlan(const string& current) {
    static const vector<string> plans = {"Equilibrado", "Fisico", "Defensa", "Creatividad", "Finalizacion", "Liderazgo"};
    for (size_t i = 0; i < plans.size(); ++i) {
        if (plans[i] == current) return plans[(i + 1) % plans.size()];
    }
    return plans.front();
}

string nextInstructionForPlayer(const Player& player) {
    static const vector<string> goalkeepers = {"Libre", "Conservar posicion", "Abrir campo", "Descanso medico"};
    static const vector<string> defenders = {"Libre", "Cerrar por dentro", "Marcar fuerte", "Abrir campo", "Conservar posicion", "Descanso medico"};
    static const vector<string> midfielders = {"Libre", "Arriesgar pase", "Abrir campo", "Conservar posicion", "Marcar fuerte", "Descanso medico"};
    static const vector<string> forwards = {"Libre", "Atacar espalda", "Abrir campo", "Conservar posicion", "Marcar fuerte", "Descanso medico"};
    const vector<string>* options = &midfielders;
    const string pos = normalizePosition(player.position);
    if (pos == "ARQ") options = &goalkeepers;
    else if (pos == "DEF") options = &defenders;
    else if (pos == "DEL") options = &forwards;
    for (size_t i = 0; i < options->size(); ++i) {
        if ((*options)[i] == player.individualInstruction) return (*options)[(i + 1) % options->size()];
    }
    return options->front();
}

string nextTrainingFocus(const string& current) {
    static const vector<string> focuses = {"Balanceado", "Recuperacion", "Ataque", "Defensa",
                                           "Resistencia", "Tecnico", "Tactico", "Preparacion partido"};
    for (size_t i = 0; i < focuses.size(); ++i) {
        if (focuses[i] == current) return focuses[(i + 1) % focuses.size()];
    }
    return focuses.front();
}

string nextMatchInstruction(const string& current) {
    static const vector<string> instructions = {"Equilibrado", "Laterales altos", "Bloque bajo", "Balon parado",
                                                "Presion final", "Por bandas", "Juego directo",
                                                "Contra-presion", "Pausar juego"};
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i] == current) return instructions[(i + 1) % instructions.size()];
    }
    return instructions.front();
}

}  // namespace

ServiceResult cyclePlayerDevelopmentPlanService(Career& career, const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    Player& player = team.players[static_cast<size_t>(index)];
    player.developmentPlan = nextDevelopmentPlan(player.developmentPlan);
    career.addNews("Plan individual actualizado para " + player.name + ": " + player.developmentPlan + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back(player.name + " cambia su plan a " + player.developmentPlan + ".");
    return result;
}

ServiceResult cyclePlayerInstructionService(Career& career, const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    Player& player = team.players[static_cast<size_t>(index)];
    player.individualInstruction = nextInstructionForPlayer(player);
    career.addNews("Instruccion individual actualizada para " + player.name + ": " + player.individualInstruction + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back(player.name + " cambia su instruccion a " + player.individualInstruction + ".");
    return result;
}

ServiceResult holdTeamMeetingService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    const DressingRoomSnapshot before = dressing_room_service::buildSnapshot(team, career.currentWeek);

    int improvedPlayers = 0;
    for (auto& player : team.players) {
        int delta = 1;
        if (player.wantsToLeave || promiseAtRisk(player, career.currentWeek)) delta += 2;
        if (player.happiness <= 45 || player.socialGroup == "Frustrados") delta += 2;
        if (player.leadership >= 72 || playerHasTrait(player, "Lider")) delta += 1;
        player.happiness = clampInt(player.happiness + delta, 1, 99);
        player.chemistry = clampInt(player.chemistry + max(1, delta / 2), 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum + max(1, delta / 2), -25, 25);
        if (!promiseAtRisk(player, career.currentWeek) && player.happiness >= 52) {
            player.wantsToLeave = false;
        }
        if (delta >= 3) improvedPlayers++;
    }

    team.morale = clampInt(team.morale + 2 + before.leadershipSupport + (before.socialTension >= 5 ? 2 : 0), 0, 100);
    const DressingRoomSnapshot after = dressing_room_service::buildSnapshot(team, career.currentWeek);

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Reunion de plantel realizada. Mejoran el clima " + to_string(improvedPlayers) + " jugador(es).");
    result.messages.push_back("Tension social: " + to_string(before.socialTension) + " -> " + to_string(after.socialTension) +
                              " | Moral del equipo " + to_string(team.morale) + ".");
    career.addNews("El manager convoca una reunion de plantel para ordenar el clima interno en " + team.name + ".");
    return result;
}

ServiceResult talkToPlayerService(Career& career, const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    Player& player = team.players[static_cast<size_t>(index)];

    const bool roleConflict = !player.promisedPosition.empty() &&
                              normalizePosition(player.promisedPosition) != normalizePosition(player.position);
    int openness = player.professionalism + player.happiness + player.leadership + team.morale / 2;
    if (player.wantsToLeave) openness -= 18;
    if (promiseAtRisk(player, career.currentWeek)) openness -= 12;
    if (roleConflict) openness -= 10;

    ServiceResult result;
    result.ok = true;
    if (openness >= 135) {
        player.happiness = clampInt(player.happiness + 6, 1, 99);
        player.chemistry = clampInt(player.chemistry + 3, 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum + 4, -25, 25);
        player.wantsToLeave = false;
        result.messages.push_back("La charla con " + player.name + " sale muy bien y baja su tension.");
    } else if (openness >= 110) {
        player.happiness = clampInt(player.happiness + 3, 1, 99);
        player.chemistry = clampInt(player.chemistry + 2, 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum + 2, -25, 25);
        result.messages.push_back(player.name + " acepta el mensaje y queda mas alineado con el plan del club.");
    } else {
        player.happiness = clampInt(player.happiness - 1, 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum - 1, -25, 25);
        result.messages.push_back("La charla con " + player.name + " queda en observacion y no despeja del todo el malestar.");
    }
    if (roleConflict) {
        if (player.versatility >= 58 || player.professionalism >= 70) {
            player.position = normalizePosition(player.promisedPosition);
            player.roleDuty = defaultDutyForPosition(player.position);
            result.messages.push_back("Se realinea su uso con la posicion prometida (" + player.promisedPosition + ").");
        } else {
            result.messages.push_back("Sigue pendiente ordenar su rol de posicion prometida (" + player.promisedPosition + ").");
        }
    }
    career.addNews("El manager mantiene una charla individual con " + player.name + ".");
    return result;
}

ServiceResult cycleTrainingFocusService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    team.trainingFocus = nextTrainingFocus(team.trainingFocus);
    ensureTeamIdentity(team);

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Plan semanal actualizado: " + team.trainingFocus + ".");
    const vector<development::TrainingSessionPlan> schedule = development::buildWeeklyTrainingSchedule(team, false);
    for (size_t i = 0; i < schedule.size() && i < 3; ++i) {
        result.messages.push_back("- " + schedule[i].day + ": " + schedule[i].focus + " | " + schedule[i].note);
    }
    career.addNews("El cuerpo tecnico redefine el microciclo de " + team.name + " hacia un plan " + team.trainingFocus + ".");
    return result;
}

ServiceResult cycleMatchInstructionService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    team.matchInstruction = nextMatchInstruction(team.matchInstruction);
    ensureTeamIdentity(team);
    career.addNews("Nueva instruccion de partido en " + team.name + ": " + team.matchInstruction + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Instruccion de partido actual: " + team.matchInstruction + ".");
    return result;
}
