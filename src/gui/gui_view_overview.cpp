#include "gui/gui_view_builders.h"

#ifdef _WIN32

#include "ai/ai_squad_planner.h"
#include "career/analytics_service.h"
#include "career/dressing_room_service.h"
#include "career/inbox_service.h"
#include "career/manager_advice.h"
#include "career/staff_service.h"
#include "career/weekly_focus_service.h"
#include "career/career_support.h"
#include "utils/utils.h"

#include <algorithm>
#include <map>
#include <sstream>

namespace {

std::vector<std::string> focusedAlertLines(const std::vector<std::string>& alerts,
                                           const std::vector<std::string>& keywords) {
    std::vector<std::string> out;
    for (const std::string& line : alerts) {
        const std::string lower = toLower(line);
        bool match = false;
        for (const std::string& keyword : keywords) {
            if (lower.find(keyword) != std::string::npos) {
                match = true;
                break;
            }
        }
        if (!match) continue;
        out.push_back(line);
        if (out.size() >= 8) break;
    }
    return out.empty() ? alerts : out;
}

std::string inferActionDestination(const std::string& text) {
    const std::string lower = toLower(text);
    if (lower.find("fatiga") != std::string::npos ||
        lower.find("fisico") != std::string::npos ||
        lower.find("lesion") != std::string::npos ||
        lower.find("moral") != std::string::npos ||
        lower.find("vestuario") != std::string::npos ||
        lower.find("plantel") != std::string::npos ||
        lower.find("promesa") != std::string::npos) {
        return "Plantilla";
    }
    if (lower.find("fich") != std::string::npos ||
        lower.find("mercado") != std::string::npos ||
        lower.find("shortlist") != std::string::npos ||
        lower.find("contrato") != std::string::npos ||
        lower.find("renov") != std::string::npos) {
        return "Fichajes";
    }
    if (lower.find("presupuesto") != std::string::npos ||
        lower.find("caja") != std::string::npos ||
        lower.find("deuda") != std::string::npos ||
        lower.find("salario") != std::string::npos ||
        lower.find("finanz") != std::string::npos) {
        return "Finanzas";
    }
    if (lower.find("directiva") != std::string::npos ||
        lower.find("objetivo") != std::string::npos ||
        lower.find("confianza") != std::string::npos ||
        lower.find("staff") != std::string::npos) {
        return "Directiva";
    }
    if (lower.find("rival") != std::string::npos ||
        lower.find("partido") != std::string::npos ||
        lower.find("tact") != std::string::npos ||
        lower.find("formacion") != std::string::npos ||
        lower.find("presion") != std::string::npos) {
        return "Tacticas";
    }
    if (lower.find("liga") != std::string::npos ||
        lower.find("puesto") != std::string::npos ||
        lower.find("puntos") != std::string::npos) {
        return "Liga";
    }
    if (lower.find("decision") != std::string::npos ||
        lower.find("noticia") != std::string::npos ||
        lower.find("inbox") != std::string::npos) {
        return "Noticias";
    }
    return "Inicio";
}

std::string inferActionCommand(const std::string& text) {
    const std::string lower = toLower(text);
    if (lower.find("fatiga") != std::string::npos || lower.find("fisico") != std::string::npos) return "Gestionar carga";
    if (lower.find("moral") != std::string::npos || lower.find("vestuario") != std::string::npos) return "Reunion/charla";
    if (lower.find("lesion") != std::string::npos) return "Revisar medico";
    if (lower.find("contrato") != std::string::npos || lower.find("renov") != std::string::npos) return "Revisar contrato";
    if (lower.find("fich") != std::string::npos || lower.find("mercado") != std::string::npos) return "Abrir mercado";
    if (lower.find("presupuesto") != std::string::npos || lower.find("finanz") != std::string::npos) return "Revisar caja";
    if (lower.find("objetivo") != std::string::npos || lower.find("directiva") != std::string::npos) return "Revisar objetivo";
    if (lower.find("rival") != std::string::npos || lower.find("partido") != std::string::npos) return "Preparar partido";
    if (lower.find("decision") != std::string::npos) return "Aplicar decision";
    return "Revisar";
}

void pushDashboardActionRow(gui_win32::ListPanelModel& model,
                            const std::string& priority,
                            const std::string& text,
                            const std::string& destination = std::string(),
                            const std::string& command = std::string()) {
    if (text.empty() || model.rows.size() >= 7) return;
    model.rows.push_back({
        priority,
        destination.empty() ? inferActionDestination(text) : destination,
        command.empty() ? inferActionCommand(text) : command,
        text
    });
}

std::vector<std::string> latestPostWeekDigestLines(const Career& career) {
    std::vector<std::string> lines;
    for (auto it = career.managerInbox.rbegin(); it != career.managerInbox.rend(); ++it) {
        if (it->find("[Centro semanal]") == std::string::npos) continue;

        std::string entry = *it;
        const std::string prefix = "[Centro semanal]";
        const size_t prefixPos = entry.find(prefix);
        if (prefixPos != std::string::npos) {
            entry = trim(entry.substr(prefixPos + prefix.size()));
        }

        const std::vector<std::string> parts = splitByDelimiter(entry, '|');
        for (const std::string& raw : parts) {
            const std::string part = trim(raw);
            if (!part.empty() && std::find(lines.begin(), lines.end(), part) == lines.end()) {
                lines.push_back(part);
            }
            if (lines.size() >= 4) break;
        }
        if (lines.empty() && !trim(entry).empty()) lines.push_back(trim(entry));
        break;
    }
    return lines;
}

std::string postWeekDecisionLine(const std::vector<std::string>& digest) {
    for (const std::string& line : digest) {
        if (toLower(line).find("decision:") != std::string::npos) return line;
    }
    return digest.empty() ? std::string() : digest.front();
}

struct TacticalRead {
    int familiarity = 50;
    int risk = 35;
    int attackDuties = 0;
    int supportDuties = 0;
    int defendDuties = 0;
    int lowFitnessPlayers = 0;
    int roleWarnings = 0;
    int averageDiscipline = 0;
    int averageChemistry = 0;
};

std::string activeDutyFor(const Player& player) {
    return player.roleDuty.empty() ? defaultDutyForPosition(player.position) : player.roleDuty;
}

bool tacticTextContains(const Team& team, const std::string& needle) {
    const std::string text = toLower(team.tactics + " " + team.matchInstruction + " " + team.trainingFocus);
    return text.find(needle) != std::string::npos;
}

bool usesHighPressPlan(const Team& team) {
    return team.pressingIntensity >= 4 ||
           tacticTextContains(team, "press") ||
           tacticTextContains(team, "presion");
}

bool usesLowBlockPlan(const Team& team) {
    return team.defensiveLine <= 2 ||
           tacticTextContains(team, "bloque bajo") ||
           tacticTextContains(team, "defensive");
}

int roleFitScore(const Team& team, const Player& player) {
    const std::string pos = normalizePosition(player.position);
    const std::string duty = toLower(activeDutyFor(player));
    int score = 58;

    score += (player.tacticalDiscipline - 55) / 3;
    score += (player.chemistry - 55) / 5;
    score += (player.currentForm - 55) / 6;
    score += (player.fitness - 65) / 7;

    if (pos == "ARQ") {
        score += duty.find("defensa") != std::string::npos ? 8 : -6;
    } else if (pos == "DEF") {
        score += duty.find("defensa") != std::string::npos ? 9 : -5;
    } else if (pos == "MED") {
        score += duty.find("apoyo") != std::string::npos ? 8 : 1;
    } else if (pos == "DEL") {
        score += duty.find("ataque") != std::string::npos ? 8 : -2;
    }

    if (usesHighPressPlan(team)) {
        score += player.stamina >= 72 ? 7 : (player.stamina < 60 ? -11 : 0);
        score += player.fitness >= 72 ? 5 : (player.fitness < 62 ? -9 : 0);
        if (pos == "DEL" && duty.find("apoyo") != std::string::npos) score += 3;
        if (pos == "DEF" && duty.find("ataque") != std::string::npos) score -= 6;
    }

    if (usesLowBlockPlan(team)) {
        score += player.defense >= player.attack ? 5 : -3;
        if (pos == "DEF" && duty.find("defensa") != std::string::npos) score += 5;
        if (pos != "DEL" && duty.find("ataque") != std::string::npos) score -= 4;
    }

    if (team.tempo >= 4) {
        score += player.skill >= 68 ? 4 : -3;
        score += player.currentForm >= 62 ? 3 : -4;
    }
    if (team.width >= 4 && player.versatility >= 62) score += 3;
    if (player.injured || player.matchesSuspended > 0) score -= 25;

    return clampInt(score, 1, 99);
}

std::string roleFitLabel(const Team& team, const Player& player) {
    const int score = roleFitScore(team, player);
    if (score >= 78) return "Encaje alto " + std::to_string(score);
    if (score >= 62) return "Encaje medio " + std::to_string(score);
    if (score >= 48) return "Ajuste delicado " + std::to_string(score);
    return "Riesgo de rol " + std::to_string(score);
}

std::string tacticalFamiliarityLabel(int score) {
    if (score >= 82) return "Fluida";
    if (score >= 68) return "Fuerte";
    if (score >= 52) return "Mixta";
    return "Fragil";
}

std::string tacticalRiskLabel(int risk) {
    if (risk >= 72) return "Alto";
    if (risk >= 50) return "Medio";
    if (risk >= 30) return "Controlado";
    return "Bajo";
}

TacticalRead buildTacticalRead(const Team& team, const std::vector<int>& startingXi, bool congestedWeek) {
    TacticalRead read;
    int totalDiscipline = 0;
    int totalChemistry = 0;
    int totalForm = 0;
    int totalFitness = 0;
    int count = 0;

    for (int index : startingXi) {
        if (index < 0 || index >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(index)];
        const std::string duty = toLower(activeDutyFor(player));
        if (duty.find("ataque") != std::string::npos) {
            ++read.attackDuties;
        } else if (duty.find("defensa") != std::string::npos) {
            ++read.defendDuties;
        } else {
            ++read.supportDuties;
        }

        totalDiscipline += player.tacticalDiscipline;
        totalChemistry += player.chemistry;
        totalForm += player.currentForm;
        totalFitness += player.fitness;
        if (player.fitness < 64 || player.fatigueLoad >= 58) ++read.lowFitnessPlayers;
        if (roleFitScore(team, player) < 58) ++read.roleWarnings;
        ++count;
    }

    count = std::max(1, count);
    read.averageDiscipline = totalDiscipline / count;
    read.averageChemistry = totalChemistry / count;
    const int averageForm = totalForm / count;
    const int averageFitness = totalFitness / count;

    int familiarity = (read.averageDiscipline * 3 + read.averageChemistry * 2 +
                       averageForm + averageFitness + team.morale) / 8;
    if (toLower(team.trainingFocus).find("tact") != std::string::npos) familiarity += 5;
    if (toLower(team.trainingFocus).find("balance") != std::string::npos) familiarity += 2;
    if (usesHighPressPlan(team) && read.averageDiscipline < 60) familiarity -= 5;
    if (usesLowBlockPlan(team) && read.defendDuties < 4) familiarity -= 4;
    familiarity -= read.roleWarnings * 3;
    familiarity -= std::max(0, read.lowFitnessPlayers - 2) * 2;
    read.familiarity = clampInt(familiarity, 1, 99);

    int risk = 18;
    risk += std::max(0, team.pressingIntensity - 3) * 13;
    risk += std::max(0, team.tempo - 3) * 10;
    risk += std::max(0, team.defensiveLine - 3) * 7;
    risk += std::max(0, team.width - 4) * 3;
    risk += read.lowFitnessPlayers * 4;
    risk += read.roleWarnings * 3;
    if (congestedWeek) risk += 12;
    const std::string focus = toLower(team.trainingFocus);
    if (focus.find("recuper") != std::string::npos) risk -= 10;
    if (focus.find("fisico") != std::string::npos) risk += 8;
    const std::string rotation = toLower(team.rotationPolicy);
    if (rotation.find("rot") != std::string::npos || rotation.find("reserv") != std::string::npos) risk -= 5;
    read.risk = clampInt(risk, 0, 100);

    return read;
}

std::string tacticalRoleBalanceLine(const TacticalRead& read) {
    std::ostringstream out;
    out << "Ataque " << read.attackDuties
        << " | Apoyo " << read.supportDuties
        << " | Defensa " << read.defendDuties;
    return out.str();
}

std::string tacticalRecommendation(const Team& team, const TacticalRead& read) {
    if (read.familiarity < 50 && read.risk >= 60) {
        return "Baja presion/ritmo o dedica el microciclo a tactica antes del partido.";
    }
    if (read.roleWarnings >= 3) {
        return "Revisa roles del XI: hay varios jugadores fuera de encaje natural.";
    }
    if (read.lowFitnessPlayers >= 3) {
        return "Rota piezas o usa Recuperacion para sostener el plan sin romper fisico.";
    }
    if (usesHighPressPlan(team) && read.risk >= 55) {
        return "Presion viable, pero prepara cambios temprano y laterales frescos.";
    }
    if (usesLowBlockPlan(team) && read.defendDuties < 4) {
        return "El bloque pide mas deberes defensivos para cerrar area y segundas jugadas.";
    }
    if (read.familiarity >= 75 && read.risk <= 45) {
        return "Plan estable: conviene mantener roles y ajustar solo por rival.";
    }
    return "Plan util, revisar rival y forma antes de simular.";
}

std::string compactTacticalToken(const Team& team, const Player& player) {
    std::ostringstream out;
    out << player.name << " [" << player.role << "/" << activeDutyFor(player)
        << " | " << roleFitLabel(team, player) << "]";
    return out.str();
}

std::string tacticalPitchLine(const Team& team,
                              const std::string& label,
                              const std::vector<const Player*>& players) {
    std::ostringstream out;
    out << label << ": ";
    if (players.empty()) {
        out << "-";
        return out.str();
    }
    for (size_t i = 0; i < players.size(); ++i) {
        if (i) out << "  /  ";
        out << compactTacticalToken(team, *players[i]);
    }
    return out.str();
}

std::string buildTacticalPitchText(const Team& team, const std::vector<int>& startingXi) {
    std::vector<const Player*> goalkeepers;
    std::vector<const Player*> defenders;
    std::vector<const Player*> midfielders;
    std::vector<const Player*> forwards;

    for (int index : startingXi) {
        if (index < 0 || index >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(index)];
        const std::string position = normalizePosition(player.position);
        if (position == "ARQ") goalkeepers.push_back(&player);
        else if (position == "DEF") defenders.push_back(&player);
        else if (position == "MED") midfielders.push_back(&player);
        else if (position == "DEL") forwards.push_back(&player);
    }

    std::ostringstream out;
    out << "Mapa tactico\r\n";
    out << "Sistema " << team.formation
        << " | mentalidad " << team.tactics
        << " | instruccion " << team.matchInstruction << "\r\n";
    out << tacticalPitchLine(team, "DEL", forwards) << "\r\n";
    out << tacticalPitchLine(team, "MED", midfielders) << "\r\n";
    out << tacticalPitchLine(team, "DEF", defenders) << "\r\n";
    out << tacticalPitchLine(team, "ARQ", goalkeepers);
    return out.str();
}

}  // namespace

