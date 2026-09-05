#pragma once

#ifdef _WIN32

#include "career/app_services.h"
#include "engine/game_settings.h"
#include "career/career_reports.h"
#include "engine/models.h"

#include <string>
#include <map>
#include <utility>
#include <vector>

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

#include <windows.h>
#include <commctrl.h>

namespace gui_win32 {

enum ControlId {
    IDC_DIVISION_COMBO = 1001,
    IDC_TEAM_COMBO,
    IDC_MANAGER_EDIT,
    IDC_NEW_CAREER_BUTTON,
    IDC_LOAD_BUTTON,
    IDC_SAVE_BUTTON,
    IDC_SIMULATE_BUTTON,
    IDC_VALIDATE_BUTTON,
    IDC_DISPLAY_MODE_BUTTON,
    IDC_FRONT_MENU_BUTTON,
    IDC_FILTER_COMBO,
    IDC_SUMMARY_EDIT,
    IDC_NEWS_LIST,
    IDC_TABLE_LIST,
    IDC_SQUAD_LIST,
    IDC_TRANSFER_LIST,
    IDC_DETAIL_EDIT,
    IDC_SCOUT_BUTTON,
    IDC_SHORTLIST_BUTTON,
    IDC_FOLLOW_SHORTLIST_BUTTON,
    IDC_BUY_BUTTON,
    IDC_PRECONTRACT_BUTTON,
    IDC_LOAN_BUTTON,
    IDC_RENEW_BUTTON,
    IDC_SELL_BUTTON,
    IDC_PLAN_BUTTON,
    IDC_INSTRUCTION_BUTTON,
    IDC_YOUTH_UPGRADE_BUTTON,
    IDC_TRAINING_UPGRADE_BUTTON,
    IDC_SCOUTING_UPGRADE_BUTTON,
    IDC_STADIUM_UPGRADE_BUTTON,
    IDC_PAGE_DASHBOARD_BUTTON,
    IDC_PAGE_SQUAD_BUTTON,
    IDC_PAGE_TACTICS_BUTTON,
    IDC_PAGE_CALENDAR_BUTTON,
    IDC_PAGE_LEAGUE_BUTTON,
    IDC_PAGE_TRANSFERS_BUTTON,
    IDC_PAGE_FINANCES_BUTTON,
    IDC_PAGE_YOUTH_BUTTON,
    IDC_PAGE_BOARD_BUTTON,
    IDC_PAGE_NEWS_BUTTON,
    IDC_MENU_CONTINUE_BUTTON,
    IDC_MENU_PLAY_BUTTON,
    IDC_MENU_SETTINGS_BUTTON,
    IDC_MENU_LOAD_BUTTON,
    IDC_MENU_DELETE_SAVE_BUTTON,
    IDC_MENU_CREDITS_BUTTON,
    IDC_MENU_EXIT_BUTTON,
    IDC_MENU_BACK_BUTTON,
    IDC_MENU_VOLUME_BUTTON,
    IDC_MENU_DIFFICULTY_BUTTON,
    IDC_MENU_SPEED_BUTTON,
    IDC_MENU_SIMULATION_BUTTON,
    IDC_MENU_LANGUAGE_BUTTON,
    IDC_MENU_TEXT_SPEED_BUTTON,
    IDC_MENU_VISUAL_BUTTON,
    IDC_MENU_MUSICMODE_BUTTON,
    IDC_MENU_AUDIOFADE_BUTTON,
    IDC_MENU_APPLY_SETTINGS_BUTTON,
    IDC_MENU_RESET_SETTINGS_BUTTON,
    IDC_EMPTY_NEW_BUTTON,
    IDC_EMPTY_LOAD_BUTTON,
    IDC_EMPTY_VALIDATE_BUTTON
};

enum class GuiPage {
    MainMenu,
    Settings,
    Credits,
    Saves,
    Dashboard,
    Squad,
    Tactics,
    Calendar,
    League,
    Transfers,
    Finances,
    Youth,
    Board,
    News
};

enum class DisplayMode {
    RestoredWindow,
    MaximizedWindow,
    BorderlessFullscreen
};

enum class InsightAction {
    None,
    FocusDivision,
    FocusClub,
    FocusManager,
    StartCareer,
    OpenLeague,
    OpenSquad,
    OpenBoard,
    RefreshMarketPulse,
    RunScouting,
    OpenFinanceSummary,
    OpenFinanceSalaries,
    OpenFinanceInfrastructure,
    OpenBoardSummary,
    OpenBoardObjectives,
    OpenBoardHistory
};

struct SortState {
    int column = 0;
    bool ascending = true;
};

struct DashboardMetric {
    std::string label;
    std::string value;
    COLORREF accent = RGB(90, 120, 140);
};

struct TextPanelModel {
    std::string title;
    std::string content;
};

struct ListPanelModel {
    std::string title;
    std::vector<std::pair<std::wstring, int> > columns;
    std::vector<std::vector<std::string> > rows;
};

struct FeedPanelModel {
    std::string title;
    std::vector<std::string> lines;
};

struct GameSetupState {
    std::string division;
    std::string club;
    std::string manager;
    bool ready = false;
    int currentStep = 1;
    std::string managerError;
    std::string inlineMessage;
};

struct GuiPageModel {
    std::string title;
    std::string breadcrumb;
    std::string infoLine;
    TextPanelModel summary;
    ListPanelModel primary;
    ListPanelModel secondary;
    ListPanelModel footer;
    TextPanelModel detail;
    FeedPanelModel feed;
    std::vector<DashboardMetric> metrics;
};

struct InsightHotspot {
    RECT rect{};
    InsightAction action = InsightAction::None;
    std::wstring hint;
};

struct PanelBounds {
    RECT outer{};
    RECT title{};
    RECT body{};
    bool visible = false;
};

struct LayoutSnapshot {
    RECT client{};
    RECT topBar{};
    RECT sideMenu{};
    RECT sideMenuTitle{};
    RECT contentShell{};
    RECT shellInner{};
    RECT pageHeader{};
    RECT headerTextArea{};
    RECT headerFilterArea{};
    RECT breadcrumb{};
    RECT pageTitle{};
    RECT infoLine{};
    RECT filterLabel{};
    RECT filterField{};
    RECT actionStrip{};
    RECT scrollViewport{};
    RECT spotlightBand{};
    RECT mainArea{};
    RECT centerColumn{};
    RECT rightColumn{};
    RECT statusBar{};
    RECT contextCard{};
    PanelBounds summaryPanel;
    PanelBounds primaryPanel;
    PanelBounds secondaryPanel;
    PanelBounds footerPanel;
    PanelBounds detailPanel;
    PanelBounds newsPanel;
    bool frontMenuPage = false;
    bool dashboardPage = false;
    bool dashboardEmptyState = false;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    UINT dpi = 96;
    bool isBorderlessFullscreen = false;
    WINDOWPLACEMENT restorePlacement{};
    DWORD restoreStyle = 0;
    DWORD restoreExStyle = 0;
    HFONT font = nullptr;
    HFONT titleFont = nullptr;
    HFONT heroFont = nullptr;
    HFONT sectionFont = nullptr;
    HFONT monoFont = nullptr;
    HBRUSH backgroundBrush = nullptr;
    HBRUSH panelBrush = nullptr;
    HBRUSH headerBrush = nullptr;
    HBRUSH topBarBrush = nullptr;
    HBRUSH shellBrush = nullptr;
    HBRUSH inputBrush = nullptr;
    HBITMAP mainMenuBackground = nullptr;

