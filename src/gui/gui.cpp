#include "gui/gui.h"
#include "gui/gui_audio.h"
#include "gui/gui_internal.h"
#include <cstring>

#ifdef _WIN32

namespace gui_win32 {

namespace {

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using SetProcessDpiAwareFn = BOOL(WINAPI*)();

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

void enableHighDpiSupport() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");

    if (!user32) {
        return;
    }

    FARPROC proc = GetProcAddress(user32, "SetProcessDpiAwarenessContext");

    SetProcessDpiAwarenessContextFn setDpiContext = nullptr;

    if (proc) {
        setDpiContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(reinterpret_cast<void*>(proc));
    }

    if (setDpiContext) {
        if (setDpiContext(reinterpret_cast<HANDLE>(-4))) {
            return;
        }
    }


    proc = GetProcAddress(user32, "SetProcessDPIAware");

    SetProcessDpiAwareFn setDpiAware = nullptr;

    if (proc) {
        setDpiAware = reinterpret_cast<SetProcessDpiAwareFn>(reinterpret_cast<void*>(proc));
    }

    if (setDpiAware) {
        setDpiAware();
    }
}


UINT queryWindowDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");

    if (user32) {

        FARPROC proc = GetProcAddress(user32, "GetDpiForWindow");

        GetDpiForWindowFn getDpiForWindow = nullptr;

        if (proc) {
            getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(reinterpret_cast<void*>(proc));
        }

        if (getDpiForWindow) {

            UINT dpi = getDpiForWindow(hwnd);

            if (dpi != 0) {
                return dpi;
            }
        }
    }


    HDC hdc = GetDC(hwnd ? hwnd : nullptr);

    UINT dpi = 96;

    if (hdc) {
        dpi = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX));
        ReleaseDC(hwnd ? hwnd : nullptr, hdc);
    }

    return dpi == 0 ? 96 : dpi;
}

RECT primaryMonitorWorkArea() {
    RECT rect{0, 0, 1600, 900};
    POINT origin{0, 0};
    HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        rect = info.rcWork;
    }
    return rect;
}

DisplayMode currentDisplayMode(const AppState& state) {
    if (state.isBorderlessFullscreen) return DisplayMode::BorderlessFullscreen;
    if (state.window && IsZoomed(state.window)) return DisplayMode::MaximizedWindow;
    return DisplayMode::RestoredWindow;
}

std::string displayModeButtonText(const AppState& state) {
    switch (currentDisplayMode(state)) {
        case DisplayMode::RestoredWindow:
            return "Maximizar F11";
        case DisplayMode::MaximizedWindow:
            return "Pantalla F11";
        case DisplayMode::BorderlessFullscreen:
            return "Ventana F11";
    }
    return "Pantalla F11";
}

void syncDisplayModeButton(AppState& state) {
    if (!state.displayModeButton) return;
    setWindowTextUtf8(state.displayModeButton, displayModeButtonText(state));
    InvalidateRect(state.displayModeButton, nullptr, TRUE);
}

void openPageWithFilter(AppState& state, GuiPage page, const std::string& filter) {
    if (state.currentPage != page) {
        state.pageScrollY = 0;
    }
    state.currentPage = page;
    state.currentFilter = filter;
    refreshCurrentPage(state);
}

bool scrollViewportTo(AppState& state, int target) {
    const int clamped = std::max(0, std::min(target, std::max(0, state.maxPageScrollY)));
    if (clamped == state.pageScrollY) return false;
    state.pageScrollY = clamped;
    layoutWindow(state);
    InvalidateRect(state.window, nullptr, TRUE);
    return true;
}

bool scrollViewportBy(AppState& state, int delta) {
    return scrollViewportTo(state, state.pageScrollY + delta);
}

int pageScrollStep(const AppState& state) {
    return scaleByDpi(state, 72);
}

int pageScrollJump(const AppState& state) {
    RECT client{};
    GetClientRect(state.window, &client);
    return std::max(scaleByDpi(state, 240), static_cast<int>((client.bottom * 3) / 4));
}

