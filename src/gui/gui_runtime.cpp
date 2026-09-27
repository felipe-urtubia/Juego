#include "gui/gui_internal.h"

#include "gui/gui_audio.h"
#include "gui/gui_view_builders.h"

#ifdef _WIN32

#include "utils/utils.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace gui_win32 {

GuiPageModel buildModel(AppState& state);

namespace {

std::vector<HWND> pageRefreshTargets(const AppState& state) {
    return {
        state.divisionLabel, state.teamLabel, state.managerLabel, state.managerHelpLabel,
        state.divisionCombo, state.teamCombo, state.managerEdit,
        state.newCareerButton, state.loadButton, state.saveButton, state.simulateButton, state.validateButton,
        state.displayModeButton, state.frontMenuButton,
        state.dashboardButton, state.squadButton, state.tacticsButton, state.calendarButton, state.leagueButton,
        state.transfersButton, state.financesButton, state.youthButton, state.boardButton, state.newsButton,
        state.pageTitleLabel, state.breadcrumbLabel, state.infoLabel, state.filterLabel, state.filterCombo,
        state.summaryLabel, state.summaryEdit, state.tableLabel, state.tableList,
        state.squadLabel, state.squadList, state.transferLabel, state.transferList,
        state.detailLabel, state.detailEdit, state.newsLabel, state.newsList, state.statusLabel,
        state.scoutActionButton, state.shortlistButton, state.followShortlistButton, state.buyButton,
        state.preContractButton, state.loanButton, state.renewButton, state.sellButton, state.planButton, state.instructionButton,
        state.youthUpgradeButton, state.trainingUpgradeButton, state.scoutingUpgradeButton, state.stadiumUpgradeButton,
        state.menuContinueButton, state.menuPlayButton, state.menuSettingsButton, state.menuLoadButton,
        state.menuDeleteSaveButton,
        state.menuCreditsButton, state.menuExitButton, state.menuBackButton, state.menuVolumeButton,
        state.menuDifficultyButton, state.menuSpeedButton, state.menuSimulationButton, state.menuLanguageButton,
        state.menuTextSpeedButton, state.menuVisualButton, state.menuMusicModeButton,
        state.menuAudioFadeButton, state.menuApplySettingsButton, state.menuResetSettingsButton,
        state.emptyNewButton, state.emptyLoadButton, state.emptyValidateButton
    };
}

bool dashboardShowsPostWeekDigest(const AppState& state) {
    return state.currentPage == GuiPage::Dashboard &&
           (state.currentModel.summary.content.find("Cierre post-semana") != std::string::npos ||
            state.currentModel.detail.content.find("Cierre post-semana") != std::string::npos);
}

std::string pageCacheKey(const AppState& state, GuiPage page) {
    std::ostringstream out;
    out << static_cast<int>(page) << "|" << state.currentFilter;
    return out.str();
}

template <typename T>
void mixDigest(std::size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

std::size_t digestLines(const std::vector<std::string>& lines) {
    std::size_t seed = lines.size();
    for (const auto& line : lines) mixDigest(seed, line);
    return seed;
}

std::size_t digestPendingTransfers(const std::vector<PendingTransfer>& transfers) {
    std::size_t seed = transfers.size();
    for (const auto& transfer : transfers) {
        mixDigest(seed, transfer.playerName);
        mixDigest(seed, transfer.fromTeam);
        mixDigest(seed, transfer.toTeam);
        mixDigest(seed, transfer.fee);
        mixDigest(seed, transfer.wage);
        mixDigest(seed, transfer.contractWeeks);
        mixDigest(seed, transfer.effectiveSeason);
        mixDigest(seed, transfer.loanWeeks);
        mixDigest(seed, transfer.preContract);
        mixDigest(seed, transfer.loan);
        mixDigest(seed, transfer.promisedRole);
    }
    return seed;
}

std::size_t digestPlayers(const std::vector<Player>& players) {
    std::size_t seed = players.size();
    for (const auto& player : players) {
        mixDigest(seed, player.name);
        mixDigest(seed, player.position);
        mixDigest(seed, player.age);
        mixDigest(seed, player.skill);
        mixDigest(seed, player.value);
        mixDigest(seed, player.wage);
        mixDigest(seed, player.contractWeeks);
        mixDigest(seed, player.happiness);
        mixDigest(seed, player.currentForm);
        mixDigest(seed, player.fitness);
        mixDigest(seed, player.injured);
        mixDigest(seed, player.injuryWeeks);
        mixDigest(seed, player.matchesSuspended);
    }
    return seed;
}

std::string pageCacheSignature(const AppState& state, GuiPage page) {
    const Career& career = state.career;
    std::size_t digest = static_cast<std::size_t>(page);
    mixDigest(digest, state.currentFilter);
    mixDigest(digest, career.currentSeason);
    mixDigest(digest, career.currentWeek);
    mixDigest(digest, career.boardConfidence);
    mixDigest(digest, career.boardMonthlyObjective);
    mixDigest(digest, career.boardMonthlyProgress);
    mixDigest(digest, state.selectedPlayerName);
    mixDigest(digest, state.selectedTransferPlayer);
    mixDigest(digest, state.selectedTransferClub);
    mixDigest(digest, digestLines(career.newsFeed));
    mixDigest(digest, digestLines(career.managerInbox));
    mixDigest(digest, digestLines(career.scoutInbox));
    mixDigest(digest, digestLines(career.scoutingShortlist));
    mixDigest(digest, digestPendingTransfers(career.pendingTransfers));
    if (career.myTeam) {
        mixDigest(digest, career.myTeam->name);
        mixDigest(digest, career.myTeam->budget);
        mixDigest(digest, career.myTeam->debt);
        mixDigest(digest, career.myTeam->sponsorWeekly);
        mixDigest(digest, career.myTeam->morale);
        mixDigest(digest, career.myTeam->points);
        mixDigest(digest, career.myTeam->stadiumLevel);
        mixDigest(digest, career.myTeam->trainingFacilityLevel);
        mixDigest(digest, career.myTeam->youthFacilityLevel);
        mixDigest(digest, digestPlayers(career.myTeam->players));
    }
    return std::to_string(digest);
}

bool canUseCachedModel(const AppState& state) {
    return isHeavyPage(state.currentPage);
}

class ScopedRefreshRedrawLock {
public:
    explicit ScopedRefreshRedrawLock(AppState& state) : state_(state), targets_(pageRefreshTargets(state)) {
        for (HWND target : targets_) {
            if (target && IsWindow(target)) SendMessageW(target, WM_SETREDRAW, FALSE, 0);
        }
    }

    ~ScopedRefreshRedrawLock() {
        for (HWND target : targets_) {
            if (!target || !IsWindow(target)) continue;
            SendMessageW(target, WM_SETREDRAW, TRUE, 0);
            if (IsWindowVisible(target)) {
                RedrawWindow(target, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
            }
        }
        if (state_.window) {
            RedrawWindow(state_.window,
                         nullptr,
                         nullptr,
                         RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        }
        state_.pageRefreshInProgress = false;
    }

private:
    AppState& state_;
    std::vector<HWND> targets_;
};

std::string friendlyPanelTitle(const std::string& title) {
    if (title == "FrontMenuOverview") return "Panorama de arranque";
    if (title == "FrontMenuProfile") return "Perfil del manager";
    if (title == "FrontMenuRoadmap") return "Hoja de ruta";
    if (title == "SaveBrowserOverview") return "Guardados disponibles";
    if (title == "SaveBrowserDetail") return "Detalle del guardado";
    if (title == "SaveBrowserList") return "Lista de guardados";
    if (title == "SettingsDesk") return "Cabina de ajustes";
    if (title == "SettingsImpact") return "Impacto";
    if (title == "SettingsReturn") return "Retorno y estado";
    if (title == "DashboardPanel") return "Centro del club";
    if (title == "LeagueTableView") return "Clasificacion";
    if (title == "MatchSummaryPanel") return "Proximo encuentro";
    if (title == "InjuryListWidget") return "Lesiones";
    if (title == "FixtureListView") return "Calendario";
    if (title == "UpcomingMatchWidget") return "Proximo encuentro";
    if (title == "LastResultPanel") return "Ultimo partido";
    if (title == "TeamStatusPanel") return "Estado del plantel";
    if (title == "ActionCuePanel") return "Proximas acciones";
    if (title == "LeaguePositionWidget") return "Posicion competitiva";
    if (title == "TeamMoraleWidget") return "Estado del vestuario";
    if (title == "PlayerTableView") return "Plantilla";
    if (title == "YouthPlayerTableView") return "Cantera";
    if (title == "PlayerProfilePanel") return "Ficha del jugador";
    if (title == "PlayerComparisonPanel") return "Comparador";
    if (title == "SquadSummaryPanel") return "Resumen de plantilla";
    if (title == "YouthSummaryPanel") return "Resumen de cantera";
    if (title == "PlayerDevelopmentPanel") return "Desarrollo";
    if (title == "SquadUnitOverview") return "Balance por lineas";
    if (title == "TacticalSummary") return "Resumen tactico";
    if (title == "TacticsBoard") return "Disposicion en el campo";
    if (title == "FormationSelector") return "Once titular";
    if (title == "TacticalImpactSummary") return "Impacto tactico";
    if (title == "MatchAnalysisPanel") return "Informe tactico";
    if (title == "CalendarSummary") return "Resumen del calendario";
    if (title == "CupAndSeasonNotes") return "Competiciones";
    if (title == "SeasonHistory") return "Historial de temporada";
    if (title == "SeasonFlowController") return "Resumen semanal";
    if (title == "CompetitionSummary") return "Resumen competitivo";
    if (title == "RaceContext") return "Contexto de la tabla";
    if (title == "SeasonRecords") return "Historial reciente";
    if (title == "CompetitionReport") return "Informe de liga";
    if (title == "TransferSearchPanel") return "Radar de mercado";
    if (title == "TransferMarketView") return "Mercado de fichajes";
    if (title == "TransferTargetCard") return "Objetivo observado";
    if (title == "TransferPipeline") return "Negociaciones y movimientos";
    if (title == "FinanceSummary") return "Resumen financiero";
    if (title == "FinanceBreakdown") return "Balance economico";
    if (title == "SalaryTable") return "Masa salarial";
    if (title == "Infrastructure") return "Infraestructura";
    if (title == "ClubFinancePanel") return "Panel financiero";
    if (title == "BoardObjectives") return "Objetivos de directiva";
    if (title == "BoardObjectiveTable") return "Seguimiento de objetivos";
    if (title == "ClubProfile") return "Perfil del club";
    if (title == "CoachHistory") return "Historial del entrenador";
    if (title == "BoardReport") return "Informe de directiva";
    if (title == "NewsFeedPanel") return "Titulares del mundo";
    if (title == "NewsCardList") return "Titulares";
    if (title == "ScoutingInbox") return "Centro del manager";
    if (title == "NewsDetail") return "Detalle";
    if (title == "AlertPanel") return "Alertas";
    if (title == "AlertPanel / NewsFeedPanel") return "Pulso del club";
    if (title == "LaunchChecklistPanel") return "Paso a paso";
    if (title == "GameSetupStatusPanel") return "Estado actual de la partida";
    if (title == "GameSetupChecklistPanel") return "Checklist dinamico";
    return title;
}

std::vector<std::string> filterOptionsForPage(GuiPage page) {
    switch (page) {
        case GuiPage::MainMenu:
        case GuiPage::Settings:
        case GuiPage::Credits:
        case GuiPage::Saves:
            return {};
        case GuiPage::Dashboard: return {"Todo", "Alertas", "Lesiones", "Contratos"};
        case GuiPage::Squad: return {"Todos", "XI", "ARQ", "DEF", "MED", "DEL", "Lesionados"};
        case GuiPage::Tactics: return {"Balanceado", "Presion alta", "Bloque bajo"};
        case GuiPage::Calendar: return {"Proximos 5", "Proximos 10", "Toda la fase"};
        case GuiPage::League: return {"Grupo actual", "Tabla general"};
        case GuiPage::Transfers: return {"Todos", "ARQ", "DEF", "MED", "DEL", "Sub-23", "Potencial 75+", "Precio accesible", "Salario bajo"};
        case GuiPage::Finances: return {"Resumen", "Salarios", "Infraestructura"};
        case GuiPage::Youth: return {"Todos", "Sub-21", "Elite", "Cedidos"};
        case GuiPage::Board: return {"Resumen", "Objetivos", "Historial"};
        case GuiPage::News: return {"Todo", "Alertas", "Mercado", "Resultados"};
    }
    return {"Todo"};
}

void renderFeed(HWND list, const std::vector<std::string>& lines) {
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    std::vector<std::string> safeLines = lines;
    if (safeLines.empty()) safeLines.push_back("No hay noticias recientes.");
    for (const auto& line : safeLines) {
        std::wstring wide = utf8ToWide(line);
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
    }
}

void renderListPanel(AppState& state, HWND label, HWND list, const ListPanelModel& model) {
    updateDynamicStaticText(state, label, friendlyPanelTitle(model.title));
    resetListViewColumns(list, model.columns);
    clearListView(list);
    for (const auto& row : model.rows) {
        addListViewRow(list, row);
    }
    int selectedRow = -1;
    if (state.currentPage == GuiPage::Transfers && list == state.tableList) {
        for (size_t i = 0; i < model.rows.size(); ++i) {
            const auto& row = model.rows[i];
            if (row.size() > 10 && row[0] == state.selectedTransferPlayer && row[10] == state.selectedTransferClub) {
                selectedRow = static_cast<int>(i);
                break;
            }
        }
    } else if ((state.currentPage == GuiPage::Squad || state.currentPage == GuiPage::Youth) && list == state.squadList) {
        for (size_t i = 0; i < model.rows.size(); ++i) {
            const auto& row = model.rows[i];
            if (!row.empty() && row[0] == state.selectedPlayerName) {
                selectedRow = static_cast<int>(i);
                break;
            }
        }
    }
    if (selectedRow >= 0) {
        ListView_SetItemState(list, selectedRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list, selectedRow, FALSE);
    }
    applyTeamLogosToList(state, list, model);
}

void autosizeVisibleLists(AppState& state) {
    if (state.tableList && IsWindowVisible(state.tableList)) {
        autosizeListViewColumns(state, state.tableList, state.currentModel.primary.columns);
    }
    if (state.squadList && IsWindowVisible(state.squadList)) {
        autosizeListViewColumns(state, state.squadList, state.currentModel.secondary.columns);
    }
    if (state.transferList && IsWindowVisible(state.transferList)) {
        autosizeListViewColumns(state, state.transferList, state.currentModel.footer.columns);
    }
}

void refreshFilterComboOptions(AppState& state) {
    if (!state.filterCombo) return;
    std::vector<std::string> options = filterOptionsForPage(state.currentPage);
    if (std::find(options.begin(), options.end(), state.currentFilter) == options.end()) {
        state.currentFilter = options.empty() ? std::string() : options.front();
    }
    state.suppressFilterEvents = true;
    SendMessageW(state.filterCombo, CB_RESETCONTENT, 0, 0);
    int selected = 0;
    for (size_t i = 0; i < options.size(); ++i) {
        addComboItem(state.filterCombo, options[i]);
        if (options[i] == state.currentFilter) selected = static_cast<int>(i);
    }
    if (!options.empty()) SendMessageW(state.filterCombo, CB_SETCURSEL, selected, 0);
    state.suppressFilterEvents = false;
}

bool isKnownDivision(const AppState& state, const std::string& divisionId) {
    for (const auto& division : state.career.divisions) {
        if (division.id == divisionId) return true;
    }
    return false;
}

std::string resolveKnownDivisionId(const AppState& state, const std::string& rawValue) {
    const std::string normalized = toLower(trim(rawValue));
    if (normalized.empty()) return std::string();
    for (const auto& division : state.career.divisions) {
        if (division.id == normalized || toLower(trim(division.display)) == normalized) {
            return division.id;
        }
    }
    return std::string();
}

std::string inferHeaderDivisionId(const AppState& state) {
    std::vector<std::string> candidates;
    if (!state.gameSetup.division.empty()) candidates.push_back(state.gameSetup.division);
    if (!state.career.activeDivision.empty()) candidates.push_back(state.career.activeDivision);
    if (state.career.myTeam) {
        candidates.push_back(state.career.myTeam->division);
        candidates.push_back(state.career.myTeam->name);
    }
    if (!state.gameSetup.club.empty()) candidates.push_back(state.gameSetup.club);

    for (const auto& candidate : candidates) {
        const std::string resolved = resolveKnownDivisionId(state, candidate);
        if (!resolved.empty()) return resolved;

        for (const auto& team : state.career.allTeams) {
            if (toLower(trim(team.name)) != toLower(trim(candidate))) continue;
            const std::string teamDivision = resolveKnownDivisionId(state, team.division);
            if (!teamDivision.empty()) return teamDivision;
        }
    }
    return std::string();
}

bool clubBelongsToDivision(const AppState& state, const std::string& divisionId, const std::string& clubName) {
    if (divisionId.empty() || clubName.empty()) return false;
    return std::any_of(state.career.allTeams.begin(), state.career.allTeams.end(), [&](const Team& team) {
        return team.division == divisionId && team.name == clubName;
    });
}

bool hasAvailableTeams(const AppState& state, const std::string& divisionId) {
    if (divisionId.empty()) return false;
    return std::any_of(state.career.allTeams.begin(), state.career.allTeams.end(), [&](const Team& team) {
        return team.division == divisionId;
    });
}

bool hasPersistedCareer(const AppState& state) {
    const std::string savePath = state.career.saveFile.empty() ? std::string("saves/career_save.txt") : state.career.saveFile;
    return pathExists(savePath) || (savePath == "saves/career_save.txt" && pathExists("career_save.txt"));
}

bool isDashboardActionCueList(const AppState& state, int controlId) {
    return state.currentPage == GuiPage::Dashboard &&
           controlId == IDC_TRANSFER_LIST &&
           state.currentModel.footer.title == "ActionCuePanel";
}

GuiPage dashboardActionDestinationPage(const std::string& destination) {
    const std::string normalized = toLower(trim(destination));
    if (normalized == "plantilla") return GuiPage::Squad;
    if (normalized == "fichajes" || normalized == "mercado") return GuiPage::Transfers;
    if (normalized == "finanzas") return GuiPage::Finances;
    if (normalized == "directiva") return GuiPage::Board;
    if (normalized == "tacticas" || normalized == "tactica") return GuiPage::Tactics;
    if (normalized == "liga") return GuiPage::League;
    if (normalized == "calendario") return GuiPage::Calendar;
    if (normalized == "noticias" || normalized == "inbox") return GuiPage::News;
    return GuiPage::Dashboard;
}

bool isApplyDecisionCommand(const std::string& action) {
    return toLower(trim(action)) == "aplicar decision";
}

void syncSaveBrowserSelection(AppState& state) {
    if (state.currentPage != GuiPage::Saves || !state.newsList) return;
    int selectedIndex = -1;
    for (size_t i = 0; i < state.saveSlotPaths.size(); ++i) {
        if (state.saveSlotPaths[i] == state.selectedSavePath) {
            selectedIndex = static_cast<int>(i);
            break;
        }
    }
    SendMessageW(state.newsList, LB_SETCURSEL, selectedIndex, 0);
}

constexpr wchar_t kPlayerContextMenuClass[] = L"ChileanFootballitoPlayerContextMenu";

int playerContextMenuHeaderHeight(const AppState& state) {
    return scaleByDpi(state, 38);
}

int playerContextMenuOuterPadding(const AppState& state) {
    return scaleByDpi(state, 8);
}

int playerContextMenuItemHeight(const AppState& state) {
    return scaleByDpi(state, 38);
}

int playerContextMenuSeparatorHeight(const AppState& state) {
    return scaleByDpi(state, 12);
}

RECT playerContextMenuItemRect(const AppState& state, int index, int width) {
    int top = playerContextMenuHeaderHeight(state) + playerContextMenuOuterPadding(state);
    for (int i = 0; i < index; ++i) {
        top += state.playerContextMenu.entries[static_cast<size_t>(i)].separator
            ? playerContextMenuSeparatorHeight(state)
            : playerContextMenuItemHeight(state);
    }

    const int height = state.playerContextMenu.entries[static_cast<size_t>(index)].separator
        ? playerContextMenuSeparatorHeight(state)
        : playerContextMenuItemHeight(state);

    return RECT{
        playerContextMenuOuterPadding(state),
        top,
        width - playerContextMenuOuterPadding(state),
        top + height
    };
}

int playerContextMenuHeight(const AppState& state) {
    int height = playerContextMenuHeaderHeight(state) + playerContextMenuOuterPadding(state) * 2;
    for (const auto& entry : state.playerContextMenu.entries) {
        height += entry.separator
            ? playerContextMenuSeparatorHeight(state)
            : playerContextMenuItemHeight(state);
    }
    return height;
}

int playerContextMenuHitTest(const AppState& state, POINT point) {
    if (!state.playerContextMenu.window) return -1;

    RECT client{};
    GetClientRect(state.playerContextMenu.window, &client);
    const int width = client.right - client.left;

    for (int i = 0; i < static_cast<int>(state.playerContextMenu.entries.size()); ++i) {
        const auto& entry = state.playerContextMenu.entries[static_cast<size_t>(i)];
        if (entry.separator || !entry.enabled) continue;
        RECT item = playerContextMenuItemRect(state, i, width);
        if (PtInRect(&item, point)) return i;
    }
    return -1;
}

void executePlayerContextCommand(AppState& state, int commandId) {
    switch (commandId) {
        case IDC_SHORTLIST_BUTTON:
            runShortlistAction(state);
            break;
        case IDC_BUY_BUTTON:
            runBuyAction(state);
            break;
        case IDC_PRECONTRACT_BUTTON:
            runPreContractAction(state);
            break;
        case IDC_LOAN_BUTTON:
            runLoanAction(state);
            break;
        case IDC_RENEW_BUTTON:
            runRenewAction(state);
            break;
        case IDC_SELL_BUTTON:
            runSellAction(state);
            break;
        case IDC_PLAN_BUTTON:
            runPlanAction(state);
            break;
        case IDC_INSTRUCTION_BUTTON:
            runInstructionAction(state);
            break;
        default:
            break;
    }
}

LRESULT CALLBACK playerContextMenuProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<AppState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            if (state) {
                PAINTSTRUCT paint{};
                HDC hdc = BeginPaint(hwnd, &paint);

                RECT client{};
                GetClientRect(hwnd, &client);
                const auto s = [&](int value) { return scaleByDpi(*state, value); };

                drawRoundedPanel(hdc,
                                 client,
                                 RGB(7, 20, 28),
                                 RGB(54, 96, 119),
                                 s(18));

                RECT accent{
                    client.left + s(14),
                    client.top + s(10),
                    client.left + s(72),
                    client.top + s(14)
                };
                HBRUSH accentBrush = CreateSolidBrush(kThemeAccent);
                FillRect(hdc, &accent, accentBrush);
                DeleteObject(accentBrush);

                RECT heading{
                    client.left + s(14),
                    client.top + s(14),
                    client.right - s(14),
                    client.top + playerContextMenuHeaderHeight(*state)
                };
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(193, 207, 216));
                HGDIOBJ oldFont = SelectObject(hdc,
                    state->sectionFont ? state->sectionFont :
                    (state->font ? state->font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))));
                DrawTextW(hdc,
                          L"ACCIONES DEL JUGADOR",
                          -1,
                          &heading,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc,
                    state->font ? state->font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));