namespace gui_win32 {

GuiPageModel buildDashboardModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    const auto actionLines = manager_advice::buildManagerActionLines(state.career, 4);
    const auto storyLines = manager_advice::buildCareerStorylines(state.career, 3);
    const auto weeklyFocus = weekly_focus_service::buildWeeklyFocusSnapshot(state.career, 3, 4, 3);
    const auto weeklyDecisionOptions = buildWeeklyDecisionOptions(state.career);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = state.career.myTeam
        ? weeklyFocus.headline
        : state.gameSetup.inlineMessage;
    model.summary.title = "UpcomingMatchWidget";
    model.primary = buildLeagueTableModel(state.career, "Grupo actual");
    model.secondary = buildTeamStatusModel(state.career);
    model.footer = buildInjuryModel(state.career);
    model.detail.title = "LastResultPanel";
    model.feed.title = "NewsFeedPanel";
    model.feed.lines = state.currentFilter == "Alertas" ? alerts : buildFeedLines(state.career, state.currentFilter == "Todo" ? "" : state.currentFilter);

    if (!state.career.myTeam) {
        const Team* setupTeam = nullptr;
        for (const auto& team : state.career.allTeams) {
            if (team.division == state.gameSetup.division && team.name == state.gameSetup.club) {
                setupTeam = &team;
                break;
            }
        }
        const bool hasDivision = !state.gameSetup.division.empty();
        const bool hasClub = setupTeam != nullptr;
        const bool hasManager = !state.gameSetup.manager.empty();
        const int completedSteps = static_cast<int>(hasDivision) + static_cast<int>(hasClub) + static_cast<int>(hasManager);
        const std::string stepLine = state.gameSetup.currentStep == 1 ? "Paso actual: 1. Elige division"
                                   : state.gameSetup.currentStep == 2 ? "Paso actual: 2. Elige club"
                                                                      : (state.gameSetup.ready ? "Paso actual: listo para crear carrera"
                                                                                               : "Paso actual: 3. Nombra al manager");
        model.summary.title = "LaunchChecklistPanel";
        model.detail.title = "GameSetupStatusPanel";
        model.feed.title = "GameSetupChecklistPanel";
        model.summary.content =
            "Flujo de inicio\r\n\r\n"
            "[1] Division\r\n"
            "Estado: " + std::string(hasDivision ? "OK" : "PENDIENTE") + "\r\n"
            "Actual: " + (hasDivision ? divisionDisplay(state.gameSetup.division) : std::string("Sin elegir")) + "\r\n"
            "Define el universo competitivo y habilita el listado de clubes.\r\n\r\n"
            "[2] Club\r\n"
            "Estado: " + std::string(hasClub ? "OK" : "PENDIENTE") + "\r\n"
            "Actual: " + (hasClub ? setupTeam->name : std::string("Sin elegir")) + "\r\n"
            "Presupuesto base: " + (hasClub ? formatMoneyValue(setupTeam->budget) : std::string("No disponible")) + "\r\n\r\n"
            "[3] Manager\r\n"
            "Estado: " + std::string(hasManager ? "OK" : "PENDIENTE") + "\r\n"
            "Actual: " + (hasManager ? state.gameSetup.manager : std::string("Ingresa nombre del manager")) + "\r\n" +
            (state.gameSetup.managerError.empty() ? std::string("Validacion en vivo activa.") : ("Error: " + state.gameSetup.managerError)) +
            "\r\n\r\n"
            "[Desbloqueas al iniciar]\r\n"
            "- centro del club, partido y clasificacion\r\n"
            "- mercado, contratos, finanzas y scouting\r\n"
            "- cantera, directiva y noticias";
        model.detail.content =
            "Estado de setup\r\n\r\n"
            "Division: " + (hasDivision ? divisionDisplay(state.gameSetup.division) : std::string("Pendiente")) + "\r\n"
            "Club: " + (hasClub ? setupTeam->name : std::string("Pendiente")) + "\r\n"
            "Manager: " + (hasManager ? state.gameSetup.manager : std::string("Pendiente")) + "\r\n"
            "Progreso: " + std::to_string(completedSteps) + "/3 pasos completados\r\n"
            "Inicio habilitado: " + std::string(state.gameSetup.ready ? "SI" : "NO") + "\r\n"
            + stepLine + "\r\n\r\n"
            "Validacion automatica\r\n"
            + (state.gameSetup.ready
                   ? std::string("No hay errores bloqueantes. Puedes crear la partida ahora.")
                   : state.gameSetup.inlineMessage + (state.gameSetup.managerError.empty() ? std::string() : "\r\n" + state.gameSetup.managerError))
            + "\r\n\r\n"
            "Acciones disponibles\r\n"
            "- Nueva partida se activa cuando el checklist queda completo.\r\n"
            "- Cargar abre un guardado existente sin romper este flujo.";
        model.feed.lines.clear();
        model.feed.lines.push_back(std::string(hasDivision ? "[OK] " : "[X] ") + "Division " + (hasDivision ? "elegida" : "pendiente"));
        model.feed.lines.push_back(std::string(hasClub ? "[OK] " : "[X] ") + "Club " + (hasClub ? ("elegido: " + setupTeam->name) : "pendiente"));
        model.feed.lines.push_back(std::string(hasManager ? "[OK] " : "[X] ") + "Manager " + (hasManager ? ("listo: " + state.gameSetup.manager) : "pendiente"));
        model.feed.lines.push_back(std::string(state.gameSetup.ready ? "[OK] " : "[..] ") + stepLine);
        model.feed.lines.push_back("[Carga] Usa Cargar si quieres retomar un guardado existente.");
        return model;
    }

