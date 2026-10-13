#include "career/weekly_focus_service.h"

#include "ai/ai_squad_planner.h"
#include "career/career_support.h"
#include "career/career_reports.h"
#include "career/dressing_room_service.h"
#include "career/game_events_system.h"
#include "career/match_center_service.h"
#include "finance/finance_system.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

struct RankedFocus {
    int urgency = 0;
    string bucket;
    string summary;
    string action;
};

void pushUniqueLine(vector<string>& lines, const string& line) {
    if (line.empty()) return;
    if (find(lines.begin(), lines.end(), line) == lines.end()) {
        lines.push_back(line);
    }
}

void pushFocus(vector<RankedFocus>& items,
               int urgency,
               const string& bucket,
               const string& summary,
               const string& action) {
    if (bucket.empty() || summary.empty() || action.empty()) return;
    auto existing = find_if(items.begin(), items.end(), [&](const RankedFocus& item) {
        return item.bucket == bucket;
    });
    if (existing != items.end()) {
        if (urgency > existing->urgency) {
            existing->urgency = urgency;
            existing->summary = summary;
            existing->action = action;
        }
        return;
    }
    items.push_back({urgency, bucket, summary, action});
}

bool monthlyObjectiveUnderPressure(const Career& career) {
    if (career.boardMonthlyObjective.empty() || career.boardMonthlyTarget <= 0) return false;
    const string objective = toLower(career.boardMonthlyObjective);
    if (objective.find("posicion") != string::npos ||
        objective.find("puesto") != string::npos ||
        objective.find("top") != string::npos ||
        objective.find("descender") != string::npos ||
        objective.find("zona de riesgo de descenso") != string::npos) {
        return career.boardMonthlyProgress > 0 &&
               career.boardMonthlyProgress > career.boardMonthlyTarget;
    }
    return career.boardMonthlyProgress < career.boardMonthlyTarget;
}

int countHeavyLoad(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.fitness < 62 || player.fatigueLoad >= 60) ++total;
    }
    return total;
}

int countInjured(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.injured) ++total;
    }
    return total;
}

int countSuspended(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.matchesSuspended > 0) ++total;
    }
    return total;
}

int countExpiringContracts(const Team& team, int maxWeeks) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.contractWeeks > 0 && player.contractWeeks <= maxWeeks) ++total;
    }
    return total;
}

int countYouthMinutes(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.age <= 20 && player.matchesPlayed > 0) ++total;
    }
    return total;
}

int countYouthProjects(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.age <= 21 && player.potential >= player.skill + 8) ++total;
    }
    return total;
}

string formatFocusLine(const RankedFocus& item) {
    return item.bucket + " | " + item.summary + " | Accion: " + item.action;
}

string formatKpiLine(const string& label, const string& value) {
    return label + " | " + value;
}

}  // namespace

