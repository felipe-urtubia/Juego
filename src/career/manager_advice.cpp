#include "career/board_objective.h"
#include "career/manager_advice.h"

#include "ai/ai_squad_planner.h"
#include "career/career_reports.h"
#include "career/career_support.h"
#include "career/dressing_room_service.h"
#include "career/staff_service.h"
#include "engine/rivalry_system.h"

#include <algorithm>

using namespace std;

namespace {

void pushUniqueLine(vector<string>& lines, const string& line) {
    if (line.empty()) return;
    if (find(lines.begin(), lines.end(), line) == lines.end()) {
        lines.push_back(line);
    }
}

int shortContractCount(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.contractWeeks <= 12) ++total;
    }
    return total;
}

int injuredCount(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.injured) ++total;
    }
    return total;
}

int heavyLoadCount(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.fitness < 62 || player.fatigueLoad >= 60) ++total;
    }
    return total;
}

int youthProjects(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.age <= 21 && player.potential >= player.skill + 8) ++total;
    }
    return total;
}

bool monthlyObjectiveUnderPressure(const Career& career) {
    if (career.boardMonthlyObjective.empty()) return false;
    return boardObjectiveUnderPressure(
        career.effectiveBoardMonthlyObjectiveType(),
        career.boardMonthlyProgress,
        career.boardMonthlyTarget);
}

string nextFixtureVenueLabel(const Career& career) {
    if (!career.myTeam || career.currentWeek <= 0 ||
        career.currentWeek > static_cast<int>(career.schedule.size())) {
        return "";
    }

    const vector<pair<int, int>>& matches = career.schedule[static_cast<size_t>(career.currentWeek - 1)];
    for (const auto& match : matches) {
        const Team* home = career.getActiveTeamAt(match.first);
        const Team* away = career.getActiveTeamAt(match.second);
        if (!home || !away) continue;
        if (home == career.myTeam) return "Local";
        if (away == career.myTeam) return "Visita";
    }
    return "";
}

string regionLabel(const Team& team) {
    return team.youthRegion.empty() ? "zona sin definir" : team.youthRegion;
}

string buildSuggestedBoardObjectiveInternal(const Career& career) {
    if (!career.myTeam) return "";
    const Team& team = *career.myTeam;
    const int rank = career.currentCompetitiveRank();
    const int fieldSize = max(1, career.currentCompetitiveFieldSize());
    const bool hasYouth = team.youthIdentity.find("Cantera") != string::npos;
    const bool lowBudget = team.budget < max(150000LL, team.sponsorWeekly * 3) || team.debt > team.sponsorWeekly * 10;
    const bool relegationRisk = rank > max(1, fieldSize * 3 / 4);
    const bool promotionPush = rank <= max(1, fieldSize / 4);
    const int injuries = injuredCount(team);
    const int philosophyScore = clubPhilosophyAlignmentScore(career, team);

    if (lowBudget) {
        return "Mantener presupuesto por sobre el 80% del presupuesto actual";
    }
    if (hasYouth && philosophyScore >= 40) {
        return "Dar 2 titularidades a sub-20 en 4 semanas";
    }
    if (injuries >= 3) {
        return "Rotar jugadores y cuidar cargas en 4 semanas";
    }
    if (relegationRisk) {
        return "Salir de la zona de riesgo de descenso en 4 semanas";
    }
    if (promotionPush) {
        return "Sumar al menos 6 puntos en 4 semanas";
    }
    return "Mejorar la posicion liguera antes de 4 semanas";
}

string buildSuggestedBoardObjectiveReasonInternal(const Career& career) {
    if (!career.myTeam) return "Sin razon disponible.";
    const Team& team = *career.myTeam;
    const int rank = career.currentCompetitiveRank();
    const int fieldSize = max(1, career.currentCompetitiveFieldSize());
    const bool hasYouth = team.youthIdentity.find("Cantera") != string::npos;
    const bool lowBudget = team.budget < max(150000LL, team.sponsorWeekly * 3) || team.debt > team.sponsorWeekly * 10;
    const bool relegationRisk = rank > max(1, fieldSize * 3 / 4);
    const bool promotionPush = rank <= max(1, fieldSize / 4);
    const int injuries = injuredCount(team);
    const int philosophyScore = clubPhilosophyAlignmentScore(career, team);

    if (lowBudget) {
        return "El club tiene caja ajustada o deuda alta; la prioridad es proteger las finanzas.";
    }
    if (hasYouth && philosophyScore >= 40) {
        return "La politica de cantera y la filosofia de club respaldan un objetivo enfocado en juveniles.";
    }
    if (injuries >= 3) {
        return "Hay varios lesionados; conviene cuidar cargas y evitar mas bajas.";
    }
    if (relegationRisk) {
        return "El equipo esta en zona de riesgo de descenso y necesita un objetivo defensivo.";
    }
    if (promotionPush) {
        return "El equipo esta en la pelea alta de la tabla, por lo que un objetivo agresivo es adecuado.";
    }
    if (!team.clubStyle.empty()) {
        return "El objetivo busca alinear la posicion liguera con el estilo actual del club.";
    }
    return "Objetivo general para mejorar el rendimiento competitivo en las proximas semanas.";
}

}  // namespace