    Career career;
    GameSettings settings;
    GameSettings savedSettings;
    bool settingsDirty = false;
    bool suppressComboEvents = false;
    bool suppressFilterEvents = false;
    bool pageRefreshInProgress = false;
    bool pageChangeQueued = false;
    bool menuMusicOpened = false;
    bool actionInProgress = false;
    bool simulationProgressActive = false;
    bool menuMusicPlaying = false;
    bool menuMusicMissingReported = false;
    int simulationProgressPercent = 0;
    int menuMusicAppliedVolume = -1;
    int pageScrollY = 0;
    int maxPageScrollY = 0;
    int pageContentHeight = 0;
    std::wstring menuMusicPath;
    GuiPage queuedPage = GuiPage::MainMenu;
    GuiPage currentPage = GuiPage::Dashboard;
    std::string currentFilter = "Todo";
    GameSetupState gameSetup;
    SortState squadSort;
    std::string selectedPlayerName;
    std::string selectedTransferPlayer;
    std::string selectedTransferClub;
    std::string selectedSavePath;
    std::string simulationProgressPhase;
    std::string simulationProgressDetail;
    std::vector<std::string> simulationProgressEvents;
    std::vector<std::string> saveSlotPaths;
    GuiPageModel currentModel;
    std::vector<InsightHotspot> insightHotspots;
    std::map<std::string, std::vector<int> > columnWidthMemory;
    std::map<std::string, GuiPageModel> modelCache;
    std::map<std::string, std::string> modelCacheSignatures;
    std::map<std::string, int> teamLogoImageIndices;
    std::map<int, long long> pageTraceMs;
    std::string lastPageTrace;
    HIMAGELIST teamLogoImageList = nullptr;
    LayoutSnapshot layout;

