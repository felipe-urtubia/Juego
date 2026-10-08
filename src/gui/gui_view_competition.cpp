#include "gui/gui_view_builders.h"

#ifdef _WIN32

#include "career/career_support.h"
#include "utils/utils.h"

#include <algorithm>
#include <sstream>

namespace {

struct CalendarPreview {
    const Team* opponent = nullptr;
    std::string venue = "-";
    int week = 0;
    int fixtureDensity = 0;
    int heavyLegs = 0;
    int risk = 0;
    std::string riskReason;
    std::string focus;
    std::string focusReason;
};

int countUpcomingManagedFixtures(const Career& career, int lookaheadWeeks) {
    if (!career.myTeam || career.currentWeek < 1) return 0;
    int fixtures = 0;
    const int finalWeek = std::min(static_cast<int>(career.schedule.size()), career.currentWeek + lookaheadWeeks - 1);
    for (int week = career.currentWeek; week <= finalWeek; ++week) {
        for (const auto& match : career.schedule[static_cast<size_t>(week - 1)]) {
            const Team* home = career.getActiveTeamAt(match.first);
            const Team* away = career.getActiveTeamAt(match.second);
            if (home == career.myTeam || away == career.myTeam) ++fixtures;
        }
    }
    return fixtures;
}

CalendarPreview buildCalendarPreview(const Career& career) {
    CalendarPreview preview;
    if (!career.myTeam || career.currentWeek < 1 ||
        career.currentWeek > static_cast<int>(career.schedule.size())) {
        preview.riskReason = "Sin fecha activa";
        preview.focus = "Planificar cierre";
        preview.focusReason = "No hay rival de liga inmediato.";
        return preview;
    }

    preview.week = career.currentWeek;
    for (const auto& match : career.schedule[static_cast<size_t>(career.currentWeek - 1)]) {
        const Team* home = career.getActiveTeamAt(match.first);
        const Team* away = career.getActiveTeamAt(match.second);
        if (!home || !away) continue;
        if (home == career.myTeam) {
            preview.opponent = away;
            preview.venue = "Local";
            break;
        }
        if (away == career.myTeam) {
            preview.opponent = home;
            preview.venue = "Visita";
            break;
        }
    }

    preview.fixtureDensity = countUpcomingManagedFixtures(career, 4);
    preview.heavyLegs = static_cast<int>(std::count_if(career.myTeam->players.begin(), career.myTeam->players.end(), [](const Player& player) {
        return player.fitness < 62 || player.fatigueLoad >= 60 || player.injured;
    }));

    if (!preview.opponent) {
        preview.riskReason = "Tu club no aparece en la fecha actual";
        preview.focus = "Recuperacion";
        preview.focusReason = "Semana libre para bajar carga y ajustar roles.";
        return preview;
    }

    const Team& team = *career.myTeam;
    const Team& opponent = *preview.opponent;
    const bool derby = areRivalClubs(team, opponent);
    int risk = 18;
    risk += std::max(0, opponent.getAverageSkill() - team.getAverageSkill()) * 2;
    risk += std::max(0, opponent.morale - team.morale) / 2;
    risk += preview.venue == "Visita" ? 8 : 0;
    risk += derby ? 15 : 0;
    risk += std::max(0, preview.fixtureDensity - 2) * 6;
    risk += preview.heavyLegs * 3;
    risk += career.boardWarningWeeks > 0 ? 6 : 0;
    preview.risk = clampInt(risk, 0, 100);

    if (derby) {
        preview.riskReason = "Clasico: sube presion emocional y disciplina.";
    } else if (opponent.getAverageSkill() > team.getAverageSkill() + 4) {
        preview.riskReason = "Rival superior en media de plantel.";
    } else if (preview.heavyLegs >= 4) {
        preview.riskReason = "El XI llega con carga fisica acumulada.";
    } else if (preview.venue == "Visita") {
        preview.riskReason = "Salida de visita con menor margen de control.";
    } else {
        preview.riskReason = "Riesgo competitivo normal para la fecha.";
    }

    if (preview.heavyLegs >= 4 || preview.fixtureDensity >= 3) {
        preview.focus = "Recuperacion";
        preview.focusReason = "Bajar carga antes de sostener intensidad.";
    } else if (opponent.getTotalAttack() > team.getTotalDefense() + 25) {
        preview.focus = "Defensa";
        preview.focusReason = "Proteger area y reducir volumen rival.";
    } else if (opponent.defensiveLine <= 2 || opponent.width <= 2) {
        preview.focus = "Ataque por bandas";
        preview.focusReason = "Abrir un bloque que defiende bajo o estrecho.";
    } else if (opponent.pressingIntensity >= 4 || opponent.tempo >= 4) {
        preview.focus = "Salida limpia";
        preview.focusReason = "Superar presion y pausar transiciones.";
    } else if (team.getTotalAttack() < opponent.getTotalDefense()) {
        preview.focus = "Balon parado";
        preview.focusReason = "Buscar ventajas cuando el rival defiende mejor.";
    } else {
        preview.focus = "Preparacion partido";
        preview.focusReason = "Mantener plan base y ajustar por rival.";
    }
    return preview;
}

std::string calendarRiskLabel(int risk) {
    if (risk >= 72) return "Alto";
    if (risk >= 50) return "Medio";
    if (risk >= 30) return "Controlado";
    return "Bajo";
}

std::string calendarPreviewSummary(const Career& career, const CalendarPreview& preview) {
    std::ostringstream out;
    out << gui_win32::findNextMatchLine(career) << "\r\n";
    out << "Partidos visibles: ";
    if (career.currentWeek >= 1) {
        out << countUpcomingManagedFixtures(career, 10);
    } else {
        out << 0;
    }
    if (preview.opponent) {
        out << "\r\nPrevia: " << preview.venue << " vs " << preview.opponent->name
            << " | Riesgo " << calendarRiskLabel(preview.risk) << " " << preview.risk << "/100"
            << " | Foco sugerido: " << preview.focus;
        out << "\r\nRival media " << preview.opponent->getAverageSkill()
            << " | moral " << preview.opponent->morale
            << " | formacion " << preview.opponent->formation
            << " | proximas 4 sem " << preview.fixtureDensity << " partido(s)";
    } else {
        out << "\r\nPrevia: sin rival inmediato | Foco sugerido: " << preview.focus;
    }
    return out.str();
}

}  // namespace