namespace manager_advice {

string buildSuggestedBoardObjective(const Career& career) {
    return buildSuggestedBoardObjectiveInternal(career);
}

string buildSuggestedBoardObjectiveReason(const Career& career) {
    return buildSuggestedBoardObjectiveReasonInternal(career);
}

vector<string> buildManagerActionLines(const Career& career, size_t limit) {
    vector<string> lines;
    if (!career.myTeam) return lines;

    const Team& team = *career.myTeam;
    const DressingRoomSnapshot dressing = dressing_room_service::buildSnapshot(team, career.currentWeek);
    const SquadNeedReport planner = ai_squad_planner::analyzeSquad(team);
    const int shortContracts = shortContractCount(team);
    const int injured = injuredCount(team);
    const int heavyLoad = heavyLoadCount(team);
    const int philosophyScore = clubPhilosophyAlignmentScore(career, team);

    if (heavyLoad >= 4 || dressing.fatigueRiskCount >= 3) {
        pushUniqueLine(lines,
                       "Bajar la carga semanal: hay " + to_string(max(heavyLoad, dressing.fatigueRiskCount)) +
                           " jugadores llegando exigidos.");
    }
    if (dressing.lowMoraleCount >= 3 || dressing.conflictCount > 0 || team.morale < 48) {
        pushUniqueLine(lines,
                       "Gestionar vestuario: conviene reunion o charla individual antes de perder control emocional.");
    }
    if (shortContracts > 0) {
        pushUniqueLine(lines,
                       "Renovar pronto: " + to_string(shortContracts) +
                           " contrato(s) entran en la zona corta de 12 semanas.");
    }
    if (!planner.thinPositions.empty() || planner.rotationRisk >= 55) {
        const string need = planner.weakestPosition.empty() ? "MED" : planner.weakestPosition;
        pushUniqueLine(lines,
                       "Mercado activo: prioriza " + need +
                           " para bajar el riesgo de rotacion del plantel.");
    }
    if (dressing.promiseRiskCount > 0 || !career.activePromises.empty()) {
        pushUniqueLine(lines,
                       "Proteger promesas: hay compromisos deportivos que pueden tensionar el vestuario.");
    }
    if (career.boardConfidence <= 42 || monthlyObjectiveUnderPressure(career)) {
        pushUniqueLine(lines,
                       "Directiva en alerta: toca sumar progreso visible en el objetivo mensual o en la tabla.");
    }
    if (team.youthIdentity.find("Cantera") != string::npos && career.effectiveBoardMonthlyObjectiveType() == BoardObjectiveType::YouthStarts) {
        pushUniqueLine(lines,
                       "Activar proyecto juvenil: comienza a dar minutos a los sub-20 para cumplir el objetivo mensual.");
    }
    if (career.scoutingAssignments.empty() && team.scoutingChief >= 50) {
        pushUniqueLine(lines,
                       "Refuerza scouting: asigna cobertura a regiones clave y cierra la diferencia entre informe y necesidad actual.");
    }
    if (!team.clubStyle.empty() && team.matchInstruction.empty()) {
        pushUniqueLine(lines,
                       "Completa tu identidad: define una instruccion de partido que acompañe el estilo del club.");
    }
    if (philosophyScore < 45) {
        pushUniqueLine(lines,
                       "Revisar coherencia de club: estilo, cantera y objetivo mensual no estan alineados.");
    } else if (philosophyScore >= 78) {
        pushUniqueLine(lines,
                       "La filosofia del club, los objetivos y el plan tactico muestran buena coherencia.");
    }
    if (team.debt > team.sponsorWeekly * 10 || team.budget < max(150000LL, team.sponsorWeekly * 3)) {
        pushUniqueLine(lines,
                       "Caja comprometida: evita paquetes salariales altos y considera una venta de equilibrio.");
    }
    if (injured >= 3) {
        pushUniqueLine(lines,
                       "Parte medico exigente: prepara rotacion porque hay " + to_string(injured) + " lesionado(s).");
    }
    const auto staffAlerts = staff_service::buildStaffRecommendations(career, 2);
    for (const auto& alert : staffAlerts) {
        if (alert.urgency < 55) continue;
        pushUniqueLine(lines,
                       alert.staffRole + ": " + alert.suggestedAction);
    }
    if (lines.empty()) {
        pushUniqueLine(lines, "Semana estable: puedes empujar desarrollo, scouting y preparacion del proximo rival.");
    }
    if (lines.size() > limit) lines.resize(limit);
    return lines;
}

vector<string> buildCareerStorylines(const Career& career, size_t limit) {
    vector<string> lines;
    if (!career.myTeam) return lines;

    const Team& team = *career.myTeam;
    const Team* opponent = nextOpponent(career);
    const int rank = career.currentCompetitiveRank();
    const int fieldSize = max(1, career.currentCompetitiveFieldSize());
    const int youthCount = youthProjects(team);

    if (opponent) {
        const int rivalryIntensity = getRivalryIntensity(career.rivalryDynamics, team.name, opponent->name);
        const bool rivalryWeek = areRivalClubs(team, *opponent) || rivalryIntensity >= 65;
        const string venue = nextFixtureVenueLabel(career);
        if (rivalryWeek) {
            pushUniqueLine(lines,
                           "Semana de clasico ante " + opponent->name +
                               ": prensa, hinchas y vestuario van a medir cada decision.");
        }
        if (venue == "Visita" && !team.youthRegion.empty() && !opponent->youthRegion.empty() &&
            team.youthRegion != opponent->youthRegion) {
            pushUniqueLine(lines,
                           "Semana de viaje: de " + regionLabel(team) + " a " + regionLabel(*opponent) +
                               " para visitar a " + opponent->name + "; conviene cuidar cargas y ritmo.");
        } else if (venue == "Visita") {
            pushUniqueLine(lines,
                           "Salida de visita ante " + opponent->name +
                               ": el primer tramo puede marcar si el partido se juega a tu plan o al suyo.");
        } else if (venue == "Local" && team.fanBase >= opponent->fanBase) {
            pushUniqueLine(lines,
                           "Semana de localia fuerte: la hinchada de " + team.name +
                               " puede empujar, pero tambien exige una postura protagonista.");
        }
        if (career.currentWeek >= 4 && abs(team.points - opponent->points) <= 3) {
            pushUniqueLine(lines,
                           "Partido bisagra en la tabla: " + team.name + " y " + opponent->name +
                               " llegan separados por " + to_string(abs(team.points - opponent->points)) + " punto(s).");
        }
    }
    if (rank > 0 && rank <= max(2, fieldSize / 4)) {
        pushUniqueLine(lines, team.name + " esta en la pelea alta y cada punto empieza a pesar mas.");
    } else if (rank > fieldSize - max(2, fieldSize / 4)) {
        pushUniqueLine(lines, team.name + " entra en zona de presion competitiva y el margen de error se achica.");
    }
    if (career.boardConfidence <= 35) {
        pushUniqueLine(lines, "La directiva entra en zona critica: el proyecto necesita una reaccion inmediata.");
    } else if (career.boardConfidence >= 70) {
        pushUniqueLine(lines, "La directiva respalda el proceso y te da aire para pensar mas alla de una sola fecha.");
    }
    if (career.managerReputation >= 75) {
        pushUniqueLine(lines, "Tu reputacion ya empieza a mover mercado, vestuario y percepcion externa del club.");
    } else if (career.managerReputation <= 45) {
        pushUniqueLine(lines, "El margen del DT aun se construye: cada semana suma o resta credito.");
    }
    if (youthCount >= 3) {
        pushUniqueLine(lines,
                       "La identidad de " + team.name + " puede apoyarse en cantera: hay " +
                           to_string(youthCount) + " proyecto(s) juveniles listos para empujar.");
    }
    if (!team.clubStyle.empty()) {
        pushUniqueLine(lines, "La identidad de club hoy pasa por " + team.clubStyle + ".");
    }
    if (!team.youthIdentity.empty()) {
        pushUniqueLine(lines, "La politica formativa del club se lee como " + team.youthIdentity + ".");
    }
    if (!team.clubStyle.empty() && !team.matchInstruction.empty()) {
        bool mismatch =
            (team.clubStyle == "Control de posesion" && team.matchInstruction == "Juego directo") ||
            (team.clubStyle == "Presion vertical" && team.matchInstruction == "Bloque bajo") ||
            (team.clubStyle == "Ataque por bandas" && team.matchInstruction == "Pausar juego");
        if (mismatch) {
            pushUniqueLine(lines,
                           "Alerta de coherencia: la instruccion actual no refleja el estilo de club.");
        } else {
            pushUniqueLine(lines,
                           "Coherencia tactica: la instruccion actual acompana el sello del club.");
        }
    }
    if (!career.history.empty()) {
        const SeasonHistoryEntry& last = career.history.back();
        pushUniqueLine(lines,
                       "Memoria reciente: el club llega desde T" + to_string(last.season) +
                           " con cierre " + to_string(last.finish) + " y nota \"" + last.note + "\".");
    }
    if (!career.activePromises.empty()) {
        pushUniqueLine(lines, "El vestuario sigue midiendo si las promesas del manager se cumplen o se rompen.");
    }
    if (!career.scoutingAssignments.empty()) {
        pushUniqueLine(lines,
                       "La red de scouting trabaja en paralelo y puede abrir una oportunidad de mercado en las proximas semanas.");
    }
    if (!career.lastMatchCenter.opponentName.empty()) {
        pushUniqueLine(lines,
                       "El ultimo " + career.lastMatchCenter.competitionLabel +
                           " dejo una lectura tactica que puede marcar la semana.");
    }
    if (lines.empty()) {
        pushUniqueLine(lines, "La temporada sigue abierta: no hay una narrativa dominante todavia.");
    }
    if (lines.size() > limit) lines.resize(limit);
    return lines;
}

string buildTransferCompetitionLabel(const Career& career,
                                     const Team& seller,
                                     const Player& player,
                                     const TransferTarget& target) {
    if (!career.myTeam) return "Mercado indefinido";
    int pressure = target.competitionScore;
    if (player.contractWeeks <= 14) pressure += 8;
    if (player.wantsToLeave) pressure -= 4;
    if (teamPrestigeScore(seller) > teamPrestigeScore(*career.myTeam)) pressure += 5;
    if (target.scoutingConfidence < 55) pressure += 3;
    if (pressure >= 30) return "Competencia alta";
    if (pressure >= 18) return "Mercado movido";
    return "Ventana manejable";
}

string buildTransferActionLabel(const Career& career,
                                const Team& seller,
                                const Player& player,
                                const TransferTarget& target) {
    (void)career;
    (void)seller;
    if (target.urgentNeed && target.affordabilityScore >= 18) return "Mover ahora";
    if (target.contractRunningOut && target.affordabilityScore >= 10) return "Entrar antes del cierre";
    if (target.availableForLoan && target.affordabilityScore < 12) return "Explorar cesion";
    if (target.scoutingConfidence < 60) return "Pedir mas scouting";
    if (player.wantsToLeave && target.affordabilityScore >= 10) return "Presionar negociacion";
    if (target.affordabilityScore < 6) return "Seguir sin ofertar";
    return "Preparar oferta";
}

string buildTransferPackageLabel(const TransferTarget& target) {
    const long long upfrontPackage = target.expectedFee + target.expectedAgentFee;
    return "Entrada " + formatMoneyValue(upfrontPackage) +
           " | salario " + formatMoneyValue(target.expectedWage);
}

bool adoptSuggestedBoardObjective(Career& career) {
    if (!career.myTeam) return false;

    const string objective = buildSuggestedBoardObjective(career);
    if (objective.empty()) return false;

    career.boardMonthlyObjective = objective;
    career.boardMonthlyObjectiveType = boardObjectiveTypeFromText(objective);
    career.boardMonthlyDeadlineWeek =
        min(static_cast<int>(career.schedule.size()), career.currentWeek + 4);
    career.boardMonthlyProgress = 0;
    career.boardMonthlyTarget = 0;

    switch (boardObjectiveTypeFromText(objective)) {
        case BoardObjectiveType::YouthStarts:
            career.boardMonthlyTarget = 2;
            break;

        case BoardObjectiveType::Budget:
            career.boardMonthlyTarget = static_cast<int>(
                min<long long>(
                    max(100000LL, career.myTeam->budget * 80 / 100),
                    2000000000LL));
            career.boardMonthlyProgress = static_cast<int>(
                min<long long>(career.myTeam->budget, 2000000000LL));
            break;

        case BoardObjectiveType::RelegationRisk:
            career.boardMonthlyTarget =
                max(1, career.currentCompetitiveFieldSize() * 3 / 4);
            career.boardMonthlyProgress = career.currentCompetitiveRank();
            break;

        case BoardObjectiveType::Points:
            career.boardMonthlyTarget = 6;
            break;

        case BoardObjectiveType::Position:
            career.boardMonthlyTarget =
                max(1, career.currentCompetitiveRank() - 1);
            career.boardMonthlyProgress = career.currentCompetitiveRank();
            break;

        case BoardObjectiveType::Recovery:
        case BoardObjectiveType::Unknown:
            career.boardMonthlyTarget = 1;
            break;
    }

    career.addNews(
        "Directiva: objetivo mensual actualizado a \"" +
        career.boardMonthlyObjective + "\".");

    return true;
}
}  // namespace manager_advice