namespace weekly_focus_service {

WeeklyFocusSnapshot buildWeeklyFocusSnapshot(const Career& career,
                                             size_t priorityLimit,
                                             size_t kpiLimit,
                                             size_t tutorialLimit) {
    WeeklyFocusSnapshot snapshot;
    if (!career.myTeam) {
        snapshot.headline = "Cockpit semanal inactivo: no hay carrera cargada.";
        snapshot.priorityLines.push_back("Carga o crea una carrera para generar prioridades y ayudas contextuales.");
        return snapshot;
    }

    const Team& team = *career.myTeam;
    const DressingRoomSnapshot dressing = dressing_room_service::buildSnapshot(team, career.currentWeek);
    const SquadNeedReport planner = ai_squad_planner::analyzeSquad(team);
    const WeeklyFinanceReport finance = finance_system::projectWeeklyReport(team);
    const MatchCenterView matchCenter = match_center_service::buildLastMatchCenter(career, 2, 3);
    const Team* opponent = nextOpponent(career);

    const int heavyLoad = countHeavyLoad(team);
    const int injured = countInjured(team);
    const int suspended = countSuspended(team);
    const int expiring = countExpiringContracts(team, 8);
    const int youthMinutes = countYouthMinutes(team);
    const int youthProjects = countYouthProjects(team);
    const bool boardUnderPressure = career.boardConfidence <= 42 || monthlyObjectiveUnderPressure(career);
    const bool financesTight = finance.netCashFlow < 0 ||
                               team.debt > team.sponsorWeekly * 10 ||
                               team.budget < max(150000LL, team.sponsorWeekly * 3);

    vector<RankedFocus> priorities;
    if (boardUnderPressure) {
        string summary = "Confianza " + to_string(career.boardConfidence) + "/100";
        if (!career.boardMonthlyObjective.empty()) {
            summary += " y objetivo mensual en riesgo (" +
                       to_string(career.boardMonthlyProgress) + "/" +
                       to_string(career.boardMonthlyTarget) + ")";
        }
        pushFocus(priorities,
                  95,
                  "Directiva",
                  summary,
                  "Sumar progreso visible en tabla o resolver una urgencia del centro del manager.");
    }
    if (heavyLoad >= 4 || dressing.fatigueRiskCount >= 3 || injured >= 3) {
        pushFocus(priorities,
                  90,
                  "Fisico",
                  to_string(max(heavyLoad, dressing.fatigueRiskCount)) +
                      " jugadores llegan exigidos; lesionados " + to_string(injured),
                  "Baja la carga, rota y prioriza recuperacion antes de volver a simular.");
    }
    if (dressing.lowMoraleCount >= 3 || dressing.conflictCount > 0 ||
        dressing.promiseRiskCount > 0 || team.morale < 48) {
        pushFocus(priorities,
                  87,
                  "Vestuario",
                  "Moral " + to_string(team.morale) +
                      " | tension " + to_string(dressing.socialTension) +
                      " | promesas en riesgo " + to_string(dressing.promiseRiskCount),
                  "Agenda reunion o charla individual antes de que el clima arrastre rendimiento.");
    }
    if (expiring > 0) {
        pushFocus(priorities,
                  82,
                  "Contratos",
                  to_string(expiring) + " contrato(s) entran en zona corta de 8 semanas",
                  "Renueva o prepara salida antes de perder control de la negociacion.");
    }
    if (financesTight) {
        pushFocus(priorities,
                  80,
                  "Caja",
                  finance.riskLevel + " | flujo " + formatMoneyValue(finance.netCashFlow) +
                      " | deuda " + formatMoneyValue(team.debt),
                  "Evita salarios altos y estudia una venta o cesion de equilibrio.");
    }
    if (!planner.thinPositions.empty() || planner.rotationRisk >= 55) {
        const string need = planner.weakestPosition.empty() ? "MED" : planner.weakestPosition;
        pushFocus(priorities,
                  77,
                  "Mercado",
                  "Necesidad " + need +
                      " | riesgo de rotacion " + to_string(planner.rotationRisk),
                  career.scoutingAssignments.empty()
                      ? "Activa scouting corto y arma shortlist antes de ofertar."
                      : "Prioriza " + need + " y compara paquetes antes de entrar a negociar.");
    } else if (career.scoutingAssignments.empty() && career.scoutingShortlist.empty()) {
        pushFocus(priorities,
                  68,
                  "Mercado",
                  "No hay scouting ni shortlist activos para sostener decisiones de fichajes",
                  "Abre una asignacion corta para no entrar al mercado a ciegas.");
    }
    if (matchCenter.available && !matchCenter.recommendationLines.empty()) {
        pushFocus(priorities,
                  74,
                  "Partido",
                  matchCenter.recommendationLines.front(),
                  "Conecta ese ajuste con tactica o plan semanal antes del proximo rival.");
    }
    if (youthProjects >= 2 && career.boardYouthTarget > 0 && youthMinutes < career.boardYouthTarget) {
        pushFocus(priorities,
                  70,
                  "Cantera",
                  "Juveniles con minutos " + to_string(youthMinutes) +
                      "/" + to_string(career.boardYouthTarget) +
                      " | proyectos listos " + to_string(youthProjects),
                  "Da minutos controlados a un juvenil y acompanalo con un plan individual.");
    }

    sort(priorities.begin(), priorities.end(), [](const RankedFocus& a, const RankedFocus& b) {
        if (a.urgency != b.urgency) return a.urgency > b.urgency;
        return a.bucket < b.bucket;
    });

    if (!priorities.empty()) {
        const string rivalLabel = opponent ? (" antes de " + opponent->name) : ".";
        snapshot.headline = "Cockpit semanal: " + priorities.front().bucket + " al frente" + rivalLabel;
    } else {
        snapshot.headline =
            "Cockpit semanal: panorama estable para empujar desarrollo, scouting y preparacion del proximo partido.";
    }

    const size_t priorityCount = min(priorityLimit, priorities.size());
    for (size_t i = 0; i < priorityCount; ++i) {
        snapshot.priorityLines.push_back(formatFocusLine(priorities[i]));
    }
    if (snapshot.priorityLines.empty()) {
        snapshot.priorityLines.push_back(
            "Semana estable | No aparece una urgencia dominante | Accion: aprovechar para crecer en scouting, cantera o estructura.");
    }

    pushUniqueLine(snapshot.kpiLines,
                   formatKpiLine("Estado fisico",
                                 to_string(heavyLoad) + " exigidos | " +
                                     to_string(injured) + " lesionados | " +
                                     to_string(suspended) + " suspendidos"));
    pushUniqueLine(snapshot.kpiLines,
                   formatKpiLine("Vestuario",
                                 "moral " + to_string(team.morale) +
                                     " | tension " + to_string(dressing.socialTension) +
                                     " | apoyo " + to_string(dressing.leadershipSupport)));
    pushUniqueLine(snapshot.kpiLines,
                   formatKpiLine("Caja",
                                 formatMoneyValue(finance.netCashFlow) +
                                     " flujo | deuda " + formatMoneyValue(team.debt) +
                                     " | buffer " + formatMoneyValue(finance.transferBuffer)));
    pushUniqueLine(snapshot.kpiLines,
                   formatKpiLine("Plantilla",
                                 "necesidad " + (planner.weakestPosition.empty() ? string("MED") : planner.weakestPosition) +
                                     " | rotacion " + to_string(planner.rotationRisk) +
                                     " | contratos cortos " + to_string(expiring)));
    if (!career.boardMonthlyObjective.empty()) {
        pushUniqueLine(snapshot.kpiLines,
                       formatKpiLine("Objetivo mensual",
                                     career.boardMonthlyObjective + " | " +
                                         to_string(career.boardMonthlyProgress) + "/" +
                                         to_string(career.boardMonthlyTarget)));
    }
    if (opponent) {
        pushUniqueLine(snapshot.kpiLines,
                       formatKpiLine("Proximo rival",
                                     opponent->name + " | " + buildOpponentReport(career)));
    }
    const int unreadEvents = career_events::EventNotificationSystem::getUnreadCount();
    if (unreadEvents > 0) {
        pushUniqueLine(snapshot.kpiLines,
                       formatKpiLine("Eventos", to_string(unreadEvents) + " alerta(s) sin revisar"));
    }
    if (snapshot.kpiLines.size() > kpiLimit) snapshot.kpiLines.resize(kpiLimit);

    if (career.currentSeason == 1 && career.currentWeek <= 2) {
        pushUniqueLine(snapshot.tutorialLines,
                       "Piensa la semana como un ciclo corto: una decision deportiva, una fisica/vestuario y una estructural.");
    }
    if (career.scoutingAssignments.empty()) {
        pushUniqueLine(snapshot.tutorialLines,
                       "Sin scouting activo el mercado se vuelve mas caro y menos fiable; una asignacion corta ya baja mucho el riesgo.");
    }
    if (matchCenter.available) {
        pushUniqueLine(snapshot.tutorialLines,
                       "El ultimo partido ya deja ajustes concretos: aplicalos en tactica o entrenamiento antes de simular de nuevo.");
    }
    if (expiring > 0) {
        pushUniqueLine(snapshot.tutorialLines,
                       "Los contratos cortos se endurecen rapido; renovar antes de las 8 semanas suele darte mas control.");
    }
    if (heavyLoad >= 3 || injured > 0) {
        pushUniqueLine(snapshot.tutorialLines,
                       "Cuando sube la fatiga, recuperacion y rotacion suelen rendir mas que insistir con entrenamiento duro.");
    }
    if (career.boardYouthTarget > 0 && youthProjects > 0 && youthMinutes < career.boardYouthTarget) {
        pushUniqueLine(snapshot.tutorialLines,
                       "Dar minutos controlados a juveniles suma al objetivo de directiva y acelera el desarrollo del proyecto.");
    }
    if (financesTight) {
        pushUniqueLine(snapshot.tutorialLines,
                       "Con caja corta conviene priorizar cesiones, ventas oportunas y paquetes salariales sostenibles.");
    }
    if (!career.activePromises.empty()) {
        pushUniqueLine(snapshot.tutorialLines,
                       "Las promesas incumplidas no solo bajan moral: tambien erosionan el clima social y la estabilidad del proyecto.");
    }
    if (snapshot.tutorialLines.empty()) {
        snapshot.tutorialLines.push_back(
            "Semana limpia: puedes usar este margen para crecer en scouting, cantera y preparacion del siguiente rival.");
    }
    if (snapshot.tutorialLines.size() > tutorialLimit) snapshot.tutorialLines.resize(tutorialLimit);

    return snapshot;
}

vector<string> buildPriorityLines(const Career& career, size_t limit) {
    return buildWeeklyFocusSnapshot(career, limit, 0, 0).priorityLines;
}

vector<string> buildTutorialLines(const Career& career, size_t limit) {
    return buildWeeklyFocusSnapshot(career, 0, 0, limit).tutorialLines;
}

}  // namespace weekly_focus_service