    Team& team = *state.career.myTeam;
    const DressingRoomSnapshot dressing = dressing_room_service::buildSnapshot(team, state.career.currentWeek);
    const bool congestedWeek = isCongestedWeek(state.career);
    const auto hubLines = inbox_service::buildPriorityInboxLines(state.career, 5);
    const auto postWeekDigest = latestPostWeekDigestLines(state.career);
    int injured = 0;
    int lowMorale = 0;
    int suspended = 0;
    int lowFitness = 0;
    int expiringContracts = 0;
    int totalAge = 0;
    int totalFitness = 0;
    int totalForm = 0;
    int totalSkill = 0;
    long long weeklyWages = 0;
    for (const auto& player : team.players) {
        totalAge += player.age;
        totalFitness += player.fitness;
        totalForm += player.currentForm;
        totalSkill += player.skill;
        weeklyWages += player.wage;
        if (player.injured) injured++;
        if (player.matchesSuspended > 0) suspended++;
        if (!player.injured && player.fitness < 68) lowFitness++;
        if (player.contractWeeks > 0 && player.contractWeeks <= 8) expiringContracts++;
        if (player.happiness < 45) lowMorale++;
    }
    const int playerCount = static_cast<int>(team.players.size());
    const int safePlayerCount = std::max(1, playerCount);
    const int avgAge = totalAge / safePlayerCount;
    const int avgFitness = totalFitness / safePlayerCount;
    const int avgForm = totalForm / safePlayerCount;
    const int avgSkill = totalSkill / safePlayerCount;
    const int availablePlayers = std::max(0, playerCount - injured - suspended);