    HWND divisionCombo = nullptr;
    HWND teamCombo = nullptr;
    HWND managerEdit = nullptr;
    HWND divisionLabel = nullptr;
    HWND teamLabel = nullptr;
    HWND managerLabel = nullptr;
    HWND filterLabel = nullptr;
    HWND filterCombo = nullptr;
    HWND managerHelpLabel = nullptr;
    HWND newCareerButton = nullptr;
    HWND loadButton = nullptr;
    HWND saveButton = nullptr;
    HWND simulateButton = nullptr;
    HWND validateButton = nullptr;
    HWND displayModeButton = nullptr;
    HWND frontMenuButton = nullptr;

    HWND dashboardButton = nullptr;
    HWND squadButton = nullptr;
    HWND tacticsButton = nullptr;
    HWND calendarButton = nullptr;
    HWND leagueButton = nullptr;
    HWND transfersButton = nullptr;
    HWND financesButton = nullptr;
    HWND youthButton = nullptr;
    HWND boardButton = nullptr;
    HWND newsButton = nullptr;
    HWND menuContinueButton = nullptr;
    HWND menuPlayButton = nullptr;
    HWND menuSettingsButton = nullptr;
    HWND menuLoadButton = nullptr;
    HWND menuDeleteSaveButton = nullptr;
    HWND menuCreditsButton = nullptr;
    HWND menuExitButton = nullptr;
    HWND menuBackButton = nullptr;
    HWND menuVolumeButton = nullptr;
    HWND menuDifficultyButton = nullptr;
    HWND menuSpeedButton = nullptr;
    HWND menuSimulationButton = nullptr;
    HWND menuLanguageButton = nullptr;
    HWND menuTextSpeedButton = nullptr;
    HWND menuVisualButton = nullptr;
    HWND menuMusicModeButton = nullptr;
    HWND menuAudioFadeButton = nullptr;
    HWND menuApplySettingsButton = nullptr;
    HWND menuResetSettingsButton = nullptr;
    HWND emptyNewButton = nullptr;
    HWND emptyLoadButton = nullptr;
    HWND emptyValidateButton = nullptr;

    HWND scoutActionButton = nullptr;
    HWND shortlistButton = nullptr;
    HWND followShortlistButton = nullptr;
    HWND buyButton = nullptr;
    HWND preContractButton = nullptr;
    HWND loanButton = nullptr;
    HWND renewButton = nullptr;
    HWND sellButton = nullptr;
    HWND planButton = nullptr;
    HWND instructionButton = nullptr;
    HWND youthUpgradeButton = nullptr;
    HWND trainingUpgradeButton = nullptr;
    HWND scoutingUpgradeButton = nullptr;
    HWND stadiumUpgradeButton = nullptr;