                const int width = client.right - client.left;
                for (int i = 0; i < static_cast<int>(state->playerContextMenu.entries.size()); ++i) {
                    const auto& entry = state->playerContextMenu.entries[static_cast<size_t>(i)];
                    RECT item = playerContextMenuItemRect(*state, i, width);

                    if (entry.separator) {
                        const int y = (item.top + item.bottom) / 2;
                        HPEN pen = CreatePen(PS_SOLID, 1, RGB(40, 63, 77));
                        HGDIOBJ oldPen = SelectObject(hdc, pen);
                        MoveToEx(hdc, item.left + s(8), y, nullptr);
                        LineTo(hdc, item.right - s(8), y);
                        SelectObject(hdc, oldPen);
                        DeleteObject(pen);
                        continue;
                    }

                    const bool hovered = i == state->playerContextMenu.hoveredIndex;
                    if (hovered) {
                        drawRoundedPanel(hdc,
                                         item,
                                         RGB(22, 58, 75),
                                         RGB(68, 124, 153),
                                         s(10));
                    }

                    RECT marker{
                        item.left + s(9),
                        item.top + s(13),
                        item.left + s(14),
                        item.bottom - s(13)
                    };
                    HBRUSH markerBrush = CreateSolidBrush(
                        entry.commandId == IDC_SELL_BUTTON ? RGB(198, 96, 102) :
                        (hovered ? kThemeAccent : RGB(71, 126, 161)));
                    FillRect(hdc, &marker, markerBrush);
                    DeleteObject(markerBrush);

                    RECT textRect = item;
                    textRect.left += s(26);
                    textRect.right -= s(10);
                    SetTextColor(hdc,
                                 entry.enabled
                                     ? (hovered ? RGB(255, 247, 219) : kThemeText)
                                     : RGB(102, 117, 126));
                    DrawTextW(hdc,
                              entry.text.c_str(),
                              -1,
                              &textRect,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }

                SelectObject(hdc, oldFont);
                EndPaint(hwnd, &paint);
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (state) {
                POINT point{
                    static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
                    static_cast<LONG>(static_cast<short>(HIWORD(lParam)))
                };
                const int hovered = playerContextMenuHitTest(*state, point);
                if (hovered != state->playerContextMenu.hoveredIndex) {
                    state->playerContextMenu.hoveredIndex = hovered;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                SetCursor(LoadCursorW(nullptr, hovered >= 0 ? MAKEINTRESOURCEW(32649) : MAKEINTRESOURCEW(32512)));
                return 0;
            }
            break;

        case WM_LBUTTONUP:
            if (state) {
                POINT point{
                    static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
                    static_cast<LONG>(static_cast<short>(HIWORD(lParam)))
                };
                const int index = playerContextMenuHitTest(*state, point);
                int commandId = 0;
                if (index >= 0) {
                    commandId = state->playerContextMenu.entries[static_cast<size_t>(index)].commandId;
                }

                if (GetCapture() == hwnd) ReleaseCapture();
                DestroyWindow(hwnd);

                if (commandId != 0) {
                    executePlayerContextCommand(*state, commandId);
                }
                return 0;
            }
            break;

        case WM_RBUTTONUP:
            if (GetCapture() == hwnd) ReleaseCapture();
            DestroyWindow(hwnd);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (GetCapture() == hwnd) ReleaseCapture();
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            if (state && state->playerContextMenu.window == hwnd) {
                if (GetCapture() == hwnd) ReleaseCapture();
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_NCDESTROY:
            if (state && state->playerContextMenu.window == hwnd) {
                state->playerContextMenu.window = nullptr;
                state->playerContextMenu.hoveredIndex = -1;
            }
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ensurePlayerContextMenuClass(AppState& state) {
    static bool registered = false;
    if (registered) return true;

    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = playerContextMenuProc;
    cls.hInstance = state.instance;
    cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    cls.hbrBackground = nullptr;
    cls.lpszClassName = kPlayerContextMenuClass;

    if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    registered = true;
    return true;
}

void showPlayerContextMenuPopup(AppState& state, POINT cursor) {
    if (state.playerContextMenu.window) {
        DestroyWindow(state.playerContextMenu.window);
        state.playerContextMenu.window = nullptr;
    }
    if (!ensurePlayerContextMenuClass(state) || state.playerContextMenu.entries.empty()) return;

    const int width = scaleByDpi(state, 278);
    const int height = playerContextMenuHeight(state);

    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(monitor, &monitorInfo);

    int x = cursor.x + scaleByDpi(state, 8);
    int y = cursor.y + scaleByDpi(state, 6);
    x = std::max(static_cast<int>(monitorInfo.rcWork.left) + scaleByDpi(state, 6),
                 std::min(x, static_cast<int>(monitorInfo.rcWork.right) - width - scaleByDpi(state, 6)));
    y = std::max(static_cast<int>(monitorInfo.rcWork.top) + scaleByDpi(state, 6),
                 std::min(y, static_cast<int>(monitorInfo.rcWork.bottom) - height - scaleByDpi(state, 6)));

    HWND popup = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kPlayerContextMenuClass,
        L"",
        WS_POPUP,
        x,
        y,
        width,
        height,
        state.window,
        nullptr,
        state.instance,
        &state);

    if (!popup) return;

    state.playerContextMenu.window = popup;
    state.playerContextMenu.hoveredIndex = -1;

    HRGN region = CreateRoundRectRgn(0,
                                     0,
                                     width + 1,
                                     height + 1,
                                     scaleByDpi(state, 18),
                                     scaleByDpi(state, 18));
    if (region && SetWindowRgn(popup, region, FALSE) == 0) {
        DeleteObject(region);
    }

    SetWindowPos(popup,
                 HWND_TOP,
                 x,
                 y,
                 width,
                 height,
                 SWP_SHOWWINDOW);
    SetForegroundWindow(popup);
    SetFocus(popup);
    SetCapture(popup);
}
}  // namespace

bool isFrontMenuPage(GuiPage page) {
    return page == GuiPage::MainMenu || page == GuiPage::Settings || page == GuiPage::Credits || page == GuiPage::Saves;
}

bool isHeavyPage(GuiPage page) {
    return page == GuiPage::Transfers || page == GuiPage::Finances || page == GuiPage::News;
}

namespace {

void syncSetupButtonsAndHints(AppState& state) {
    const bool hasCareer = state.career.myTeam != nullptr;
    const bool hasSavedCareer = hasPersistedCareer(state);
    const bool busy = state.actionInProgress;
    EnableWindow(state.divisionCombo, !busy && !hasCareer);
    EnableWindow(state.teamCombo, !busy && !hasCareer && hasAvailableTeams(state, state.gameSetup.division));
    EnableWindow(state.managerEdit, !busy);
    EnableWindow(state.newCareerButton, !busy && !hasCareer && state.gameSetup.ready);
    EnableWindow(state.emptyNewButton, !busy && state.gameSetup.ready);
    EnableWindow(state.loadButton, !busy);
    EnableWindow(state.saveButton, !busy && hasCareer);
    EnableWindow(state.simulateButton, !busy && hasCareer);
    EnableWindow(state.validateButton, !busy);
    EnableWindow(state.displayModeButton, !busy);
    EnableWindow(state.frontMenuButton, !busy);
    EnableWindow(state.menuContinueButton, !busy && (hasCareer || hasSavedCareer));
    EnableWindow(state.menuLoadButton, state.currentPage == GuiPage::MainMenu
                                           ? !busy
                                           : (!busy && (state.currentPage == GuiPage::Saves ? !state.selectedSavePath.empty() : hasSavedCareer)));
    EnableWindow(state.menuDeleteSaveButton, state.currentPage == GuiPage::MainMenu
                                                ? !busy
                                                : (!busy && (state.currentPage == GuiPage::Saves ? (!state.selectedSavePath.empty() && !hasCareer) : hasSavedCareer)));
    EnableWindow(state.menuApplySettingsButton, !busy && state.settingsDirty);
    EnableWindow(state.menuResetSettingsButton, !busy && state.settingsDirty);

    if (state.divisionLabel) {
        std::string badge = "Division";
        const std::string resolved = resolveKnownDivisionId(state, state.gameSetup.division.empty() ? state.career.activeDivision : state.gameSetup.division);
        if (hasCareer && !resolved.empty()) {
            badge += " [" + divisionDisplay(resolved) + "]";
        }
        setWindowTextUtf8(state.divisionLabel, badge);
    }

    if (state.managerHelpLabel) {
        std::string helper;
        if (hasCareer) {
            const std::string resolved = resolveKnownDivisionId(state, state.career.activeDivision);
            helper = "Carrera activa.";
            if (!resolved.empty()) helper += " Division resuelta: " + divisionDisplay(resolved) + ".";
            helper += " Guardar y simular ya estan disponibles.";
        } else {
            helper = state.gameSetup.managerError;
            if (helper.empty()) {
                helper = state.gameSetup.manager.empty()
                    ? "Completa el nombre del manager."
                    : (state.gameSetup.ready ? "Manager valido. Puedes iniciar la partida."
                                             : "Manager listo. Completa los pasos pendientes.");
            }
        }
        updateDynamicStaticText(state, state.managerHelpLabel, helper);
    }
    if (state.newCareerButton) InvalidateRect(state.newCareerButton, nullptr, TRUE);
    if (state.emptyNewButton) InvalidateRect(state.emptyNewButton, nullptr, TRUE);
}

}  // namespace

std::string selectedDivisionId(const AppState& state) {
    if (!state.gameSetup.division.empty()) return state.gameSetup.division;
    int index = comboIndex(state.divisionCombo);
    if (index < 0 || index >= static_cast<int>(state.career.divisions.size())) return std::string();
    return state.career.divisions[static_cast<size_t>(index)].id;
}

void fillDivisionCombo(AppState& state, const std::string& selectedId) {
    state.suppressComboEvents = true;
    SendMessageW(state.divisionCombo, CB_RESETCONTENT, 0, 0);
    int selectedIndex = -1;
    for (size_t i = 0; i < state.career.divisions.size(); ++i) {
        addComboItem(state.divisionCombo, state.career.divisions[i].display);
        if (!selectedId.empty() && state.career.divisions[i].id == selectedId) selectedIndex = static_cast<int>(i);
    }
    SendMessageW(state.divisionCombo, CB_SETCURSEL, selectedIndex, 0);
    state.suppressComboEvents = false;
}

void fillTeamCombo(AppState& state, const std::string& divisionId, const std::string& selectedTeam) {
    state.suppressComboEvents = true;
    SendMessageW(state.teamCombo, CB_RESETCONTENT, 0, 0);
    std::vector<Team*> teams = state.career.getDivisionTeams(divisionId);
    int selectedIndex = -1;
    for (size_t i = 0; i < teams.size(); ++i) {
        addComboItem(state.teamCombo, teams[i]->name);
        if (!selectedTeam.empty() && teams[i]->name == selectedTeam) selectedIndex = static_cast<int>(i);
    }
    SendMessageW(state.teamCombo, CB_SETCURSEL, selectedIndex, 0);
    state.suppressComboEvents = false;
    EnableWindow(state.teamCombo, !state.career.myTeam && !divisionId.empty() && !teams.empty());
}

void syncManagerNameFromUi(AppState& state) {
    std::string name = getWindowTextUtf8(state.managerEdit);
    state.gameSetup.manager = trim(name);
    if (state.career.myTeam) {
        state.career.managerName = state.gameSetup.manager.empty() ? "Manager" : state.gameSetup.manager;
    }
}

void syncCombosFromCareer(AppState& state) {
    const std::string resolvedDivision = inferHeaderDivisionId(state);

    if (!resolvedDivision.empty() && state.career.activeDivision != resolvedDivision) {
        state.career.setActiveDivision(resolvedDivision);
    }
    if (state.career.myTeam) {
        state.gameSetup.division = !resolvedDivision.empty() ? resolvedDivision : state.career.myTeam->division;
        state.gameSetup.club = state.career.myTeam->name;
        state.gameSetup.manager = trim(state.career.managerName);
    } else if (!resolvedDivision.empty()) {
        state.gameSetup.division = resolvedDivision;
    }
    fillDivisionCombo(state, state.gameSetup.division);
    fillTeamCombo(state, state.gameSetup.division, state.gameSetup.club);
    if (state.managerEdit && trim(getWindowTextUtf8(state.managerEdit)) != state.gameSetup.manager) {
        setWindowTextUtf8(state.managerEdit, state.gameSetup.manager);
    }
    check_game_ready(state);
}

void set_division(AppState& state, const std::string& divisionId) {
    state.gameSetup.division = isKnownDivision(state, divisionId) ? divisionId : std::string();
    if (!clubBelongsToDivision(state, state.gameSetup.division, state.gameSetup.club)) {
        state.gameSetup.club.clear();
    }
    fillDivisionCombo(state, state.gameSetup.division);
    fillTeamCombo(state, state.gameSetup.division, state.gameSetup.club);
    check_game_ready(state);
    setStatus(state, state.gameSetup.inlineMessage);
    refreshCurrentPage(state);
}

void set_club(AppState& state, const std::string& clubName) {
    state.gameSetup.club = clubBelongsToDivision(state, state.gameSetup.division, clubName) ? clubName : std::string();
    fillTeamCombo(state, state.gameSetup.division, state.gameSetup.club);
    check_game_ready(state);
    setStatus(state, state.gameSetup.inlineMessage);
    refreshCurrentPage(state);
}

void set_manager(AppState& state, const std::string& managerName) {
    state.gameSetup.manager = trim(managerName);
    check_game_ready(state);
    setStatus(state, state.gameSetup.ready ? "Checklist completa. Nueva partida habilitada." : state.gameSetup.inlineMessage);
    refreshCurrentPage(state);
}

bool check_game_ready(AppState& state) {
    state.gameSetup.manager = trim(state.gameSetup.manager);
    state.gameSetup.ready = false;
    state.gameSetup.managerError.clear();
    state.gameSetup.inlineMessage.clear();

    if (state.gameSetup.division.empty()) {
        state.gameSetup.currentStep = 1;
        state.gameSetup.inlineMessage = "Paso 1 pendiente: elige una division para cargar clubes.";
    } else if (!hasAvailableTeams(state, state.gameSetup.division)) {
        state.gameSetup.currentStep = 2;
        state.gameSetup.inlineMessage = "Paso 2 bloqueado: esta division no tiene clubes disponibles.";
    } else if (state.gameSetup.club.empty()) {
        state.gameSetup.currentStep = 2;
        state.gameSetup.inlineMessage = "Paso 2 pendiente: selecciona el club que quieres dirigir.";
    } else if (state.gameSetup.manager.empty()) {
        state.gameSetup.currentStep = 3;
        state.gameSetup.managerError = "Ingresa nombre del manager.";
        state.gameSetup.inlineMessage = "Paso 3 pendiente: escribe el nombre del manager para iniciar.";
    } else {
        state.gameSetup.currentStep = 3;
        state.gameSetup.ready = true;
        state.gameSetup.inlineMessage = "Checklist completa. La partida esta lista para empezar.";
    }

    syncSetupButtonsAndHints(state);
    return state.gameSetup.ready;
}

void setStatus(AppState& state, const std::string& text) {
    updateDynamicStaticText(state, state.statusLabel, text);
}

void refreshCurrentPage(AppState& state) {
    if (state.pageRefreshInProgress) return;
    state.pageRefreshInProgress = true;
    ScopedRefreshRedrawLock redrawLock(state);
    const DWORD refreshStart = GetTickCount();

    if (state.career.myTeam || (state.gameSetup.division.empty() && !state.gameSetup.club.empty())) {
        syncCombosFromCareer(state);
    }
    check_game_ready(state);
    refreshFilterComboOptions(state);
    const std::string cacheKey = pageCacheKey(state, state.currentPage);
    const std::string cacheSignature = pageCacheSignature(state, state.currentPage);
    if (canUseCachedModel(state) &&
        state.modelCacheSignatures.count(cacheKey) &&
        state.modelCacheSignatures[cacheKey] == cacheSignature &&
        state.modelCache.count(cacheKey)) {
        state.currentModel = state.modelCache[cacheKey];
    } else {
        state.currentModel = buildModel(state);
        if (canUseCachedModel(state)) {
            state.modelCache[cacheKey] = state.currentModel;
            state.modelCacheSignatures[cacheKey] = cacheSignature;
        }
    }
    syncSetupButtonsAndHints(state);
    syncMenuMusicForPage(state);
    state.insightHotspots.clear();
    bool dashboardEmptyState = state.currentPage == GuiPage::Dashboard && !state.career.myTeam;
    const bool frontMenuPage = isFrontMenuPage(state.currentPage);

    if (frontMenuPage) {
        updateDynamicStaticText(state, state.pageTitleLabel, "");
        updateDynamicStaticText(state, state.breadcrumbLabel, "");
        updateDynamicStaticText(state, state.infoLabel, "");
    } else {
        updateDynamicStaticText(state, state.pageTitleLabel, state.currentModel.title);
        updateDynamicStaticText(state, state.breadcrumbLabel, state.currentModel.breadcrumb);
        updateDynamicStaticText(state, state.infoLabel, state.currentModel.infoLine);
    }
    updateDynamicStaticText(state, state.summaryLabel, friendlyPanelTitle(state.currentModel.summary.title));
    setWindowTextUtf8(state.summaryEdit, state.currentModel.summary.content);
    updateDynamicStaticText(state, state.detailLabel, friendlyPanelTitle(state.currentModel.detail.title));
    setWindowTextUtf8(state.detailEdit, state.currentModel.detail.content);
    updateDynamicStaticText(state, state.newsLabel, friendlyPanelTitle(state.currentModel.feed.title));
    if (state.instructionButton) {
        std::string actionLabel = "Instruccion";
        if (state.currentPage == GuiPage::Dashboard) {
            actionLabel = dashboardShowsPostWeekDigest(state) ? "Decidir" : "Reunion";
        }
        else if (state.currentPage == GuiPage::Squad || state.currentPage == GuiPage::Youth) actionLabel = "Hablar";
        else if (state.currentPage == GuiPage::Board) actionLabel = "Staff";
        else if (state.currentPage == GuiPage::News) actionLabel = "Decidir";
        else if (state.currentPage == GuiPage::Tactics || state.currentPage == GuiPage::Calendar) actionLabel = "Plan rival";
        setWindowTextUtf8(state.instructionButton, actionLabel);
    }
    if (state.planButton) {
        std::string planLabel = "Plan";
        if (state.currentPage == GuiPage::Squad) planLabel = "Instrucc.";
        setWindowTextUtf8(state.planButton, planLabel);
    }
    if (state.scoutActionButton) {
        setWindowTextUtf8(state.scoutActionButton, state.currentPage == GuiPage::News ? "Asignar" : "Otear");
    }
    if (state.loanButton) {
        setWindowTextUtf8(state.loanButton, state.currentPage == GuiPage::Transfers ? "Pedir cesion" : "Ceder");
    }

    renderListPanel(state, state.tableLabel, state.tableList, state.currentModel.primary);
    renderListPanel(state, state.squadLabel, state.squadList, state.currentModel.secondary);
    renderListPanel(state, state.transferLabel, state.transferList, state.currentModel.footer);
    renderFeed(state.newsList, state.currentModel.feed.lines);
    syncSaveBrowserSelection(state);

    bool showTable = !frontMenuPage && state.currentPage != GuiPage::Tactics && !dashboardEmptyState;
    bool showTableLabel = showTable;
    bool showSquad = !frontMenuPage && !dashboardEmptyState;
    bool showSquadLabel = showSquad;
    bool showFooter = !frontMenuPage && !dashboardEmptyState;
    bool showFooterLabel = showFooter;
    bool showFilter = !frontMenuPage && !dashboardEmptyState && state.currentPage != GuiPage::Dashboard && !(state.currentPage == GuiPage::Transfers && state.globalSearchActive);
    bool showGlobalSearch = !frontMenuPage && state.currentPage == GuiPage::Transfers && state.globalSearchActive;
    setControlVisibility(state, state.tableList, showTable);
    setControlVisibility(state, state.tableLabel, showTableLabel);
    setControlVisibility(state, state.squadList, showSquad);
    setControlVisibility(state, state.squadLabel, showSquadLabel);
    setControlVisibility(state, state.transferList, showFooter);
    setControlVisibility(state, state.transferLabel, showFooterLabel);
    setControlVisibility(state, state.filterLabel, showFilter);
    setControlVisibility(state, state.filterCombo, showFilter);
    setControlVisibility(state, state.globalSearchEdit, showGlobalSearch);

    layoutWindow(state);
    autosizeVisibleLists(state);
    const long long elapsed = static_cast<long long>(GetTickCount() - refreshStart);
    state.pageTraceMs[static_cast<int>(state.currentPage)] = elapsed;
    state.lastPageTrace = pageTitleFor(state.currentPage) + " render " + std::to_string(elapsed) + " ms";
    if (isHeavyPage(state.currentPage) && state.infoLabel) {
        setWindowTextUtf8(state.infoLabel, state.currentModel.infoLine + " | " + state.lastPageTrace);
    }
}

void autosizeCurrentLists(AppState& state) {
    autosizeVisibleLists(state);
}

void refreshAll(AppState& state) {
    check_game_ready(state);
    refreshCurrentPage(state);
}

void setCurrentPage(AppState& state, GuiPage page) {
    if (state.currentPage != page) {
        state.pageScrollY = 0;
    }
    state.currentPage = page;
    std::vector<std::string> options = filterOptionsForPage(page);
    state.currentFilter = options.empty() ? std::string() : options.front();
    refreshCurrentPage(state);
}

void queuePageTransition(AppState& state, GuiPage page) {
    if (!state.window || !IsWindow(state.window)) {
        setCurrentPage(state, page);
        return;
    }
    if (state.pageChangeQueued && state.queuedPage == page) return;
    state.queuedPage = page;
    state.pageChangeQueued = true;
    setCurrentPage(state, page); // cambiar ahora para evitar que el menu principal quede mostrado momentáneamente
    PostMessageW(state.window, kGuiPageTransitionMessage, static_cast<WPARAM>(static_cast<int>(page)), 0);
}

void handleFilterChange(AppState& state) {
    state.currentFilter = comboText(state.filterCombo);
    refreshCurrentPage(state);
}

void handleListSelectionChange(AppState& state, int controlId) {
    if (state.pageRefreshInProgress) return;
    int row = -1;
    if (controlId == IDC_SQUAD_LIST) row = selectedListViewRow(state.squadList);
    if (controlId == IDC_TRANSFER_LIST) row = selectedListViewRow(state.transferList);
    if (controlId == IDC_TABLE_LIST) row = selectedListViewRow(state.tableList);
    if (row < 0) return;

    if (isDashboardActionCueList(state, controlId)) {
        const std::string destination = listViewText(state.transferList, row, 1);
        const std::string action = listViewText(state.transferList, row, 2);
        setStatus(state, "Doble click o Enter: " + action + " -> " + destination + ".");
        return;
    }

    if ((state.currentPage == GuiPage::Squad || state.currentPage == GuiPage::Youth) && controlId == IDC_SQUAD_LIST) {
        const std::string playerName = listViewText(state.squadList, row, 0);
        if (playerName.empty() || playerName == state.selectedPlayerName) return;
        state.selectedPlayerName = playerName;
        refreshCurrentPage(state);
        return;
    }
    if (state.currentPage == GuiPage::Transfers && controlId == IDC_TABLE_LIST) {
        const std::string playerName = listViewText(state.tableList, row, 0);
        const std::string clubName = listViewText(state.tableList, row, 10);
        if (playerName.empty() || clubName.empty()) return;
        if (playerName == state.selectedTransferPlayer && clubName == state.selectedTransferClub) return;
        state.selectedTransferPlayer = playerName;
        state.selectedTransferClub = clubName;
        refreshCurrentPage(state);
        return;
    }
    if ((state.currentPage == GuiPage::League || state.currentPage == GuiPage::Calendar) && controlId == IDC_TABLE_LIST) {
        InvalidateRect(state.window, nullptr, TRUE);
    }
}

void handleFeedSelectionChange(AppState& state, int controlId) {
    if (state.pageRefreshInProgress || controlId != IDC_NEWS_LIST || state.currentPage != GuiPage::Saves) return;
    const LRESULT row = SendMessageW(state.newsList, LB_GETCURSEL, 0, 0);
    if (row == LB_ERR || row < 0 || row >= static_cast<LRESULT>(state.saveSlotPaths.size())) return;
    state.selectedSavePath = state.saveSlotPaths[static_cast<size_t>(row)];
    refreshCurrentPage(state);
    setStatus(state, "Guardado seleccionado: " + state.selectedSavePath + ".");
}

void activateListAction(AppState& state, int controlId) {
    if (state.pageRefreshInProgress) return;
    int row = -1;
    if (controlId == IDC_SQUAD_LIST) row = selectedListViewRow(state.squadList);
    if (controlId == IDC_TRANSFER_LIST) row = selectedListViewRow(state.transferList);
    if (controlId == IDC_TABLE_LIST) row = selectedListViewRow(state.tableList);
    if (row < 0) return;

    if (isDashboardActionCueList(state, controlId)) {
        const std::string destination = listViewText(state.transferList, row, 1);
        const std::string action = listViewText(state.transferList, row, 2);
        if (isApplyDecisionCommand(action)) {
            if (state.actionInProgress) {
                setStatus(state, "Accion bloqueada: espera a que termine la simulacion semanal.");
                return;
            }
            ServiceResult result = applyWeeklyDecisionService(state.career, WeeklyDecision::Auto);
            if (result.ok) {
                consumeLatestWeeklyDigestService(state.career);
                syncCombosFromCareer(state);
                refreshAll(state);
            }
            setStatus(state, result.messages.empty()
                                 ? (result.ok ? "Decision semanal aplicada." : "No se pudo aplicar la decision semanal.")
                                 : result.messages.back());
            return;
        }
        const GuiPage targetPage = dashboardActionDestinationPage(destination);
        if (targetPage == GuiPage::Dashboard && toLower(trim(action)) == "simular") {
            if (state.actionInProgress) {
                setStatus(state, "Simulacion en progreso: la accion de simular ya esta corriendo.");
                return;
            }
            simulateWeek(state);
            return;
        }
        setCurrentPage(state, targetPage);
        setStatus(state, "Accion abierta: " + action + " -> " + pageTitleFor(targetPage) + ".");
    }
}

void handlePlayerContextMenu(AppState& state, int controlId, const NMITEMACTIVATE& activation) {
    if (state.pageRefreshInProgress || activation.iItem < 0) return;

    const bool managedPlayer =
        (state.currentPage == GuiPage::Squad || state.currentPage == GuiPage::Youth) &&
        controlId == IDC_SQUAD_LIST;
    const bool transferTarget =
        state.currentPage == GuiPage::Transfers &&
        controlId == IDC_TABLE_LIST;

    if (!managedPlayer && !transferTarget) return;

    HWND list = managedPlayer ? state.squadList : state.tableList;
    if (!list) return;

    POINT cursor{};
    GetCursorPos(&cursor);

    ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(list,
                          activation.iItem,
                          LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(list, activation.iItem, FALSE);

    if (managedPlayer) {
        const std::string playerName = listViewText(list, activation.iItem, 0);
        if (playerName.empty()) return;
        state.selectedPlayerName = playerName;
    } else {
        const std::string playerName = listViewText(list, activation.iItem, 0);
        const std::string clubName = listViewText(list, activation.iItem, 10);
        if (playerName.empty() || clubName.empty()) return;
        state.selectedTransferPlayer = playerName;
        state.selectedTransferClub = clubName;
    }

    refreshCurrentPage(state);

    state.playerContextMenu.entries.clear();
    if (transferTarget) {
        state.playerContextMenu.entries.push_back({IDC_SHORTLIST_BUTTON, L"Agregar a shortlist", false, !state.actionInProgress});
        state.playerContextMenu.entries.push_back({0, L"", true, false});
        state.playerContextMenu.entries.push_back({IDC_BUY_BUTTON, L"Comprar", false, !state.actionInProgress});
        state.playerContextMenu.entries.push_back({IDC_PRECONTRACT_BUTTON, L"Precontrato", false, !state.actionInProgress});
        state.playerContextMenu.entries.push_back({IDC_LOAN_BUTTON, L"Pedir cesion", false, !state.actionInProgress});
    } else {
        state.playerContextMenu.entries.push_back({IDC_LOAN_BUTTON, L"Ceder", false, !state.actionInProgress});
        state.playerContextMenu.entries.push_back({IDC_RENEW_BUTTON, L"Renovar contrato", false, !state.actionInProgress});
        state.playerContextMenu.entries.push_back({IDC_SELL_BUTTON, L"Vender", false, !state.actionInProgress});
        state.playerContextMenu.entries.push_back({0, L"", true, false});
        state.playerContextMenu.entries.push_back({
            IDC_PLAN_BUTTON,
            state.currentPage == GuiPage::Youth ? L"Cambiar plan de desarrollo" : L"Cambiar instruccion individual",
            false,
            !state.actionInProgress
        });
        state.playerContextMenu.entries.push_back({IDC_INSTRUCTION_BUTTON, L"Hablar con jugador", false, !state.actionInProgress});
    }

    showPlayerContextMenuPopup(state, cursor);
}
void handleListColumnClick(AppState& state, const NMLISTVIEW& view) {
    if (state.pageRefreshInProgress) return;
    if ((state.currentPage == GuiPage::Squad || state.currentPage == GuiPage::Youth) && view.hdr.idFrom == IDC_SQUAD_LIST) {
        if (state.squadSort.column == view.iSubItem) {
            state.squadSort.ascending = !state.squadSort.ascending;
        } else {
            state.squadSort.column = view.iSubItem;
            state.squadSort.ascending = true;
        }
        refreshCurrentPage(state);
    }
}

}  // namespace gui_win32

#endif