namespace gui_win32 {

GuiPageModel buildCalendarModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = "Calendario competitivo con proximos partidos, copa y contexto de temporada.";
    model.summary.title = "CalendarSummary";
    model.primary = buildFixtureModel(state.career, state.currentFilter == "Toda la fase" ? 24 : (state.currentFilter == "Proximos 10" ? 10 : 5));
    model.secondary.title = "CupAndSeasonNotes";
    model.secondary.columns = {{L"Item", 180}, {L"Estado", 280}, {L"Contexto", 180}};
    model.footer.title = "SeasonHistory";
    model.footer.columns = {{L"Temp", 60}, {L"Club", 170}, {L"Puesto", 60}, {L"Campeon", 180}, {L"Nota", 280}};
    model.detail.title = "SeasonFlowController";
    model.feed.title = "NewsFeedPanel";
    model.feed.lines = buildFeedLines(state.career, "");

    if (!state.career.myTeam) {
        model.summary.content = "No hay calendario cargado.";
        model.detail.content = "Inicia una carrera.";
        return model;
    }

    const CalendarPreview preview = buildCalendarPreview(state.career);
    model.summary.content = calendarPreviewSummary(state.career, preview) +
                            "\r\nFiltro calendario: " + state.currentFilter;
    model.secondary.title = "MatchPrepPanel";
    model.secondary.columns = {{L"Area", 150}, {L"Lectura", 240}, {L"Accion", 260}};
    model.secondary.rows.push_back({"Division activa", divisionDisplay(state.career.activeDivision), "Sem " + std::to_string(state.career.currentWeek)});
    if (preview.opponent) {
        model.secondary.rows.push_back({"Proximo rival",
                                        preview.venue + " vs " + preview.opponent->name,
                                        "Media " + std::to_string(preview.opponent->getAverageSkill()) +
                                            " | moral " + std::to_string(preview.opponent->morale)});
        model.secondary.rows.push_back({"Riesgo de fecha",
                                        calendarRiskLabel(preview.risk) + " " + std::to_string(preview.risk) + "/100",
                                        preview.riskReason});
        model.secondary.rows.push_back({"Foco semanal", preview.focus, preview.focusReason});
        model.secondary.rows.push_back({"Carga calendario",
                                        std::to_string(preview.fixtureDensity) + " partido(s) en 4 semanas",
                                        preview.heavyLegs >= 4 ? "Rotar y bajar intensidad" : "Carga manejable"});
    } else {
        model.secondary.rows.push_back({"Proximo rival", "Sin partido inmediato", preview.focusReason});
    }
    model.secondary.rows.push_back({"Copa", state.career.cupActive ? "Activa" : "Cerrada", state.career.cupChampion.empty() ? "Sin campeon" : state.career.cupChampion});
    model.secondary.rows.push_back({"Transicion", state.career.currentWeek > static_cast<int>(state.career.schedule.size()) ? "Cierre de temporada" : "Fase regular", "SeasonService"});