    HWND breadcrumbLabel = nullptr;
    HWND pageTitleLabel = nullptr;
    HWND infoLabel = nullptr;
    HWND summaryLabel = nullptr;
    HWND summaryEdit = nullptr;
    HWND tableLabel = nullptr;
    HWND tableList = nullptr;
    HWND squadLabel = nullptr;
    HWND squadList = nullptr;
    HWND transferLabel = nullptr;
    HWND transferList = nullptr;
    HWND detailLabel = nullptr;
    HWND detailEdit = nullptr;
    HWND newsLabel = nullptr;
    HWND newsList = nullptr;
    HWND statusLabel = nullptr;
};

static const COLORREF kThemeBg = RGB(7, 16, 22);
static const COLORREF kThemeHeader = RGB(10, 28, 35);
static const COLORREF kThemeTopBarPanel = RGB(10, 23, 30);
static const COLORREF kThemeShell = RGB(10, 21, 29);
static const COLORREF kThemePanel = RGB(16, 28, 39);
static const COLORREF kThemePanelAlt = RGB(12, 24, 33);
static const COLORREF kThemeInput = RGB(11, 19, 27);
static const COLORREF kThemeAccent = RGB(226, 191, 92);
static const COLORREF kThemeAccentBlue = RGB(71, 126, 212);
static const COLORREF kThemeAccentGreen = RGB(46, 142, 96);
static const COLORREF kThemeText = RGB(234, 238, 241);
static const COLORREF kThemeMuted = RGB(145, 163, 176);
static const COLORREF kThemeDanger = RGB(186, 78, 78);
static const COLORREF kThemeWarning = RGB(208, 167, 72);
static const COLORREF kThemeSelection = RGB(64, 91, 109);
static const UINT kGuiPageTransitionMessage = WM_APP + 17;
static const UINT kGuiSimulationProgressMessage = WM_APP + 18;
static const UINT kGuiSimulationCompleteMessage = WM_APP + 19;

RECT childRectOnParent(HWND child, HWND parent);
RECT expandedRect(RECT rect, int dx, int dy);
std::wstring utf8ToWide(const std::string& text);
std::string wideToUtf8(const std::wstring& text);
std::string toEditText(const std::string& text);
void setWindowTextUtf8(HWND hwnd, const std::string& text);
std::string getWindowTextUtf8(HWND hwnd);
void updateDynamicStaticText(AppState& state, HWND hwnd, const std::string& text);
void hideControlAndInvalidate(AppState& state, HWND hwnd, int padX = 8, int padY = 6);
void showControlAndInvalidate(AppState& state, HWND hwnd, int padX = 8, int padY = 6);
void setControlVisibility(AppState& state, HWND hwnd, bool visible, int padX = 8, int padY = 6);
void moveControlAndInvalidate(AppState& state, HWND hwnd, int x, int y, int width, int height, int padX = 10, int padY = 8);
void applyControlViewportClip(AppState& state, HWND hwnd, const RECT* clipRect);
void applyEditInteriorPadding(AppState& state, HWND hwnd, int horizontalPadding, int verticalPadding);
HWND createControl(AppState& state,
                   DWORD exStyle,
                   const wchar_t* className,
                   const wchar_t* text,
                   DWORD style,
                   int x,
                   int y,
                   int width,
                   int height,
                   HWND parent,
                   int id);
void addComboItem(HWND combo, const std::string& text);
void resetComboItems(HWND combo, const std::vector<std::string>& items);
int comboIndex(HWND combo);
std::string comboText(HWND combo);
int selectedListViewRow(HWND list);
std::string listViewText(HWND list, int row, int subItem);
void clearListView(HWND list);
void addListViewRow(HWND list, const std::vector<std::string>& values);
void resetListViewColumns(HWND list, const std::vector<std::pair<std::wstring, int> >& columns);
void autosizeListViewColumns(AppState& state, HWND list, const std::vector<std::pair<std::wstring, int> >& columns);
void drawRoundedPanel(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius = 16);
void drawPitchOverlay(HDC hdc, const RECT& rect);
void drawSectionHeader(HDC hdc, const RECT& rect, const std::wstring& title);
void drawStatBar(HDC hdc, const RECT& rect, const std::wstring& label, int value, int maxValue, COLORREF accent);
int scaleByDpi(const AppState& state, int value);
void rebuildTeamLogoImageList(AppState& state);
void applyTeamLogosToList(AppState& state, HWND list, const ListPanelModel& model);
void drawContextTeamLogo(AppState& state, HDC hdc);
const Team* currentContextTeam(const AppState& state);

GuiPage pageForControlId(int id);
bool isPageButtonId(int id);
bool isPrimaryButtonId(int id);
bool isUpgradeButtonId(int id);
bool isActionButtonId(int id);
bool isHeavyPage(GuiPage page);

void initializeInterface(AppState& state);
void rebuildFonts(AppState& state);
void applyInterfaceFonts(AppState& state);
void layoutWindow(AppState& state);
void paintWindowChrome(AppState& state, HDC hdc);
void drawThemedButton(AppState& state, const DRAWITEMSTRUCT* drawItem);
LRESULT handleListCustomDraw(AppState& state, LPNMHDR header);
void cycleDisplayMode(AppState& state);
bool isFrontMenuPage(GuiPage page);

std::string selectedDivisionId(const AppState& state);
void fillDivisionCombo(AppState& state, const std::string& selectedId = std::string());
void fillTeamCombo(AppState& state, const std::string& divisionId, const std::string& selectedTeam = std::string());
void syncCombosFromCareer(AppState& state);
void syncManagerNameFromUi(AppState& state);
void set_division(AppState& state, const std::string& divisionId);
void set_club(AppState& state, const std::string& clubName);
void set_manager(AppState& state, const std::string& managerName);
bool check_game_ready(AppState& state);
void setStatus(AppState& state, const std::string& text);
void setCurrentPage(AppState& state, GuiPage page);
void queuePageTransition(AppState& state, GuiPage page);
void refreshAll(AppState& state);
void refreshCurrentPage(AppState& state);
void autosizeCurrentLists(AppState& state);
void handleFilterChange(AppState& state);
void handleListSelectionChange(AppState& state, int controlId);
void handleFeedSelectionChange(AppState& state, int controlId);
void activateListAction(AppState& state, int controlId);
void handleListColumnClick(AppState& state, const NMLISTVIEW& view);

void startNewCareer(AppState& state);
void continueCareer(AppState& state);
void loadCareer(AppState& state);
void saveCareer(AppState& state);
void deleteCareerSave(AppState& state);
void simulateWeek(AppState& state);
void handleSimulationProgress(AppState& state, LPARAM payload);
void completeSimulationWeek(AppState& state, LPARAM payload);
void validateSystem(AppState& state);
void runScoutingAction(AppState& state);
void runBuyAction(AppState& state);
void runPreContractAction(AppState& state);
void runLoanAction(AppState& state);
void runRenewAction(AppState& state);
void runSellAction(AppState& state);
void runPlanAction(AppState& state);
void runInstructionAction(AppState& state);
void runShortlistAction(AppState& state);
void runFollowShortlistAction(AppState& state);
void runUpgradeAction(AppState& state, ClubUpgrade upgrade, const std::string& title);
void openFrontendMenu(AppState& state);
void openSettingsMenu(AppState& state);
void openCreditsPage(AppState& state);
void openSavesPage(AppState& state);
void cycleFrontendVolume(AppState& state);
void cycleFrontendDifficulty(AppState& state);
void cycleFrontendSimulationSpeed(AppState& state);
void cycleFrontendSimulationMode(AppState& state);
void cycleFrontendLanguage(AppState& state);
void cycleFrontendTextSpeed(AppState& state);
void cycleFrontendVisualProfile(AppState& state);
void cycleFrontendMenuMusicMode(AppState& state);
void toggleFrontendAudioFade(AppState& state);
void applyFrontendSettings(AppState& state);
void restoreFrontendSettings(AppState& state);

}  // namespace gui_win32

#endif