bool restoreWindowedPlacement(AppState& state) {
    if (!state.window) return false;

    if (state.restoreStyle != 0) {
        SetWindowLongPtrW(state.window, GWL_STYLE, static_cast<LONG_PTR>(state.restoreStyle));
        SetWindowLongPtrW(state.window, GWL_EXSTYLE, static_cast<LONG_PTR>(state.restoreExStyle));
    }

    WINDOWPLACEMENT placement = state.restorePlacement;
    placement.length = sizeof(WINDOWPLACEMENT);
    if (placement.rcNormalPosition.left == 0 && placement.rcNormalPosition.right == 0) {
        GetWindowPlacement(state.window, &placement);
    }
    placement.flags = 0;
    placement.showCmd = SW_SHOWNORMAL;
    SetWindowPlacement(state.window, &placement);
    SetWindowPos(state.window,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    ShowWindow(state.window, SW_RESTORE);
    state.isBorderlessFullscreen = false;
    syncDisplayModeButton(state);
    setStatus(state, "Ventana restaurada. El siguiente paso del ciclo es maximizar y luego fullscreen.");
    return true;
}

bool enterBorderlessFullscreen(AppState& state) {
    if (!state.window) return false;

    state.restoreStyle = static_cast<DWORD>(GetWindowLongPtrW(state.window, GWL_STYLE));
    state.restoreExStyle = static_cast<DWORD>(GetWindowLongPtrW(state.window, GWL_EXSTYLE));
    state.restorePlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(state.window, &state.restorePlacement);

    HMONITOR monitor = MonitorFromWindow(state.window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT target = primaryMonitorWorkArea();
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        target = info.rcMonitor;
    }

    SetWindowLongPtrW(state.window,
                      GWL_STYLE,
                      static_cast<LONG_PTR>((state.restoreStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP | WS_VISIBLE));
    SetWindowLongPtrW(state.window, GWL_EXSTYLE, static_cast<LONG_PTR>(state.restoreExStyle));
    SetWindowPos(state.window,
                 HWND_TOP,
                 target.left,
                 target.top,
                 target.right - target.left,
                 target.bottom - target.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    state.isBorderlessFullscreen = true;
    syncDisplayModeButton(state);
    setStatus(state, "Pantalla completa sin borde activa. El siguiente paso del ciclo devuelve la ventana restaurada.");
    return true;
}

void maximizeWindow(AppState& state) {
    if (!state.window) return;
    ShowWindow(state.window, SW_MAXIMIZE);
    state.isBorderlessFullscreen = false;
    syncDisplayModeButton(state);
    setStatus(state, "Ventana maximizada. Usa Pantalla o F11 para pasar al fullscreen sin borde.");
}

const InsightHotspot* hitInsightHotspot(const AppState& state, POINT point) {
    for (const auto& hotspot : state.insightHotspots) {
        if (hotspot.action != InsightAction::None && PtInRect(&hotspot.rect, point)) {
            return &hotspot;
        }
    }
    return nullptr;
}

bool insightActionAllowedDuringAction(InsightAction action) {
    switch (action) {
        case InsightAction::OpenLeague:
        case InsightAction::OpenSquad:
        case InsightAction::OpenBoard:
        case InsightAction::OpenFinanceSummary:
        case InsightAction::OpenFinanceSalaries:
        case InsightAction::OpenFinanceInfrastructure:
        case InsightAction::OpenBoardSummary:
        case InsightAction::OpenBoardObjectives:
        case InsightAction::OpenBoardHistory:
        case InsightAction::None:
            return true;
        case InsightAction::FocusDivision:
        case InsightAction::FocusClub:
        case InsightAction::FocusManager:
        case InsightAction::StartCareer:
        case InsightAction::RefreshMarketPulse:
        case InsightAction::RunScouting:
            return false;
    }
    return false;
}

void executeInsightAction(AppState& state, InsightAction action) {
    if (state.actionInProgress && !insightActionAllowedDuringAction(action)) {
        setStatus(state, "Simulacion en progreso: espera el cierre de la semana para ejecutar esa accion.");
        return;
    }

    switch (action) {
        case InsightAction::FocusDivision:
            SetFocus(state.divisionCombo);
            setStatus(state, "Insight: elige la division para definir el universo de la partida.");
            return;
        case InsightAction::FocusClub:
            SetFocus(state.teamCombo);
            setStatus(state, "Insight: elige el club para fijar proyecto, presupuesto y plantilla inicial.");
            return;
        case InsightAction::FocusManager:
            SetFocus(state.managerEdit);
            setStatus(state, "Insight: escribe el nombre del manager para completar la partida.");
            return;
        case InsightAction::StartCareer:
            startNewCareer(state);
            return;
        case InsightAction::OpenLeague:
            openPageWithFilter(state, GuiPage::League, "Grupo actual");
            setStatus(state, "Insight: se abrio la vista de liga para revisar la carrera competitiva.");
            return;
        case InsightAction::OpenSquad:
            openPageWithFilter(state, GuiPage::Squad, "Todos");
            setStatus(state, "Insight: se abrio la plantilla para revisar forma, moral y salud.");
            return;
        case InsightAction::OpenBoard:
            openPageWithFilter(state, GuiPage::Board, "Resumen");
            setStatus(state, "Insight: se abrio la directiva para revisar confianza y objetivos.");
            return;
        case InsightAction::RefreshMarketPulse:
            runFollowShortlistAction(state);
            return;
        case InsightAction::RunScouting:
            runScoutingAction(state);
            return;
        case InsightAction::OpenFinanceSummary:
            openPageWithFilter(state, GuiPage::Finances, "Resumen");
            setStatus(state, "Insight: se abrio el resumen financiero del club.");
            return;
        case InsightAction::OpenFinanceSalaries:
            openPageWithFilter(state, GuiPage::Finances, "Salarios");
            setStatus(state, "Insight: se abrio la vista salarial para revisar contratos y masa salarial.");
            return;
        case InsightAction::OpenFinanceInfrastructure:
            openPageWithFilter(state, GuiPage::Finances, "Infraestructura");
            setStatus(state, "Insight: se abrio infraestructura para revisar capacidad de crecimiento.");
            return;
        case InsightAction::OpenBoardSummary:
            openPageWithFilter(state, GuiPage::Board, "Resumen");
            setStatus(state, "Insight: se abrio el resumen de la directiva.");
            return;
        case InsightAction::OpenBoardObjectives:
            openPageWithFilter(state, GuiPage::Board, "Objetivos");
            setStatus(state, "Insight: se abrio el seguimiento de objetivos mensuales.");
            return;
        case InsightAction::OpenBoardHistory:
            openPageWithFilter(state, GuiPage::Board, "Historial");
            setStatus(state, "Insight: se abrio el historial para evaluar la presion del cargo.");
            return;
        case InsightAction::None:
            return;
    }
}

std::vector<HWND> activeFrontMenuButtons(const AppState& state) {
    if (state.currentPage == GuiPage::MainMenu) {
        return {
            state.menuContinueButton,
            state.menuPlayButton,
            state.menuSettingsButton,
            state.menuLoadButton,
            state.menuDeleteSaveButton,
            state.menuCreditsButton,
            state.menuExitButton
        };
    }
    if (state.currentPage == GuiPage::Settings) {
        return {
            state.menuVolumeButton,
            state.menuDifficultyButton,
            state.menuSpeedButton,
            state.menuSimulationButton,
            state.menuLanguageButton,
            state.menuTextSpeedButton,
            state.menuVisualButton,
            state.menuMusicModeButton,
            state.menuAudioFadeButton,
            state.menuApplySettingsButton,
            state.menuResetSettingsButton,
            state.menuBackButton
        };
    }
    if (state.currentPage == GuiPage::Credits) {
        return {state.menuBackButton};
    }
    if (state.currentPage == GuiPage::Saves) {
        return {
            state.menuLoadButton,
            state.menuDeleteSaveButton,
            state.menuBackButton
        };
    }
    return {};
}

void focusFrontMenuButton(const std::vector<HWND>& buttons, int index) {
    if (buttons.empty()) return;
    const int count = static_cast<int>(buttons.size());
    index = (index % count + count) % count;
    for (int attempts = 0; attempts < count; ++attempts) {
        HWND candidate = buttons[static_cast<size_t>((index + attempts) % count)];
        if (candidate && IsWindowVisible(candidate) && IsWindowEnabled(candidate)) {
            SetFocus(candidate);
            return;
        }
    }
}

bool moveFrontMenuFocus(const AppState& state, int delta) {
    const std::vector<HWND> buttons = activeFrontMenuButtons(state);
    if (buttons.empty()) return false;

    HWND focused = GetFocus();
    int current = 0;
    for (size_t i = 0; i < buttons.size(); ++i) {
        if (buttons[i] == focused) {
            current = static_cast<int>(i);
            break;
        }
    }
    focusFrontMenuButton(buttons, current + delta);
    return true;
}

bool clickFrontMenuButton(HWND button) {
    if (!button || !IsWindowVisible(button) || !IsWindowEnabled(button)) return false;
    SendMessageW(button, BM_CLICK, 0, 0);
    return true;
}

bool commandAllowedDuringAction(int id, int notifyCode) {
    if (isPageButtonId(id)) return true;
    if (id == IDC_FILTER_COMBO && notifyCode == CBN_SELCHANGE) return true;
    if (id == IDC_NEWS_LIST && notifyCode == LBN_SELCHANGE) return true;
    return false;
}

bool handleFrontMenuKey(AppState& state, WPARAM key) {
    if (!isFrontMenuPage(state.currentPage)) return false;

    switch (key) {
        case VK_LEFT:
        case VK_UP:
            return moveFrontMenuFocus(state, -1);
        case VK_RIGHT:
        case VK_DOWN:
        case VK_TAB:
            return moveFrontMenuFocus(state, 1);
        case VK_HOME:
            focusFrontMenuButton(activeFrontMenuButtons(state), 0);
            return true;
        case VK_END: {
            const std::vector<HWND> buttons = activeFrontMenuButtons(state);
            focusFrontMenuButton(buttons, static_cast<int>(buttons.size()) - 1);
            return true;
        }
        case VK_RETURN:
        case VK_SPACE:
            return clickFrontMenuButton(GetFocus());
        case 'T':
            return state.currentPage == GuiPage::MainMenu && clickFrontMenuButton(state.menuContinueButton);
        case 'J':
            return state.currentPage == GuiPage::MainMenu && clickFrontMenuButton(state.menuPlayButton);
        case 'C':
            return state.currentPage == GuiPage::MainMenu && clickFrontMenuButton(state.menuSettingsButton);
        case 'L':
            if (state.currentPage == GuiPage::MainMenu || state.currentPage == GuiPage::Saves) {
                return clickFrontMenuButton(state.menuLoadButton);
            }
            return false;
        case 'R':
            return state.currentPage == GuiPage::MainMenu && clickFrontMenuButton(state.menuCreditsButton);
        case 'X':
            return state.currentPage == GuiPage::MainMenu && clickFrontMenuButton(state.menuExitButton);
        case 'D':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuDifficultyButton);
        case 'S':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuSpeedButton);
        case 'M':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuSimulationButton);
        case 'V':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuVolumeButton);
        case 'I':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuLanguageButton);
        case 'P':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuVisualButton);
        case 'U':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuMusicModeButton);
        case 'F':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuAudioFadeButton);
        case 'A':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuApplySettingsButton);
        case 'O':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuResetSettingsButton);
        case 'B':
            if (state.currentPage == GuiPage::MainMenu || state.currentPage == GuiPage::Saves) {
                return clickFrontMenuButton(state.menuDeleteSaveButton);
            }
            return false;
        case VK_ESCAPE:
            if (state.currentPage == GuiPage::Settings || state.currentPage == GuiPage::Credits || state.currentPage == GuiPage::Saves) {
                return clickFrontMenuButton(state.menuBackButton);
            }
            return false;
        case '1':
            if (state.currentPage == GuiPage::MainMenu) return clickFrontMenuButton(state.menuContinueButton);
            return clickFrontMenuButton(state.menuVolumeButton);
        case '2':
            if (state.currentPage == GuiPage::MainMenu) return clickFrontMenuButton(state.menuPlayButton);
            return clickFrontMenuButton(state.menuDifficultyButton);
        case '3':
            if (state.currentPage == GuiPage::MainMenu) return clickFrontMenuButton(state.menuLoadButton);
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuSpeedButton);
        case '4':
            if (state.currentPage == GuiPage::MainMenu) return clickFrontMenuButton(state.menuSettingsButton);
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuSimulationButton);
        case '5':
            if (state.currentPage == GuiPage::MainMenu) return clickFrontMenuButton(state.menuCreditsButton);
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuLanguageButton);
        case '6':
            if (state.currentPage == GuiPage::MainMenu) return clickFrontMenuButton(state.menuExitButton);
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuTextSpeedButton);
        case '7':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuVisualButton);
        case '8':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuMusicModeButton);
        case '9':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuAudioFadeButton);
        case '0':
            return state.currentPage == GuiPage::Settings && clickFrontMenuButton(state.menuBackButton);
        default:
            return false;
    }
}

}  // namespace