    model.metrics = {
        {"Club", team.name, kThemeAccentBlue},
        {"Presupuesto", formatMoneyValue(team.budget), team.budget >= 0 ? kThemeAccentGreen : kThemeDanger},
        {"Edad media", std::to_string(avgAge), kThemeAccentBlue},
        {"Forma media", std::to_string(avgForm), avgForm >= 60 ? kThemeAccentGreen : (avgForm >= 45 ? kThemeWarning : kThemeDanger)},
        {"Alertas", std::to_string(alerts.size()), alerts.empty() ? kThemeAccentGreen : kThemeWarning}
    };

    model.footer.title = "ActionCuePanel";
    model.footer.columns = {{L"Prioridad", 90}, {L"Destino", 110}, {L"Accion", 150}, {L"Motivo", 420}};
    model.footer.rows.clear();
    if (!postWeekDigest.empty()) {
        pushDashboardActionRow(model.footer,
                               "Alta",
                               "Cierre post-semana: " + postWeekDecisionLine(postWeekDigest),
                               "Inicio",
                               "Aplicar decision");
    }
    if (availablePlayers < 18) {
        pushDashboardActionRow(model.footer,
                               "Alta",
                               "Solo " + std::to_string(availablePlayers) +
                                   " jugadores disponibles para la proxima convocatoria.",
                               "Plantilla",
                               "Ajustar convocatoria");
    }
    if (injured + suspended >= 3) {
        pushDashboardActionRow(model.footer,
                               "Alta",
                               "Bajas acumuladas: " + std::to_string(injured) +
                                   " lesionado(s) y " + std::to_string(suspended) + " suspendido(s).",
                               "Plantilla",
                               "Revisar rotacion");
    }
    if (state.career.boardWarningWeeks > 0 || state.career.boardConfidence < 45) {
        pushDashboardActionRow(model.footer,
                               "Alta",
                               "Directiva bajo presion: confianza " +
                                   std::to_string(state.career.boardConfidence) +
                                   "/100 y advertencias " + std::to_string(state.career.boardWarningWeeks) + ".",
                               "Directiva",
                               "Revisar objetivo");
    }
    if (avgFitness < 72 || lowFitness >= 4) {
        pushDashboardActionRow(model.footer,
                               "Alta",
                               "Fisico medio " + std::to_string(avgFitness) +
                                   " y " + std::to_string(lowFitness) + " jugador(es) bajo 68.",
                               "Plantilla",
                               "Gestionar carga");
    }
    if (expiringContracts > 0) {
        pushDashboardActionRow(model.footer,
                               "Alta",
                               std::to_string(expiringContracts) + " contrato(s) vencen en 8 semanas o menos.",
                               "Fichajes",
                               "Revisar contrato");
    }
    if (weeklyWages > 0 && team.budget < weeklyWages * 6) {
        pushDashboardActionRow(model.footer,
                               "Media",
                               "Caja equivalente a menos de 6 semanas de salarios.",
                               "Finanzas",
                               "Revisar caja");
    }
    if (lowMorale > 0 || team.morale < 52) {
        pushDashboardActionRow(model.footer,
                               "Alta",
                               "Moral colectiva " + std::to_string(team.morale) +
                                   " y " + std::to_string(lowMorale) + " jugador(es) bajo 45.",
                               "Plantilla",
                               "Reunion/charla");
    }
    for (const auto& line : weeklyFocus.priorityLines) {
        pushDashboardActionRow(model.footer, "Alta", line);
    }
    for (const auto& line : actionLines) {
        pushDashboardActionRow(model.footer, model.footer.rows.size() < 3 ? "Alta" : "Media", line);
    }
    for (const auto& option : weeklyDecisionOptions) {
        pushDashboardActionRow(model.footer, "Media", option, "Noticias", "Aplicar decision");
    }
    for (const auto& alert : alerts) {
        pushDashboardActionRow(model.footer, "Media", alert, inferActionDestination(alert), "Atender alerta");
    }
    if (model.footer.rows.empty()) {
        model.footer.rows.push_back({"Baja", "Inicio", "Simular", "No hay alertas criticas: puedes preparar el partido y avanzar la semana."});
    }

