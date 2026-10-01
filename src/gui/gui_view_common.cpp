#include "gui/gui_view_builders.h"

#ifdef _WIN32

#include "ai/ai_squad_planner.h"
#include "career/analytics_service.h"
#include "career/inbox_service.h"
#include "career/manager_advice.h"
#include "career/medical_service.h"
#include "career/staff_service.h"
#include "career/transfer_briefing.h"
#include "ai/ai_transfer_manager.h"
#include "career/dressing_room_service.h"
#include "career/match_analysis_store.h"
#include "career/match_center_service.h"
#include "career/career_support.h"
#include "development/training_impact_system.h"
#include "finance/finance_system.h"
#include "transfers/negotiation_system.h"
#include "ui/global_search.h"
#include "utils/utils.h"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <sstream>

namespace gui_win32 {

namespace {

const Team* selectedSetupTeam(const AppState& state) {
    if (state.gameSetup.division.empty() || state.gameSetup.club.empty()) return nullptr;
    for (const auto& team : state.career.allTeams) {
        if (team.division == state.gameSetup.division && team.name == state.gameSetup.club) return &team;
    }
    return nullptr;
}

std::string setupDivisionLabel(const AppState& state) {
    for (const auto& division : state.career.divisions) {
        if (division.id == state.gameSetup.division) return division.display;
    }
    return "Sin division";
}

std::string setupStatusLabel(const AppState& state) {
    if (state.gameSetup.ready) return "Lista para iniciar";
    if (state.gameSetup.currentStep == 1) return "Paso 1 pendiente";
    if (state.gameSetup.currentStep == 2) return "Paso 2 en progreso";
    return "Paso 3 en progreso";
}

}  // namespace

std::string pageTitleFor(GuiPage page) {
    switch (page) {
        case GuiPage::MainMenu: return game_settings::gameTitle();
        case GuiPage::Settings: return "Configuraciones";
        case GuiPage::Credits: return "Creditos";
        case GuiPage::Saves: return "Guardados";
        case GuiPage::NewCareer: return "Nueva carrera";
        case GuiPage::Dashboard: return "Resumen del club";
        case GuiPage::Squad: return "Plantilla";
        case GuiPage::Tactics: return "Tacticas";
        case GuiPage::Calendar: return "Calendario";
        case GuiPage::League: return "Liga";
        case GuiPage::Transfers: return "Fichajes";
        case GuiPage::Finances: return "Finanzas";
        case GuiPage::Youth: return "Cantera";
        case GuiPage::Board: return "Directiva";
        case GuiPage::News: return "Noticias";
    }
    return "Resumen del club";
}

std::string breadcrumbFor(GuiPage page) {
    if (page == GuiPage::MainMenu) return "Inicio > Menu principal";
    if (page == GuiPage::Settings) return "Inicio > Configuraciones";
    if (page == GuiPage::Credits) return "Inicio > Creditos";
    if (page == GuiPage::Saves) return "Inicio > Guardados";
    if (page == GuiPage::NewCareer) return "Inicio > Nueva carrera";
    return "Club > " + pageTitleFor(page);
}

bool isCongestedWeek(const Career& career) {
    return career.cupActive && career.currentWeek >= 1 &&
           (career.currentWeek == 1 || career.currentWeek % 4 == 0 ||
            career.currentWeek == static_cast<int>(career.schedule.size()));
}

std::string trainingSchedulePreview(const Team& team, bool congestedWeek, size_t limit) {
    std::ostringstream out;
    const auto schedule = development::buildWeeklyTrainingSchedule(team, congestedWeek);
    for (size_t i = 0; i < schedule.size() && i < limit; ++i) {
        if (i) out << "\r\n";
        out << schedule[i].day << ": " << schedule[i].focus << " | " << schedule[i].note;
    }
    return out.str();
}

std::string boardStatusLabel(int confidence) {
    if (confidence >= 75) return "Muy alta";
    if (confidence >= 55) return "Estable";
    if (confidence >= 35) return "En observacion";
    if (confidence >= 20) return "Bajo presion";
    return "Critica";
}

std::vector<DashboardMetric> buildMetrics(const AppState& state, const std::vector<std::string>& alerts) {
    const Career& career = state.career;
    if (state.currentPage == GuiPage::MainMenu || state.currentPage == GuiPage::Settings ||
        state.currentPage == GuiPage::Credits || state.currentPage == GuiPage::Saves) {
        return {
            {"Titulo", game_settings::gameTitle(), kThemeAccentBlue},
            {"Volumen", game_settings::volumeLabel(state.settings.volume), kThemeAccentGreen},
            {"Dificultad", game_settings::difficultyLabel(state.settings.difficulty), kThemeAccent},
            {"Velocidad", game_settings::simulationSpeedLabel(state.settings.simulationSpeed), kThemeWarning},
            {"Simulacion", game_settings::simulationModeLabel(state.settings.simulationMode), kThemeAccentBlue}
        };
    }
    if (!career.myTeam) {
        const Team* setupTeam = selectedSetupTeam(state);
        const bool hasDivision = !state.gameSetup.division.empty();
        const bool hasClub = setupTeam != nullptr;
        const bool hasManager = !state.gameSetup.manager.empty();
        return {
            {"Division", hasDivision ? setupDivisionLabel(state) : "Pendiente", hasDivision ? kThemeAccentGreen : kThemeDanger},
            {"Club", hasClub ? setupTeam->name : "Club pendiente", hasClub ? kThemeAccentGreen : (hasDivision ? kThemeWarning : kThemeDanger)},
            {"Presupuesto", hasClub ? formatMoneyValue(setupTeam->budget) : "No disponible", hasClub ? kThemeAccentGreen : kThemeDanger},
            {"Manager", hasManager ? state.gameSetup.manager : "Pendiente", hasManager ? kThemeAccentGreen : kThemeDanger},
            {"Estado", setupStatusLabel(state), state.gameSetup.ready ? kThemeAccentGreen : kThemeWarning}
        };
    }

    return {
        {"Club", career.myTeam->name, kThemeAccentBlue},
        {"Fecha", "Temp " + std::to_string(career.currentSeason) + " / Sem " + std::to_string(career.currentWeek), kThemeAccent},
        {"Presupuesto", formatMoneyValue(career.myTeam->budget), kThemeAccentGreen},
        {"Reputacion", std::to_string(career.managerReputation), kThemeAccentBlue},
        {"Alertas", std::to_string(alerts.size()), alerts.empty() ? kThemeAccentGreen : kThemeWarning}
    };
}

LeagueTable selectedLeagueTable(const AppState& state) {
    if (state.currentFilter == "Tabla general") {
        LeagueTable table = state.career.leagueTable;
        table.sortTable();
        return table;
    }
    return buildRelevantCompetitionTable(state.career);
}

std::string findNextMatchLine(const Career& career) {
    if (!career.myTeam) return "Sin partido programado.";
    if (career.currentWeek < 1 || career.currentWeek > static_cast<int>(career.schedule.size())) {
        return "No quedan fechas programadas en la division activa.";
    }
    const auto& fixtures = career.schedule[static_cast<size_t>(career.currentWeek - 1)];
    for (const auto& match : fixtures) {
        const Team* home = career.getActiveTeamAt(match.first);
        const Team* away = career.getActiveTeamAt(match.second);
        if (!home || !away) continue;
        if (home != career.myTeam && away != career.myTeam) continue;
        const Team* opponent = home == career.myTeam ? away : home;
        return std::string(home == career.myTeam ? "Local vs " : "Visita vs ") + opponent->name +
               " | " + divisionDisplay(career.activeDivision);
    }
    return "Tu club no tiene partido visible en la proxima fecha.";
}

std::vector<std::vector<std::string> > buildFixtureRows(const Career& career, int limit) {
    std::vector<std::vector<std::string> > rows;
    if (!career.myTeam) return rows;
    int added = 0;
    for (int week = std::max(1, career.currentWeek); week <= static_cast<int>(career.schedule.size()) && added < limit; ++week) {
        for (const auto& match : career.schedule[static_cast<size_t>(week - 1)]) {
            const Team* home = career.getActiveTeamAt(match.first);
            const Team* away = career.getActiveTeamAt(match.second);
            if (!home || !away) continue;
            if (home != career.myTeam && away != career.myTeam) continue;
            rows.push_back({
                std::to_string(week),
                divisionDisplay(career.activeDivision),
                home == career.myTeam ? away->name : home->name,
                home == career.myTeam ? "Local" : "Visita",
                week == career.currentWeek ? "Proximo" : "Pendiente"
            });
            ++added;
        }
    }
    return rows;
}

std::vector<std::string> buildAlertLines(const Career& career) {
    std::vector<std::string> alerts;
    if (!career.myTeam) {
        alerts.push_back("Crea o carga una carrera para activar alertas.");
        return alerts;
    }

    for (const auto& player : career.myTeam->players) {
        if (player.injured) {
            alerts.push_back("[Lesion] " + player.name + " - " + player.injuryType + " (" + std::to_string(player.injuryWeeks) + " sem)");
        }
        if (player.fitness < 60) {
            alerts.push_back("[Fatiga] " + player.name + " llega con condicion " + std::to_string(player.fitness));
        }
        if (player.happiness < 45) {
            alerts.push_back("[Moral] " + player.name + " baja a " + std::to_string(player.happiness));
        }
        if (player.contractWeeks <= 12) {
            alerts.push_back("[Contrato] " + player.name + " termina en " + std::to_string(player.contractWeeks) + " semanas");
        }
        if (player.wantsToLeave) {
            alerts.push_back("[Salida] " + player.name + " quiere salir del club");
        }
    }

    if (career.boardConfidence < 35) {
        alerts.push_back("[Directiva] La confianza cae a " + std::to_string(career.boardConfidence) + "/100");
    }
    if (!career.boardMonthlyObjective.empty() && career.boardMonthlyProgress < career.boardMonthlyTarget &&
        career.boardMonthlyDeadlineWeek - career.currentWeek <= 2) {
        alerts.push_back("[Objetivo] " + career.boardMonthlyObjective + " queda bajo presion");
    }

    DressingRoomSnapshot dressing = dressing_room_service::buildSnapshot(*career.myTeam, career.currentWeek);
    if (dressing.socialTension >= 5) {
        alerts.push_back("[Vestuario] La tension interna sube a " + std::to_string(dressing.socialTension));
    }
    if (dressing.promiseRiskCount > 0) {
        alerts.push_back("[Promesas] " + std::to_string(dressing.promiseRiskCount) + " promesa(s) bajo presion");
    }
    if (!career.scoutingAssignments.empty()) {
        const auto& assignment = career.scoutingAssignments.front();
        alerts.push_back("[Scouting] Seguimiento activo en " + assignment.region + " para " + assignment.focusPosition);
    }
    const auto staffProfiles = staff_service::buildStaffProfiles(*career.myTeam);
    auto weakestStaff = std::min_element(staffProfiles.begin(), staffProfiles.end(), [](const staff_service::StaffProfile& left, const staff_service::StaffProfile& right) {
        return left.rating < right.rating;
    });
    if (weakestStaff != staffProfiles.end() && weakestStaff->rating <= 54) {
        alerts.push_back("[Staff] Area debil: " + weakestStaff->role + " (" + std::to_string(weakestStaff->rating) + ")");
    }
    if (alerts.empty()) {
        alerts.push_back("[Estado] No hay alertas criticas esta semana");
    }
    return alerts;
}

std::string teamConditionLabel(int averageFitness) {
    if (averageFitness >= 84) return "Alta";
    if (averageFitness >= 68) return "Media";
    return "Baja";
}

std::string teamMoraleLabel(int morale) {
    if (morale >= 72) return "Alta";
    if (morale >= 52) return "Media";
    return "Baja";
}

std::string severityLabel(int value, int high, int medium) {
    if (value >= high) return "Alta";
    if (value >= medium) return "Media";
    return "Baja";
}

bool newsMatchesFilter(const std::string& line, const std::string& filter) {
    if (filter == "Todo" || filter.empty()) return true;
    std::string lower = toLower(line);
    if (filter == "Alertas") return lower.find("alerta") != std::string::npos || lower.find("riesgo") != std::string::npos;
    if (filter == "Mercado") return lower.find("fich") != std::string::npos || lower.find("contrat") != std::string::npos || lower.find("shortlist") != std::string::npos;
    if (filter == "Resultados") return lower.find("copa") != std::string::npos || lower.find("campeon") != std::string::npos || lower.find("gana") != std::string::npos;
    if (filter == "Lesiones") return lower.find("lesion") != std::string::npos;
    if (filter == "Contratos") return lower.find("contrato") != std::string::npos || lower.find("renov") != std::string::npos;
    return true;
}

std::string lastMatchPanelText(const Career& career, size_t maxReportLines, size_t maxEvents) {
    return match_center_service::formatLastMatchCenter(career, maxReportLines, maxEvents);
}

std::string dressingRoomPanelText(const Career& career, size_t maxAlerts) {
    return dressing_room_service::formatSnapshot(career, maxAlerts);
}

std::vector<std::string> buildFeedLines(const Career& career, const std::string& filter, size_t limit) {
    std::vector<std::string> lines;
    if (!career.lastMatchAnalysis.empty()) {
        lines.push_back("[Ultimo partido] " + career.lastMatchAnalysis);
    }
    for (auto it = career.newsFeed.rbegin(); it != career.newsFeed.rend() && lines.size() < limit; ++it) {
        if (!newsMatchesFilter(*it, filter)) continue;
        lines.push_back(*it);
    }
    if (lines.empty()) lines.push_back("No hay entradas para el filtro actual.");
    return lines;
}

std::string playerStatus(const Player& player) {
    std::vector<std::string> flags;
    if (player.injured) flags.push_back("Les");
    if (player.matchesSuspended > 0) flags.push_back("Susp");
    if (player.wantsToLeave) flags.push_back("Salida");
    if (player.onLoan) flags.push_back("Prestamo");
    if (flags.empty()) return "OK";
    return joinStringValues(flags, " | ");
}

int playerSortMetric(const Player& player, int column) {
    switch (column) {
        case 1: return player.age;
        case 3: return player.skill;
        case 4: return player.potential;
        case 5: return player.happiness;
        case 6: return player.currentForm;
        case 7: return player.fitness;
        case 9: return player.contractWeeks;
        default: return player.skill;
    }
}

bool playerMatchesFilter(const Team& team, const Player& player, const std::string& filter) {
    if (filter == "Todos" || filter == "Todo" || filter.empty()) return true;
    if (filter == "Lesionados") return player.injured;
    if (filter == "XI") {
        std::vector<int> xi = team.getStartingXIIndices();
        for (int idx : xi) {
            if (idx >= 0 && idx < static_cast<int>(team.players.size()) && &team.players[static_cast<size_t>(idx)] == &player) return true;
        }
        return false;
    }
    return normalizePosition(player.position) == filter;
}

bool youthMatchesFilter(const Player& player, const std::string& filter) {
    if (filter == "Todos" || filter.empty()) return player.age <= 23;
    if (filter == "Sub-21") return player.age <= 21;
    if (filter == "Elite") return player.age <= 21 && player.potential >= 78;
    if (filter == "Cedidos") return player.onLoan;
    return player.age <= 23;
}

std::string expectedRoleLabel(const Team& team, const Player& player) {
    int avg = team.getAverageSkill();
    if (player.skill >= avg + 6) return "Titular esperado";
    if (player.potential >= player.skill + 8) return "Proyecto";
    if (player.skill >= avg - 2) return "Rotacion fuerte";
    return "Rotacion";
}

std::string playerInterestLabel(const Career& career, const Team& seller, const Player& player) {
    if (!career.myTeam) return "Indefinido";
    int score = teamPrestigeScore(*career.myTeam) - teamPrestigeScore(seller);
    score += career.managerReputation / 10;
    score += player.wage < career.myTeam->budget / 35 ? 8 : -6;
    if (player.skill > career.myTeam->getAverageSkill() + 4) score += 5;
    if (score >= 18) return "Alto";
    if (score >= 6) return "Medio";
    return "Bajo";
}

std::string sellerInterestLabel(const Team& seller, const Player& player) {
    int pressure = 0;
    if (player.contractWeeks <= 18) pressure += 2;
    if (seller.players.size() > 24) pressure += 2;
    if (player.wantsToLeave) pressure += 3;
    if (pressure >= 5) return "Vendible";
    if (pressure >= 2) return "Escuchan";
    return "Resistiran";
}

std::string transferRadarLabel(const TransferTarget& target) {
    std::string prefix = target.onShortlist ? "Shortlist" : (target.urgentNeed ? "Prioridad" : "Seguimiento");
    if (target.scoutingConfidence >= 82) prefix += " exacto";
    else if (target.scoutingConfidence >= 68) prefix += " fiable";
    else if (target.scoutingConfidence >= 52) prefix += " parcial";
    else prefix += " verde";
    return prefix + " " + std::to_string(target.scoutingConfidence) + "%";
}

std::string transferMarketLabel(const TransferTarget& target) {
    if (target.contractRunningOut) return "Contrato corto";
    if (target.availableForLoan && target.affordabilityScore < 12) return "Cesion viable";
    if (target.affordabilityScore >= 20) return "Viable";
    if (target.affordabilityScore >= 8) return "Coste tenso";
    return "Coste alto";
}

std::string scoutingRiskLabel(int confidence) {
    if (confidence >= 82) return "Bajo: atributos y cifras casi cerradas.";
    if (confidence >= 68) return "Medio: quedan pequenos margenes de error.";
    if (confidence >= 52) return "Alto: conviene ampliar informe antes de ofertar.";
    return "Muy alto: informe verde, datos demasiado abiertos.";
}

std::string scoutingNextStepLabel(const TransferPreviewItem& target) {
    if (target.scoutingConfidence < 60) return "Asignar scouting antes de negociar.";
    if (!target.onShortlist) return "Agregar a shortlist y seguir una semana.";
    if (target.scoutingConfidence < 82) return "Actualizar shortlist para cerrar rangos.";
    if (target.marketLabel == "Cesion viable") return "Listo para intentar cesion.";
    if (target.affordabilityScore < 8) return "Negociar solo si liberas salario o vendes.";
    return "Listo para oferta con perfil prudente.";
}

const Player* findPlayerByName(const Team& team, const std::string& name) {
    for (const auto& player : team.players) {
        if (player.name == name) return &player;
    }
    return nullptr;
}

int averageInt(std::initializer_list<int> values) {
    int total = 0;
    int count = 0;
    for (int value : values) {
        total += value;
        ++count;
    }
    return count == 0 ? 0 : total / count;
}

std::string ratingLabel(int value) {
    if (value >= 82) return "Elite";
    if (value >= 72) return "Muy alto";
    if (value >= 62) return "Competitivo";
    if (value >= 52) return "Correcto";
    return "Riesgo";
}

std::string minutesProfileLabel(const Team& team, const Player& player) {
    if (player.injured) return "Baja medica";
    if (player.matchesSuspended > 0) return "Suspendido";
    const int avg = team.getAverageSkill();
    if (player.skill >= avg + 7) return "Titular clave";
    if (player.skill >= avg + 2) return "Titular/rotacion alta";
    if (player.potential >= player.skill + 10 && player.age <= 22) return "Proyecto con minutos";
    return "Rotacion";
}

void addUniqueLimited(std::vector<std::string>& values, const std::string& value, size_t limit = 4) {
    if (value.empty() || values.size() >= limit) return;
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

std::vector<std::string> playerStrengthLines(const Team& team, const Player& player) {
    std::vector<std::string> lines;
    const std::string role = normalizePosition(player.position);
    const int avg = team.getAverageSkill();
    if (player.skill >= avg + 6) addUniqueLimited(lines, "Nivel actual por encima de la media del plantel.");
    if (player.potential >= player.skill + 9) addUniqueLimited(lines, "Margen de crecimiento alto para plan individual.");
    if (role == "DEL" && player.attack >= 72) addUniqueLimited(lines, "Amenaza ofensiva fiable para el ultimo tercio.");
    if ((role == "DEF" || role == "ARQ") && player.defense >= 72) addUniqueLimited(lines, "Base defensiva fuerte para sostener el bloque.");
    if (role == "MED" && player.attack + player.defense >= 135) addUniqueLimited(lines, "Perfil mixto util para conectar fases.");
    if (player.professionalism >= 74) addUniqueLimited(lines, "Profesionalismo alto; responde bien al entrenamiento.");
    if (player.consistency >= 72) addUniqueLimited(lines, "Consistencia alta en semanas competitivas.");
    if (player.leadership >= 72) addUniqueLimited(lines, "Puede sostener liderazgo interno.");
    if (player.versatility >= 72 || !player.secondaryPositions.empty()) addUniqueLimited(lines, "Versatilidad para cubrir mas de una posicion.");
    if (player.currentForm >= 70) addUniqueLimited(lines, "Llega en buena forma reciente.");
    if (player.setPieceSkill >= 72) addUniqueLimited(lines, "Aporta valor en balon parado.");
    if (lines.empty()) lines.push_back("Perfil equilibrado sin fortaleza dominante clara.");
    return lines;
}

std::vector<std::string> playerWeaknessLines(const Player& player) {
    std::vector<std::string> lines;
    if (player.contractWeeks <= 12) addUniqueLimited(lines, "Contrato corto: resolver renovacion o venta.");
    if (player.happiness < 45 || player.unhappinessWeeks >= 2) addUniqueLimited(lines, "Moral baja: conviene charla individual.");
    if (player.wantsToLeave) addUniqueLimited(lines, "Quiere salir; riesgo de vestuario y mercado.");
    if (player.injured) addUniqueLimited(lines, "No disponible por lesion actual.");
    if (player.matchesSuspended > 0) addUniqueLimited(lines, "Suspendido para los proximos partidos.");
    if (player.fitness < 62 || player.fatigueLoad >= 60) addUniqueLimited(lines, "Carga fisica alta o condicion baja.");
    if (player.injuryProneness >= 70 || player.injuryHistory >= 3) addUniqueLimited(lines, "Riesgo medico elevado.");
    if (player.consistency < 50) addUniqueLimited(lines, "Consistencia irregular.");
    if (player.tacticalDiscipline < 55 || player.discipline < 55) addUniqueLimited(lines, "Disciplina tactica por mejorar.");
    if (player.adaptation < 50) addUniqueLimited(lines, "Adaptacion baja a contextos nuevos.");
    if (lines.empty()) lines.push_back("Sin debilidades urgentes detectadas por el staff.");
    return lines;
}

std::string playerStaffRecommendation(const Team& team, const Player& player) {
    if (player.injured) return "Priorizar recuperacion y bajar carga hasta alta medica.";
    if (player.contractWeeks <= 12) return "Resolver contrato antes de aumentar minutos o rechazar ofertas.";
    if (player.wantsToLeave || player.happiness < 45) return "Abrir charla individual y revisar promesas/minutos.";
    if (player.age <= 21 && player.potential >= player.skill + 10) {
        return player.skill >= team.getAverageSkill() - 2
                   ? "Dar minutos controlados y mantener plan de desarrollo."
                   : "Buscar cesion con minutos si no entra en la rotacion.";
    }
    if (player.fitness < 62 || player.fatigueLoad >= 60) return "Rotar una semana y ajustar entrenamiento fisico.";
    if (player.skill >= team.getAverageSkill() + 5) return "Usarlo como pieza central del rol actual.";
    return "Mantener seguimiento mensual y ajustar rol segun rendimiento.";
}

int playerContractRiskScore(const Player& player) {
    int risk = 0;
    if (player.contractWeeks <= 4) risk += 42;
    else if (player.contractWeeks <= 12) risk += 30;
    else if (player.contractWeeks <= 26) risk += 14;
    if (player.wantsToLeave) risk += 24;
    if (player.happiness < 45) risk += 14;
    if (player.promisedRole != "Sin promesa" && player.startsThisSeason < player.desiredStarts) risk += 8;
    if (player.age <= 22 && player.potential >= player.skill + 10) risk += 6;
    return clampInt(risk, 0, 100);
}

std::string playerContractRiskLabel(int risk) {
    if (risk >= 72) return "Critico";
    if (risk >= 50) return "Alto";
    if (risk >= 28) return "Medio";
    return "Controlado";
}

std::string playerManagerDecision(const Team& team, const Player& player) {
    if (player.injured) return "Recuperar";
    if (player.contractWeeks <= 12 && (player.skill >= team.getAverageSkill() - 2 || player.potential >= 75)) return "Renovar";
    if (player.wantsToLeave && player.skill < team.getAverageSkill() + 4) return "Vender";
    if (player.age <= 21 && player.potential >= player.skill + 10 && player.skill < team.getAverageSkill() - 3) return "Ceder";
    if (player.fitness < 62 || player.fatigueLoad >= 60) return "Rotar";
    if (player.skill >= team.getAverageSkill() + 5 && player.happiness >= 45) return "Titular";
    return "Gestionar rol";
}

std::vector<std::string> playerNextMonthPlan(const Team& team, const Player& player) {
    std::vector<std::string> plan;
    const std::string decision = playerManagerDecision(team, player);
    if (decision == "Recuperar") {
        plan.push_back("Semana 1: carga baja y revision medica.");
        plan.push_back("Semana 2: reintroducir minutos solo si mejora la condicion.");
    } else if (decision == "Renovar") {
        plan.push_back("Semana 1: abrir renovacion antes de simular otra fecha.");
        plan.push_back("Semana 2: revisar promesa de rol y clausula.");
    } else if (decision == "Ceder") {
        plan.push_back("Semana 1: buscar destino con minutos reales.");
        plan.push_back("Semana 2: mantener entrenamiento individual si no sale cedido.");
    } else if (decision == "Vender") {
        plan.push_back("Semana 1: escuchar ofertas y proteger el clima del vestuario.");
        plan.push_back("Semana 2: preparar reemplazo antes de aceptar salida.");
    } else if (decision == "Rotar") {
        plan.push_back("Semana 1: bajar carga y usarlo desde el banco.");
        plan.push_back("Semana 2: volver al XI si fisico supera 68.");
    } else {
        plan.push_back("Semana 1: mantener rol actual y medir rendimiento.");
        plan.push_back("Semana 2: ajustar plan individual segun forma y rival.");
    }
    if (player.potential >= player.skill + 8) plan.push_back("Mes: priorizar desarrollo sin romper la moral por falta de minutos.");
    if (player.contractWeeks <= 26) plan.push_back("Mes: no dejar que el contrato llegue a zona de salida libre.");
    return plan;
}

std::string buildPlayerProfile(const Team& team, const Player* player) {
    if (!player) return "Selecciona un jugador para abrir su ficha completa.";
    std::ostringstream out;
    const int technicalScore = averageInt({player->attack, player->defense, player->setPieceSkill, player->versatility, player->skill});
    const int mentalScore = averageInt({player->leadership, player->professionalism, player->ambition, player->consistency,
                                        player->bigMatches, player->tacticalDiscipline, player->discipline, player->adaptation});
    const int physicalScore = averageInt({player->stamina, player->fitness, std::max(1, 100 - player->fatigueLoad),
                                          std::max(1, 100 - player->injuryProneness)});
    const int contractRisk = playerContractRiskScore(*player);
    const std::vector<std::string> strengths = playerStrengthLines(team, *player);
    const std::vector<std::string> weaknesses = playerWeaknessLines(*player);
    const std::vector<std::string> nextMonthPlan = playerNextMonthPlan(team, *player);

    out << "Resumen\r\n";
    out << player->name << " | " << normalizePosition(player->position)
        << " | " << player->age << " anos"
        << " | Media " << player->skill
        << " | Potencial " << player->potential << "\r\n";
    out << "Rol " << player->role << " (" << player->roleDuty << ")"
        << " | Instr. " << player->individualInstruction
        << " | Promesa " << player->promisedRole
        << " | Posicion prometida " << player->promisedPosition
        << " | Pie " << player->preferredFoot << "\r\n\r\n";
    out << "Perfil staff: " << minutesProfileLabel(team, *player)
        << " | Fiabilidad " << playerReliabilityLabel(*player)
        << " | Forma " << playerFormLabel(*player) << "\r\n\r\n";
    out << "Radar FM\r\n";
    out << "Tecnico " << ratingLabel(technicalScore) << " " << technicalScore << "/100"
        << " | Mental " << ratingLabel(mentalScore) << " " << mentalScore << "/100"
        << " | Fisico " << ratingLabel(physicalScore) << " " << physicalScore << "/100"
        << " | Riesgo contrato " << playerContractRiskLabel(contractRisk) << " " << contractRisk << "/100\r\n";
    out << "Decision manager: " << playerManagerDecision(team, *player)
        << " | Encaje plantilla: " << expectedRoleLabel(team, *player)
        << " | Proximo paso: " << playerStaffRecommendation(team, *player) << "\r\n\r\n";

    out << "Atributos tecnicos\r\n";
    out << "Ataque " << player->attack
        << " | Defensa " << player->defense
        << " | Balon parado " << player->setPieceSkill
        << " | Versatilidad " << player->versatility
        << " | Lectura " << ratingLabel(technicalScore) << " (" << technicalScore << "/100)\r\n";
    out << "Atributos mentales\r\n";
    out << "Liderazgo " << player->leadership
        << " | Profesionalismo " << player->professionalism
        << " | Ambicion " << player->ambition
        << " | Consistencia " << player->consistency
        << " | Grandes citas " << player->bigMatches << "\r\n";
    out << "Personalidad " << (player->personality.empty() ? "Equilibrado" : player->personality)
        << " | Disc. tactica " << player->tacticalDiscipline
        << " | Disciplina " << player->discipline
        << " | Adaptacion " << player->adaptation
        << " | Lectura " << ratingLabel(mentalScore) << " (" << mentalScore << "/100)\r\n";
    out << "Atributos fisicos\r\n";
    out << "Resistencia " << player->stamina
        << " | Fisico " << player->fitness
        << " | Carga " << player->fatigueLoad
        << " | Prop. lesiones " << player->injuryProneness
        << " | Lectura " << ratingLabel(physicalScore) << " (" << physicalScore << "/100)\r\n\r\n";

    out << "Contrato y mercado\r\n";
    out << "Salario " << formatMoneyValue(player->wage)
        << " | Clausula " << formatMoneyValue(player->releaseClause)
        << " | Valor " << formatMoneyValue(player->value)
        << " | Semanas restantes " << player->contractWeeks << "\r\n";
    out << "Estado " << playerStatus(*player)
        << " | Desea salir: " << (player->wantsToLeave ? "Si" : "No");
    if (player->onLoan) {
        out << " | Cedido por " << (player->parentClub.empty() ? "-" : player->parentClub)
            << " | Restan " << player->loanWeeksRemaining << " sem";
    }
    out << "\r\n\r\n";

    out << "Rendimiento temporada\r\n";
    out << "PJ " << player->matchesPlayed
        << " | Titularidades " << player->startsThisSeason
        << " / deseadas " << player->desiredStarts
        << " | Goles " << player->goals
        << " | Asistencias " << player->assists << "\r\n";
    out << "Forma " << player->currentForm
        << " (" << playerFormLabel(*player) << ")"
        << " | Moral " << player->happiness
        << " | Momento " << player->moraleMomentum
        << " | Quimica " << player->chemistry << "\r\n\r\n";

    out << "Plan de desarrollo\r\n";
    out << "Plan " << player->developmentPlan
        << " | Grupo " << player->socialGroup
        << " | Posiciones secundarias " << (player->secondaryPositions.empty() ? "-" : joinStringValues(player->secondaryPositions, ", ")) << "\r\n";
    out << "Rasgos " << (player->traits.empty() ? "-" : joinStringValues(player->traits, ", ")) << "\r\n\r\n";
    out << "Plan 30 dias\r\n";
    for (const std::string& line : nextMonthPlan) out << "- " << line << "\r\n";
    out << "\r\n";

    out << "Disponibilidad e historial\r\n";
    out << "Lesiones previas " << player->injuryHistory
        << " | Amarillas " << player->seasonYellowCards
        << " | Acumulacion " << player->yellowAccumulation
        << " | Rojas " << player->seasonRedCards << "\r\n";
    const auto medicalStatuses = medical_service::buildMedicalStatuses(team);
    auto medical = std::find_if(medicalStatuses.begin(), medicalStatuses.end(), [&](const medical_service::MedicalStatus& status) {
        return status.playerName == player->name;
    });
    out << "Aporte relativo: " << expectedRoleLabel(team, *player) << "\r\n";
    out << "Centro medico\r\n";
    if (medical != medicalStatuses.end()) {
        out << medical->diagnosis
            << " | carga " << medical->workloadRisk
            << " | recaida " << medical->relapseRisk
            << " | " << medical->recommendation;
        if (medical->weeksOut > 0) out << " | baja " << medical->weeksOut << " sem";
    } else {
        out << "Sin alertas medicas relevantes";
    }
    out << "\r\n\r\nInforme del staff\r\n";
    out << "Fortalezas\r\n";
    for (const std::string& line : strengths) out << "- " << line << "\r\n";
    out << "Debilidades\r\n";
    for (const std::string& line : weaknesses) out << "- " << line << "\r\n";
    out << "Recomendacion\r\n";
    out << playerStaffRecommendation(team, *player);
    return out.str();
}

ListPanelModel buildTeamStatusModel(const Career& career) {
    ListPanelModel model;
    model.title = "TeamStatusPanel";
    model.columns = {{L"Indicador", 160}, {L"Estado", 100}, {L"Detalle", 300}};
    if (!career.myTeam) {
        model.rows.push_back({"Estado general", "Sin datos", "No hay carrera activa. Crea una nueva partida para comenzar."});
        return model;
    }

    const Team& team = *career.myTeam;
    const DressingRoomSnapshot dressing = dressing_room_service::buildSnapshot(team, career.currentWeek);
    int injured = 0;
    int suspended = 0;
    int lowMorale = 0;
    int lowFitness = 0;
    int fitnessTotal = 0;
    int fatigueLoad = 0;
    for (const auto& player : team.players) {
        fitnessTotal += player.fitness;
        fatigueLoad += player.fatigueLoad;
        if (player.injured) ++injured;
        if (player.matchesSuspended > 0) ++suspended;
        if (player.happiness < 45) ++lowMorale;
        if (player.fitness < 62 || player.fatigueLoad >= 60) ++lowFitness;
    }
    int squadSize = std::max(1, static_cast<int>(team.players.size()));
    int averageFitness = fitnessTotal / squadSize;
    int averageLoad = fatigueLoad / squadSize;

    model.rows.push_back({"Fatiga", teamConditionLabel(averageFitness), "Promedio fisico " + std::to_string(averageFitness) + " / 100"});
    model.rows.push_back({"Moral", teamMoraleLabel(team.morale), "Moral colectiva " + std::to_string(team.morale) + " / 100"});
    model.rows.push_back({"Lesionados", severityLabel(injured, 3, 1), std::to_string(injured) + " jugadores fuera"});
    model.rows.push_back({"Suspendidos", severityLabel(suspended, 2, 1), std::to_string(suspended) + " jugadores sancionados"});
    model.rows.push_back({"Carga alta", severityLabel(lowFitness, 4, 2), std::to_string(lowFitness) + " jugadores con fisico bajo"});
    model.rows.push_back({"Vestuario", severityLabel(lowMorale, 4, 2), std::to_string(lowMorale) + " jugadores con moral baja"});
    model.rows.push_back({"Carga acumulada", severityLabel(averageLoad, 65, 40), "Promedio de carga " + std::to_string(averageLoad) + " / 100"});
    model.rows.push_back({"Promesas", severityLabel(static_cast<int>(career.activePromises.size()), 3, 1), std::to_string(career.activePromises.size()) + " compromisos activos"});
    model.rows.push_back({"Conflictos", severityLabel(dressing.conflictCount, 3, 1), std::to_string(dressing.conflictCount) + " focos de tension interna"});
    return model;
}

ListPanelModel buildLeagueTableModel(const Career& career, const std::string& filter) {
    ListPanelModel model;
    model.title = "LeagueTableView";
    model.columns = {
        {L"Pos", 46}, {L"Equipo", 220}, {L"PJ", 48}, {L"G", 40}, {L"E", 40},
        {L"P", 40}, {L"GF", 46}, {L"GC", 46}, {L"DG", 48}, {L"PTS", 52}, {L"Forma", 96}
    };
    if (!career.myTeam) return model;

    LeagueTable table = filter == "Tabla general" ? career.leagueTable : buildRelevantCompetitionTable(career);
    table.sortTable();
    for (size_t i = 0; i < table.teams.size(); ++i) {
        Team* team = table.teams[i];
        if (!team) continue;
        int played = team->wins + team->draws + team->losses;
        int diff = team->goalsFor - team->goalsAgainst;
        std::string name = team->name;
        if (team == career.myTeam) name += " *";
        std::string form = team->morale >= 70 ? "Alta" : (team->morale >= 50 ? "Mixta" : "Baja");
        model.rows.push_back({
            std::to_string(i + 1), name, std::to_string(played), std::to_string(team->wins), std::to_string(team->draws),
            std::to_string(team->losses), std::to_string(team->goalsFor), std::to_string(team->goalsAgainst),
            std::to_string(diff), std::to_string(team->points), form
        });
    }
    return model;
}

ListPanelModel buildInjuryModel(const Career& career) {
    ListPanelModel model;
    model.title = "InjuryListWidget";
    model.columns = {{L"Jugador", 220}, {L"Tipo", 120}, {L"Sem", 50}, {L"Riesgo", 60}, {L"Nota", 220}};
    if (!career.myTeam) return model;
    for (const auto& status : medical_service::buildMedicalStatuses(*career.myTeam)) {
        model.rows.push_back({
            status.playerName,
            status.diagnosis,
            status.weeksOut > 0 ? std::to_string(status.weeksOut) : "-",
            std::to_string(status.relapseRisk),
            status.recommendation
        });
    }
    if (model.rows.empty()) {
        model.rows.push_back({"No hay alertas medicas", "-", "-", "-", "Plantilla completa y disponible"});
    }
    return model;
}

ListPanelModel buildFixtureModel(const Career& career, int limit) {
    ListPanelModel model;
    model.title = "FixtureListView";
    model.columns = {{L"Sem", 48}, {L"Comp", 120}, {L"Rival", 220}, {L"Sede", 70}, {L"Estado", 100}};
    model.rows = buildFixtureRows(career, limit);
    if (model.rows.empty()) {
        model.rows.push_back({"-", "-", "No hay partidos programados", "-", "-"});
    }
    return model;
}

ListPanelModel buildSquadUnitModel(const Team& team) {
    ListPanelModel model;
    model.title = "SquadUnitOverview";
    model.columns = {{L"Linea", 90}, {L"Jug.", 50}, {L"Media", 60}, {L"Pot", 60}, {L"Fisico", 70}, {L"Estado", 120}};

    struct Acc {
        int count = 0;
        int skill = 0;
        int potential = 0;
        int fitness = 0;
        int warnings = 0;
    };
    std::map<std::string, Acc> groups;
    for (const auto& player : team.players) {
        std::string key = normalizePosition(player.position);
        Acc& acc = groups[key];
        acc.count++;
        acc.skill += player.skill;
        acc.potential += player.potential;
        acc.fitness += player.fitness;
        if (player.injured || player.fitness < 60 || player.happiness < 45) acc.warnings++;
    }
    for (const auto& entry : groups) {
        const Acc& acc = entry.second;
        if (acc.count == 0) continue;
        model.rows.push_back({
            entry.first,
            std::to_string(acc.count),
            std::to_string(acc.skill / acc.count),
            std::to_string(acc.potential / acc.count),
            std::to_string(acc.fitness / acc.count),
            acc.warnings > 0 ? std::to_string(acc.warnings) + " alertas" : "Estable"
        });
    }
    return model;
}

ListPanelModel buildPlayerTableModel(AppState& state, bool youthOnly) {
    ListPanelModel model;
    model.title = youthOnly ? "YouthPlayerTableView" : "PlayerTableView";
    model.columns = {
        {L"Nombre", 208}, {L"Edad", 54}, {L"Pos", 54}, {L"Media", 58}, {L"Pot", 58},
        {L"Moral", 60}, {L"Forma", 60}, {L"Fisico", 60}, {L"Salario", 92}, {L"Contrato", 66},
        {L"Decision", 98}, {L"Estado", 120}
    };
    if (!state.career.myTeam) return model;

    Team& team = *state.career.myTeam;
    std::vector<const Player*> players;
    for (const auto& player : team.players) {
        bool visible = youthOnly ? youthMatchesFilter(player, state.currentFilter) : playerMatchesFilter(team, player, state.currentFilter);
        if (visible) players.push_back(&player);
    }
    std::sort(players.begin(), players.end(), [&](const Player* left, const Player* right) {
        if (state.squadSort.column == 0) {
            return state.squadSort.ascending ? left->name < right->name : left->name > right->name;
        }
        if (state.squadSort.column == 2) {
            return state.squadSort.ascending ? normalizePosition(left->position) < normalizePosition(right->position)
                                             : normalizePosition(left->position) > normalizePosition(right->position);
        }
        if (state.squadSort.column == 8) {
            return state.squadSort.ascending ? left->wage < right->wage : left->wage > right->wage;
        }
        int lv = playerSortMetric(*left, state.squadSort.column);
        int rv = playerSortMetric(*right, state.squadSort.column);
        return state.squadSort.ascending ? lv < rv : lv > rv;
    });

    if (state.selectedPlayerName.empty() && !players.empty()) {
        state.selectedPlayerName = players.front()->name;
    }

    for (const Player* player : players) {
        model.rows.push_back({
            player->name,
            std::to_string(player->age),
            normalizePosition(player->position),
            std::to_string(player->skill),
            std::to_string(player->potential),
            std::to_string(player->happiness),
            std::to_string(player->currentForm),
            std::to_string(player->fitness),
            formatMoneyValue(player->wage),
            std::to_string(player->contractWeeks),
            playerManagerDecision(team, *player),
            playerStatus(*player)
        });
    }
    if (model.rows.empty()) {
        model.rows.push_back({"Sin jugadores", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-"});
    }
    return model;
}

ListPanelModel buildComparisonModel(const Career& career, const Player* selected) {
    ListPanelModel model;
    model.title = "PlayerComparisonPanel";
    model.columns = {{L"Campo", 120}, {L"Seleccionado", 120}, {L"Referencia", 120}, {L"Mercado", 120}};
    if (!career.myTeam || !selected) {
        model.rows.push_back({"Comparador", "-", "-", "-"});
        return model;
    }

    const Player* reference = nullptr;
    const Player* market = nullptr;
    for (const auto& player : career.myTeam->players) {
        if (&player == selected) continue;
        if (normalizePosition(player.position) != normalizePosition(selected->position)) continue;
        if (!reference || player.skill > reference->skill) reference = &player;
    }
    for (const auto& team : career.allTeams) {
        if (&team == career.myTeam) continue;
        for (const auto& player : team.players) {
            if (normalizePosition(player.position) != normalizePosition(selected->position)) continue;
            if (!market || player.skill > market->skill) market = &player;
        }
    }

    model.rows.push_back({"Edad", std::to_string(selected->age), reference ? std::to_string(reference->age) : "-", market ? std::to_string(market->age) : "-"});
    model.rows.push_back({"Media", std::to_string(selected->skill), reference ? std::to_string(reference->skill) : "-", market ? std::to_string(market->skill) : "-"});
    model.rows.push_back({"Potencial", std::to_string(selected->potential), reference ? std::to_string(reference->potential) : "-", market ? std::to_string(market->potential) : "-"});
    model.rows.push_back({"Forma", std::to_string(selected->currentForm), reference ? std::to_string(reference->currentForm) : "-", market ? std::to_string(market->currentForm) : "-"});
    model.rows.push_back({"Disciplina", std::to_string(selected->discipline), reference ? std::to_string(reference->discipline) : "-", market ? std::to_string(market->discipline) : "-"});
    model.rows.push_back({"Adaptacion", std::to_string(selected->adaptation), reference ? std::to_string(reference->adaptation) : "-", market ? std::to_string(market->adaptation) : "-"});
    model.rows.push_back({"Prop. lesiones", std::to_string(selected->injuryProneness), reference ? std::to_string(reference->injuryProneness) : "-", market ? std::to_string(market->injuryProneness) : "-"});
    model.rows.push_back({"Valor", formatMoneyValue(selected->value), reference ? formatMoneyValue(reference->value) : "-", market ? formatMoneyValue(market->value) : "-"});
    model.rows.push_back({"Salario", formatMoneyValue(selected->wage), reference ? formatMoneyValue(reference->wage) : "-", market ? formatMoneyValue(market->wage) : "-"});
    return model;
}

ListPanelModel buildTransferPipelineModel(const Career& career) {
    ListPanelModel model;
    model.title = "TransferPipeline";
    model.columns = {{L"Tipo", 100}, {L"Jugador", 180}, {L"Club", 180}, {L"Detalle", 260}, {L"Monto", 120}};
    if (!career.myTeam) return model;

    for (const auto& move : career.pendingTransfers) {
        std::string type = move.loan ? "Prestamo" : (move.preContract ? "Precontrato" : "Pendiente");
        std::ostringstream detail;
        detail << "T" << move.effectiveSeason;
        if (move.contractWeeks > 0) detail << " | " << move.contractWeeks << " sem";
        if (!move.promisedRole.empty()) detail << " | " << move.promisedRole;
        model.rows.push_back({type, move.playerName, move.fromTeam + " -> " + move.toTeam, detail.str(),
                              formatMoneyValue(move.fee > 0 ? move.fee : move.wage)});
    }
    if (model.rows.empty()) {
        model.rows.push_back({"Info", "-", career.myTeam->name, "No hay movimientos pendientes.", "-"});
    }
    return model;
}

std::vector<TransferPreviewItem> buildTransferTargets(const Career& career, const std::string& filter, const std::string& searchQuery) {
    std::vector<TransferPreviewItem> rows;
    if (!career.myTeam) return rows;
    const Team& buyer = *career.myTeam;
    const ClubTransferStrategy strategy = ai_transfer_manager::buildClubTransferStrategy(career, buyer);
    const std::vector<SearchResult> searchResults = searchQuery.empty()
        ? std::vector<SearchResult>{}
        : global_search::PlayerSearchEngine::search(searchQuery);

    for (const auto& seller : career.allTeams) {
        if (&seller == career.myTeam) continue;
        for (const auto& player : seller.players) {
            std::string position = normalizePosition(player.position);
            if (!searchQuery.empty()) {
                const bool matchesSearch = std::any_of(searchResults.begin(), searchResults.end(), [&](const SearchResult& result) {
                    return result.playerName == player.name && result.clubName == seller.name;
                });
                if (!matchesSearch) continue;
            }
            if (filter == "Sub-23" && player.age > 23) continue;
            if (filter == "Potencial 75+" && player.potential < 75) continue;
            if (filter == "ARQ" || filter == "DEF" || filter == "MED" || filter == "DEL") {
                if (position != filter) continue;
            }
            if (player.age > 35 || player.onLoan) continue;

            const TransferTarget target = ai_transfer_manager::evaluateTarget(career, buyer, seller, player, strategy);
            const long long upfrontPackage = target.expectedFee + target.expectedAgentFee;
            if (filter == "Precio accesible" && upfrontPackage > strategy.maxTransferBudget) continue;
            if (filter == "Salario bajo" && target.expectedWage > strategy.maxWageBudget) continue;

            rows.push_back({
                player.name,
                position,
                player.age,
                player.skill,
                player.potential,
                transfer_briefing::scoutingAttributeLabel(player.skill, target.scoutingConfidence),
                transfer_briefing::scoutingAttributeLabel(player.potential, target.scoutingConfidence),
                target.expectedFee,
                target.expectedWage,
                target.expectedAgentFee,
                transfer_briefing::scoutingMoneyLabel(target.expectedFee, target.scoutingConfidence),
                transfer_briefing::scoutingMoneyLabel(target.expectedWage, target.scoutingConfidence),
                transfer_briefing::scoutingMoneyLabel(target.expectedAgentFee, target.scoutingConfidence),
                seller.name,
                transferRadarLabel(target),
                transferMarketLabel(target),
                expectedRoleLabel(buyer, player),
                target.scoutingNote,
                manager_advice::buildTransferCompetitionLabel(career, seller, player, target),
                manager_advice::buildTransferActionLabel(career, seller, player, target),
                manager_advice::buildTransferPackageLabel(target),
                target.onShortlist,
                target.urgentNeed,
                target.scoutingConfidence,
                target.affordabilityScore,
                target.totalScore
            });
        }
    }

    std::sort(rows.begin(), rows.end(), [](const TransferPreviewItem& left, const TransferPreviewItem& right) {
        if (left.onShortlist != right.onShortlist) return left.onShortlist > right.onShortlist;
        if (left.urgentNeed != right.urgentNeed) return left.urgentNeed > right.urgentNeed;
        if (left.totalScore != right.totalScore) return left.totalScore > right.totalScore;
        return left.expectedFee + left.expectedAgentFee < right.expectedFee + right.expectedAgentFee;
    });
    if (rows.size() > 20) rows.resize(20);
    return rows;
}


}  // namespace gui_win32

#endif



