#include "ui/career_reports_ui.h"

#include "career/app_services.h"
#include "career/career_reports.h"
#include "career/career_support.h"
#include "career/inbox_service.h"
#include "career/manager_advice.h"
#include "career/match_center_service.h"
#include "career/transfer_briefing.h"
#include "career/weekly_focus_service.h"
#include "utils/utils.h"

#include <algorithm>
#include <iostream>
#include <vector>

using namespace std;

namespace {

void printMessages(const ServiceResult& result) {
    for (const auto& message : result.messages) {
        cout << message << endl;
    }
}

void printReport(const CareerReport& report) {
    cout << "\n" << formatCareerReport(report);
}

void printSection(const string& title, const vector<string>& lines) {
    if (lines.empty()) return;
    cout << "\n" << title << ":" << endl;
    for (const auto& line : lines) {
        cout << "- " << line << endl;
    }
}

void setBoardObjective(Career& career) {
    if (!career.myTeam) return;
    cout << "\nObjetivo mensual actual: " << (career.boardMonthlyObjective.empty() ? "Sin objetivo definido" : career.boardMonthlyObjective) << endl;
    cout << "1. Sumar al menos 6 puntos en 4 semanas" << endl;
    cout << "2. Dar 2 titularidades a sub-20 en 4 semanas" << endl;
    cout << "3. Mejorar la posicion liguera antes de 4 semanas" << endl;
    cout << "4. Mantener presupuesto por sobre el 80% del presupuesto actual" << endl;
    cout << "5. Limpiar objetivo" << endl;
    cout << "6. Volver" << endl;
    int choice = readInt("Elige objetivo: ", 1, 6);
    if (choice == 6) return;

    career.boardMonthlyDeadlineWeek = min(static_cast<int>(career.schedule.size()), career.currentWeek + 4);
    career.boardMonthlyProgress = 0;
    career.boardMonthlyTarget = 0;

    if (choice == 1) {
        career.boardMonthlyTarget = 6;
        career.boardMonthlyObjective = "Sumar al menos 6 puntos en 4 semanas";
    } else if (choice == 2) {
        career.boardMonthlyTarget = 2;
        career.boardMonthlyObjective = "Dar 2 titularidades a sub-20 en 4 semanas";
    } else if (choice == 3) {
        int rank = max(1, career.currentCompetitiveRank());
        career.boardMonthlyTarget = max(1, rank - 1);
        career.boardMonthlyProgress = rank;
        career.boardMonthlyObjective = "Mejorar la posicion liguera antes de 4 semanas";
    } else if (choice == 4) {
        long long target = max(100000LL, career.myTeam->budget * 80 / 100);
        career.boardMonthlyTarget = static_cast<int>(min<long long>(target, 2000000000LL));
        career.boardMonthlyProgress = static_cast<int>(min<long long>(career.myTeam->budget, 2000000000LL));
        career.boardMonthlyObjective = "Mantener presupuesto por sobre $" + to_string(career.boardMonthlyTarget) + " en 4 semanas";
    } else if (choice == 5) {
        career.boardMonthlyObjective.clear();
        career.boardMonthlyObjectiveType = BoardObjectiveType::Unknown;
        career.boardMonthlyTarget = 0;
        career.boardMonthlyProgress = 0;
        career.boardMonthlyDeadlineWeek = 0;
        cout << "Objetivo mensual borrado." << endl;
        return;
    }

    career.boardMonthlyObjectiveType = boardObjectiveTypeFromText(career.boardMonthlyObjective);

    cout << "Nuevo objetivo: " << career.boardMonthlyObjective << endl;
    cout << "Fecha limite semana: " << career.boardMonthlyDeadlineWeek << endl;
}

void printActiveScoutingAssignments(const Career& career) {
    if (career.scoutingAssignments.empty()) {
        cout << "No hay asignaciones activas." << endl;
        return;
    }
    cout << "Asignaciones activas:" << endl;
    for (const auto& assignment : career.scoutingAssignments) {
        cout << "- " << assignment.region
             << " | foco " << assignment.focusPosition
             << " | prioridad " << assignment.priority
             << " | conocimiento " << assignment.knowledgeLevel << "%"
             << " | resta " << assignment.weeksRemaining << " sem" << endl;
    }
}

void editClubPhilosophy(Career& career) {
    if (!career.myTeam) return;
    Team& team = *career.myTeam;

    cout << "\n=== Editor de filosofia de club ===" << endl;
    cout << "1. Cambiar identidad de club" << endl;
    cout << "2. Cambiar politica juvenil" << endl;
    cout << "3. Cambiar politica de mercado" << endl;
    cout << "4. Volver" << endl;
    int choice = readInt("Elige una opcion: ", 1, 4);
    if (choice == 4) return;

    if (choice == 1) {
        cout << "Identidad de club actual: " << (team.clubStyle.empty() ? "No definida" : team.clubStyle) << endl;
        cout << "1. Control de posesion" << endl;
        cout << "2. Presion vertical" << endl;
        cout << "3. Ataque por bandas" << endl;
        cout << "4. Bloque ordenado" << endl;
        cout << "5. Transicion directa" << endl;
        cout << "6. Sin estilo concreto" << endl;
        int identityChoice = readInt("Elige identidad: ", 1, 6);
        switch (identityChoice) {
            case 1: team.clubStyle = "Control de posesion"; break;
            case 2: team.clubStyle = "Presion vertical"; break;
            case 3: team.clubStyle = "Ataque por bandas"; break;
            case 4: team.clubStyle = "Bloque ordenado"; break;
            case 5: team.clubStyle = "Transicion directa"; break;
            default: team.clubStyle.clear(); break;
        }
        cout << "Identidad de club actualizada a " << (team.clubStyle.empty() ? "sin estilo definido" : team.clubStyle) << endl;
        return;
    }

    if (choice == 2) {
        cout << "Politica juvenil actual: " << (team.youthIdentity.empty() ? "No definida" : team.youthIdentity) << endl;
        cout << "1. Cantera estructurada" << endl;
        cout << "2. Desarrollo mixto" << endl;
        cout << "3. Talento local" << endl;
        cout << "4. Sin politica especifica" << endl;
        int youthChoice = readInt("Elige politica juvenil: ", 1, 4);
        switch (youthChoice) {
            case 1: team.youthIdentity = "Cantera estructurada"; break;
            case 2: team.youthIdentity = "Desarrollo mixto"; break;
            case 3: team.youthIdentity = "Talento local"; break;
            default: team.youthIdentity.clear(); break;
        }
        cout << "Politica juvenil actualizada a " << (team.youthIdentity.empty() ? "sin politica definida" : team.youthIdentity) << endl;
        return;
    }

    if (choice == 3) {
        cout << "Politica de mercado actual: " << (team.transferPolicy.empty() ? "Mixta" : team.transferPolicy) << endl;
        cout << "1. Cantera y valor futuro" << endl;
        cout << "2. Vender antes de comprar" << endl;
        cout << "3. Mixta" << endl;
        cout << "4. Sin politica concreta" << endl;
        int marketChoice = readInt("Elige politica de mercado: ", 1, 4);
        switch (marketChoice) {
            case 1: team.transferPolicy = "Cantera y valor futuro"; break;
            case 2: team.transferPolicy = "Vender antes de comprar"; break;
            case 3: team.transferPolicy = "Mixta"; break;
            default: team.transferPolicy.clear(); break;
        }
        cout << "Politica de mercado actualizada a " << (team.transferPolicy.empty() ? "sin politica definida" : team.transferPolicy) << endl;
        return;
    }
}

void applySuggestedBoardObjective(Career& career) {
    if (!career.myTeam) return;

    if (!manager_advice::adoptSuggestedBoardObjective(career)) {
        cout << "No hay sugerencia disponible." << endl;
        return;
    }

    cout << "Objetivo sugerido aplicado: "
         << career.boardMonthlyObjective << endl;
}

vector<string> latestMatchDigest(const Career& career) {
    vector<string> lines;
    const MatchCenterView center = match_center_service::buildLastMatchCenter(career, 2, 3);
    if (!center.available) return lines;

    if (!center.scoreboard.empty()) lines.push_back(center.scoreboard);
    if (!center.tacticalSummary.empty()) lines.push_back(center.tacticalSummary);
    if (!center.fatigueSummary.empty()) lines.push_back(center.fatigueSummary);
    if (!center.playerOfTheMatch.empty()) lines.push_back("Figura: " + center.playerOfTheMatch);
    return lines;
}

}  // namespace