    std::ostringstream out;
    out << findNextMatchLine(state.career) << "\r\n\r\n";
    out << "Posicion " << state.career.currentCompetitiveRank() << "/" << state.career.currentCompetitiveFieldSize()
        << " | Puntos " << team.points << " | DG " << (team.goalsFor - team.goalsAgainst) << "\r\n";
    out << "Moral " << team.morale << " | Lesionados " << injured << " | Tension " << dressing.socialTension << "\r\n";
    out << "Plan semanal: " << team.trainingFocus << (congestedWeek ? " | semana congestionada" : " | semana regular") << "\r\n";
    out << "Objetivo: " << (state.career.boardMonthlyObjective.empty() ? "Sin objetivo mensual activo" : state.career.boardMonthlyObjective) << "\r\n\r\n";
    if (!postWeekDigest.empty()) {
        out << "Cierre post-semana\r\n";
        for (const auto& line : postWeekDigest) out << "- " << line << "\r\n";
        out << "\r\n";
    }
    out << "KPIs del club\r\n";
    out << "- Plantel " << playerCount << " | Disponibles " << availablePlayers << "/" << playerCount
        << " | Edad media " << avgAge << "\r\n";
    out << "- Skill medio " << avgSkill << " | Fisico medio " << avgFitness
        << " | Forma media " << avgForm << "\r\n";
    out << "- Salarios semanales " << formatMoneyValue(weeklyWages)
        << " | Contratos <=8s " << expiringContracts << "\r\n\r\n";
    if (availablePlayers < 18 || injured + suspended >= 3 ||
        state.career.boardWarningWeeks > 0 || state.career.boardConfidence < 45) {
        out << "Riesgos inmediatos\r\n";
        if (availablePlayers < 18) {
            out << "- Convocatoria corta: " << availablePlayers << " disponibles.\r\n";
        }
        if (injured + suspended >= 3) {
            out << "- Bajas acumuladas: " << injured << " lesionado(s), " << suspended << " suspendido(s).\r\n";
        }
        if (state.career.boardWarningWeeks > 0 || state.career.boardConfidence < 45) {
            out << "- Directiva: confianza " << state.career.boardConfidence
                << "/100 | advertencias " << state.career.boardWarningWeeks << "\r\n";
        }
        out << "\r\n";
    }
    if (!weeklyFocus.priorityLines.empty()) {
        out << "Cockpit semanal\r\n";
        for (const auto& line : weeklyFocus.priorityLines) out << "- " << line << "\r\n";
        out << "\r\n";
    }
    if (!weeklyFocus.kpiLines.empty()) {
        out << "KPIs accionables\r\n";
        for (const auto& line : weeklyFocus.kpiLines) out << "- " << line << "\r\n";
        out << "\r\n";
    }
    if (!actionLines.empty()) {
        out << "Decisiones sugeridas\r\n";
        for (const auto& line : actionLines) out << "- " << line << "\r\n";
        out << "\r\n";
    }
    if (!weeklyDecisionOptions.empty()) {
        out << "Centro semanal\r\n";
        for (size_t i = 0; i < weeklyDecisionOptions.size() && i < 4; ++i) out << "- " << weeklyDecisionOptions[i] << "\r\n";
        out << "Usa Noticias > Decidir para aplicar la recomendacion automatica.\r\n\r\n";
    }
    if (!storyLines.empty()) {
        out << "Clima de la semana\r\n";
        for (const auto& line : storyLines) out << "- " << line << "\r\n";
        out << "\r\n";
    }
    if (!hubLines.empty()) {
        out << "Centro del manager\r\n";
        for (size_t i = 0; i < hubLines.size() && i < 3; ++i) {
            out << "- " << hubLines[i] << "\r\n";
        }
        out << "\r\n";
    }
    out << "Microciclo\r\n" << trainingSchedulePreview(team, congestedWeek) << "\r\n\r\n";
    out << "Lectura del rival\r\n" << buildOpponentReport(state.career);
    const auto opponentPlan = buildNextOpponentPlanLines(state.career, 4);
    if (!opponentPlan.empty()) {
        out << "\r\n\r\nPlan de proximo rival\r\n";
        for (const auto& line : opponentPlan) out << "- " << line << "\r\n";
    }
    model.summary.content = out.str();

