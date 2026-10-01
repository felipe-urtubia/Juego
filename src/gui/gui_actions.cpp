#include "gui/gui_internal.h"
#include "gui/gui_audio.h"

#ifdef _WIN32

#include "competition/competition.h"
#include "career/career_runtime.h"
#include "career/game_events_system.h"
#include "engine/game_settings.h"
#include "transfers/negotiation_system.h"
#include "utils/utils.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

namespace gui_win32 {

namespace {

bool containsText(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::wstring buildValidationDialogText(const ValidationSuiteSummary& summary) {
    std::ostringstream out;
    out << "Resumen de validacion\n\n";
    out << "Fallos logicos: " << summary.logicFailureCount << "\n";
    out << "Errores de datos: " << summary.dataErrorCount << "\n";
    out << "Advertencias de datos: " << summary.dataWarningCount << "\n";

    int shown = 0;
    for (const auto& line : summary.lines) {
        if (!(containsText(line, "[FAIL]") || containsText(line, "[ERROR]") || containsText(line, "[WARNING]"))) {
            continue;
        }
        if (shown == 0) out << "\nIncidencias destacadas:";
        if (shown >= 6) {
            out << "\n- ... ver reporte completo en saves/roster_validation_report.txt";
            break;
        }
        out << "\n- " << line;
        ++shown;
    }

    if (shown == 0) {
        out << "\n\nNo se detectaron incidencias activas.";
    }
    out << "\n\nDetalle completo disponible en saves/roster_validation_report.txt";
    return utf8ToWide(out.str());
}

void showServiceMessages(AppState& state, const ServiceResult& result, const std::string& title) {
    if (result.messages.empty()) return;
    std::ostringstream out;
    for (size_t i = 0; i < result.messages.size(); ++i) {
        if (i) out << "\n";
        out << result.messages[i];
    }
    MessageBoxW(state.window,
                utf8ToWide(out.str()).c_str(),
                utf8ToWide(title).c_str(),
                MB_OK | (result.ok ? MB_ICONINFORMATION : MB_ICONWARNING));
}

void showLoadMessagesCompact(AppState& state, const ServiceResult& result, const std::string& title) {
    if (result.messages.empty()) return;

    int generatedTemporarySquads = 0;
    int warningLines = 0;
    std::string auditLine;
    std::vector<std::string> highlights;

    for (const auto& message : result.messages) {
        if (containsText(message, "se genera una base temporal para")) {
            generatedTemporarySquads++;
            continue;
        }
        if (containsText(message, "Errores: ") && containsText(message, "Advertencias:")) {
            auditLine = message;
            continue;
        }
        if (containsText(message, "roster_validation_report.txt")) {
            continue;
        }
        if (containsText(message, "[WARNING]") || containsText(message, "[ERROR]")) {
            warningLines++;
            if (highlights.size() < 3) highlights.push_back(message);
        }
    }

    const bool hasHardAuditErrors = containsText(auditLine, "Errores: ") && !containsText(auditLine, "Errores: 0");
    const bool hasUsefulSummary = generatedTemporarySquads > 0 || !auditLine.empty() || !highlights.empty();
    if (!hasUsefulSummary) return;
    if (generatedTemporarySquads == 0 && !hasHardAuditErrors) return;

    std::ostringstream out;
    out << result.messages.front();

    if (generatedTemporarySquads > 0) {
        out << "\n\nPlantillas temporales generadas: " << generatedTemporarySquads << ".";
    }
    if (!auditLine.empty()) {
        out << "\n" << auditLine;
    }
    if (!highlights.empty()) {
        out << "\n\nIncidencias destacadas:";
        for (const auto& line : highlights) {
            out << "\n- " << line;
        }
    }
    if (warningLines > static_cast<int>(highlights.size())) {
        out << "\n- ... y " << (warningLines - static_cast<int>(highlights.size())) << " incidencia(s) adicional(es).";
    }
    out << "\n\nDetalle completo disponible en saves/roster_validation_report.txt";

    MessageBoxW(state.window,
                utf8ToWide(out.str()).c_str(),
                utf8ToWide(title).c_str(),
                MB_OK | MB_ICONINFORMATION);
}

void recordCriticalGuiServiceEvent(const ServiceResult& result, const std::string& title) {
    if (result.ok) return;

    const std::string detail = result.messages.empty()
        ? "La accion del frontend fallo sin detalle adicional."
        : result.messages.back();

    career_events::EventNotificationSystem::recordEvent(
        career_events::EventType::ManagerAlert,
        "Accion critica: " + title,
        detail
    );
}

void finalizeAction(AppState& state,
                    const ServiceResult& result,
                    const std::string& title,
                    bool forceDialog = false) {
    recordCriticalGuiServiceEvent(result, title);

    if (result.ok) {
        syncCombosFromCareer(state);
        refreshAll(state);
    }
    if (!result.messages.empty()) {
        setStatus(state, result.messages.back());
    }
    if (!result.ok || forceDialog || result.messages.size() > 2) {
        showServiceMessages(state, result, title);
    }
}

bool dashboardShowsPostWeekDigest(const AppState& state) {
    return state.currentPage == GuiPage::Dashboard &&
           (state.currentModel.summary.content.find("Cierre post-semana") != std::string::npos ||
            state.currentModel.detail.content.find("Cierre post-semana") != std::string::npos);
}

void pulseFrontendTiming(AppState& state) {
    const int delay = game_settings::pageTransitionDelayMs(state.settings);
    if (delay <= 0) return;

    UpdateWindow(state.window);
    const DWORD stopTime = GetTickCount() + static_cast<DWORD>(delay);
    MSG msg{};
    while (GetTickCount() < stopTime) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(msg.wParam));
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(4);
    }
}

struct SimulationThreadArgs {
    HWND window = nullptr;
    Career career;
    GameSettings settings;
};

struct SimulationJobResult {
    Career career;
    ServiceResult result;
    std::string managedTeamName;
};

struct SimulationProgressPayload {
    std::string phase;
    std::string detail;
    int percent = 0;
    std::vector<std::string> events;
};

struct SimulationWorkerProgressContext {
    HWND window = nullptr;
    GameSettings settings;
    int spinner = 0;
    std::vector<std::string> events;
    int lastPercent = 35;
};

thread_local SimulationWorkerProgressContext* g_workerProgressContext = nullptr;

constexpr size_t kMaxSimulationProgressEvents = 4;
constexpr size_t kMaxSimulationProgressEventChars = 118;

std::string buildSimulationStatus(const GameSettings& settings, int spinnerIndex) {
    static const char spinner[] = "|/-\\";
    std::ostringstream status;
    status << "Simulando semana en modo "
           << game_settings::simulationModeLabel(settings.simulationMode)
           << " a velocidad "
           << game_settings::simulationSpeedLabel(settings.simulationSpeed)
           << " " << spinner[spinnerIndex % 4];
    return status.str();
}

std::string progressStatusLine(const std::string& phase, const std::string& detail, int percent) {
    std::ostringstream status;
    status << phase << " (" << clampValue(percent, 0, 100) << "%)";
    if (!detail.empty()) status << ": " << detail;
    return status.str();
}

void setSimulationProgress(AppState& state,
                           const std::string& phase,
                           const std::string& detail,
                           int percent,
                           const std::vector<std::string>& events = {}) {
    const bool wasActive = state.simulationProgressActive;
    state.simulationProgressActive = true;
    state.simulationProgressPhase = phase;
    state.simulationProgressDetail = detail;
    state.simulationProgressPercent = clampValue(percent, 0, 100);
    state.simulationProgressEvents = events;
    setStatus(state, progressStatusLine(phase, detail, state.simulationProgressPercent));
    if (state.window && IsWindow(state.window)) {
        if (!wasActive) layoutWindow(state);
        InvalidateRect(state.window, nullptr, TRUE);
    }
}

void clearSimulationProgress(AppState& state) {
    const bool wasActive = state.simulationProgressActive;
    state.simulationProgressActive = false;
    state.simulationProgressPhase.clear();
    state.simulationProgressDetail.clear();
    state.simulationProgressEvents.clear();
    state.simulationProgressPercent = 0;
    if (state.window && IsWindow(state.window)) {
        if (wasActive) layoutWindow(state);
        InvalidateRect(state.window, nullptr, TRUE);
    }
}

std::string managedTeamName(const Career& career) {
    return career.myTeam ? career.myTeam->name : std::string();
}

const Player* findManagedPlayer(const Career& career, const std::string& playerName) {
    if (!career.myTeam) return nullptr;
    for (const Player& player : career.myTeam->players) {
        if (player.name == playerName) return &player;
    }
    return nullptr;
}

std::string recommendedLoanDestination(const Career& career, const Player& player) {
    if (!career.myTeam) return "";
    const std::string role = normalizePosition(player.position);
    const Team* bestTeam = nullptr;
    int bestScore = -100000;
    for (const Team& candidate : career.allTeams) {
        if (&candidate == career.myTeam) continue;
        if (candidate.name == player.parentClub) continue;
        const int maxSquadSize = getCompetitionConfig(candidate.division).maxSquadSize;
        if (maxSquadSize > 0 && static_cast<int>(candidate.players.size()) >= maxSquadSize) continue;

        bool duplicateName = false;
        int sameRoleCount = 0;
        int sameRoleSkill = 0;
        for (const Player& squadPlayer : candidate.players) {
            if (squadPlayer.name == player.name) {
                duplicateName = true;
                break;
            }
            if (normalizePosition(squadPlayer.position) == role) {
                sameRoleCount++;
                sameRoleSkill += squadPlayer.skill;
            }
        }
        if (duplicateName) continue;

        const int averageRoleSkill = sameRoleCount == 0 ? 45 : sameRoleSkill / sameRoleCount;
        int score = 0;
        score += candidate.division == career.myTeam->division ? 10 : 0;
        score += std::max(0, player.skill - averageRoleSkill);
        score += std::max(0, 3 - sameRoleCount) * 8;
        score += std::max(0, 24 - static_cast<int>(candidate.players.size()));
        score += candidate.trainingFacilityLevel + candidate.youthFacilityLevel;
        if (!bestTeam || score > bestScore || (score == bestScore && candidate.name < bestTeam->name)) {
            bestTeam = &candidate;
            bestScore = score;
        }
    }
    return bestTeam ? bestTeam->name : std::string();
}

bool selectedTransferTarget(AppState& state, std::string& sellerTeamName, std::string& playerName) {
    int row = selectedListViewRow(state.tableList);
    if (row >= 0) {
        playerName = listViewText(state.tableList, row, 0);
        sellerTeamName = listViewText(state.tableList, row, 10);
        return !playerName.empty() && !sellerTeamName.empty();
    }
    playerName = state.selectedTransferPlayer;
    sellerTeamName = state.selectedTransferClub;
    return !playerName.empty() && !sellerTeamName.empty();
}

bool selectedManagedPlayerName(AppState& state, std::string& playerName) {
    int row = selectedListViewRow(state.squadList);
    if (row >= 0) {
        playerName = listViewText(state.squadList, row, 0);
        return !playerName.empty();
    }
    playerName = state.selectedPlayerName;
    return !playerName.empty();
}

Player* findMutablePlayer(Team& team, const std::string& playerName) {
    for (Player& player : team.players) {
        if (player.name == playerName) return &player;
    }
    return nullptr;
}

const Player* findPlayerInTeam(const Team& team, const std::string& playerName) {
    for (const Player& player : team.players) {
        if (player.name == playerName) return &player;
    }
    return nullptr;
}

NegotiationPromise guiPromiseForTarget(const Team& buyer, const Player& player) {
    const int avgSkill = buyer.getAverageSkill();
    if (player.skill >= avgSkill + 6) return NegotiationPromise::Starter;
    if (player.age <= 21 && player.potential >= player.skill + 9) return NegotiationPromise::Prospect;
    if (player.skill >= avgSkill - 2) return NegotiationPromise::Rotation;
    return NegotiationPromise::None;
}

NegotiationProfile guiTransferProfileForTarget(const Career& career, const Team& seller, const Player& player) {
    if (!career.myTeam) return NegotiationProfile::Balanced;
    const Team& buyer = *career.myTeam;
    const long long estimatedPackage = player.value + estimatedAgentFee(player, std::max(1LL, player.value)) +
                                       std::max(player.wage, wageDemandFor(player)) * 26;
    const bool keyUpgrade = player.skill >= buyer.getAverageSkill() + 6;
    const bool openMarket = player.contractWeeks <= 16 || player.wantsToLeave;
    const bool rivalryDeal = areRivalClubs(buyer, seller);
    if (keyUpgrade && buyer.budget >= estimatedPackage * 3 / 2) return NegotiationProfile::Safe;
    if (rivalryDeal && buyer.budget >= estimatedPackage * 2) return NegotiationProfile::Safe;
    if (openMarket || buyer.budget < estimatedPackage) return NegotiationProfile::Aggressive;
    return NegotiationProfile::Balanced;
}

NegotiationProfile guiRenewalProfileForPlayer(const Team& team, const Player& player) {
    const bool keyPlayer = player.skill >= team.getAverageSkill() + 4 || player.potential >= 78;
    if (player.contractWeeks <= 4 || player.wantsToLeave || (keyPlayer && player.happiness < 50)) {
        return NegotiationProfile::Safe;
    }
    const long long projectedCost = std::max(player.wage, wageDemandFor(player)) * 12;
    if (!keyPlayer && team.budget < projectedCost) return NegotiationProfile::Aggressive;
    return NegotiationProfile::Balanced;
}

void prependNegotiationPlan(ServiceResult& result,
                            NegotiationProfile profile,
                            NegotiationPromise promise,
                            const std::string& context) {
    std::string line = "Plan de negociacion GUI: perfil " + negotiationLabel(profile) +
                       " | promesa " + promiseLabel(promise);
    if (!context.empty()) line += " | " + context;
    result.messages.insert(result.messages.begin(), line + ".");
}

void relinkCareerPointers(Career& career, const std::string& teamName) {
    std::string divisionId = career.activeDivision;
    if (divisionId.empty() && !teamName.empty()) {
        if (Team* team = career.findTeamByName(teamName)) {
            divisionId = team->division;
        }
    }

    career.refreshActiveDivisionTeamLinks(divisionId);
    career.myTeam = teamName.empty() ? nullptr : career.findTeamByName(teamName);
}

void postSimulationProgress(HWND window,
                            const std::string& phase,
                            const std::string& detail,
                            int percent,
                            const std::vector<std::string>& events = {}) {
    if (!window || !IsWindow(window)) return;
    auto* payload = new SimulationProgressPayload{phase, detail, clampValue(percent, 0, 100), events};
    if (!PostMessageW(window, kGuiSimulationProgressMessage, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
}

std::string compactSimulationEvent(const std::string& message) {
    std::string event = trim(message);
    if (event.empty()) return {};
    std::replace(event.begin(), event.end(), '\r', ' ');
    std::replace(event.begin(), event.end(), '\n', ' ');
    while (event.find("  ") != std::string::npos) {
        event.replace(event.find("  "), 2, " ");
    }
    if (event.size() > kMaxSimulationProgressEventChars) {
        event = event.substr(0, kMaxSimulationProgressEventChars - 3) + "...";
    }
    return event;
}

bool shouldShowSimulationEvent(const std::string& event) {
    if (event.empty()) return false;
    if (event.rfind("---", 0) == 0 || event.rfind("===", 0) == 0) return false;

    const std::string lower = toLower(event);
    return lower.find("simulando semana") != std::string::npos ||
           lower.find("[evento]") != std::string::npos ||
           lower.find("[mundo]") != std::string::npos ||
           lower.find("[staff]") != std::string::npos ||
           lower.find("[deuda]") != std::string::npos ||
           lower.find("[vestuario]") != std::string::npos ||
           lower.find("[directiva]") != std::string::npos ||
           lower.find("[hito]") != std::string::npos ||
           lower.find("finanzas semanales") != std::string::npos ||
           lower.find("oferta recibida") != std::string::npos ||
           lower.find("transferencia aceptada") != std::string::npos ||
           lower.find("contraoferta") != std::string::npos ||
           lower.find("contrato expirado") != std::string::npos ||
           lower.find("renovado.") != std::string::npos ||
           lower.find("deja el club") != std::string::npos ||
           lower.find("copa") != std::string::npos ||
           lower.find("campeon") != std::string::npos ||
           lower.find("logro desbloqueado") != std::string::npos ||
           lower.find("no hay calendario") != std::string::npos ||
           lower.find("semana invalida") != std::string::npos;
}

std::string simulationPhaseForEvent(const std::string& event) {
    const std::string lower = toLower(event);
    if (lower.find("finanzas") != std::string::npos ||
        lower.find("deuda") != std::string::npos ||
        lower.find("patrocinio") != std::string::npos) {
        return "Finanzas";
    }
    if (lower.find("mercado") != std::string::npos ||
        lower.find("oferta") != std::string::npos ||
        lower.find("transferencia") != std::string::npos ||
        lower.find("contraoferta") != std::string::npos ||
        lower.find("contrato") != std::string::npos ||
        lower.find("renovado") != std::string::npos) {
        return "Mercado";
    }
    if (lower.find("lesion") != std::string::npos ||
        lower.find("entrenamiento") != std::string::npos ||
        lower.find("vestuario") != std::string::npos ||
        lower.find("moral") != std::string::npos ||
        lower.find("staff") != std::string::npos ||
        lower.find("cantera") != std::string::npos) {
        return "Plantel";
    }
    if (lower.find("mundo") != std::string::npos ||
        lower.find("directiva") != std::string::npos ||
        lower.find("hito") != std::string::npos ||
        lower.find("cierre") != std::string::npos ||
        lower.find("logro") != std::string::npos) {
        return "Noticias";
    }
    return "Partidos";
}

int progressPercentForEventPhase(const std::string& phase, int fallback) {
    int phasePercent = 45;
    if (phase == "Plantel") phasePercent = 62;
    else if (phase == "Mercado") phasePercent = 70;
    else if (phase == "Finanzas") phasePercent = 78;
    else if (phase == "Noticias") phasePercent = 86;
    return clampValue(std::max(fallback, phasePercent), 0, 95);
}

void postWorkerSimulationEvent(const std::string& message) {
    if (!g_workerProgressContext) return;

    SimulationWorkerProgressContext& progress = *g_workerProgressContext;
    const std::string event = compactSimulationEvent(message);
    if (!shouldShowSimulationEvent(event)) return;

    if (progress.events.empty() || progress.events.back() != event) {
        progress.events.push_back(event);
        if (progress.events.size() > kMaxSimulationProgressEvents) {
            progress.events.erase(progress.events.begin());
        }
    }

    const std::string phase = simulationPhaseForEvent(event);
    progress.lastPercent = progressPercentForEventPhase(phase, progress.lastPercent);
    postSimulationProgress(progress.window, phase, event, progress.lastPercent, progress.events);
}

void pumpWorkerSimulationProgress() {
    if (!g_workerProgressContext) return;

    SimulationWorkerProgressContext& progress = *g_workerProgressContext;
    const int sweep = (progress.spinner % 36);
    const int percent = std::max(progress.lastPercent, 35 + std::min(35, sweep));
    progress.lastPercent = percent;
    if ((progress.spinner % 8) == 0) {
        postSimulationProgress(progress.window,
                               "Partidos",
                               buildSimulationStatus(progress.settings, progress.spinner),
                               percent,
                               progress.events);
    }
    progress.spinner = (progress.spinner + 1) % 64;
}

DWORD WINAPI simulationThreadProc(LPVOID rawArgs) {
    std::unique_ptr<SimulationThreadArgs> args(static_cast<SimulationThreadArgs*>(rawArgs));
    if (!args) return 0;

    SimulationWorkerProgressContext progress;
    progress.window = args->window;
    progress.settings = args->settings;
    g_workerProgressContext = &progress;
    postSimulationProgress(args->window, "Partidos", "Simulando partidos de la semana...", 35);

    CareerRuntimeContext runtime = currentCareerRuntimeContext();
    runtime.presentation = WeekSimulationPresentation::Compact;
    runtime.uiMessage = postWorkerSimulationEvent;
    runtime.idle = pumpWorkerSimulationProgress;
    ScopedCareerRuntimeContext scopedRuntime(runtime);
    ServiceResult result = simulateCareerWeekService(args->career, pumpWorkerSimulationProgress);
    postSimulationProgress(args->window,
                           "Tabla y noticias",
                           "Actualizando finanzas, tabla e inbox semanal.",
                           88,
                           progress.events);
    g_workerProgressContext = nullptr;

    const std::string teamName = managedTeamName(args->career);
    auto* payload = new SimulationJobResult{std::move(args->career), std::move(result), teamName};
    if (!args->window || !IsWindow(args->window) ||
        !PostMessageW(args->window, kGuiSimulationCompleteMessage, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
    return 0;
}

void markSettingsDirty(AppState& state, const std::string& status) {
    state.settingsDirty = true;
    refreshCurrentPage(state);
    setStatus(state, status + " Pendiente: aplica o restaura los ajustes.");
}

}  // namespace

void startNewCareer(AppState& state) {
    if (state.career.divisions.empty()) {
        MessageBoxW(state.window, L"No se encontraron divisiones disponibles.", L"Football Manager", MB_OK | MB_ICONWARNING);
        return;
    }

    syncManagerNameFromUi(state);
    if (!check_game_ready(state)) {
        refreshCurrentPage(state);
        if (state.gameSetup.division.empty()) {
            SetFocus(state.divisionCombo);
        } else if (state.gameSetup.club.empty()) {
            SetFocus(state.teamCombo);
        } else {
            SetFocus(state.managerEdit);
        }
        setStatus(state, state.gameSetup.inlineMessage + (state.gameSetup.managerError.empty() ? std::string() : " " + state.gameSetup.managerError));
        return;
    }

    ServiceResult result = startCareerService(state.career,
                                              state.gameSetup.division,
                                              state.gameSetup.club,
                                              state.gameSetup.manager);
    if (result.ok) {
        game_settings::applyNewCareerDifficulty(state.career, state.settings);
        result.messages.push_back("Configuracion aplicada: " + game_settings::settingsSummary(state.settings) + ".");
    }
    if (!result.ok) {
        std::string message = result.messages.empty() ? "No se pudo iniciar la carrera." : result.messages.front();
        MessageBoxW(state.window, utf8ToWide(message).c_str(), L"Football Manager", MB_OK | MB_ICONWARNING);
        setStatus(state, message);
        refreshCurrentPage(state);
        return;
    }
    syncCombosFromCareer(state);
    state.selectedPlayerName.clear();
    state.selectedTransferPlayer.clear();
    setCurrentPage(state, GuiPage::Dashboard);
    setStatus(state, result.messages.empty()
                         ? (result.ok ? "Nueva carrera iniciada." : "No se pudo iniciar la carrera.")
                         : result.messages.back());
    if (!result.messages.empty() && result.messages.size() > 1) {
        showLoadMessagesCompact(state, result, "Nueva carrera");
    }
}

void continueCareer(AppState& state) {
    if (state.career.myTeam) {
        pulseFrontendTiming(state);
        queuePageTransition(state, GuiPage::Dashboard);
        setStatus(state, "Carrera activa retomada desde memoria.");
        return;
    }
    loadCareer(state);
}

void loadCareer(AppState& state) {
    pulseFrontendTiming(state);
    if (state.currentPage == GuiPage::Saves && !state.selectedSavePath.empty()) {
        state.career.saveFile = state.selectedSavePath;
    }
    ServiceResult result = loadCareerService(state.career);
    if (!result.ok) {
        std::string message = result.messages.empty() ? "No se encontro una carrera guardada." : result.messages.front();
        MessageBoxW(state.window, utf8ToWide(message).c_str(), L"Football Manager", MB_OK | MB_ICONINFORMATION);
        setStatus(state, message);
        fillDivisionCombo(state, state.gameSetup.division);
        fillTeamCombo(state, state.gameSetup.division, state.gameSetup.club);
        refreshAll(state);
        return;
    }

    syncCombosFromCareer(state);
    state.selectedPlayerName.clear();
    state.selectedTransferPlayer.clear();
    queuePageTransition(state, GuiPage::Dashboard);
    setStatus(state, result.messages.empty() ? "Carrera cargada." : result.messages.back());
    if (!result.messages.empty() && result.messages.size() > 1) {
        showLoadMessagesCompact(state, result, "Carga de carrera");
    }
}

void saveCareer(AppState& state) {
    if (!state.career.myTeam) return;
    syncManagerNameFromUi(state);
    ServiceResult result = saveCareerService(state.career);
    refreshAll(state);
    setStatus(state, result.messages.empty() ? "Carrera guardada." : result.messages.back());
}

void deleteCareerSave(AppState& state) {
    if (state.career.myTeam) {
        MessageBoxW(state.window, L"No se puede borrar un guardado mientras hay una carrera activa.", L"Football Manager", MB_OK | MB_ICONWARNING);
        setStatus(state, "No se puede borrar un guardado con carrera activa.");
        return;
    }

    std::string savePath = (state.currentPage == GuiPage::Saves && !state.selectedSavePath.empty())
        ? state.selectedSavePath
        : (state.career.saveFile.empty() ? std::string("saves/career_save.txt") : state.career.saveFile);
    
    if (!pathExists(savePath) && savePath == "saves/career_save.txt" && pathExists("career_save.txt")) {
        savePath = "career_save.txt";
    }

    if (!pathExists(savePath)) {
        MessageBoxW(state.window, L"No hay un guardado para borrar.", L"Football Manager", MB_OK | MB_ICONINFORMATION);
        setStatus(state, "No hay guardado disponible para borrar.");
        return;
    }

    int result = MessageBoxW(state.window, 
                            utf8ToWide("¿Estás seguro de que deseas borrar el guardado?\n\n" + savePath).c_str(), 
                            L"Confirmar borrado", 
                            MB_YESNO | MB_ICONQUESTION);
    
    if (result == IDYES) {
        try {
            std::remove(savePath.c_str());
            if (pathExists(savePath + ".bak")) {
                std::remove((savePath + ".bak").c_str());
            }
            state.career.myTeam = nullptr;
            state.selectedSavePath.clear();
            state.saveSlotPaths.clear();
            refreshAll(state);
            setStatus(state, "Guardado eliminado correctamente.");
            MessageBoxW(state.window, L"El guardado ha sido eliminado.", L"Football Manager", MB_OK | MB_ICONINFORMATION);
        } catch (...) {
            MessageBoxW(state.window, L"No se pudo eliminar el guardado.", L"Football Manager", MB_OK | MB_ICONERROR);
            setStatus(state, "Error al eliminar el guardado.");
        }
    } else {
        setStatus(state, "Borrado cancelado.");
    }
}

void simulateWeek(AppState& state) {
    if (!state.career.myTeam) return;
    if (state.actionInProgress) return;
    syncManagerNameFromUi(state);

    setSimulationProgress(state, "Autosave", "Guardando carrera antes de simular.", 8);
    UpdateWindow(state.window);
    ServiceResult autosave = saveCareerService(state.career);
    if (!autosave.ok) {
        const std::string message = autosave.messages.empty()
            ? std::string("No se pudo crear el autosave antes de simular.")
            : autosave.messages.back();
        const int choice = MessageBoxW(state.window,
                                       utf8ToWide(message + "\n\nQuieres simular igual?").c_str(),
                                       L"Autosave",
                                       MB_YESNO | MB_ICONWARNING);
        if (choice != IDYES) {
            clearSimulationProgress(state);
            setStatus(state, "Simulacion cancelada: autosave no disponible.");
            refreshAll(state);
            return;
        }
        setSimulationProgress(state, "Autosave", "Autosave no disponible; simulando por decision del manager.", 18);
    } else {
        setSimulationProgress(state,
                              "Autosave",
                              autosave.messages.empty()
                                  ? "Autosave listo. Preparando la semana."
                                  : autosave.messages.back() + " Preparando la semana.",
                              22);
    }
    UpdateWindow(state.window);

    setSimulationProgress(state,
                          "Preparacion",
                          "Modo " + game_settings::simulationModeLabel(state.settings.simulationMode) +
                              " | Velocidad " + game_settings::simulationSpeedLabel(state.settings.simulationSpeed),
                          30);
    UpdateWindow(state.window);

    const std::string teamName = managedTeamName(state.career);
    Career workerCareer = state.career;
    relinkCareerPointers(workerCareer, teamName);
    auto* args = new SimulationThreadArgs{state.window, std::move(workerCareer), state.settings};

    state.actionInProgress = true;
    if (state.simulateButton) setWindowTextUtf8(state.simulateButton, "Simulando...");
    refreshAll(state);

    HANDLE thread = CreateThread(nullptr, 0, simulationThreadProc, args, 0, nullptr);
    if (!thread) {
        delete args;
        state.actionInProgress = false;
        if (state.simulateButton) setWindowTextUtf8(state.simulateButton, "Simular");
        clearSimulationProgress(state);
        refreshAll(state);
        MessageBoxW(state.window,
                    L"No se pudo iniciar la simulacion en segundo plano.",
                    L"Simulacion",
                    MB_OK | MB_ICONERROR);
        setStatus(state, "No se pudo iniciar la simulacion de la semana.");
        return;
    }
    CloseHandle(thread);
    setSimulationProgress(state, "Partidos", "Simulacion semanal en progreso. La interfaz sigue disponible.", 34);
}

void handleSimulationProgress(AppState& state, LPARAM payload) {
    std::unique_ptr<SimulationProgressPayload> progress(reinterpret_cast<SimulationProgressPayload*>(payload));
    if (!progress) return;
    setSimulationProgress(state, progress->phase, progress->detail, progress->percent, progress->events);
}

void completeSimulationWeek(AppState& state, LPARAM payload) {
    std::unique_ptr<SimulationJobResult> job(reinterpret_cast<SimulationJobResult*>(payload));
    if (!job) return;

    state.career = std::move(job->career);
    relinkCareerPointers(state.career, job->managedTeamName);
    syncCombosFromCareer(state);
    state.actionInProgress = false;
    if (state.simulateButton) setWindowTextUtf8(state.simulateButton, "Simular");
    setSimulationProgress(state, "Finalizando", "Refrescando paneles y estado de carrera.", 100);
    refreshAll(state);
    clearSimulationProgress(state);
    setStatus(state, job->result.messages.empty() ? "Semana simulada." : job->result.messages.back());
}

void validateSystem(AppState& state) {
    setStatus(state, "Ejecutando auditoria de datos...");
    UpdateWindow(state.window);
    ValidationSuiteSummary summary = runValidationService();
    const std::wstring dialogText = buildValidationDialogText(summary);
    if (summary.ok) {
        MessageBoxW(state.window, dialogText.c_str(), L"Football Manager", MB_OK | MB_ICONINFORMATION);
        setStatus(state, "Auditoria completada sin fallas.");
    } else {
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::ManagerAlert,
            "Auditoria con fallas",
            "La auditoria del sistema detecto errores o advertencias que requieren revision."
        );
        MessageBoxW(state.window, dialogText.c_str(), L"Football Manager", MB_OK | MB_ICONWARNING);
        setStatus(state, "Auditoria completada con fallas.");
    }
}

void runScoutingAction(AppState& state) {
    ServiceResult result = state.currentPage == GuiPage::News
        ? createScoutingAssignmentService(state.career, "", "", 3)
        : scoutPlayersService(state.career, "Todas", "");
    finalizeAction(state, result, "Scouting", true);
}

void runBuyAction(AppState& state) {
    if (state.currentPage != GuiPage::Transfers) return;
    std::string sellerTeamName;
    std::string playerName;
    if (!selectedTransferTarget(state, sellerTeamName, playerName)) {
        MessageBoxW(state.window, L"Selecciona un objetivo del mercado.", L"Mercado", MB_OK | MB_ICONINFORMATION);
        return;
    }
    Team* seller = state.career.findTeamByName(sellerTeamName);
    const Player* player = seller ? findPlayerInTeam(*seller, playerName) : nullptr;
    if (!state.career.myTeam || !seller || !player) {
        MessageBoxW(state.window, L"No se pudo leer el objetivo seleccionado.", L"Mercado", MB_OK | MB_ICONWARNING);
        return;
    }
    const NegotiationProfile profile = guiTransferProfileForTarget(state.career, *seller, *player);
    const NegotiationPromise promise = guiPromiseForTarget(*state.career.myTeam, *player);
    ServiceResult result = buyTransferTargetService(state.career, sellerTeamName, playerName, profile, promise);
    prependNegotiationPlan(result, profile, promise, "compra contextual");
    finalizeAction(state, result, "Fichaje");
}

void runPreContractAction(AppState& state) {
    if (state.currentPage != GuiPage::Transfers) return;
    std::string sellerTeamName;
    std::string playerName;
    if (!selectedTransferTarget(state, sellerTeamName, playerName)) {
        MessageBoxW(state.window, L"Selecciona un objetivo del mercado.", L"Precontrato", MB_OK | MB_ICONINFORMATION);
        return;
    }
    Team* seller = state.career.findTeamByName(sellerTeamName);
    const Player* player = seller ? findPlayerInTeam(*seller, playerName) : nullptr;
    if (!state.career.myTeam || !seller || !player) {
        MessageBoxW(state.window, L"No se pudo leer el objetivo seleccionado.", L"Precontrato", MB_OK | MB_ICONWARNING);
        return;
    }
    const NegotiationPromise promise = guiPromiseForTarget(*state.career.myTeam, *player);
    const NegotiationProfile profile =
        (player->skill >= state.career.myTeam->getAverageSkill() + 5) ? NegotiationProfile::Safe
                                                                      : NegotiationProfile::Balanced;
    ServiceResult result = signPreContractService(state.career, sellerTeamName, playerName, profile, promise);
    prependNegotiationPlan(result, profile, promise, "precontrato contextual");
    finalizeAction(state, result, "Precontrato");
}

void runLoanAction(AppState& state) {
    constexpr int kDefaultLoanWeeks = 26;
    if (state.currentPage == GuiPage::Transfers) {
        std::string sellerTeamName;
        std::string playerName;
        if (!selectedTransferTarget(state, sellerTeamName, playerName)) {
            MessageBoxW(state.window, L"Selecciona un objetivo del mercado.", L"Cesion", MB_OK | MB_ICONINFORMATION);
            return;
        }
        ServiceResult result = loanInPlayerService(state.career, sellerTeamName, playerName, kDefaultLoanWeeks);
        finalizeAction(state, result, "Cesion entrante");
        return;
    }

    if (state.currentPage == GuiPage::Squad || state.currentPage == GuiPage::Youth) {
        std::string playerName;
        if (!selectedManagedPlayerName(state, playerName)) {
            MessageBoxW(state.window, L"Selecciona un jugador de tu plantilla.", L"Ceder jugador", MB_OK | MB_ICONINFORMATION);
            return;
        }
        const Player* player = findManagedPlayer(state.career, playerName);
        if (!player) {
            MessageBoxW(state.window, L"No se encontro el jugador seleccionado.", L"Ceder jugador", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string destination = recommendedLoanDestination(state.career, *player);
        if (destination.empty()) {
            MessageBoxW(state.window,
                        L"No hay un club destino con cupo para recibir la cesion.",
                        L"Ceder jugador",
                        MB_OK | MB_ICONINFORMATION);
            return;
        }
        ServiceResult result = loanOutPlayerService(state.career, playerName, destination, kDefaultLoanWeeks);
        finalizeAction(state, result, "Cesion saliente");
        return;
    }

    MessageBoxW(state.window, L"Abre Fichajes para pedir cesiones o Plantilla para ceder jugadores.", L"Cesion", MB_OK | MB_ICONINFORMATION);
}

void runRenewAction(AppState& state) {
    int row = selectedListViewRow(state.squadList);
    if (row < 0 || (state.currentPage != GuiPage::Squad && state.currentPage != GuiPage::Youth && state.currentPage != GuiPage::Finances)) {
        MessageBoxW(state.window, L"Selecciona un jugador.", L"Renovar", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const std::string playerName = listViewText(state.squadList, row, 0);
    Team* team = state.career.myTeam;
    Player* player = team ? findMutablePlayer(*team, playerName) : nullptr;
    if (!team || !player) {
        MessageBoxW(state.window, L"No se pudo leer el jugador seleccionado.", L"Renovar", MB_OK | MB_ICONWARNING);
        return;
    }
    const NegotiationProfile profile = guiRenewalProfileForPlayer(*team, *player);
    const NegotiationPromise promise = guiPromiseForTarget(*team, *player);
    ServiceResult result = renewPlayerContractService(state.career, playerName, profile, promise);
    prependNegotiationPlan(result, profile, promise, "renovacion contextual");
    finalizeAction(state, result, "Renovacion");
}

void runSellAction(AppState& state) {
    int row = selectedListViewRow(state.squadList);
    if (row < 0 || state.currentPage == GuiPage::Transfers) {
        MessageBoxW(state.window, L"Selecciona un jugador de tu plantilla.", L"Venta", MB_OK | MB_ICONINFORMATION);
        return;
    }
    ServiceResult result = sellPlayerService(state.career, listViewText(state.squadList, row, 0));
    finalizeAction(state, result, "Venta");
}

void runPlanAction(AppState& state) {
    int row = selectedListViewRow(state.squadList);
    if (row < 0 || (state.currentPage != GuiPage::Squad && state.currentPage != GuiPage::Youth)) {
        MessageBoxW(state.window, L"Selecciona un jugador.", L"Plan individual", MB_OK | MB_ICONINFORMATION);
        return;
    }
    ServiceResult result = state.currentPage == GuiPage::Squad
        ? cyclePlayerInstructionService(state.career, listViewText(state.squadList, row, 0))
        : cyclePlayerDevelopmentPlanService(state.career, listViewText(state.squadList, row, 0));
    finalizeAction(state, result, state.currentPage == GuiPage::Squad ? "Instruccion individual" : "Plan individual");
}

void runInstructionAction(AppState& state) {
    if (state.currentPage == GuiPage::Dashboard) {
        if (dashboardShowsPostWeekDigest(state)) {
            ServiceResult result = applyWeeklyDecisionService(state.career, WeeklyDecision::Auto);
            if (result.ok) consumeLatestWeeklyDigestService(state.career);
            finalizeAction(state, result, "Decision semanal", true);
            return;
        }
        finalizeAction(state, holdTeamMeetingService(state.career), "Reunion");
        return;
    }
    if (state.currentPage == GuiPage::News) {
        ServiceResult result = applyWeeklyDecisionService(state.career, WeeklyDecision::Auto);
        if (result.ok) consumeLatestWeeklyDigestService(state.career);
        finalizeAction(state, result, "Decision semanal", true);
        return;
    }
    if (state.currentPage == GuiPage::Board) {
        finalizeAction(state, reviewStaffStructureService(state.career), "Staff", true);
        return;
    }
    if (state.currentPage == GuiPage::Squad || state.currentPage == GuiPage::Youth) {
        int row = selectedListViewRow(state.squadList);
        if (row < 0) {
            MessageBoxW(state.window, L"Selecciona un jugador.", L"Charla", MB_OK | MB_ICONINFORMATION);
            return;
        }
        finalizeAction(state, talkToPlayerService(state.career, listViewText(state.squadList, row, 0)), "Charla");
        return;
    }
    if (state.currentPage == GuiPage::Tactics || state.currentPage == GuiPage::Calendar) {
        finalizeAction(state, applyMatchPreparationPlanService(state.career), "Plan de partido", true);
        return;
    }
    finalizeAction(state, cycleMatchInstructionService(state.career), "Instruccion");
}

void runShortlistAction(AppState& state) {
    if (state.currentPage != GuiPage::Transfers) return;
    int row = selectedListViewRow(state.tableList);
    if (row < 0) {
        MessageBoxW(state.window, L"Selecciona un objetivo del mercado.", L"Shortlist", MB_OK | MB_ICONINFORMATION);
        return;
    }
    ServiceResult result = shortlistPlayerService(state.career,
                                                  listViewText(state.tableList, row, 10),
                                                  listViewText(state.tableList, row, 0));
    finalizeAction(state, result, "Shortlist");
}

void runFollowShortlistAction(AppState& state) {
    ServiceResult result = followShortlistService(state.career);
    finalizeAction(state, result, "Seguimiento", true);
}

void runUpgradeAction(AppState& state, ClubUpgrade upgrade, const std::string& title) {
    ServiceResult result = upgradeClubService(state.career, upgrade);
    finalizeAction(state, result, title);
}

void openFrontendMenu(AppState& state) {
    pulseFrontendTiming(state);
    setCurrentPage(state, GuiPage::MainMenu);
    setStatus(state,
              state.career.myTeam
                  ? "Volviste al menu principal. Usa Continuar para retomar la carrera activa."
                  : "Menu principal listo. Entra a Jugar o revisa Configuraciones.");
    if (state.menuContinueButton && IsWindowEnabled(state.menuContinueButton)) SetFocus(state.menuContinueButton);
    else if (state.menuPlayButton) SetFocus(state.menuPlayButton);
}

void openSettingsMenu(AppState& state) {
    pulseFrontendTiming(state);
    setCurrentPage(state, GuiPage::Settings);
    setStatus(state, "Configuraciones abiertas. Ajusta frontend, accesibilidad y audio del menu.");
    if (state.menuVolumeButton) SetFocus(state.menuVolumeButton);
}

void openCreditsPage(AppState& state) {
    pulseFrontendTiming(state);
    setCurrentPage(state, GuiPage::Credits);
    setStatus(state, "Creditos abiertos. La portada mantiene la identidad del manager game.");
    if (state.menuBackButton) SetFocus(state.menuBackButton);
}

void openSavesPage(AppState& state) {
    pulseFrontendTiming(state);
    setCurrentPage(state, GuiPage::Saves);
    setStatus(state, "Guardados abiertos. Selecciona un save y usa Abrir o Borrar.");
    if (state.menuLoadButton && IsWindowEnabled(state.menuLoadButton)) SetFocus(state.menuLoadButton);
    else if (state.menuBackButton) SetFocus(state.menuBackButton);
}

void cycleFrontendVolume(AppState& state) {
    game_settings::cycleVolume(state.settings);
    refreshMenuMusicVolume(state);
    markSettingsDirty(state, "Volumen ajustado a " + game_settings::volumeLabel(state.settings.volume) + ".");
}

void cycleFrontendDifficulty(AppState& state) {
    game_settings::cycleDifficulty(state.settings);
    markSettingsDirty(state, "Dificultad actual: " + game_settings::difficultyLabel(state.settings.difficulty) + ".");
}

void cycleFrontendSimulationSpeed(AppState& state) {
    game_settings::cycleSimulationSpeed(state.settings);
    markSettingsDirty(state, "Velocidad actual: " + game_settings::simulationSpeedLabel(state.settings.simulationSpeed) + ".");
}

void cycleFrontendSimulationMode(AppState& state) {
    game_settings::cycleSimulationMode(state.settings);
    markSettingsDirty(state, "Modo de simulacion actual: " + game_settings::simulationModeLabel(state.settings.simulationMode) + ".");
}

void cycleFrontendLanguage(AppState& state) {
    game_settings::cycleLanguage(state.settings);
    markSettingsDirty(state, "Idioma actual: " + game_settings::languageLabel(state.settings.language) + ".");
}

void cycleFrontendTextSpeed(AppState& state) {
    game_settings::cycleTextSpeed(state.settings);
    markSettingsDirty(state, "Velocidad de texto: " + game_settings::textSpeedLabel(state.settings.textSpeed) + ".");
}

void cycleFrontendVisualProfile(AppState& state) {
    game_settings::cycleVisualProfile(state.settings);
    markSettingsDirty(state, "Perfil visual: " + game_settings::visualProfileLabel(state.settings.visualProfile) + ".");
}

void cycleFrontendMenuMusicMode(AppState& state) {
    game_settings::cycleMenuMusicMode(state.settings);
    markSettingsDirty(state, "Musica del frontend: " + game_settings::menuMusicModeLabel(state.settings.menuMusicMode) + ".");
}

void toggleFrontendAudioFade(AppState& state) {
    game_settings::toggleMenuAudioFade(state.settings);
    markSettingsDirty(state, "Audio del menu: " + game_settings::menuAudioFadeLabel(state.settings.menuAudioFade) + ".");
}

void applyFrontendSettings(AppState& state) {
    game_settings::sanitize(state.settings);
    const bool saved = game_settings::saveToDisk(state.settings);
    if (saved) {
        state.savedSettings = state.settings;
        state.settingsDirty = false;
    }
    refreshCurrentPage(state);
    if (!saved) {
        MessageBoxW(state.window,
                    L"No se pudieron guardar los ajustes en disco.",
                    L"Configuraciones",
                    MB_OK | MB_ICONWARNING);
        setStatus(state, "Ajustes aplicados en esta sesion, pero no se pudieron guardar.");
        return;
    }
    setStatus(state, "Ajustes aplicados y guardados: " + game_settings::settingsSummary(state.settings) + ".");
}

void restoreFrontendSettings(AppState& state) {
    state.settings = state.savedSettings;
    game_settings::sanitize(state.settings);
    state.settingsDirty = false;
    refreshCurrentPage(state);
    setStatus(state, "Ajustes restaurados al ultimo estado aplicado.");
}

}  // namespace gui_win32

#endif