    for (auto it = state.career.history.rbegin(); it != state.career.history.rend() && model.footer.rows.size() < 6; ++it) {
        model.footer.rows.push_back({
            std::to_string(it->season), it->club, std::to_string(it->finish), it->champion, it->note
        });
    }
    if (model.footer.rows.empty()) model.footer.rows.push_back({"-", "-", "-", "-", "Sin historial"});
    std::ostringstream detail;
    detail << "Previa del partido\r\n";
    if (preview.opponent) {
        detail << "- Rival: " << preview.opponent->name << " | " << preview.venue
               << " | semana " << preview.week << "\r\n";
        detail << "- Riesgo: " << calendarRiskLabel(preview.risk) << " "
               << preview.risk << "/100 | " << preview.riskReason << "\r\n";
        detail << "- Foco sugerido: " << preview.focus << " | " << preview.focusReason << "\r\n";
        detail << "- Carga: " << preview.fixtureDensity << " partido(s) en 4 semanas"
               << " | jugadores cargados " << preview.heavyLegs << "\r\n\r\n";
        detail << "Informe rival\r\n" << buildOpponentReport(state.career) << "\r\n\r\n";
        const auto plan = buildNextOpponentPlanLines(state.career, 5);
        if (!plan.empty()) {
            detail << "Checklist previo\r\n";
            for (const std::string& line : plan) detail << "- " << line << "\r\n";
            detail << "\r\n";
        }
    } else {
        detail << "- Sin rival inmediato. Usa la semana para recuperar fisico y ordenar objetivos.\r\n\r\n";
    }
    detail << "Microciclo\r\n" << trainingSchedulePreview(*state.career.myTeam, isCongestedWeek(state.career), 6) << "\r\n\r\n";
    detail << "El flujo semanal sincroniza calendario, finanzas, desarrollo y copa.\r\n";
    detail << "Filtro actual: " << state.currentFilter;
    model.detail.content = detail.str();
    return model;
}

GuiPageModel buildLeagueModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = "Tabla de liga, zonas de objetivo y contexto competitivo del club.";
    model.summary.title = "CompetitionSummary";
    model.primary = buildLeagueTableModel(state.career, state.currentFilter);
    model.secondary.title = "RaceContext";
    model.secondary.columns = {{L"Zona", 110}, {L"Club", 180}, {L"Pts", 60}, {L"Dato", 200}};
    model.footer.title = "SeasonRecords";
    model.footer.columns = {{L"Temp", 60}, {L"Club", 170}, {L"Puesto", 60}, {L"Ascenso/Descenso", 240}, {L"Nota", 200}};
    model.detail.title = "CompetitionReport";
    model.feed.title = "NewsFeedPanel";
    model.feed.lines = buildFeedLines(state.career, state.currentFilter == "Grupo actual" ? "" : "Resultados");

    model.summary.content = buildCompetitionSummaryService(state.career);
    model.detail.content = buildCompetitionSummaryService(state.career);

    if (state.career.myTeam && !model.primary.rows.empty()) {
        model.secondary.rows.push_back({"Tu club", state.career.myTeam->name, std::to_string(state.career.myTeam->points),
                                        "Puesto " + std::to_string(state.career.currentCompetitiveRank())});
        LeagueTable table = selectedLeagueTable(state);
        Team* leader = table.teams.empty() ? nullptr : table.teams.front();
        const int relegationSlots = state.career.activeDivision == "primera division" ? 2 : 1;
        if (leader) {
            model.secondary.rows.push_back({"Lider", leader->name, std::to_string(leader->points),
                                            "Media " + std::to_string(leader->getAverageSkill())});
        }
        const int visibleRelegationSlots =
            std::min(relegationSlots, static_cast<int>(table.teams.size()));
        for (int i = visibleRelegationSlots; i > 0; --i) {
            const size_t index = table.teams.size() - static_cast<size_t>(i);
            Team* relegationTeam = table.teams[index];
            if (!relegationTeam) continue;

            model.secondary.rows.push_back({
                "Descenso",
                relegationTeam->name,
                std::to_string(relegationTeam->points),
                "Puesto " + std::to_string(index + 1) +
                    " | Moral " + std::to_string(relegationTeam->morale)
            });
        }
    }
    for (auto it = state.career.history.rbegin(); it != state.career.history.rend() && model.footer.rows.size() < 6; ++it) {
        model.footer.rows.push_back({
            std::to_string(it->season), it->club, std::to_string(it->finish),
            it->promoted + " / " + it->relegated, it->note
        });
    }
    return model;
}

}  // namespace gui_win32

#endif