namespace ui_reports {

void displayCompetitionCenter(Career& career) {
    if (!career.myTeam) return;
    printReport(buildCompetitionReport(career));
}

void displayBoardStatus(Career& career) {
    if (!career.myTeam) return;

    printReport(buildBoardReport(career));
    cout << "\n1. Buscar ofertas de club" << endl;
    cout << "2. Cambiar objetivo mensual" << endl;
    cout << "3. Volver" << endl;
    int action = readInt("Elige opcion: ", 1, 3);
    if (action == 3) return;
    if (action == 2) {
        setBoardObjective(career);
        return;
    }

    vector<Team*> jobs = buildJobMarket(career, false);
    if (jobs.empty()) {
        cout << "No hay ofertas adecuadas por ahora." << endl;
        return;
    }

    cout << "\nOfertas disponibles:" << endl;
    for (size_t i = 0; i < jobs.size(); ++i) {
        cout << i + 1 << ". " << jobs[i]->name << " (" << divisionDisplay(jobs[i]->division) << ")"
             << " | Valor plantel " << formatMoneyValue(jobs[i]->getSquadValue()) << endl;
    }
    int choice = readInt("Club (0 para cancelar): ", 0, static_cast<int>(jobs.size()));
    if (choice == 0) return;

    ServiceResult result = takeManagerJobService(career, jobs[static_cast<size_t>(choice - 1)]->name);
    printMessages(result);
}

void displayNewsFeed(Career& career) {
    cout << "\n=== Centro del Manager ===" << endl;
    const weekly_focus_service::WeeklyFocusSnapshot weeklyFocus =
        weekly_focus_service::buildWeeklyFocusSnapshot(career, 3, 4, 3);

    cout << weeklyFocus.headline << endl;

    const auto inboxEntries = inbox_service::buildCombinedInbox(career, 10);
    if (inboxEntries.empty()) {
        cout << "Inbox sin decisiones urgentes." << endl;
    } else {
        cout << "Inbox activo:" << endl;
        for (const auto& entry : inboxEntries) {
            cout << "- [" << entry.channel << "] " << entry.text << endl;
        }
    }

    cout << "\nNoticias recientes:" << endl;
    if (career.newsFeed.empty()) {
        cout << "- No hay novedades registradas." << endl;
    } else {
        int start = max(0, static_cast<int>(career.newsFeed.size()) - 10);
        for (size_t i = static_cast<size_t>(start); i < career.newsFeed.size(); ++i) {
            cout << "- " << career.newsFeed[i] << endl;
        }
    }

    printSection("Cockpit semanal", weeklyFocus.priorityLines);
    printSection("Plan de proximo rival", buildNextOpponentPlanLines(career, 5));
    printSection("Decisiones semanales", buildWeeklyDecisionOptions(career));
    printSection("KPIs accionables", weeklyFocus.kpiLines);
    printSection("Acciones sugeridas", manager_advice::buildManagerActionLines(career, 4));
    printSection("Ayuda contextual", weeklyFocus.tutorialLines);
    printSection("Narrativa de la semana", manager_advice::buildCareerStorylines(career, 3));
    printSection("Pulso de mercado", transfer_briefing::buildMarketPulseLines(career, 3));
    printSection("Lectura del ultimo partido", latestMatchDigest(career));
    const MatchCenterView center = match_center_service::buildLastMatchCenter(career, 2, 3);
    printSection("Ajustes inmediatos", center.recommendationLines);

    cout << "\n";
    printActiveScoutingAssignments(career);

    cout << "\n1. Aplicar decision semanal" << endl;
    cout << "2. Resolver decision prioritaria" << endl;
    cout << "3. Crear asignacion de scouting" << endl;
    cout << "4. Volver" << endl;
    int action = readInt("Elige opcion: ", 1, 4);
    if (action == 4) return;

    if (action == 1) {
        const vector<pair<string, WeeklyDecision>> decisions = {
            {"Auto: que el staff elija", WeeklyDecision::Auto},
            {"Recuperar plantel", WeeklyDecision::Recovery},
            {"Entrenar fuerte", WeeklyDecision::HighIntensityTraining},
            {"Ordenar vestuario", WeeklyDecision::DressingRoom},
            {"Preparar rival", WeeklyDecision::MatchPreparation},
            {"Control financiero", WeeklyDecision::FinancialControl},
            {"Impulsar juveniles", WeeklyDecision::YouthPathway},
            {"Descanso del manager", WeeklyDecision::ManagerRest},
        };
        cout << "\nDecisiones disponibles:" << endl;
        for (size_t i = 0; i < decisions.size(); ++i) {
            cout << i + 1 << ". " << decisions[i].first << endl;
        }
        int decisionChoice = readInt("Decision: ", 1, static_cast<int>(decisions.size()));
        printMessages(applyWeeklyDecisionService(career, decisions[static_cast<size_t>(decisionChoice - 1)].second));
        return;
    }

    if (action == 2) {
        printMessages(resolveInboxDecisionService(career));
        return;
    }

    vector<string> regions = {"Metropolitana", "Norte", "Centro", "Sur", "Patagonia", "Todas"};
    cout << "Regiones:" << endl;
    for (size_t i = 0; i < regions.size(); ++i) {
        cout << i + 1 << ". " << regions[i] << endl;
    }
    int regionChoice = readInt("Elegir region: ", 1, static_cast<int>(regions.size()));
    string focusPos = normalizePosition(readLine("Foco de posicion (ARQ/DEF/MED/DEL o Enter para necesidad): "));
    if (focusPos == "N/A") focusPos.clear();
    int durationWeeks = readInt("Duracion de seguimiento (2-6 semanas): ", 2, 6);
    printMessages(createScoutingAssignmentService(career,
                                                  regions[static_cast<size_t>(regionChoice - 1)],
                                                  focusPos,
                                                  durationWeeks));
}

void displaySeasonHistory(const Career& career) {
    cout << "\n=== Historial de Temporadas ===" << endl;
    if (career.history.empty()) {
        cout << "Aun no hay temporadas finalizadas." << endl;
        return;
    }
    for (const auto& entry : career.history) {
        cout << "Temporada " << entry.season << " | " << divisionDisplay(entry.division)
             << " | Club " << entry.club << " | Puesto " << entry.finish << endl;
        cout << "  Campeon: " << entry.champion << endl;
        if (!entry.promoted.empty()) cout << "  Ascensos: " << entry.promoted << endl;
        if (!entry.relegated.empty()) cout << "  Descensos: " << entry.relegated << endl;
        if (!entry.note.empty()) cout << "  Nota: " << entry.note << endl;
    }
}

void displayClubOperations(Career& career) {
    if (!career.myTeam) return;

    printReport(buildClubReport(career));
    cout << "\nSugerencia de objetivo: " << manager_advice::buildSuggestedBoardObjective(career) << endl;
    cout << "Razon: " << manager_advice::buildSuggestedBoardObjectiveReason(career) << endl;
    cout << "Coherencia de filosofia: " << clubPhilosophyAlignmentScore(career, *career.myTeam) << "/100" << endl;
    cout << "\n1. Mejorar estadio" << endl;
    cout << "2. Mejorar cantera" << endl;
    cout << "3. Mejorar centro de entrenamiento" << endl;
    cout << "4. Contratar asistente tecnico" << endl;
    cout << "5. Contratar preparador fisico" << endl;
    cout << "6. Contratar jefe de scouting" << endl;
    cout << "7. Contratar jefe de juveniles" << endl;
    cout << "8. Mejorar cuerpo medico" << endl;
    cout << "9. Contratar entrenador de arqueros" << endl;
    cout << "10. Contratar analista de rendimiento" << endl;
    cout << "11. Revisar estructura de staff" << endl;
    cout << "12. Cambiar region juvenil" << endl;
    cout << "13. Editar filosofia de club" << endl;
    cout << "14. Aplicar objetivo sugerido" << endl;
    cout << "15. Cambiar objetivo mensual" << endl;
    cout << "16. Volver" << endl;
    int choice = readInt("Elige opcion: ", 1, 16);
    if (choice == 16) return;

    ServiceResult result;
    switch (choice) {
        case 1: result = upgradeClubService(career, ClubUpgrade::Stadium); break;
        case 2: result = upgradeClubService(career, ClubUpgrade::Youth); break;
        case 3: result = upgradeClubService(career, ClubUpgrade::Training); break;
        case 4: result = upgradeClubService(career, ClubUpgrade::AssistantCoach); break;
        case 5: result = upgradeClubService(career, ClubUpgrade::FitnessCoach); break;
        case 6: result = upgradeClubService(career, ClubUpgrade::Scouting); break;
        case 7: result = upgradeClubService(career, ClubUpgrade::YouthCoach); break;
        case 8: result = upgradeClubService(career, ClubUpgrade::Medical); break;
        case 9: result = upgradeClubService(career, ClubUpgrade::GoalkeepingCoach); break;
        case 10: result = upgradeClubService(career, ClubUpgrade::PerformanceAnalyst); break;
        case 11: result = reviewStaffStructureService(career); break;
        case 12: {
            vector<string> regions = listYouthRegionsService();
            for (size_t i = 0; i < regions.size(); ++i) {
                cout << i + 1 << ". " << regions[i] << endl;
            }
            int regionChoice = readInt("Nueva region juvenil: ", 1, static_cast<int>(regions.size()));
            result = changeYouthRegionService(career, regions[static_cast<size_t>(regionChoice - 1)]);
            break;
        }
        case 13:
            editClubPhilosophy(career);
            return;
        case 14:
            applySuggestedBoardObjective(career);
            return;
        case 15:
            setBoardObjective(career);
            return;
    }
    printMessages(result);
}

}  // namespace ui_reports