    const auto analyticsLines = analytics_service::buildTeamAnalyticsLines(state.career, team);
    std::ostringstream detail;
    if (!postWeekDigest.empty()) {
        detail << "Cierre post-semana\r\n";
        for (const auto& line : postWeekDigest) detail << "- " << line << "\r\n";
        detail << "\r\n";
    }
    detail << "Ultimo resultado\r\n";
    detail << lastMatchPanelText(state.career, 6, 6) << "\r\n\r\n";
    detail << "Impacto inmediato\r\n";
    detail << "Confianza " << state.career.boardConfidence << "/100"
           << " | Progreso " << state.career.boardMonthlyProgress << "/" << state.career.boardMonthlyTarget << "\r\n";
    detail << "Apoyo de lideres " << dressing.leadershipSupport
           << " | Aislados " << dressing.isolatedPlayers
           << " | Promesas en riesgo " << dressing.promiseRiskCount << "\r\n";
    detail << "DT del club " << team.headCoachName
           << " | Seguridad " << team.jobSecurity
           << " | Politica " << team.transferPolicy << "\r\n";
    if (!analyticsLines.empty()) {
        detail << analyticsLines.front() << "\r\n";
        if (analyticsLines.size() > 1) detail << analyticsLines[1] << "\r\n";
        if (analyticsLines.size() > 2) detail << analyticsLines[2] << "\r\n";
    }
    if (!weeklyFocus.kpiLines.empty()) {
        detail << "\r\nKPIs accionables\r\n";
        for (const auto& line : weeklyFocus.kpiLines) detail << "- " << line << "\r\n";
    }
    if (!actionLines.empty()) {
        detail << "\r\nAcciones sugeridas\r\n";
        for (const auto& line : actionLines) detail << "- " << line << "\r\n";
    }
    if (!weeklyFocus.tutorialLines.empty()) {
        detail << "\r\nAyuda contextual\r\n";
        for (const auto& line : weeklyFocus.tutorialLines) detail << "- " << line << "\r\n";
    }
    if (!storyLines.empty()) {
        detail << "\r\nNarrativa de la semana\r\n";
        for (const auto& line : storyLines) detail << "- " << line << "\r\n";
    }
    detail << "\r\n" << staff_service::buildStaffRecommendationDigest(state.career, 4);
    detail << "\r\n" << dressingRoomPanelText(state.career, 4);
    model.detail.content = detail.str();
    return model;
}