bool isCtrlKeyDown() {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

bool handleCareerShortcut(AppState& state, const MSG& msg) {
    if (msg.message != WM_KEYDOWN || state.actionInProgress || isFrontMenuPage(state.currentPage)) return false;
    const WPARAM key = msg.wParam;
    const bool repeated = (msg.lParam & (1u << 30)) != 0;
    const bool hasCareer = state.career.myTeam != nullptr;

    if (key == VK_F5) {
        if (repeated) return true;
        if (hasCareer) {
            simulateWeek(state);
        } else {
            setStatus(state, "Inicia o carga una carrera para simular.");
        }
        return true;
    }

    if (!isCtrlKeyDown()) return false;

    if (key == 'S') {
        if (repeated) return true;
        if (hasCareer) {
            saveCareer(state);
        } else {
            setStatus(state, "Inicia o carga una carrera para guardar.");
        }
        return true;
    }

    if (key == 'F') {
        if (hasCareer) {
            setCurrentPage(state, GuiPage::Transfers);
            setStatus(state, "Radar de mercado abierto.");
        } else {
            setCurrentPage(state, GuiPage::Dashboard);
            setStatus(state, "Completa la carrera para habilitar el radar de mercado.");
        }
        return true;
    }

    if (key == VK_RETURN) {
        if (repeated) return true;
        if (hasCareer) {
            simulateWeek(state);
        } else {
            setStatus(state, "Inicia o carga una carrera para simular.");
        }
        return true;
    }

    const std::vector<GuiPage> shortcutPages = {
        GuiPage::Dashboard,
        GuiPage::Squad,
        GuiPage::Tactics,
        GuiPage::Calendar,
        GuiPage::League,
        GuiPage::Transfers,
        GuiPage::Finances,
        GuiPage::Youth,
        GuiPage::Board,
        GuiPage::News
    };
    if (key >= '1' && key <= '9') {
        setCurrentPage(state, shortcutPages[static_cast<size_t>(key - '1')]);
        return true;
    }
    if (key == '0') {
        setCurrentPage(state, GuiPage::News);
        return true;
    }

    return false;
}

void cycleDisplayMode(AppState& state) {
    switch (currentDisplayMode(state)) {
        case DisplayMode::RestoredWindow:
            maximizeWindow(state);
            return;
        case DisplayMode::MaximizedWindow:
            enterBorderlessFullscreen(state);
            return;
        case DisplayMode::BorderlessFullscreen:
            restoreWindowedPlacement(state);
            return;
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* incomingState = reinterpret_cast<AppState*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(incomingState));
            incomingState->window = hwnd;
            return TRUE;
        }
        case WM_CREATE:
            if (state) {
                state->dpi = queryWindowDpi(hwnd);
                initializeInterface(*state);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (state) {
                layoutWindow(*state);
                autosizeCurrentLists(*state);
                syncDisplayModeButton(*state);
            }
            return 0;
        case WM_DPICHANGED:
            if (state) {
                state->dpi = HIWORD(wParam) ? HIWORD(wParam) : queryWindowDpi(hwnd);
                rebuildFonts(*state);
                applyInterfaceFonts(*state);
                rebuildTeamLogoImageList(*state);
                auto* suggested = reinterpret_cast<const RECT*>(lParam);
                if (suggested) {
                    SetWindowPos(hwnd,
                                 nullptr,
                                 suggested->left,
                                 suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                syncDisplayModeButton(*state);
                refreshCurrentPage(*state);
            }
            return 0;
        case WM_SETCURSOR:
            if (state && LOWORD(lParam) == HTCLIENT) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(hwnd, &cursor);
                const InsightHotspot* hotspot = hitInsightHotspot(*state, cursor);
                if (hotspot && (!state->actionInProgress || insightActionAllowedDuringAction(hotspot->action))) {
                    SetCursor(LoadCursor(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        case WM_LBUTTONUP:
            if (state) {
                POINT point{static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
                            static_cast<LONG>(static_cast<short>(HIWORD(lParam)))};
                if (const InsightHotspot* hotspot = hitInsightHotspot(*state, point)) {
                    executeInsightAction(*state, hotspot->action);
                    return 0;
                }
            }
            break;
        case WM_NOTIFY:
            if (!state) break;
            if (reinterpret_cast<LPNMHDR>(lParam)->code == NM_CUSTOMDRAW) {
                return handleListCustomDraw(*state, reinterpret_cast<LPNMHDR>(lParam));
            }
            if (reinterpret_cast<LPNMHDR>(lParam)->code == LVN_COLUMNCLICK) {
                handleListColumnClick(*state, *reinterpret_cast<NMLISTVIEW*>(lParam));
                return 0;
            }
            if (reinterpret_cast<LPNMHDR>(lParam)->idFrom == IDC_SQUAD_LIST ||
                reinterpret_cast<LPNMHDR>(lParam)->idFrom == IDC_TABLE_LIST ||
                reinterpret_cast<LPNMHDR>(lParam)->idFrom == IDC_TRANSFER_LIST) {
                if (reinterpret_cast<LPNMHDR>(lParam)->code == LVN_ITEMCHANGED ||
                    reinterpret_cast<LPNMHDR>(lParam)->code == NM_CLICK) {
                    handleListSelectionChange(*state, static_cast<int>(reinterpret_cast<LPNMHDR>(lParam)->idFrom));
                    return 0;
                }
                if (reinterpret_cast<LPNMHDR>(lParam)->code == NM_DBLCLK ||
                    reinterpret_cast<LPNMHDR>(lParam)->code == NM_RETURN) {
                    activateListAction(*state, static_cast<int>(reinterpret_cast<LPNMHDR>(lParam)->idFrom));
                    return 0;
                }
            }
            break;
        case WM_DRAWITEM:
            if (state && wParam != 0) {
                drawThemedButton(*state, reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
                return TRUE;
            }
            break;
        case WM_VSCROLL:
            if (state) {
                SCROLLINFO info{};
                info.cbSize = sizeof(info);
                info.fMask = SIF_ALL;
                GetScrollInfo(hwnd, SB_VERT, &info);

                int target = state->pageScrollY;
                switch (LOWORD(wParam)) {
                    case SB_TOP:
                        target = 0;
                        break;
                    case SB_BOTTOM:
                        target = state->maxPageScrollY;
                        break;
                    case SB_LINEUP:
                        target -= pageScrollStep(*state);
                        break;
                    case SB_LINEDOWN:
                        target += pageScrollStep(*state);
                        break;
                    case SB_PAGEUP:
                        target -= pageScrollJump(*state);
                        break;
                    case SB_PAGEDOWN:
                        target += pageScrollJump(*state);
                        break;
                    case SB_THUMBPOSITION:
                    case SB_THUMBTRACK:
                        target = info.nTrackPos;
                        break;
                    default:
                        break;
                }
                if (scrollViewportTo(*state, target)) return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (state && state->maxPageScrollY > 0) {
                const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0 && scrollViewportBy(*state, -MulDiv(pageScrollStep(*state) * 3, delta, WHEEL_DELTA))) {
                    return 0;
                }
            }
            break;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSTATIC:
            if (state) {
                HDC hdc = reinterpret_cast<HDC>(wParam);
                HWND control = reinterpret_cast<HWND>(lParam);
                SetBkMode(hdc, TRANSPARENT);
                if (message == WM_CTLCOLOREDIT || control == state->managerEdit || control == state->summaryEdit || control == state->detailEdit) {
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, kThemeInput);
                    SetTextColor(hdc, kThemeText);
                    return reinterpret_cast<LRESULT>(state->inputBrush ? state->inputBrush : state->panelBrush);
                }
                if (control == state->divisionCombo || control == state->teamCombo || control == state->filterCombo) {
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, kThemeInput);
                    SetTextColor(hdc, kThemeText);
                    return reinterpret_cast<LRESULT>(state->inputBrush ? state->inputBrush : state->panelBrush);
                }
                if (message == WM_CTLCOLORLISTBOX || control == state->newsList) {
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, kThemeInput);
                    SetTextColor(hdc, kThemeText);
                    return reinterpret_cast<LRESULT>(state->inputBrush ? state->inputBrush : state->panelBrush);
                }
                const bool headerStatic = control == state->divisionLabel || control == state->teamLabel ||
                                          control == state->managerLabel || control == state->managerHelpLabel;
                const bool panelStatic = control == state->summaryLabel || control == state->tableLabel ||
                                         control == state->squadLabel || control == state->transferLabel ||
                                         control == state->detailLabel || control == state->newsLabel;
                const bool shellStatic = control == state->pageTitleLabel || control == state->breadcrumbLabel ||
                                         control == state->infoLabel || control == state->filterLabel;
                const bool statusStatic = control == state->statusLabel;
                if (panelStatic) {
                    SetTextColor(hdc, kThemeAccent);
                } else if (control == state->pageTitleLabel) {
                    SetTextColor(hdc, RGB(242, 247, 249));
                } else if (control == state->breadcrumbLabel) {
                    SetTextColor(hdc, kThemeMuted);
                } else if (statusStatic) {
                    SetTextColor(hdc, RGB(188, 228, 216));
                } else if (control == state->managerHelpLabel) {
                    if (state->career.myTeam || state->gameSetup.ready) {
                        SetTextColor(hdc, kThemeAccentGreen);
                    } else if (!state->gameSetup.managerError.empty()) {
                        SetTextColor(hdc, kThemeDanger);
                    } else {
                        SetTextColor(hdc, kThemeWarning);
                    }
                } else if (control == state->infoLabel) {
                    if (!state->career.myTeam) {
                        if (state->gameSetup.ready) SetTextColor(hdc, kThemeAccentGreen);
                        else if (!state->gameSetup.managerError.empty()) SetTextColor(hdc, kThemeDanger);
                        else SetTextColor(hdc, kThemeWarning);
                    } else {
                        SetTextColor(hdc, kThemeText);
                    }
                } else {
                    SetTextColor(hdc, kThemeMuted);
                }
                if (headerStatic) {
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, kThemeTopBarPanel);
                    return reinterpret_cast<LRESULT>(state->topBarBrush ? state->topBarBrush : state->headerBrush);
                }
                if (panelStatic) {
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, kThemePanel);
                    return reinterpret_cast<LRESULT>(state->panelBrush ? state->panelBrush : state->backgroundBrush);
                }
                if (shellStatic) {
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, kThemeShell);
                    return reinterpret_cast<LRESULT>(state->shellBrush ? state->shellBrush : state->backgroundBrush);
                }
                if (statusStatic) {
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, RGB(11, 23, 31));
                    return reinterpret_cast<LRESULT>(state->shellBrush ? state->shellBrush : state->backgroundBrush);
                }
                SetBkMode(hdc, OPAQUE);
                SetBkColor(hdc, kThemeBg);
                return reinterpret_cast<LRESULT>(state->backgroundBrush ? state->backgroundBrush : GetStockObject(BLACK_BRUSH));
            }
            break;
        case WM_COMMAND:
            if (!state) break;
            if (state->actionInProgress &&
                !commandAllowedDuringAction(static_cast<int>(LOWORD(wParam)), static_cast<int>(HIWORD(wParam)))) {
                return 0;
            }
            switch (LOWORD(wParam)) {
                case IDC_DIVISION_COMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE && !state->suppressComboEvents) {
                        int selected = comboIndex(state->divisionCombo);
                        std::string divisionId;
                        if (selected >= 0 && selected < static_cast<int>(state->career.divisions.size())) {
                            divisionId = state->career.divisions[static_cast<size_t>(selected)].id;
                        }
                        set_division(*state, divisionId);
                    }
                    return 0;
                case IDC_TEAM_COMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE && !state->suppressComboEvents) {
                        set_club(*state, comboText(state->teamCombo));
                    }
                    return 0;
                case IDC_MANAGER_EDIT:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        set_manager(*state, getWindowTextUtf8(state->managerEdit));
                    }
                    return 0;
                case IDC_FILTER_COMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE && !state->suppressFilterEvents) {
                        handleFilterChange(*state);
                    }
                    return 0;
                case IDC_NEWS_LIST:
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        handleFeedSelectionChange(*state, IDC_NEWS_LIST);
                    }
                    return 0;
                case IDC_NEW_CAREER_BUTTON:
                    startNewCareer(*state);
                    return 0;
                case IDC_LOAD_BUTTON:
                    loadCareer(*state);
                    return 0;
                case IDC_SAVE_BUTTON:
                    saveCareer(*state);
                    return 0;
                case IDC_SIMULATE_BUTTON:
                    simulateWeek(*state);
                    return 0;
                case IDC_VALIDATE_BUTTON:
                    validateSystem(*state);
                    return 0;
                case IDC_DISPLAY_MODE_BUTTON:
                    cycleDisplayMode(*state);
                    return 0;
                case IDC_FRONT_MENU_BUTTON:
                    openFrontendMenu(*state);
                    return 0;
                case IDC_MENU_CONTINUE_BUTTON:
                    continueCareer(*state);
                    return 0;
                case IDC_MENU_PLAY_BUTTON:
                    setCurrentPage(*state, GuiPage::Dashboard); // evitar que MainMenu se superponga al iniciar
                    setStatus(*state, "Flujo principal abierto. Ya puedes crear o cargar una carrera.");
                    return 0;
                case IDC_MENU_SETTINGS_BUTTON:
                    openSettingsMenu(*state);
                    return 0;
                case IDC_MENU_LOAD_BUTTON:
                    if (state->currentPage == GuiPage::Saves) loadCareer(*state);
                    else openSavesPage(*state);
                    return 0;
                case IDC_MENU_DELETE_SAVE_BUTTON:
                    if (state->currentPage == GuiPage::Saves) deleteCareerSave(*state);
                    else openSavesPage(*state);
                    return 0;
                case IDC_MENU_CREDITS_BUTTON:
                    openCreditsPage(*state);
                    return 0;
                case IDC_MENU_EXIT_BUTTON:
                    DestroyWindow(state->window);
                    return 0;
                case IDC_MENU_BACK_BUTTON:
                    openFrontendMenu(*state);
                    return 0;
                case IDC_MENU_VOLUME_BUTTON:
                    cycleFrontendVolume(*state);
                    return 0;
                case IDC_MENU_DIFFICULTY_BUTTON:
                    cycleFrontendDifficulty(*state);
                    return 0;
                case IDC_MENU_SPEED_BUTTON:
                    cycleFrontendSimulationSpeed(*state);
                    return 0;
                case IDC_MENU_SIMULATION_BUTTON:
                    cycleFrontendSimulationMode(*state);
                    return 0;
                case IDC_MENU_LANGUAGE_BUTTON:
                    cycleFrontendLanguage(*state);
                    return 0;
                case IDC_MENU_TEXT_SPEED_BUTTON:
                    cycleFrontendTextSpeed(*state);
                    return 0;
                case IDC_MENU_VISUAL_BUTTON:
                    cycleFrontendVisualProfile(*state);
                    return 0;
                case IDC_MENU_MUSICMODE_BUTTON:
                    cycleFrontendMenuMusicMode(*state);
                    return 0;
                case IDC_MENU_AUDIOFADE_BUTTON:
                    toggleFrontendAudioFade(*state);
                    return 0;
                case IDC_MENU_APPLY_SETTINGS_BUTTON:
                    applyFrontendSettings(*state);
                    return 0;
                case IDC_MENU_RESET_SETTINGS_BUTTON:
                    restoreFrontendSettings(*state);
                    return 0;
                case IDC_EMPTY_NEW_BUTTON:
                    startNewCareer(*state);
                    return 0;
                case IDC_EMPTY_LOAD_BUTTON:
                    loadCareer(*state);
                    return 0;
                case IDC_EMPTY_VALIDATE_BUTTON:
                    validateSystem(*state);
                    return 0;
                case IDC_PAGE_DASHBOARD_BUTTON:
                case IDC_PAGE_SQUAD_BUTTON:
                case IDC_PAGE_TACTICS_BUTTON:
                case IDC_PAGE_CALENDAR_BUTTON:
                case IDC_PAGE_LEAGUE_BUTTON:
                case IDC_PAGE_TRANSFERS_BUTTON:
                case IDC_PAGE_FINANCES_BUTTON:
                case IDC_PAGE_YOUTH_BUTTON:
                case IDC_PAGE_BOARD_BUTTON:
                case IDC_PAGE_NEWS_BUTTON:
                    setCurrentPage(*state, pageForControlId(static_cast<int>(LOWORD(wParam))));
                    return 0;
                case IDC_SCOUT_BUTTON:
                    runScoutingAction(*state);
                    return 0;
                case IDC_SHORTLIST_BUTTON:
                    runShortlistAction(*state);
                    return 0;
                case IDC_FOLLOW_SHORTLIST_BUTTON:
                    runFollowShortlistAction(*state);
                    return 0;
                case IDC_BUY_BUTTON:
                    runBuyAction(*state);
                    return 0;
                case IDC_PRECONTRACT_BUTTON:
                    runPreContractAction(*state);
                    return 0;
                case IDC_LOAN_BUTTON:
                    runLoanAction(*state);
                    return 0;
                case IDC_RENEW_BUTTON:
                    runRenewAction(*state);
                    return 0;
                case IDC_SELL_BUTTON:
                    runSellAction(*state);
                    return 0;
                case IDC_PLAN_BUTTON:
                    runPlanAction(*state);
                    return 0;
                case IDC_INSTRUCTION_BUTTON:
                    runInstructionAction(*state);
                    return 0;
                case IDC_YOUTH_UPGRADE_BUTTON:
                    runUpgradeAction(*state, ClubUpgrade::Youth, "Cantera");
                    return 0;
                case IDC_TRAINING_UPGRADE_BUTTON:
                    runUpgradeAction(*state, ClubUpgrade::Training, "Entrenamiento");
                    return 0;
                case IDC_SCOUTING_UPGRADE_BUTTON:
                    runUpgradeAction(*state, ClubUpgrade::Scouting, "Scouting");
                    return 0;
                case IDC_STADIUM_UPGRADE_BUTTON:
                    runUpgradeAction(*state, ClubUpgrade::Stadium, "Estadio");
                    return 0;
                default:
                    break;
            }
            break;
        case kGuiPageTransitionMessage:
            if (state) {
                state->pageChangeQueued = false;
                setCurrentPage(*state, static_cast<GuiPage>(static_cast<int>(wParam)));
                return 0;
            }
            break;
        case kGuiSimulationProgressMessage:
            if (state) {
                handleSimulationProgress(*state, lParam);
                return 0;
            }
            break;
        case kGuiSimulationCompleteMessage:
            if (state) {
                completeSimulationWeek(*state, lParam);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (state) {
                if (isFrontMenuPage(state->currentPage) && wParam == VK_ESCAPE) {
                    if (state->currentPage == GuiPage::Settings || state->currentPage == GuiPage::Credits || state->currentPage == GuiPage::Saves) {
                        openFrontendMenu(*state);
                        return 0;
                    }
                }
                if (state->maxPageScrollY > 0) {
                    switch (wParam) {
                        case VK_PRIOR:
                            if (scrollViewportBy(*state, -pageScrollJump(*state))) return 0;
                            break;
                        case VK_NEXT:
                            if (scrollViewportBy(*state, pageScrollJump(*state))) return 0;
                            break;
                        case VK_HOME:
                            if (scrollViewportTo(*state, 0)) return 0;
                            break;
                        case VK_END:
                            if (scrollViewportTo(*state, state->maxPageScrollY)) return 0;
                            break;
                        default:
                            break;
                    }
                }
            }
            break;
        case WM_PAINT:
            if (state) {
                PAINTSTRUCT paint{};
                HDC hdc = BeginPaint(hwnd, &paint);
                paintWindowChrome(*state, hdc);
                EndPaint(hwnd, &paint);
                return 0;
            }
            break;
        case WM_DESTROY:
            if (state) {
                shutdownMenuMusic(*state);
                if (state->teamLogoImageList) ImageList_Destroy(state->teamLogoImageList);
                if (state->font) DeleteObject(state->font);
                if (state->titleFont) DeleteObject(state->titleFont);
                if (state->heroFont) DeleteObject(state->heroFont);
                if (state->sectionFont) DeleteObject(state->sectionFont);
                if (state->monoFont) DeleteObject(state->monoFont);
                if (state->backgroundBrush) DeleteObject(state->backgroundBrush);
                if (state->panelBrush) DeleteObject(state->panelBrush);
                if (state->headerBrush) DeleteObject(state->headerBrush);
                if (state->topBarBrush) DeleteObject(state->topBarBrush);
                if (state->shellBrush) DeleteObject(state->shellBrush);
                if (state->inputBrush) DeleteObject(state->inputBrush);
                state->font = nullptr;
                state->titleFont = nullptr;
                state->heroFont = nullptr;
                state->sectionFont = nullptr;
                state->monoFont = nullptr;
                state->backgroundBrush = nullptr;
                state->panelBrush = nullptr;
                state->headerBrush = nullptr;
                state->topBarBrush = nullptr;
                state->shellBrush = nullptr;
                state->inputBrush = nullptr;
                state->teamLogoImageList = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace gui_win32

int runGuiApp(GameSettings& settings) {
    gui_win32::enableHighDpiSupport();

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);

    gui_win32::AppState state;
    state.instance = GetModuleHandleW(nullptr);
    state.settings = settings;
    state.savedSettings = settings;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = gui_win32::windowProc;
    windowClass.hInstance = state.instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = L"FootballManagerGuiWindow";
    RegisterClassExW(&windowClass);

    RECT workArea = gui_win32::primaryMonitorWorkArea();
    HWND window = CreateWindowExW(0,
                                  windowClass.lpszClassName,
                                  L"Chilean Footballito",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE | WS_VSCROLL | WS_CLIPCHILDREN,
                                  workArea.left,
                                  workArea.top,
                                  workArea.right - workArea.left,
                                  workArea.bottom - workArea.top,
                                  nullptr,
                                  nullptr,
                                  state.instance,
                                  &state);
    if (!window) {
        return 1;
    }

    ShowWindow(window, SW_MAXIMIZE);
    UpdateWindow(window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F11 && (msg.lParam & (1u << 30)) == 0) {
            gui_win32::cycleDisplayMode(state);
            continue;
        }
        if (gui_win32::handleCareerShortcut(state, msg)) {
            continue;
        }
        if (msg.message == WM_KEYDOWN && gui_win32::handleFrontMenuKey(state, msg.wParam)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    settings = state.settingsDirty ? state.savedSettings : state.settings;
    return static_cast<int>(msg.wParam);
}

#else

int runGuiApp(GameSettings&) {
    return 1;
}

#endif