GuiPageModel buildSquadModel(AppState& state, bool youthOnly) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = youthOnly ? "Cantera con filtro por edad y potencial." : "Plantilla completa con orden por columnas y ficha detallada.";
    model.summary.title = youthOnly ? "YouthSummaryPanel" : "SquadSummaryPanel";
    model.primary = state.career.myTeam ? buildSquadUnitModel(*state.career.myTeam) : ListPanelModel{};
    model.secondary = buildPlayerTableModel(state, youthOnly);
    model.feed.title = "AlertPanel";
    model.feed.lines = alerts;

    const Player* selected = nullptr;
    if (state.career.myTeam) {
        selected = findPlayerByName(*state.career.myTeam, state.selectedPlayerName);
    }
    model.footer = buildComparisonModel(state.career, selected);
    model.detail.title = "PlayerProfilePanel";
        model.detail.content = state.career.myTeam ? buildPlayerProfile(*state.career.myTeam, selected)
                                               : "Selecciona una carrera para mostrar jugadores.";

    if (!state.career.myTeam) {
        model.summary.content = "No hay plantilla disponible.";
        return model;
    }

    Team& team = *state.career.myTeam;
    int avgAge = 0;
    int avgFitness = 0;
    int avgPotential = 0;
    int count = 0;
    for (const auto& player : team.players) {
        if (youthOnly && player.age > 23) continue;
        avgAge += player.age;
        avgFitness += player.fitness;
        avgPotential += player.potential;
        count++;
    }
    count = std::max(1, count);
    const SquadNeedReport planner = ai_squad_planner::analyzeSquad(team);
    std::ostringstream out;
    out << "Jugadores visibles " << count
        << " | Edad media " << (avgAge / count)
        << " | Potencial medio " << (avgPotential / count)
        << " | Fisico medio " << (avgFitness / count) << "\r\n";
    out << "Filtro actual: " << state.currentFilter << "\r\n";
    out << "Orden: columna " << state.squadSort.column + 1
        << (state.squadSort.ascending ? " asc" : " desc") << "\r\n";
    out << "Planner: necesidad " << planner.weakestPosition
        << " | exceso " << planner.surplusPosition
        << " | riesgo de rotacion " << planner.rotationRisk << "\r\n";
    out << "Cobertura juvenil: " << (planner.youthCoverPositions.empty() ? std::string("-") : joinStringValues(planner.youthCoverPositions, ", ")) << "\r\n";
    out << "Ventas probables: " << (planner.saleCandidates.empty() ? std::string("-") : joinStringValues(planner.saleCandidates, ", ")) << "\r\n";
    model.summary.content = out.str();
    return model;
}

GuiPageModel buildTacticsModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    const bool highPressFocus = state.currentFilter == "Presion alta";
    const bool lowBlockFocus = state.currentFilter == "Bloque bajo";
    model.infoLine = highPressFocus
        ? "Enfoque en presion alta: recuperacion, desgaste y riesgo a la espalda."
        : (lowBlockFocus
               ? "Enfoque en bloque bajo: proteccion del area, centros rivales y salida tras recuperacion."
               : "Lee el plan tactico, el once inicial y el impacto esperado de cada ajuste.");
    model.summary.title = "TacticalSummary";
    model.primary.title = "TacticsBoard";
    model.primary.columns = {{L"Variable", 130}, {L"Valor", 135}, {L"Efecto estimado", 360}};
    model.secondary.title = "FormationSelector";
    model.secondary.columns = {{L"Linea", 90}, {L"Jugador", 200}, {L"Pos", 60}, {L"Hab", 50}, {L"Fisico", 60}, {L"Estado", 120}};
    model.footer.title = "TacticalImpactSummary";
    model.footer.columns = {{L"Area", 130}, {L"Lectura", 220}, {L"Riesgo", 220}};
    model.detail.title = "MatchAnalysisPanel";
    model.feed.title = "AlertPanel";
    model.feed.lines = highPressFocus ? focusedAlertLines(alerts, {"fatiga", "lesion"})
                                      : (lowBlockFocus ? buildFeedLines(state.career, "Resultados", 8) : alerts);

    if (!state.career.myTeam) {
        model.summary.content = "No hay equipo para editar tactica.";
        model.detail.content = "Inicia una carrera.";
        return model;
    }

    Team& team = *state.career.myTeam;
    const bool congestedWeek = isCongestedWeek(state.career);
    const std::vector<int> startingXi = team.getStartingXIIndices();
    const TacticalRead tacticalRead = buildTacticalRead(team, startingXi, congestedWeek);
    const std::string familiarityLabel = tacticalFamiliarityLabel(tacticalRead.familiarity);
    const std::string riskLabel = tacticalRiskLabel(tacticalRead.risk);
    model.summary.content =
        "Formacion " + team.formation + " | Mentalidad " + team.tactics +
        "\r\nPresion " + std::to_string(team.pressingIntensity) +
        " | Ritmo " + std::to_string(team.tempo) +
        " | Anchura " + std::to_string(team.width) +
        " | Linea " + std::to_string(team.defensiveLine) +
        "\r\nFamiliaridad tactica: " + familiarityLabel + " " + std::to_string(tacticalRead.familiarity) + "/100" +
        " | Riesgo del plan: " + riskLabel + " " + std::to_string(tacticalRead.risk) + "/100" +
        "\r\nRoles XI: " + tacticalRoleBalanceLine(tacticalRead) +
        " | Alertas de encaje: " + std::to_string(tacticalRead.roleWarnings) +
        "\r\nInstruccion actual: " + team.matchInstruction +
        " | Plan semanal: " + team.trainingFocus +
        "\r\nFiltro tactico: " + state.currentFilter;

    model.primary.rows.push_back({"Presion", std::to_string(team.pressingIntensity),
                                  team.pressingIntensity >= 4 ? "Recupera arriba, sube fatiga" : "Presion mas contenida"});
    model.primary.rows.push_back({"Ritmo", std::to_string(team.tempo),
                                  team.tempo >= 4 ? "Mas llegadas, mas perdida" : "Mas control del juego"});
    model.primary.rows.push_back({"Anchura", std::to_string(team.width),
                                  team.width >= 4 ? "Abre carriles y centros" : "Compacta por dentro"});
    model.primary.rows.push_back({"Linea", std::to_string(team.defensiveLine),
                                  team.defensiveLine >= 4 ? "Recupera alto, riesgo al balon largo" : "Protege espalda y concede campo"});
    model.primary.rows.push_back({"Instruccion", team.matchInstruction,
                                  team.matchInstruction == "Juego directo" ? "Acelera llegada a ultimo tercio" : "Ajuste situacional"});
    model.primary.rows.push_back({"Plan semanal", team.trainingFocus,
                                  congestedWeek ? "Microciclo corto y conservador" : "Carga completa de preparacion"});
    model.primary.rows.push_back({"Familiaridad", familiarityLabel + " " + std::to_string(tacticalRead.familiarity) + "/100",
                                  tacticalRecommendation(team, tacticalRead)});
    model.primary.rows.push_back({"Riesgo plan", riskLabel + " " + std::to_string(tacticalRead.risk) + "/100",
                                  tacticalRead.risk >= 60 ? "Gestiona minutos y cambios antes de simular" : "Carga asumible para el XI"});
    model.primary.rows.push_back({"Balance roles", tacticalRoleBalanceLine(tacticalRead),
                                  tacticalRead.roleWarnings > 0 ? "Hay roles que conviene ajustar" : "XI coherente con el plan"});

    std::map<std::string, std::vector<const Player*> > byLine;
    for (int index : startingXi) {
        if (index < 0 || index >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(index)];
        byLine[normalizePosition(player.position)].push_back(&player);
    }
    model.secondary.columns = {{L"Linea", 75}, {L"Jugador", 170}, {L"Pos", 50}, {L"Rol", 130}, {L"Encaje", 130}, {L"Fisico", 60}, {L"Estado", 110}};
    for (const auto& entry : byLine) {
        for (const Player* player : entry.second) {
            model.secondary.rows.push_back({
                entry.first,
                player->name,
                normalizePosition(player->position),
                player->role.empty() ? activeDutyFor(*player) : player->role + "/" + activeDutyFor(*player),
                roleFitLabel(team, *player),
                std::to_string(player->fitness),
                playerStatus(*player)
            });
        }
    }

    model.footer.rows.push_back({"Enfoque",
                                 highPressFocus ? "Presion alta" : (lowBlockFocus ? "Bloque bajo" : "Balanceado"),
                                 highPressFocus ? "Sube robos y desgaste"
                                                : (lowBlockFocus ? "Protege espalda y concede centros"
                                                                 : "Vista transversal del plan")});
    model.footer.rows.push_back({"Familiaridad",
                                 familiarityLabel + " " + std::to_string(tacticalRead.familiarity) + "/100",
                                 tacticalRecommendation(team, tacticalRead)});
    model.footer.rows.push_back({"Roles XI",
                                 tacticalRoleBalanceLine(tacticalRead),
                                 tacticalRead.roleWarnings > 0 ? std::to_string(tacticalRead.roleWarnings) + " encajes delicados" : "Sin alertas fuertes"});
    model.footer.rows.push_back({"Posesion", team.tempo >= 4 ? "Vertical" : "Controlada", team.pressingIntensity >= 4 ? "Transiciones largas" : "Menor recuperacion alta"});
    model.footer.rows.push_back({"Ocasiones", team.width >= 4 ? "Ataque por fuera" : "Juego interior", team.tempo >= 4 ? "Mas error tecnico" : "Menos volumen"});
    model.footer.rows.push_back({"Defensa", team.defensiveLine >= 4 ? "Bloque adelantado" : "Bloque medio/bajo", team.defensiveLine >= 4 ? "Espalda expuesta" : "Mayor asedio rival"});
    model.footer.rows.push_back({"Fatiga", congestedWeek ? "Gestion conservadora" : "Carga normal", team.pressingIntensity >= 4 ? "El XI llega mas exigido" : "Riesgo medio"});

    std::ostringstream detail;
    if (highPressFocus) {
        detail << "Enfoque activo: presion alta.\r\n";
        detail << "Prioriza distancia corta entre lineas, piernas frescas y suplentes listos para sostener la agresividad.\r\n\r\n";
    } else if (lowBlockFocus) {
        detail << "Enfoque activo: bloque bajo.\r\n";
        detail << "Prioriza proteger el area, vigilar segundas jugadas y preparar transiciones mas limpias tras recuperar.\r\n\r\n";
    } else {
        detail << "Enfoque activo: balanceado.\r\n";
        detail << "La vista mezcla presion, ritmo y proteccion del bloque para leer el plan completo.\r\n\r\n";
    }
    detail << "Presion alta aumenta recuperaciones, pero castiga a jugadores con fisico bajo.\r\n";
    detail << "Ritmo alto acelera la llegada, aunque empeora la precision en equipos con forma baja.\r\n";
    detail << "Bloque bajo reduce espacio interior y aumenta probabilidad de centros rivales.\r\n\r\n";
    detail << "Lectura de familiaridad\r\n";
    detail << "- Familiaridad tactica: " << familiarityLabel << " " << tacticalRead.familiarity << "/100"
           << " | disciplina media " << tacticalRead.averageDiscipline
           << " | quimica media " << tacticalRead.averageChemistry << "\r\n";
    detail << "- Riesgo del plan: " << riskLabel << " " << tacticalRead.risk << "/100"
           << " | jugadores con carga " << tacticalRead.lowFitnessPlayers
           << " | encajes delicados " << tacticalRead.roleWarnings << "\r\n";
    detail << "- Balance de roles: " << tacticalRoleBalanceLine(tacticalRead) << "\r\n";
    detail << "- Recomendacion: " << tacticalRecommendation(team, tacticalRead) << "\r\n\r\n";
    detail << buildTacticalPitchText(team, startingXi) << "\r\n\r\n";
    detail << "Microciclo semanal\r\n" << trainingSchedulePreview(team, congestedWeek, 6) << "\r\n\r\n";
    detail << "Informe rival: " << buildOpponentReport(state.career) << "\r\n\r\n";
    detail << dressingRoomPanelText(state.career, 4) << "\r\n";
    detail << lastMatchPanelText(state.career, 4, 5);
    model.detail.content = detail.str();
    return model;
}

}  // namespace gui_win32

#endif



