#include "gui/gui_internal.h"
#include "gui/gui_view_builders.h"

#ifdef _WIN32

#include "engine/game_settings.h"
#include "finance/finance_system.h"
#include "utils/utils.h"

#include <algorithm>
#include <array>
#include <map>
#include <sstream>

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

namespace gui_win32 {

namespace {

struct ActionButtonRef {
    HWND hwnd;
    int width;
};

struct DashboardSpotlightCard {
    std::wstring label;
    std::wstring value;
    std::wstring detail;
    std::wstring actionHint;
    COLORREF accent = RGB(71, 126, 212);
    int pulse = 0;
    InsightAction action = InsightAction::None;
};

constexpr int kWindowPadding = 12;
constexpr int kSideRailWideWidth = 182;
constexpr int kSideRailCompactWidth = 168;
constexpr int kContentGap = 16;
constexpr int kMetricGap = 14;
constexpr int kMetricHeight = 58;
constexpr int kShellPadding = 16;
constexpr int kPageSectionGap = 18;
constexpr int kPanelGap = 16;
constexpr int kPanelTitleHeight = 24;
constexpr int kPanelHeaderGap = 8;
constexpr int kPanelContentPadding = 12;
constexpr int kPanelRowGap = 18;
constexpr int kPanelLabelHeight = kPanelTitleHeight;
constexpr int kPanelBodyOffset = kPanelTitleHeight + kPanelHeaderGap;
constexpr int kPanelSectionGap = kPanelGap;
constexpr int kStatusHeight = 28;
constexpr int kHeaderFieldHeight = 32;
constexpr int kHeaderHintHeight = 18;
constexpr int kHeaderButtonHeight = 34;
constexpr int kHeaderRowGap = 8;
constexpr int kMetricCount = 5;
constexpr int kDashboardSpotlightWideReserve = 96;
constexpr int kDashboardSpotlightCompactReserve = 150;
constexpr int kInsightStripWideReserve = 88;
constexpr int kInsightStripCompactReserve = 134;
constexpr int kPageHeaderHeight = 94;
constexpr int kBreadcrumbHeight = 20;
constexpr int kPageTitleHeight = 32;
constexpr int kPageInfoHeight = 24;
constexpr int kActionButtonHeight = 28;
constexpr int kActionRowGap = 8;
constexpr int kRightRailMinWidth = 240;
constexpr int kCenterColumnMinWidth = 360;
constexpr int kContextCardHeight = 154;
constexpr int kComboPopupHeight = 260;

struct HeaderLayoutProfile {
    int sideWidth = kSideRailWideWidth;
    int topBarHeight = 138;
    int controlsBottom = 48;
    int buttonsTop = 16;
    int buttonsBottom = 50;
    int metricTop = 66;
    int metricColumns = kMetricCount;
    int metricRows = 1;
    bool shareTopRow = true;
    bool splitControls = false;
    bool stackedControls = false;
};

struct PageLayoutProfile {
    int preferredInfoWidth = 340;
    int minInfoWidth = 300;
    int summaryPercent = 40;
    int summaryMinWidth = 320;
    int topPanelHeight = 196;
    int midPanelHeight = 226;
    int footerMinHeight = 150;
    int dashboardDetailHeight = 148;
    int nonDashboardDetailExtra = 132;
};

int rectWidth(const RECT& rect) {
    return std::max(0L, rect.right - rect.left);
}

int rectHeight(const RECT& rect) {
    return std::max(0L, rect.bottom - rect.top);
}

RECT makeRect(int left, int top, int width, int height) {
    RECT rect{left, top, left + std::max(0, width), top + std::max(0, height)};
    return rect;
}

RECT shrinkRect(const RECT& rect, int horizontal, int vertical) {
    RECT out = rect;
    out.left += horizontal;
    out.right -= horizontal;
    out.top += vertical;
    out.bottom -= vertical;
    if (out.right < out.left) out.right = out.left;
    if (out.bottom < out.top) out.bottom = out.top;
    return out;
}

RECT offsetRectCopy(const RECT& rect, int dx, int dy) {
    RECT out = rect;
    OffsetRect(&out, dx, dy);
    return out;
}

void ensureMainMenuBackgroundLoaded(AppState& state) {
    if (state.mainMenuBackground) return;

    const std::string backgroundPath =
        resolveProjectPath("assets/gui/main_menu_background.bmp");

    state.mainMenuBackground = reinterpret_cast<HBITMAP>(
        LoadImageW(nullptr,
                   utf8ToWide(backgroundPath).c_str(),
                   IMAGE_BITMAP,
                   0,
                   0,
                   LR_LOADFROMFILE | LR_CREATEDIBSECTION));
}

void drawMainMenuBackground(AppState& state, HDC hdc, const RECT& client) {
    ensureMainMenuBackgroundLoaded(state);
    if (!state.mainMenuBackground) return;

    BITMAP bitmap{};
    if (GetObject(state.mainMenuBackground, sizeof(bitmap), &bitmap) == 0) return;

    HDC memoryDc = CreateCompatibleDC(hdc);
    if (!memoryDc) return;

    HGDIOBJ oldBitmap = SelectObject(memoryDc, state.mainMenuBackground);
    SetStretchBltMode(hdc, HALFTONE);
    StretchBlt(hdc,
               client.left,
               client.top,
               std::max(1L, client.right - client.left),
               std::max(1L, client.bottom - client.top),
               memoryDc,
               0,
               0,
               bitmap.bmWidth,
               bitmap.bmHeight,
               SRCCOPY);
    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);
}

struct RectCursor {
    RECT remaining{};
};

RECT takeTop(RectCursor& cursor, int height, int gap = 0) {
    height = std::max(0, std::min(height, rectHeight(cursor.remaining)));
    RECT taken{cursor.remaining.left, cursor.remaining.top, cursor.remaining.right, cursor.remaining.top + height};
    cursor.remaining.top = std::min(cursor.remaining.bottom, taken.bottom + std::max(0, gap));
    return taken;
}

RECT takeRight(RectCursor& cursor, int width, int gap = 0) {
    width = std::max(0, std::min(width, rectWidth(cursor.remaining)));
    RECT taken{cursor.remaining.right - width, cursor.remaining.top, cursor.remaining.right, cursor.remaining.bottom};
    cursor.remaining.right = std::max(cursor.remaining.left, taken.left - std::max(0, gap));
    return taken;
}

PanelBounds buildPanelBounds(const RECT& outer, int titleHeight, int contentPadding, int titleGap) {
    PanelBounds panel;
    panel.outer = outer;
    panel.visible = rectWidth(outer) > 0 && rectHeight(outer) > 0;
    RECT inner = shrinkRect(outer, contentPadding, contentPadding);
    RectCursor cursor{inner};
    panel.title = takeTop(cursor, titleHeight, titleGap);
    panel.body = cursor.remaining;
    return panel;
}

RECT controlRectForCombo(const RECT& fieldRect, int popupHeight) {
    RECT combo = fieldRect;
    combo.bottom = combo.top + popupHeight;
    return combo;
}

RECT viewportRect(const AppState& state, const RECT& docRect, bool scrollable) {
    return scrollable ? offsetRectCopy(docRect, 0, -state.pageScrollY) : docRect;
}

PanelBounds projectPanelBounds(const AppState& state, const PanelBounds& panel, bool scrollable) {
    PanelBounds projected = panel;
    projected.outer = viewportRect(state, panel.outer, scrollable);
    projected.title = viewportRect(state, panel.title, scrollable);
    projected.body = viewportRect(state, panel.body, scrollable);
    return projected;
}

bool rectHasArea(const RECT& rect) {
    return rectWidth(rect) > 0 && rectHeight(rect) > 0;
}

class ScopedClipRect {
public:
    ScopedClipRect(HDC hdc, const RECT& rect) : hdc_(hdc), savedDc_(SaveDC(hdc)) {
        IntersectClipRect(hdc_, rect.left, rect.top, rect.right, rect.bottom);
    }

    ~ScopedClipRect() {
        RestoreDC(hdc_, savedDc_);
    }

private:
    HDC hdc_;
    int savedDc_ = 0;
};

HeaderLayoutProfile buildHeaderLayout(const RECT& client) {
    HeaderLayoutProfile layout;
    const int width = client.right - client.left;
    layout.sideWidth = width < 1240 ? kSideRailCompactWidth : kSideRailWideWidth;
    layout.shareTopRow = width >= 2260;
    layout.splitControls = width < 1540;
    layout.stackedControls = width < 1180;

    if (layout.stackedControls) {
        layout.controlsBottom = 16 + kHeaderFieldHeight * 3 + kHeaderRowGap * 2 + kHeaderHintHeight + 6;
    } else if (layout.splitControls) {
        layout.controlsBottom = 16 + kHeaderFieldHeight * 2 + kHeaderRowGap + kHeaderHintHeight + 6;
    } else {
        layout.controlsBottom = 16 + kHeaderFieldHeight + kHeaderHintHeight + 6;
    }

    layout.buttonsTop = layout.shareTopRow ? 16 : layout.controlsBottom + 10;
    const int availableButtonRow = std::max(320, width - kWindowPadding * 2);
    const int fullButtonRowWidth = 156 + 112 + 112 + 126 + 138 + 154 + 94 + 10 * 6;
    const int buttonRows = availableButtonRow >= fullButtonRowWidth ? 1 : 2;
    layout.buttonsBottom = layout.buttonsTop + kHeaderButtonHeight + (buttonRows - 1) * (kHeaderButtonHeight + kHeaderRowGap);

    if (width < 980) layout.metricColumns = 2;
    else if (width < 1280) layout.metricColumns = 3;
    else layout.metricColumns = kMetricCount;
    layout.metricRows = (kMetricCount + layout.metricColumns - 1) / layout.metricColumns;
    layout.metricTop = std::max(layout.controlsBottom, layout.buttonsBottom) + 16;
    layout.topBarHeight = layout.metricTop + layout.metricRows * kMetricHeight + (layout.metricRows - 1) * kMetricGap + 14;
    return layout;
}

PageLayoutProfile buildPageLayoutProfile(GuiPage page) {
    switch (page) {
        case GuiPage::MainMenu:
        case GuiPage::Settings:
        case GuiPage::Credits:
        case GuiPage::Saves:
            return {360, 320, 52, 360, 220, 220, 160, 148, 140};
        case GuiPage::Dashboard:
            return {340, 300, 56, 348, 226, 166, 160, 148, 132};
        case GuiPage::Squad:
        case GuiPage::Youth:
            return {364, 320, 38, 312, 192, 258, 152, 148, 144};
        case GuiPage::Tactics:
            return {360, 320, 40, 336, 208, 240, 154, 152, 144};
        case GuiPage::Calendar:
            return {350, 310, 40, 320, 194, 220, 150, 148, 136};
        case GuiPage::League:
            return {350, 310, 42, 320, 194, 216, 150, 148, 136};
        case GuiPage::Transfers:
            return {392, 340, 35, 300, 202, 252, 160, 156, 150};
        case GuiPage::Finances:
            return {376, 330, 36, 308, 198, 238, 160, 152, 148};
        case GuiPage::Board:
            return {360, 320, 38, 312, 206, 236, 160, 152, 146};
        case GuiPage::News:
            return {404, 350, 32, 292, 194, 228, 166, 154, 150};
    }
    return PageLayoutProfile{};
}

bool pageUsesInsightStrip(const AppState& state) {
    if (state.currentPage == GuiPage::Dashboard) return true;
    if (!state.career.myTeam) return false;
    return state.currentPage == GuiPage::Transfers ||
           state.currentPage == GuiPage::Finances ||
           state.currentPage == GuiPage::Board;
}

const Team* previewSetupTeam(const AppState& state) {
    if (state.gameSetup.division.empty() || state.gameSetup.club.empty()) return nullptr;
    for (const auto& team : state.career.allTeams) {
        if (team.division == state.gameSetup.division && team.name == state.gameSetup.club) return &team;
    }
    return nullptr;
}

COLORREF setupStepAccent(bool completed, bool current) {
    if (completed) return kThemeAccentGreen;
    if (current) return kThemeWarning;
    return kThemeDanger;
}

int pulseFromRatio(long long current, long long good, long long dangerFloor) {
    if (current <= dangerFloor) return 6;
    if (current >= good) return 100;
    const long long span = std::max(1LL, good - dangerFloor);
    return clampValue(static_cast<int>((current - dangerFloor) * 100 / span), 0, 100);
}

std::vector<DashboardMetric> defaultMetrics() {
    return {
        {"Club", "Sin carrera", kThemeAccentBlue},
        {"Fecha", "Temporada 0 / Semana 0", kThemeAccent},
        {"Presupuesto", "$0", kThemeAccentGreen},
        {"Reputacion", "0", kThemeAccentBlue},
        {"Alertas", "0", kThemeWarning}
    };
}

std::wstring shortPlayerLabel(const std::string& name) {
    std::vector<std::string> parts = splitByDelimiter(trim(name), ' ');
    if (parts.empty()) return L"--";
    if (parts.size() == 1) return utf8ToWide(parts.front().substr(0, std::min<size_t>(parts.front().size(), 8)));
    std::string label = parts.back();
    if (!parts.front().empty()) {
        label = parts.front().substr(0, 1) + "." + label;
    }
    return utf8ToWide(label.substr(0, std::min<size_t>(label.size(), 10)));
}

std::vector<const Player*> playersForLine(const Team& team, const std::string& position) {
    std::vector<const Player*> players;
    std::vector<int> xi = team.getStartingXIIndices();
    for (int index : xi) {
        if (index < 0 || index >= static_cast<int>(team.players.size())) continue;
        const Player& player = team.players[static_cast<size_t>(index)];
        if (normalizePosition(player.position) == position) players.push_back(&player);
    }
    return players;
}

void drawPlayerDots(HDC hdc,
                    const RECT& rect,
                    const std::vector<const Player*>& players,
                    double xRatio,
                    COLORREF fill) {
    if (players.empty()) return;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x = rect.left + static_cast<int>(width * xRatio);
    int spacing = height / static_cast<int>(players.size() + 1);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kThemeText);
    for (size_t i = 0; i < players.size(); ++i) {
        int y = rect.top + spacing * static_cast<int>(i + 1);
        Ellipse(hdc, x - 13, y - 13, x + 13, y + 13);
        std::wstring label = shortPlayerLabel(players[i]->name);
        RECT textRect{x + 18, y - 10, rect.right - 10, y + 10};
        DrawTextW(hdc, label.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
}

void drawTopMetrics(AppState& state, HDC hdc, const RECT& client) {
    const HeaderLayoutProfile header = buildHeaderLayout(client);
    const auto s = [&](int value) { return scaleByDpi(state, value); };
    std::vector<DashboardMetric> metrics = state.currentModel.metrics.empty() ? defaultMetrics() : state.currentModel.metrics;
    const int left = s(kWindowPadding + 6);
    const int gap = s(kMetricGap);
    const int width = std::max(s(header.metricColumns <= 2 ? 220 : 158),
                               static_cast<int>((client.right - left * 2 - gap * (header.metricColumns - 1)) / header.metricColumns));
    const int height = s(kMetricHeight);

    for (size_t i = 0; i < metrics.size() && i < 5; ++i) {
        const int row = static_cast<int>(i) / header.metricColumns;
        const int column = static_cast<int>(i) % header.metricColumns;
        const int top = s(header.metricTop) + row * (height + gap);
        RECT card{left + column * (width + gap), top, left + column * (width + gap) + width, top + height};
        drawRoundedPanel(hdc, card, RGB(11, 24, 33), RGB(35, 58, 74), s(14));

        RECT accent = card;
        accent.bottom = accent.top + s(6);
        HBRUSH accentBrush = CreateSolidBrush(metrics[i].accent);
        FillRect(hdc, &accent, accentBrush);
        DeleteObject(accentBrush);

        RECT labelRect{card.left + s(14), card.top + s(10), card.right - s(14), card.top + s(28)};
        RECT valueRect{card.left + s(14), card.top + s(28), card.right - s(14), card.bottom - s(10)};
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kThemeMuted);
        HGDIOBJ oldFont = SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        DrawTextW(hdc, utf8ToWide(metrics[i].label).c_str(), -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        SetTextColor(hdc, kThemeText);
        DrawTextW(hdc, utf8ToWide(metrics[i].value).c_str(), -1, &valueRect, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, oldFont);
    }
}

std::vector<DashboardSpotlightCard> buildDashboardSpotlightCards(const AppState& state) {
    std::vector<DashboardSpotlightCard> cards;

    if (!state.career.myTeam) {
        const Team* setupTeam = previewSetupTeam(state);
        const bool divisionReady = !state.gameSetup.division.empty();
        const bool clubReady = setupTeam != nullptr;
        const bool managerReady = !state.gameSetup.manager.empty();
        const bool currentDivision = state.gameSetup.currentStep == 1;
        const bool currentClub = state.gameSetup.currentStep == 2;
        const bool currentManager = state.gameSetup.currentStep == 3 && !state.gameSetup.ready;
        cards.push_back({L"Paso 1 - Universo",
                         utf8ToWide(divisionReady ? divisionDisplay(state.gameSetup.division) : "Elige division"),
                         utf8ToWide(divisionReady ? "Division lista. Ya puedes pasar al club."
                                                  : "Selecciona la division para habilitar clubes y presupuesto."),
                         L"Click: elegir division",
                         setupStepAccent(divisionReady, currentDivision),
                         divisionReady ? 100 : 24,
                         InsightAction::FocusDivision});
        cards.push_back({L"Paso 2 - Proyecto",
                         utf8ToWide(clubReady ? setupTeam->name : "Club pendiente"),
                         utf8ToWide(clubReady ? ("Presupuesto " + formatMoneyValue(setupTeam->budget))
                                              : "Elige un club para ver presupuesto, contexto y estilo deportivo."),
                         L"Click: elegir club",
                         setupStepAccent(clubReady, currentClub),
                         clubReady ? 100 : (divisionReady ? 58 : 18),
                         InsightAction::FocusClub});
        cards.push_back({L"Paso 3 - Lanzamiento",
                         utf8ToWide(managerReady ? state.gameSetup.manager : "Manager pendiente"),
                         utf8ToWide(state.gameSetup.ready
                                        ? "Checklist completa. Nueva partida ya se puede iniciar."
                                        : (state.gameSetup.managerError.empty()
                                               ? "Escribe el nombre del manager para cerrar el flujo."
                                               : state.gameSetup.managerError)),
                         utf8ToWide(state.gameSetup.ready ? "Click: crear carrera" : "Click: escribir manager"),
                         setupStepAccent(managerReady, currentManager),
                         state.gameSetup.ready ? 100 : (managerReady ? 78 : 24),
                         state.gameSetup.ready ? InsightAction::StartCareer : InsightAction::FocusManager});
        return cards;
    }

    const Team& team = *state.career.myTeam;
    int injured = 0;
    int lowMorale = 0;
    int lowFitness = 0;
    for (const auto& player : team.players) {
        if (player.injured) injured++;
        if (player.happiness < 45) lowMorale++;
        if (!player.injured && player.fitness < 62) lowFitness++;
    }

    const int fieldSize = std::max(1, state.career.currentCompetitiveFieldSize());
    const int rank = std::max(1, state.career.currentCompetitiveRank());
    const int rankPulse = clampValue(100 - ((rank - 1) * 100 / std::max(1, fieldSize - 1)), 0, 100);
    const int squadPulse = clampValue(team.morale - injured * 7 - lowFitness * 3, 0, 100);
    const int deskPulse = state.career.boardMonthlyTarget > 0
        ? clampValue(state.career.boardMonthlyProgress * 100 / std::max(1, state.career.boardMonthlyTarget), 0, 100)
        : clampValue(state.career.boardConfidence, 0, 100);

    std::ostringstream competitiveValue;
    competitiveValue << "#" << rank << " de " << fieldSize;
    std::ostringstream competitiveDetail;
    competitiveDetail << "Puntos " << team.points
                      << " | DG " << (team.goalsFor - team.goalsAgainst)
                      << "\nObjetivo: "
                      << (state.career.boardMonthlyObjective.empty() ? "sin objetivo mensual" : state.career.boardMonthlyObjective);

    std::ostringstream squadValue;
    squadValue << "Moral " << team.morale << " | LES " << injured;
    std::ostringstream squadDetail;
    squadDetail << "Bajos de forma " << lowFitness
                << " | animos delicados " << lowMorale
                << "\nEntreno: " << team.trainingFocus;

    std::ostringstream deskDetail;
    deskDetail << "Shortlist " << state.career.scoutingShortlist.size()
               << " | directiva " << state.career.boardConfidence << "/100"
               << "\n" << (team.transferPolicy.empty() ? "Politica mixta" : team.transferPolicy);

    COLORREF squadAccent = injured >= 3 || lowFitness >= 4 ? kThemeWarning : kThemeAccentGreen;
    COLORREF deskAccent = team.budget < 0 ? kThemeDanger : kThemeAccent;
    cards.push_back({L"Pulso competitivo",
                     utf8ToWide(competitiveValue.str()),
                     utf8ToWide(competitiveDetail.str()),
                     L"Click: abrir liga",
                     kThemeAccentBlue,
                     rankPulse,
                     InsightAction::OpenLeague});
    cards.push_back({L"Vestuario y salud",
                     utf8ToWide(squadValue.str()),
                     utf8ToWide(squadDetail.str()),
                     L"Click: abrir plantilla",
                     squadAccent,
                     squadPulse,
                     InsightAction::OpenSquad});
    cards.push_back({L"Mesa del manager",
                     utf8ToWide(formatMoneyValue(team.budget)),
                     utf8ToWide(deskDetail.str()),
                     L"Click: abrir directiva",
                     deskAccent,
                     deskPulse,
                     InsightAction::OpenBoard});
    return cards;
}

std::vector<DashboardSpotlightCard> buildContextInsightCards(const AppState& state) {
    std::vector<DashboardSpotlightCard> cards;
    if (!state.career.myTeam) return cards;

    const Team& team = *state.career.myTeam;
    if (state.currentPage == GuiPage::Transfers) {
        int expiringContracts = 0;
        int wantsOut = 0;
        int injured = 0;
        int salaryHeavy = 0;
        for (const auto& player : team.players) {
            if (player.contractWeeks <= 12) ++expiringContracts;
            if (player.wantsToLeave) ++wantsOut;
            if (player.injured) ++injured;
            if (player.wage > std::max(25000LL, team.sponsorWeekly / 3)) ++salaryHeavy;
        }

        const int marketPressure = clampValue(expiringContracts * 11 + wantsOut * 15 +
                                              static_cast<int>(state.career.pendingTransfers.size()) * 9 + injured * 6,
                                              0,
                                              100);
        const int squadRisk = clampValue(salaryHeavy * 12 + injured * 8, 0, 100);
        cards.push_back({L"Presion de mercado",
                         utf8ToWide(std::to_string(expiringContracts + wantsOut) + " focos"),
                         utf8ToWide("Pendientes " + std::to_string(state.career.pendingTransfers.size()) +
                                    " | lesionados " + std::to_string(injured) +
                                    "\nContratos cortos " + std::to_string(expiringContracts) +
                                    " | salidas " + std::to_string(wantsOut)),
                         L"Click: actualizar radar",
                         marketPressure >= 65 ? kThemeWarning : kThemeAccentBlue,
                         marketPressure,
                         InsightAction::RefreshMarketPulse});
        cards.push_back({L"Caja y scouting",
                         utf8ToWide(formatMoneyValue(team.budget)),
                         utf8ToWide("Shortlist " + std::to_string(state.career.scoutingShortlist.size()) +
                                    " | jefe scouting " + std::to_string(team.scoutingChief) +
                                    "\nRed " + (team.scoutingRegions.empty() ? std::string("-") : joinStringValues(team.scoutingRegions, ", "))),
                         L"Click: lanzar scouting",
                         team.budget < 0 ? kThemeDanger : kThemeAccentGreen,
                         pulseFromRatio(team.budget, std::max(220000LL, team.sponsorWeekly * 5), -50000LL),
                         InsightAction::RunScouting});
        cards.push_back({L"Riesgo de plantilla",
                         utf8ToWide(std::to_string(squadRisk / 10) + "/10"),
                         utf8ToWide("Sueldos altos " + std::to_string(salaryHeavy) +
                                    " | lesionados " + std::to_string(injured) +
                                    "\nCompra con salida previa si la masa salarial ya esta tensa."),
                         L"Click: abrir salarios",
                         squadRisk >= 60 ? kThemeDanger : kThemeAccent,
                         100 - std::min(92, squadRisk),
                         InsightAction::OpenFinanceSalaries});
        return cards;
    }

    if (state.currentPage == GuiPage::Finances) {
        const WeeklyFinanceReport finance = finance_system::projectWeeklyReport(team);
        const int infraAverage = (team.youthFacilityLevel + team.trainingFacilityLevel + team.stadiumLevel +
                                  team.scoutingChief + team.performanceAnalyst + team.medicalTeam) / 6;
        const long long safeSponsor = std::max(1LL, finance.sponsorIncome);
        const int payrollLoad = clampValue(static_cast<int>(finance.wageBill * 100 / safeSponsor), 0, 180);
        cards.push_back({L"Caja operativa",
                         utf8ToWide(formatMoneyValue(finance.netCashFlow)),
                         utf8ToWide("Buffer " + formatMoneyValue(finance.transferBuffer) +
                                    " | deuda " + formatMoneyValue(team.debt) +
                                    "\nSponsor " + formatMoneyValue(finance.sponsorIncome) +
                                    " | bonos " + formatMoneyValue(finance.bonusIncome)),
                         L"Click: ver resumen",
                         finance.netCashFlow < 0 ? kThemeDanger : kThemeAccentGreen,
                         pulseFromRatio(finance.transferBuffer, std::max(180000LL, team.sponsorWeekly * 4), -80000LL),
                         InsightAction::OpenFinanceSummary});
        cards.push_back({L"Estructura salarial",
                         utf8ToWide(formatMoneyValue(finance.wageBill)),
                         utf8ToWide("Carga " + std::to_string(payrollLoad) + "% del sponsor semanal" +
                                    "\nRiesgo " + finance.riskLevel +
                                    " | taquilla " + formatMoneyValue(finance.matchdayIncome)),
                         L"Click: ver salarios",
                         payrollLoad > 110 ? kThemeWarning : kThemeAccentBlue,
                         clampValue(118 - payrollLoad, 8, 100),
                         InsightAction::OpenFinanceSalaries});
        cards.push_back({L"Infraestructura",
                         utf8ToWide(std::to_string(infraAverage) + "/100"),
                         utf8ToWide("Cantera " + std::to_string(team.youthFacilityLevel) +
                                    " | entreno " + std::to_string(team.trainingFacilityLevel) +
                                    "\nEstadio " + std::to_string(team.stadiumLevel) +
                                    " | analisis " + std::to_string(team.performanceAnalyst)),
                         L"Click: ver infraestructura",
                         infraAverage >= 60 ? kThemeAccent : kThemeAccentBlue,
                         clampValue(infraAverage, 0, 100),
                         InsightAction::OpenFinanceInfrastructure});
        return cards;
    }

    if (state.currentPage == GuiPage::Board) {
        const int rank = std::max(1, state.career.currentCompetitiveRank());
        const int fieldSize = std::max(1, state.career.currentCompetitiveFieldSize());
        const int boardPulse = clampValue(state.career.boardConfidence, 0, 100);
        const int objectivePulse = state.career.boardMonthlyTarget > 0
            ? clampValue(state.career.boardMonthlyProgress * 100 / std::max(1, state.career.boardMonthlyTarget), 0, 100)
            : std::max(35, boardPulse);
        const int jobPulse = clampValue((team.jobSecurity + boardPulse) / 2, 0, 100);
        cards.push_back({L"Confianza directiva",
                         utf8ToWide(std::to_string(state.career.boardConfidence) + "/100"),
                         utf8ToWide("Esperan puesto " + std::to_string(state.career.boardExpectedFinish) +
                                    " | actual #" + std::to_string(rank) +
                                    "\nAdvertencias " + std::to_string(state.career.boardWarningWeeks) +
                                    " | campo " + std::to_string(fieldSize)),
                         L"Click: ver resumen",
                         state.career.boardConfidence < 35 ? kThemeDanger : kThemeAccentGreen,
                         boardPulse,
                         InsightAction::OpenBoardSummary});
        cards.push_back({L"Objetivo mensual",
                         utf8ToWide(state.career.boardMonthlyTarget > 0
                                        ? std::to_string(state.career.boardMonthlyProgress) + "/" + std::to_string(state.career.boardMonthlyTarget)
                                        : "Sin target"),
                         utf8ToWide((state.career.boardMonthlyObjective.empty() ? std::string("Sin objetivo dinamico")
                                                                                 : state.career.boardMonthlyObjective) +
                                    "\nDeadline sem " + std::to_string(state.career.boardMonthlyDeadlineWeek)),
                         L"Click: ver objetivos",
                         objectivePulse < 45 ? kThemeWarning : kThemeAccentBlue,
                         objectivePulse,
                         InsightAction::OpenBoardObjectives});
        cards.push_back({L"Presion del cargo",
                         utf8ToWide(std::to_string(team.jobSecurity) + "/100"),
                         utf8ToWide("Prestigio " + std::to_string(team.clubPrestige) +
                                    " | DT " + team.headCoachName +
                                    "\nPolitica " + (team.transferPolicy.empty() ? std::string("Mixta") : team.transferPolicy)),
                         L"Click: ver historial",
                         team.jobSecurity < 35 ? kThemeDanger : kThemeAccent,
                         jobPulse,
                         InsightAction::OpenBoardHistory});
    }

    return cards;
}

void rememberInsightHotspot(AppState& state, const RECT& rect, const DashboardSpotlightCard& card) {
    if (card.action == InsightAction::None) return;
    InsightHotspot hotspot;
    hotspot.rect = rect;
    hotspot.action = card.action;
    hotspot.hint = card.actionHint;
    state.insightHotspots.push_back(hotspot);
}

void drawInsightCard(AppState& state, HDC hdc, const RECT& cardRect, const DashboardSpotlightCard& card) {
    const auto s = [&](int value) { return scaleByDpi(state, value); };
    const bool hasAction = card.action != InsightAction::None && !card.actionHint.empty();

    drawRoundedPanel(hdc, cardRect, RGB(13, 26, 35), RGB(37, 62, 78), s(16));

    RECT accent = cardRect;
    accent.bottom = accent.top + s(6);
    HBRUSH accentBrush = CreateSolidBrush(card.accent);
    FillRect(hdc, &accent, accentBrush);
    DeleteObject(accentBrush);

    RECT labelRect{cardRect.left + s(14), cardRect.top + s(10), cardRect.right - s(14), cardRect.top + s(28)};
    RECT valueRect{cardRect.left + s(14), labelRect.bottom + s(2), cardRect.right - s(14), labelRect.bottom + s(34)};
    RECT detailRect{cardRect.left + s(14), valueRect.bottom + s(2), cardRect.right - s(14),
                    cardRect.bottom - (hasAction ? s(34) : s(22))};
    RECT actionRect{cardRect.left + s(14), cardRect.bottom - s(34), cardRect.right - s(14), cardRect.bottom - s(18)};
    RECT trackRect{cardRect.left + s(14), cardRect.bottom - s(14), cardRect.right - s(14), cardRect.bottom - s(8)};

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kThemeMuted);
    HGDIOBJ oldFont = SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    DrawTextW(hdc, card.label.c_str(), -1, &labelRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
    SetTextColor(hdc, kThemeText);
    DrawTextW(hdc, card.value.c_str(), -1, &valueRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    SetTextColor(hdc, RGB(204, 219, 228));
    DrawTextW(hdc, card.detail.c_str(), -1, &detailRect, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

    if (hasAction) {
        SetTextColor(hdc, card.accent);
        DrawTextW(hdc, card.actionHint.c_str(), -1, &actionRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        rememberInsightHotspot(state, cardRect, card);
    }

    drawRoundedPanel(hdc, trackRect, RGB(19, 35, 45), RGB(39, 61, 74), s(8));
    RECT fill = trackRect;
    fill.left += 1;
    fill.top += 1;
    fill.bottom -= 1;
    const int pulseWidth = std::max(s(8),
                                    static_cast<int>((trackRect.right - trackRect.left - 2) *
                                                     clampValue(card.pulse, 0, 100) / 100));
    fill.right = std::min(trackRect.right - 1, fill.left + pulseWidth);
    drawRoundedPanel(hdc, fill, card.accent, card.accent, s(7));
    SelectObject(hdc, oldFont);
}

void drawDashboardSpotlights(AppState& state, HDC hdc, const RECT& band) {
    if (IsRectEmpty(&band)) return;

    const auto s = [&](int value) { return scaleByDpi(state, value); };
    const std::vector<DashboardSpotlightCard> cards = buildDashboardSpotlightCards(state);
    if (cards.empty()) return;

    const int width = band.right - band.left;
    const int height = band.bottom - band.top;
    const int gap = s(14);
    const int columns = width < s(900) ? 2 : 3;
    const int rows = static_cast<int>((cards.size() + columns - 1) / columns);
    const int cardWidth = std::max(s(210), (width - gap * (columns - 1)) / columns);
    const int cardHeight = std::max(s(64), (height - gap * (rows - 1)) / rows);

    for (size_t i = 0; i < cards.size(); ++i) {
        const int row = static_cast<int>(i) / columns;
        const int column = static_cast<int>(i) % columns;
        RECT card{
            band.left + column * (cardWidth + gap),
            band.top + row * (cardHeight + gap),
            std::min(band.right, band.left + column * (cardWidth + gap) + cardWidth),
            std::min(band.bottom, band.top + row * (cardHeight + gap) + cardHeight)
        };
        drawInsightCard(state, hdc, card, cards[i]);
    }
}

void drawContextSpotlights(AppState& state, HDC hdc, const RECT& band) {
    if (IsRectEmpty(&band)) return;

    const auto s = [&](int value) { return scaleByDpi(state, value); };
    const std::vector<DashboardSpotlightCard> cards = buildContextInsightCards(state);
    if (cards.empty()) return;

    const int width = band.right - band.left;
    const int height = band.bottom - band.top;
    const int gap = s(14);
    const int columns = width < s(900) ? 2 : 3;
    const int rows = static_cast<int>((cards.size() + columns - 1) / columns);
    const int cardWidth = std::max(s(196), (width - gap * (columns - 1)) / columns);
    const int cardHeight = std::max(s(62), (height - gap * (rows - 1)) / rows);

    for (size_t i = 0; i < cards.size(); ++i) {
        const int row = static_cast<int>(i) / columns;
        const int column = static_cast<int>(i) % columns;
        RECT card{
            band.left + column * (cardWidth + gap),
            band.top + row * (cardHeight + gap),
            std::min(band.right, band.left + column * (cardWidth + gap) + cardWidth),
            std::min(band.bottom, band.top + row * (cardHeight + gap) + cardHeight)
        };
        drawInsightCard(state, hdc, card, cards[i]);
    }
}

void drawTacticsBoard(AppState& state, HDC hdc, const RECT& rect) {
    drawRoundedPanel(hdc, rect, RGB(14, 31, 26), RGB(44, 86, 67), 16);
    RECT inner = rect;
    InflateRect(&inner, -14, -14);
    drawPitchOverlay(hdc, inner);

    if (!state.career.myTeam) return;
    const Team& team = *state.career.myTeam;
    drawPlayerDots(hdc, inner, playersForLine(team, "ARQ"), 0.12, RGB(66, 116, 186));
    drawPlayerDots(hdc, inner, playersForLine(team, "DEF"), 0.31, RGB(46, 142, 96));
    drawPlayerDots(hdc, inner, playersForLine(team, "MED"), 0.55, RGB(226, 191, 92));
    drawPlayerDots(hdc, inner, playersForLine(team, "DEL"), 0.79, RGB(204, 108, 74));

    RECT titleRect{inner.left + 12, inner.top + 10, inner.right - 12, inner.top + 32};
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(228, 241, 236));
    HGDIOBJ oldFont = SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
    std::wstring title = utf8ToWide(team.formation + " | " + team.tactics);
    DrawTextW(hdc, title.c_str(), -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    RECT barArea{inner.left + 12, inner.bottom - 116, inner.right - 12, inner.bottom - 12};
    int barWidth = (barArea.right - barArea.left - 18) / 2;
    std::array<RECT, 5> bars{
        RECT{barArea.left, barArea.top, barArea.left + barWidth, barArea.top + 28},
        RECT{barArea.left + barWidth + 18, barArea.top, barArea.right, barArea.top + 28},
        RECT{barArea.left, barArea.top + 34, barArea.left + barWidth, barArea.top + 62},
        RECT{barArea.left + barWidth + 18, barArea.top + 34, barArea.right, barArea.top + 62},
        RECT{barArea.left, barArea.top + 68, barArea.right, barArea.top + 96}
    };
    drawStatBar(hdc, bars[0], L"Presion", team.pressingIntensity, 5, kThemeAccentGreen);
    drawStatBar(hdc, bars[1], L"Ritmo", team.tempo, 5, kThemeAccentBlue);
    drawStatBar(hdc, bars[2], L"Anchura", team.width, 5, kThemeAccent);
    drawStatBar(hdc, bars[3], L"Linea", team.defensiveLine, 5, kThemeWarning);
    drawStatBar(hdc, bars[4], L"Moral equipo", team.morale, 100, kThemeAccentGreen);
}

void setLabelFont(HWND hwnd, HFONT font) {
    if (hwnd && font) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void setControlFont(HWND hwnd, HFONT font) {
    if (hwnd && font) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void showActionButtonsForPage(AppState& state) {
    struct Mapping {
        HWND hwnd;
        std::vector<GuiPage> pages;
    };
    const std::vector<Mapping> mappings = {
        {state.scoutActionButton, {GuiPage::Transfers, GuiPage::Dashboard, GuiPage::Youth, GuiPage::News}},
        {state.shortlistButton, {GuiPage::Transfers}},
        {state.followShortlistButton, {GuiPage::Transfers, GuiPage::News}},
        {state.buyButton, {GuiPage::Transfers}},
        {state.preContractButton, {GuiPage::Transfers}},
        {state.loanButton, {GuiPage::Transfers, GuiPage::Squad, GuiPage::Youth}},
        {state.renewButton, {GuiPage::Squad, GuiPage::Finances}},
        {state.sellButton, {GuiPage::Squad, GuiPage::Transfers}},
        {state.planButton, {GuiPage::Squad, GuiPage::Youth}},
        {state.instructionButton, {GuiPage::Tactics, GuiPage::Dashboard, GuiPage::Squad, GuiPage::Youth, GuiPage::Board, GuiPage::News}},
        {state.youthUpgradeButton, {GuiPage::Youth, GuiPage::Finances, GuiPage::Board}},
        {state.trainingUpgradeButton, {GuiPage::Tactics, GuiPage::Finances, GuiPage::Board}},
        {state.scoutingUpgradeButton, {GuiPage::Transfers, GuiPage::Finances, GuiPage::Board}},
        {state.stadiumUpgradeButton, {GuiPage::Finances, GuiPage::Board}}
    };

    bool hasCareer = state.career.myTeam != nullptr;
    const bool actionsEnabled = !state.actionInProgress;
    for (const auto& mapping : mappings) {
        bool visible = hasCareer && std::find(mapping.pages.begin(), mapping.pages.end(), state.currentPage) != mapping.pages.end();
        setControlVisibility(state, mapping.hwnd, visible);
        EnableWindow(mapping.hwnd, visible && actionsEnabled);
    }
}

void setMenuButtonsVisible(AppState& state, bool visible) {
    setControlVisibility(state, state.menuContinueButton, visible);
    setControlVisibility(state, state.menuPlayButton, visible);
    setControlVisibility(state, state.menuSettingsButton, visible);
    setControlVisibility(state, state.menuLoadButton, visible);
    setControlVisibility(state, state.menuDeleteSaveButton, visible);
    setControlVisibility(state, state.menuCreditsButton, visible);
    setControlVisibility(state, state.menuExitButton, visible);
    setControlVisibility(state, state.menuBackButton, visible);
    setControlVisibility(state, state.menuVolumeButton, visible);
    setControlVisibility(state, state.menuDifficultyButton, visible);
    setControlVisibility(state, state.menuSpeedButton, visible);
    setControlVisibility(state, state.menuSimulationButton, visible);
    setControlVisibility(state, state.menuLanguageButton, visible);
    setControlVisibility(state, state.menuTextSpeedButton, visible);
    setControlVisibility(state, state.menuVisualButton, visible);
    setControlVisibility(state, state.menuMusicModeButton, visible);
    setControlVisibility(state, state.menuAudioFadeButton, visible);
    setControlVisibility(state, state.menuApplySettingsButton, visible);
    setControlVisibility(state, state.menuResetSettingsButton, visible);
}

void updateFrontendMenuButtonLabels(AppState& state) {
    if (state.menuContinueButton) setWindowTextUtf8(state.menuContinueButton, "Continuar");
    if (state.menuPlayButton) setWindowTextUtf8(state.menuPlayButton, "Jugar");
    if (state.menuSettingsButton) setWindowTextUtf8(state.menuSettingsButton, "Opciones");
    if (state.menuLoadButton) setWindowTextUtf8(state.menuLoadButton, state.currentPage == GuiPage::Saves ? "Abrir seleccionado" : "Cargar partida");
    if (state.menuDeleteSaveButton) setWindowTextUtf8(state.menuDeleteSaveButton, state.currentPage == GuiPage::Saves ? "Borrar seleccionado" : "Borrar guardado");
    if (state.menuCreditsButton) setWindowTextUtf8(state.menuCreditsButton, "Creditos");
    if (state.menuExitButton) setWindowTextUtf8(state.menuExitButton, "Salir");
    if (state.menuBackButton) setWindowTextUtf8(state.menuBackButton, "Volver");
    if (state.menuVolumeButton) {
        setWindowTextUtf8(state.menuVolumeButton, "Volumen: " + game_settings::volumeLabel(state.settings.volume));
    }
    if (state.menuDifficultyButton) {
        setWindowTextUtf8(state.menuDifficultyButton,
                          "Dificultad: " + game_settings::difficultyLabel(state.settings.difficulty));
    }
    if (state.menuSpeedButton) {
        setWindowTextUtf8(state.menuSpeedButton,
                          "Velocidad: " + game_settings::simulationSpeedLabel(state.settings.simulationSpeed));
    }
    if (state.menuSimulationButton) {
        setWindowTextUtf8(state.menuSimulationButton,
                          "Simulacion: " + game_settings::simulationModeLabel(state.settings.simulationMode));
    }
    if (state.menuLanguageButton) {
        setWindowTextUtf8(state.menuLanguageButton,
                          "Idioma: " + game_settings::languageLabel(state.settings.language));
    }
    if (state.menuTextSpeedButton) {
        setWindowTextUtf8(state.menuTextSpeedButton,
                          "Texto: " + game_settings::textSpeedLabel(state.settings.textSpeed));
    }
    if (state.menuVisualButton) {
        setWindowTextUtf8(state.menuVisualButton,
                          "Visual: " + game_settings::visualProfileLabel(state.settings.visualProfile));
    }
    if (state.menuMusicModeButton) {
        setWindowTextUtf8(state.menuMusicModeButton,
                          "Musica: " + game_settings::menuMusicModeLabel(state.settings.menuMusicMode));
    }
    if (state.menuAudioFadeButton) {
        setWindowTextUtf8(state.menuAudioFadeButton,
                          "Audio: " + game_settings::menuAudioFadeLabel(state.settings.menuAudioFade));
    }
    if (state.menuApplySettingsButton) {
        setWindowTextUtf8(state.menuApplySettingsButton, state.settingsDirty ? "Aplicar ajustes *" : "Aplicado");
    }
    if (state.menuResetSettingsButton) setWindowTextUtf8(state.menuResetSettingsButton, "Restaurar");
}

}  // namespace

// Layout helper para el menú principal rediseñado: título grande y botones verticales centrados.
void layoutMainMenuPanel(AppState& state, const RECT& client) {
    const auto s = [&](int value) { return scaleByDpi(state, value); };

    // La portada utiliza dos zonas: acciones principales a la izquierda
    // e informacion contextual pintada directamente a la derecha.
    const int clientWidth = std::max(1, static_cast<int>(client.right - client.left));
    const int clientHeight = std::max(1, static_cast<int>(client.bottom - client.top));
    const int outerPadding = clientWidth < s(1180) ? s(30) : s(54);
    const int bottomReserve = s(70);
    const int availableWidth = std::max(s(720), clientWidth - outerPadding * 2);
    const int actionColumnWidth = std::min(s(540), std::max(s(390), availableWidth * 48 / 100));
    const int buttonWidth = std::min(s(470), actionColumnWidth);
    const int buttonHeight = clientHeight < s(760) ? s(58) : s(66);
    const int buttonGap = clientHeight < s(760) ? s(8) : s(11);

    const int contentLeft = outerPadding;
    const int actionLeft = contentLeft + std::max(0, (actionColumnWidth - buttonWidth) / 2);
    const int headerBottom = s(190);
    const int buttonsBlockHeight = 6 * buttonHeight + 5 * buttonGap;
    const int availableButtonArea = std::max(buttonsBlockHeight, clientHeight - headerBottom - bottomReserve);
    const int buttonsTop = headerBottom + std::max(0, (availableButtonArea - buttonsBlockHeight) / 2);

    // El titulo principal ahora se dibuja en paintWindowChrome para evitar
    // duplicarlo con la cabecera visual.
    setControlVisibility(state, state.pageTitleLabel, false);
    setControlVisibility(state, state.breadcrumbLabel, false);
    setControlVisibility(state, state.infoLabel, false);
    setControlVisibility(state, state.summaryLabel, false);
    setControlVisibility(state, state.summaryEdit, false);
    setControlVisibility(state, state.detailLabel, false);
    setControlVisibility(state, state.detailEdit, false);
    setControlVisibility(state, state.newsLabel, false);
    setControlVisibility(state, state.newsList, false);

    struct MainMenuButtonPlacement {
        HWND hwnd;
        const char* text;
    };

    const MainMenuButtonPlacement buttons[] = {
        {state.menuPlayButton, "Nueva carrera"},
        {state.menuContinueButton, "Continuar partida"},
        {state.menuLoadButton, "Cargar partida"},
        {state.menuSettingsButton, "Opciones"},
        {state.menuCreditsButton, "Creditos"},
        {state.menuExitButton, "Salir"}
    };

    for (int i = 0; i < static_cast<int>(sizeof(buttons) / sizeof(buttons[0])); ++i) {
        if (!buttons[i].hwnd) continue;
        setWindowTextUtf8(buttons[i].hwnd, buttons[i].text);
        MoveWindow(buttons[i].hwnd,
                   actionLeft,
                   buttonsTop + i * (buttonHeight + buttonGap),
                   buttonWidth,
                   buttonHeight,
                   TRUE);
        setControlVisibility(state, buttons[i].hwnd, true);
    }

    // Las configuraciones rápidas desaparecen de la portada, pero siguen
    // disponibles dentro de la pagina Opciones.
    setControlVisibility(state, state.menuDeleteSaveButton, false);
    setControlVisibility(state, state.menuBackButton, false);
    setControlVisibility(state, state.menuVolumeButton, false);
    setControlVisibility(state, state.menuDifficultyButton, false);
    setControlVisibility(state, state.menuSpeedButton, false);
    setControlVisibility(state, state.menuSimulationButton, false);
    setControlVisibility(state, state.menuLanguageButton, false);
    setControlVisibility(state, state.menuTextSpeedButton, false);
    setControlVisibility(state, state.menuVisualButton, false);
    setControlVisibility(state, state.menuMusicModeButton, false);
    setControlVisibility(state, state.menuAudioFadeButton, false);
    setControlVisibility(state, state.menuApplySettingsButton, false);
    setControlVisibility(state, state.menuResetSettingsButton, false);

    // La barra inferior conserva un estado breve y limpio.
    if (state.statusLabel) {
        MoveWindow(state.statusLabel,
                   outerPadding,
                   std::max(s(20), static_cast<int>(client.bottom) - bottomReserve + s(18)),
                   std::max(s(300), static_cast<int>(client.right) - outerPadding * 2),
                   s(24),
                   TRUE);
        setControlVisibility(state, state.statusLabel, true);
    }
}

// Layout helper para el dashboard del modo carrera: panel de gestión con botones en grilla e info del club.
void layoutCareerDashboardLegacy(AppState& state, const RECT& client) {
    const auto s = [&](int value) { return scaleByDpi(state, value); };
    const int padding = s(20);
    const int titleHeight = s(60);
    const int subtitleHeight = s(30);
    const int infoHeight = s(120);
    const int buttonWidth = s(180);
    const int buttonHeight = s(45);
    const int buttonGap = s(15);
    const int gridCols = 3;

    const int clientCenterX = (client.left + client.right) / 2;
    const int contentWidth = std::min(s(1000), static_cast<int>(client.right - padding * 2));
    const int contentLeft = clientCenterX - contentWidth / 2;
    const int contentTop = s(40);

    // Título principal
    setWindowTextUtf8(state.pageTitleLabel, "Panel de Carrera");
    setLabelFont(state.pageTitleLabel, state.heroFont);
    setControlVisibility(state, state.pageTitleLabel, true);
    MoveWindow(state.pageTitleLabel, contentLeft, contentTop, contentWidth, titleHeight, TRUE);

    // Subtítulo: nombre del club
    std::string clubName = "Sin club asignado";
    if (state.career.myTeam) {
        clubName = state.career.myTeam->name;
    }
    setWindowTextUtf8(state.infoLabel, ("Club: " + clubName).c_str());
    setLabelFont(state.infoLabel, state.titleFont);
    setControlVisibility(state, state.infoLabel, true);
    MoveWindow(state.infoLabel, contentLeft, contentTop + titleHeight + s(10), contentWidth, subtitleHeight, TRUE);

    // Panel de info resumida del club
    std::string infoText = "Información del club:\n\n";
    if (state.career.myTeam) {
        const Team& team = *state.career.myTeam;
        infoText += "División: " + team.division + "\n";
        infoText += "Presupuesto: " + formatMoneyValue(team.budget) + "\n";
        infoText += "Moral del equipo: " + std::to_string(team.morale) + "/100\n";
        infoText += "Próximo partido: " + findNextMatchLine(state.career) + "\n";
        infoText += "Objetivo: " + (state.career.boardMonthlyObjective.empty() ? "Sin objetivo mensual" : state.career.boardMonthlyObjective);
    } else {
        infoText += "División: Sin datos disponibles\n";
        infoText += "Presupuesto: Pendiente\n";
        infoText += "Moral del equipo: No asignado\n";
        infoText += "Próximo partido: Sin datos disponibles\n";
        infoText += "Objetivo: Pendiente";
    }
    setWindowTextUtf8(state.summaryEdit, infoText.c_str());
    setControlVisibility(state, state.summaryLabel, true);
    setControlVisibility(state, state.summaryEdit, true);
    setWindowTextUtf8(state.summaryLabel, "Resumen del Club");
    MoveWindow(state.summaryLabel, contentLeft, contentTop + titleHeight + subtitleHeight + s(20), contentWidth / 2 - s(10), s(24), TRUE);
    MoveWindow(state.summaryEdit, contentLeft, contentTop + titleHeight + subtitleHeight + s(50), contentWidth / 2 - s(10), infoHeight, TRUE);

    // Grilla de botones de navegación
    const int gridTop = contentTop + titleHeight + subtitleHeight + infoHeight + s(40);
    const int gridWidth = contentWidth;
    const int gridLeft = contentLeft;
    const int totalButtonWidth = buttonWidth * gridCols + buttonGap * (gridCols - 1);
    const int gridStartLeft = gridLeft + (gridWidth - totalButtonWidth) / 2;

    // Botones en orden: Club, Plantilla, Calendario, Mercado, Finanzas, Cantera, Scouting, Directiva, Guardar, Salir
    std::vector<std::pair<HWND, std::string>> dashboardButtons = {
        {state.leagueButton, "Club"},
        {state.squadButton, "Plantilla"},
        {state.calendarButton, "Calendario"},
        {state.transfersButton, "Mercado"},
        {state.financesButton, "Finanzas"},
        {state.youthButton, "Cantera"},
        {state.tacticsButton, "Scouting"}, // Reutilizando tacticsButton para Scouting
        {state.boardButton, "Directiva"},
        {state.saveButton, "Guardar partida"},
        {state.frontMenuButton, "Salir al menú principal"}
    };

    for (size_t i = 0; i < dashboardButtons.size(); ++i) {
        int row = static_cast<int>(i) / gridCols;
        int col = static_cast<int>(i) % gridCols;
        int x = gridStartLeft + col * (buttonWidth + buttonGap);
        int y = gridTop + row * (buttonHeight + buttonGap);
        MoveWindow(dashboardButtons[i].first, x, y, buttonWidth, buttonHeight, TRUE);
        setWindowTextUtf8(dashboardButtons[i].first, dashboardButtons[i].second.c_str());
        setControlVisibility(state, dashboardButtons[i].first, true);
    }

    // Ocultar elementos no usados
    setControlVisibility(state, state.breadcrumbLabel, false);
    setControlVisibility(state, state.detailLabel, false);
    setControlVisibility(state, state.detailEdit, false);
    setControlVisibility(state, state.newsLabel, false);
    setControlVisibility(state, state.newsList, false);
    setControlVisibility(state, state.tableLabel, false);
    setControlVisibility(state, state.tableList, false);
    setControlVisibility(state, state.squadLabel, false);
    setControlVisibility(state, state.squadList, false);
    setControlVisibility(state, state.transferLabel, false);
    setControlVisibility(state, state.transferList, false);
    setControlVisibility(state, state.dashboardButton, false); // Ya estamos en dashboard
    setControlVisibility(state, state.newsButton, false);
    setControlVisibility(state, state.simulateButton, false);
    setControlVisibility(state, state.validateButton, false);
    setControlVisibility(state, state.displayModeButton, false);
    setControlVisibility(state, state.newCareerButton, false);
    setControlVisibility(state, state.loadButton, false);
}

void layoutCareerDashboard(AppState& state, const RECT& client) {
    const auto s = [&](int value) { return scaleByDpi(state, value); };
    const int padding = s(24);
    const int gap = s(16);
    const int innerWidth = std::max(s(320), static_cast<int>(client.right) - padding * 2);
    const int contentWidth = std::min(innerWidth, clampValue(innerWidth, s(720), s(1500)));
    const int contentLeft = padding + std::max(0, (innerWidth - contentWidth) / 2);
    const int contentTop = s(32);
    const int titleHeight = s(40);
    const int infoHeight = s(24);
    const int buttonGap = s(12);
    const int buttonHeight = s(44);
    const int gridCols = contentWidth < s(620) ? 2 : (contentWidth < s(1040) ? 3 : 4);
    const int buttonWidth = std::max(s(140), (contentWidth - buttonGap * (gridCols - 1)) / gridCols);

    const std::array<HWND, 37> hiddenControls = {
        state.divisionLabel, state.teamLabel, state.managerLabel, state.managerHelpLabel,
        state.divisionCombo, state.teamCombo, state.managerEdit,
        state.filterLabel, state.filterCombo,
        state.newCareerButton, state.loadButton, state.validateButton, state.displayModeButton,
        state.breadcrumbLabel, state.newsLabel, state.newsList,
        state.scoutActionButton, state.shortlistButton, state.followShortlistButton, state.buyButton,
        state.preContractButton, state.loanButton, state.renewButton, state.sellButton, state.planButton,
        state.instructionButton, state.youthUpgradeButton, state.trainingUpgradeButton,
        state.scoutingUpgradeButton, state.stadiumUpgradeButton,
        state.menuContinueButton, state.menuPlayButton, state.menuLoadButton, state.menuBackButton,
        state.emptyNewButton, state.emptyLoadButton, state.emptyValidateButton
    };
    for (HWND hwnd : hiddenControls) setControlVisibility(state, hwnd, false);
    setMenuButtonsVisible(state, false);

    Team* team = state.career.myTeam;
    const std::string clubName = team ? team->name : std::string("Sin club");
    const std::string divisionName = team ? divisionDisplay(team->division) : std::string("Sin division");
    const std::string weekLabel = "Temp " + std::to_string(state.career.currentSeason) +
                                  " / Sem " + std::to_string(state.career.currentWeek);

    setLabelFont(state.pageTitleLabel, state.titleFont);
    setWindowTextUtf8(state.pageTitleLabel, clubName);
    setControlVisibility(state, state.pageTitleLabel, true);
    MoveWindow(state.pageTitleLabel, contentLeft, contentTop, contentWidth, titleHeight, TRUE);

    setLabelFont(state.infoLabel, state.font);
    setWindowTextUtf8(state.infoLabel,
                      divisionName + " | " + weekLabel +
                          " | Reputacion " + std::to_string(state.career.managerReputation));
    setControlVisibility(state, state.infoLabel, true);
    MoveWindow(state.infoLabel, contentLeft, contentTop + titleHeight + s(2), contentWidth, infoHeight, TRUE);

    const int buttonsTop = contentTop + titleHeight + infoHeight + s(14);
    std::vector<std::pair<HWND, std::string> > dashboardButtons = {
        {state.simulateButton, state.actionInProgress ? "Simulando..." : "Simular semana"},
        {state.squadButton, "Plantilla"},
        {state.tacticsButton, "Tacticas"},
        {state.calendarButton, "Calendario"},
        {state.leagueButton, "Liga"},
        {state.transfersButton, "Fichajes"},
        {state.financesButton, "Finanzas"},
        {state.youthButton, "Cantera"},
        {state.boardButton, "Directiva"},
        {state.newsButton, "Noticias"},
        {state.saveButton, "Guardar"},
        {state.frontMenuButton, "Menu principal"}
    };

    for (size_t i = 0; i < dashboardButtons.size(); ++i) {
        const int row = static_cast<int>(i) / gridCols;
        const int col = static_cast<int>(i) % gridCols;
        const int x = contentLeft + col * (buttonWidth + buttonGap);
        const int y = buttonsTop + row * (buttonHeight + buttonGap);
        MoveWindow(dashboardButtons[i].first, x, y, buttonWidth, buttonHeight, TRUE);
        setWindowTextUtf8(dashboardButtons[i].first, dashboardButtons[i].second);
        setControlVisibility(state, dashboardButtons[i].first, true);
        const int controlId = static_cast<int>(GetDlgCtrlID(dashboardButtons[i].first));
        const bool navigationAllowed = isPageButtonId(controlId);
        EnableWindow(dashboardButtons[i].first, !state.actionInProgress || navigationAllowed);
    }
    setControlVisibility(state, state.dashboardButton, false);

    const int buttonRows = static_cast<int>((dashboardButtons.size() + gridCols - 1) / gridCols);
    const int panelsTop = buttonsTop + buttonRows * buttonHeight + (buttonRows - 1) * buttonGap + s(22);
    const int statusTop = static_cast<int>(client.bottom) - s(kStatusHeight);
    const int availablePanelHeight = std::max(s(250), statusTop - panelsTop - s(14));
    const int topPanelHeight = clampValue(availablePanelHeight * 48 / 100, s(150), s(230));
    const int bottomPanelHeight = std::max(s(130), availablePanelHeight - topPanelHeight - gap);
    const int leftWidth = std::max(s(300), contentWidth * 36 / 100);
    const int rightWidth = contentWidth - leftWidth - gap;
    const int bottomColWidth = std::max(s(220), (contentWidth - gap * 2) / 3);

    std::string infoText = "Centro del club\r\n\r\n";
    if (team) {
        int playerCount = 0;
        int totalAge = 0;
        int totalSkill = 0;
        int totalFitness = 0;
        int totalForm = 0;
        int injured = 0;
        int suspended = 0;
        int expiringContracts = 0;
        long long weeklyWages = 0;
        for (const Player& player : team->players) {
            ++playerCount;
            totalAge += player.age;
            totalSkill += player.skill;
            totalFitness += player.fitness;
            totalForm += player.currentForm;
            weeklyWages += player.wage;
            if (player.injured) ++injured;
            if (player.matchesSuspended > 0) ++suspended;
            if (player.contractWeeks > 0 && player.contractWeeks <= 8) ++expiringContracts;
        }
        const int safeCount = std::max(1, playerCount);
        const int available = std::max(0, playerCount - injured - suspended);
        infoText += "Division: " + divisionDisplay(team->division) + "\r\n";
        infoText += "Presupuesto: " + formatMoneyValue(team->budget) + "\r\n";
        infoText += "Plantel: " + std::to_string(playerCount) +
                    " | Disponibles: " + std::to_string(available) + "/" + std::to_string(playerCount) + "\r\n";
        infoText += "Edad media: " + std::to_string(totalAge / safeCount) +
                    " | Skill medio: " + std::to_string(totalSkill / safeCount) + "\r\n";
        infoText += "Fisico: " + std::to_string(totalFitness / safeCount) +
                    " | Forma: " + std::to_string(totalForm / safeCount) +
                    " | Moral: " + std::to_string(team->morale) + "/100\r\n";
        infoText += "Lesionados: " + std::to_string(injured) +
                    " | Suspendidos: " + std::to_string(suspended) +
                    " | Contratos <=8s: " + std::to_string(expiringContracts) + "\r\n";
        infoText += "Salarios/sem: " + formatMoneyValue(weeklyWages) + "\r\n";
        infoText += "Puesto: " + std::to_string(state.career.currentCompetitiveRank()) + "/" +
                    std::to_string(state.career.currentCompetitiveFieldSize()) + "\r\n";
        infoText += "Proximo: " + findNextMatchLine(state.career) + "\r\n";
        infoText += "Objetivo: " + (state.career.boardMonthlyObjective.empty()
                                      ? std::string("Sin objetivo mensual")
                                      : state.career.boardMonthlyObjective);
    } else {
        infoText += "Division: Sin datos disponibles\r\n";
        infoText += "Presupuesto: Pendiente\r\n";
        infoText += "Moral: No asignado\r\n";
        infoText += "Proximo: Sin datos disponibles\r\n";
        infoText += "Objetivo: Pendiente";
    }
    setWindowTextUtf8(state.summaryLabel, "Resumen del club");
    setWindowTextUtf8(state.summaryEdit, infoText);
    setControlVisibility(state, state.summaryLabel, true);
    setControlVisibility(state, state.summaryEdit, true);
    MoveWindow(state.summaryLabel, contentLeft, panelsTop, leftWidth, s(kPanelLabelHeight), TRUE);
    MoveWindow(state.summaryEdit, contentLeft, panelsTop + s(kPanelBodyOffset), leftWidth, topPanelHeight, TRUE);

    setControlVisibility(state, state.transferLabel, true);
    setControlVisibility(state, state.transferList, true);
    MoveWindow(state.transferLabel, contentLeft + leftWidth + gap, panelsTop, rightWidth, s(kPanelLabelHeight), TRUE);
    MoveWindow(state.transferList, contentLeft + leftWidth + gap, panelsTop + s(kPanelBodyOffset), rightWidth, topPanelHeight, TRUE);

    const int bottomTop = panelsTop + s(kPanelBodyOffset) + topPanelHeight + gap;
    setControlVisibility(state, state.tableLabel, true);
    setControlVisibility(state, state.tableList, true);
    MoveWindow(state.tableLabel, contentLeft, bottomTop, bottomColWidth, s(kPanelLabelHeight), TRUE);
    MoveWindow(state.tableList, contentLeft, bottomTop + s(kPanelBodyOffset), bottomColWidth, bottomPanelHeight, TRUE);

    setControlVisibility(state, state.detailLabel, true);
    setControlVisibility(state, state.detailEdit, true);
    MoveWindow(state.detailLabel, contentLeft + bottomColWidth + gap, bottomTop, bottomColWidth, s(kPanelLabelHeight), TRUE);
    MoveWindow(state.detailEdit,
               contentLeft + bottomColWidth + gap,
               bottomTop + s(kPanelBodyOffset),
               bottomColWidth,
               bottomPanelHeight,
               TRUE);

    const int thirdLeft = contentLeft + (bottomColWidth + gap) * 2;
    const int thirdWidth = contentWidth - (bottomColWidth + gap) * 2;
    setControlVisibility(state, state.squadLabel, true);
    setControlVisibility(state, state.squadList, true);
    MoveWindow(state.squadLabel, thirdLeft, bottomTop, thirdWidth, s(kPanelLabelHeight), TRUE);
    MoveWindow(state.squadList, thirdLeft, bottomTop + s(kPanelBodyOffset), thirdWidth, bottomPanelHeight, TRUE);

    state.layout.statusBar = makeRect(padding, statusTop, std::max(0, static_cast<int>(client.right) - padding * 2), s(22));
    MoveWindow(state.statusLabel,
               state.layout.statusBar.left,
               state.layout.statusBar.top,
               rectWidth(state.layout.statusBar),
               s(20),
               TRUE);
    setControlVisibility(state, state.statusLabel, true);
}

void rebuildFonts(AppState& state) {
    if (state.font) DeleteObject(state.font);
    if (state.titleFont) DeleteObject(state.titleFont);
    if (state.heroFont) DeleteObject(state.heroFont);
    if (state.sectionFont) DeleteObject(state.sectionFont);
    if (state.monoFont) DeleteObject(state.monoFont);

    state.font = CreateFontW(-scaleByDpi(state, 15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Bahnschrift");
    state.titleFont = CreateFontW(-scaleByDpi(state, 24), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Bahnschrift SemiBold");
    state.heroFont = CreateFontW(-scaleByDpi(state, 42), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Bahnschrift SemiBold");
    state.sectionFont = CreateFontW(-scaleByDpi(state, 17), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Bahnschrift SemiBold");
    state.monoFont = CreateFontW(-scaleByDpi(state, 15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
}

void applyInterfaceFonts(AppState& state) {
    setLabelFont(state.pageTitleLabel, state.titleFont);
    setLabelFont(state.summaryLabel, state.sectionFont);
    setLabelFont(state.tableLabel, state.sectionFont);
    setLabelFont(state.squadLabel, state.sectionFont);
    setLabelFont(state.transferLabel, state.sectionFont);
    setLabelFont(state.detailLabel, state.sectionFont);
    setLabelFont(state.newsLabel, state.sectionFont);
    setLabelFont(state.breadcrumbLabel, state.font);
    setLabelFont(state.infoLabel, state.font);
    setLabelFont(state.statusLabel, state.font);
    setLabelFont(state.filterLabel, state.font);
    setLabelFont(state.divisionLabel, state.font);
    setLabelFont(state.teamLabel, state.font);
    setLabelFont(state.managerLabel, state.font);
    setLabelFont(state.managerHelpLabel, state.font);

    const std::array<HWND, 5> inputs = {
        state.divisionCombo, state.teamCombo, state.managerEdit, state.filterCombo, state.newsList
    };
    for (HWND hwnd : inputs) setControlFont(hwnd, state.font);

    const std::array<HWND, 2> textPanels = {state.summaryEdit, state.detailEdit};
    for (HWND hwnd : textPanels) setControlFont(hwnd, state.font);

    const std::array<HWND, 3> tablePanels = {state.tableList, state.squadList, state.transferList};
    for (HWND hwnd : tablePanels) setControlFont(hwnd, state.font);

    const std::array<HWND, 39> buttons = {
        state.newCareerButton, state.loadButton, state.saveButton, state.simulateButton, state.validateButton, state.displayModeButton, state.frontMenuButton,
        state.dashboardButton, state.squadButton, state.tacticsButton, state.calendarButton, state.leagueButton,
        state.transfersButton, state.financesButton, state.youthButton, state.boardButton, state.newsButton,
        state.menuContinueButton, state.menuPlayButton, state.menuSettingsButton, state.menuLoadButton,
        state.menuDeleteSaveButton, state.menuCreditsButton, state.menuExitButton, state.menuBackButton, state.menuVolumeButton,
        state.menuDifficultyButton, state.menuSpeedButton, state.menuSimulationButton,
        state.menuLanguageButton, state.menuTextSpeedButton, state.menuVisualButton,
        state.menuMusicModeButton, state.menuAudioFadeButton, state.menuApplySettingsButton, state.menuResetSettingsButton,
        state.emptyNewButton, state.emptyLoadButton, state.emptyValidateButton
    };
    for (HWND hwnd : buttons) setControlFont(hwnd, state.font);

    const std::array<HWND, 14> actionButtons = {
        state.scoutActionButton, state.shortlistButton, state.followShortlistButton, state.buyButton,
        state.preContractButton, state.loanButton, state.renewButton, state.sellButton, state.planButton, state.instructionButton,
        state.youthUpgradeButton, state.trainingUpgradeButton, state.scoutingUpgradeButton, state.stadiumUpgradeButton
    };
    for (HWND hwnd : actionButtons) setControlFont(hwnd, state.font);

    const int comboFieldHeight = scaleByDpi(state, 24);
    const int comboItemHeight = scaleByDpi(state, 28);
    const std::array<HWND, 3> combos = {state.divisionCombo, state.teamCombo, state.filterCombo};
    for (HWND combo : combos) {
        if (!combo) continue;
        SendMessageW(combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), comboFieldHeight);
        SendMessageW(combo, CB_SETITEMHEIGHT, 0, comboItemHeight);
    }

    if (state.newsList) {
        SendMessageW(state.newsList, LB_SETITEMHEIGHT, 0, scaleByDpi(state, 24));
    }
}

void hideSimulationProgressCoveredControls(AppState& state) {
    // The progress overlay is painted by the parent window, so child HWNDs must be hidden while it is active.
    const HWND coveredControls[] = {
        state.breadcrumbLabel, state.pageTitleLabel, state.infoLabel,
        state.filterLabel, state.filterCombo,
        state.summaryLabel, state.summaryEdit,
        state.tableLabel, state.tableList,
        state.squadLabel, state.squadList,
        state.transferLabel, state.transferList,
        state.detailLabel, state.detailEdit,
        state.newsLabel, state.newsList,
        state.emptyNewButton, state.emptyLoadButton, state.emptyValidateButton,
        state.scoutActionButton, state.shortlistButton, state.followShortlistButton,
        state.buyButton, state.preContractButton, state.loanButton, state.renewButton,
        state.sellButton, state.planButton, state.instructionButton,
        state.youthUpgradeButton, state.trainingUpgradeButton,
        state.scoutingUpgradeButton, state.stadiumUpgradeButton
    };
    for (HWND hwnd : coveredControls) {
        setControlVisibility(state, hwnd, false, 16, 16);
    }
}

void layoutWindow(AppState& state) {
    RECT client{};
    GetClientRect(state.window, &client);

    state.layout = {};
    state.layout.client = client;

    const auto s = [&](int value) { return scaleByDpi(state, value); };
    const HeaderLayoutProfile header = buildHeaderLayout(client);
    const PageLayoutProfile pageLayout = buildPageLayoutProfile(state.currentPage);
    const int padding = s(kWindowPadding);
    const int sideWidth = s(header.sideWidth);
    const int topBarHeight = s(header.topBarHeight);
    const int contentLeft = padding + sideWidth + s(kContentGap);
    int availableMainWidth = std::max(s(360), static_cast<int>(client.right - contentLeft - padding));
    int infoWidth = clampValue(s(pageLayout.preferredInfoWidth),
                               std::min(availableMainWidth, s(220)),
                               std::max(std::min(availableMainWidth, s(220)), availableMainWidth - s(320)));
    int contentWidth = availableMainWidth - infoWidth - s(kPanelGap);
    bool stackedMainColumns = contentWidth < s(kCenterColumnMinWidth);
    if (stackedMainColumns) {
        infoWidth = availableMainWidth;
        contentWidth = availableMainWidth;
    } else if (contentWidth < s(460)) {
        infoWidth = std::max(std::min(availableMainWidth, s(220)), infoWidth - (s(460) - contentWidth));
        contentWidth = availableMainWidth - infoWidth - s(kPanelGap);
    }
    const int infoLeft = stackedMainColumns ? contentLeft : contentLeft + contentWidth + s(kPanelGap);
    const bool dashboardLayout = state.currentPage == GuiPage::Dashboard;
    const bool dashboardEmptyState = dashboardLayout && !state.career.myTeam;
    const int summaryWidth = stackedMainColumns
        ? contentWidth
        : clampValue(contentWidth * pageLayout.summaryPercent / 100,
                     std::min(s(280), std::max(s(220), contentWidth - s(220) - s(kPanelGap))),
                     std::max(s(280), contentWidth - s(220) - s(kPanelGap)));
    const int tableWidth = stackedMainColumns ? contentWidth : std::max(s(220), contentWidth - summaryWidth - s(kPanelGap));
    const int topPanelHeight = s(pageLayout.topPanelHeight);
    const int midPanelHeight = s(pageLayout.midPanelHeight);
    const bool frontMenuPage = isFrontMenuPage(state.currentPage);
    state.layout.frontMenuPage = frontMenuPage;
    state.layout.dashboardPage = dashboardLayout;
    state.layout.dashboardEmptyState = dashboardEmptyState;

    const std::array<HWND, 5> topControls = {
        state.divisionCombo, state.teamCombo, state.managerEdit, state.filterCombo, state.displayModeButton
    };
    const std::array<HWND, 5> topLabels = {
        state.divisionLabel, state.teamLabel, state.managerLabel, state.filterLabel, state.managerHelpLabel
    };
    const std::array<HWND, 10> navButtons = {
        state.dashboardButton, state.squadButton, state.tacticsButton, state.calendarButton, state.leagueButton,
        state.transfersButton, state.financesButton, state.youthButton, state.boardButton, state.newsButton
    };
    const std::array<HWND, 5> headerButtons = {
        state.newCareerButton, state.loadButton, state.saveButton, state.simulateButton, state.frontMenuButton
    };
    int maxContentBottom = 0;
    int scrollViewportTop = 0;
    int scrollViewportBottom = static_cast<int>(client.bottom);
    RECT scrollClipViewport{0, 0, client.right, client.bottom};
    auto recordBottom = [&](int y, int height) {
        maxContentBottom = std::max(maxContentBottom, y + height);
    };
    auto placeWindowWithMode = [&](HWND hwnd, int x, int y, int width, int height, bool scrollable) {
        if (!hwnd) return;
        const int targetY = scrollable ? (y - state.pageScrollY) : y;
        moveControlAndInvalidate(state, hwnd, x, targetY, width, height);
        applyControlViewportClip(state, hwnd, scrollable ? &scrollClipViewport : nullptr);
        if (scrollable && IsWindowVisible(hwnd)) recordBottom(y, height);
    };
    auto placeFixedWindow = [&](HWND hwnd, int x, int y, int width, int height) {
        placeWindowWithMode(hwnd, x, y, width, height, false);
    };
    auto placeScrollableWindow = [&](HWND hwnd, int x, int y, int width, int height) {
        placeWindowWithMode(hwnd, x, y, width, height, true);
    };
    auto syncScrollState = [&]() -> bool {
        scrollViewportBottom = std::max(scrollViewportTop + 1, scrollViewportBottom);
        const int viewportHeight = std::max(1, scrollViewportBottom - scrollViewportTop);
        const int contentBottom = std::max(scrollViewportBottom, maxContentBottom + s(6));
        const int contentHeight = std::max(viewportHeight, contentBottom - scrollViewportTop);
        const int maxScroll = std::max(0, contentBottom - scrollViewportBottom);
        const int clamped = clampValue(state.pageScrollY, 0, maxScroll);
        state.pageContentHeight = contentHeight;
        state.maxPageScrollY = maxScroll;

        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin = 0;
        info.nMax = std::max(0, contentHeight - 1);
        info.nPage = viewportHeight;
        info.nPos = clamped;
        SetScrollInfo(state.window, SB_VERT, &info, TRUE);
        ShowScrollBar(state.window, SB_VERT, maxScroll > 0 ? TRUE : FALSE);

        if (clamped != state.pageScrollY) {
            state.pageScrollY = clamped;
            layoutWindow(state);
            return true;
        }
        return false;
    };

    if (frontMenuPage) {
        for (HWND hwnd : topControls) setControlVisibility(state, hwnd, false);
        for (HWND hwnd : topLabels) setControlVisibility(state, hwnd, false);
        for (HWND hwnd : navButtons) setControlVisibility(state, hwnd, false);
        for (HWND hwnd : headerButtons) setControlVisibility(state, hwnd, false);
        setControlVisibility(state, state.validateButton, false);
        hideControlAndInvalidate(state, state.breadcrumbLabel);
        hideControlAndInvalidate(state, state.pageTitleLabel);
        hideControlAndInvalidate(state, state.infoLabel);

        showActionButtonsForPage(state);
        setMenuButtonsVisible(state, true);
        updateFrontendMenuButtonLabels(state);
        setControlVisibility(state, state.emptyNewButton, false);
        setControlVisibility(state, state.emptyLoadButton, false);
        setControlVisibility(state, state.emptyValidateButton, false);

        setControlVisibility(state, state.tableLabel, false);
        setControlVisibility(state, state.tableList, false);
        setControlVisibility(state, state.squadLabel, false);
        setControlVisibility(state, state.squadList, false);
        setControlVisibility(state, state.transferLabel, false);
        setControlVisibility(state, state.transferList, false);

        const int shellLeft = padding + s(30);
        const int shellTop = padding + s(26);
        const int shellWidth = std::max(s(820), static_cast<int>(client.right - shellLeft * 2));
        const bool compactFrontMenu = state.settings.visualProfile == VisualProfile::Compact ||
                                      client.right < s(1380) || client.bottom < s(920);
        const bool ultraWideFrontMenu = client.right > s(2140);
        const int buttonTop = shellTop + s(214);
        const int controlBlockHeight = state.currentPage == GuiPage::MainMenu ? s(compactFrontMenu ? 108 : 92)
                                                                              : s(state.currentPage == GuiPage::Settings ? 288 : 64);
        const int panelsTop = buttonTop + controlBlockHeight + s(28);
        const int leftWidth = std::max(s(compactFrontMenu ? 320 : 360), shellWidth * (ultraWideFrontMenu ? 48 : 52) / 100);
        const int rightWidth = shellWidth - leftWidth - s(18);
        const int summaryHeight = std::max(s(230), static_cast<int>(client.bottom - panelsTop - s(100)));
        const int detailHeight = std::max(s(170), (summaryHeight - s(40)) / 2);
        scrollViewportTop = panelsTop;
        scrollViewportBottom = client.bottom - s(kStatusHeight) - s(8);
        scrollClipViewport = RECT{shellLeft + s(8), scrollViewportTop, shellLeft + shellWidth - s(8), scrollViewportBottom};

        placeScrollableWindow(state.summaryLabel, shellLeft + s(16), panelsTop, leftWidth, s(kPanelLabelHeight));
        placeScrollableWindow(state.summaryEdit, shellLeft + s(16), panelsTop + s(kPanelBodyOffset), leftWidth, summaryHeight);
        placeScrollableWindow(state.detailLabel, shellLeft + s(34) + leftWidth, panelsTop, rightWidth, s(kPanelLabelHeight));
        placeScrollableWindow(state.detailEdit, shellLeft + s(34) + leftWidth, panelsTop + s(kPanelBodyOffset), rightWidth, detailHeight);
        const int feedTop = panelsTop + detailHeight + s(44);
        placeScrollableWindow(state.newsLabel, shellLeft + s(34) + leftWidth, feedTop, rightWidth, s(kPanelLabelHeight));
        placeScrollableWindow(state.newsList, shellLeft + s(34) + leftWidth, feedTop + s(kPanelBodyOffset), rightWidth, summaryHeight - detailHeight - s(18));

        if (state.currentPage == GuiPage::MainMenu) {
            // Usar UI principal limpia del menú de inicio.
            layoutMainMenuPanel(state, client);
        } else if (state.currentPage == GuiPage::Saves) {
            const int saveButtonWidth = clampValue((shellWidth - s(56)) / 3, s(190), s(320));
            placeFixedWindow(state.menuLoadButton, shellLeft + s(16), buttonTop, saveButtonWidth, s(40));
            placeFixedWindow(state.menuDeleteSaveButton, shellLeft + s(28) + saveButtonWidth, buttonTop, saveButtonWidth, s(40));
            placeFixedWindow(state.menuBackButton, shellLeft + s(40) + saveButtonWidth * 2, buttonTop, saveButtonWidth, s(40));
            setControlVisibility(state, state.menuContinueButton, false);
            setControlVisibility(state, state.menuPlayButton, false);
            setControlVisibility(state, state.menuSettingsButton, false);
            setControlVisibility(state, state.menuLoadButton, true);
            setControlVisibility(state, state.menuDeleteSaveButton, true);
            setControlVisibility(state, state.menuCreditsButton, false);
            setControlVisibility(state, state.menuExitButton, false);
            setControlVisibility(state, state.menuBackButton, true);
            setControlVisibility(state, state.menuVolumeButton, false);
            setControlVisibility(state, state.menuDifficultyButton, false);
            setControlVisibility(state, state.menuSpeedButton, false);
            setControlVisibility(state, state.menuSimulationButton, false);
            setControlVisibility(state, state.menuLanguageButton, false);
            setControlVisibility(state, state.menuTextSpeedButton, false);
            setControlVisibility(state, state.menuVisualButton, false);
            setControlVisibility(state, state.menuMusicModeButton, false);
            setControlVisibility(state, state.menuAudioFadeButton, false);
            setControlVisibility(state, state.menuApplySettingsButton, false);
            setControlVisibility(state, state.menuResetSettingsButton, false);
        } else if (state.currentPage == GuiPage::Settings) {
            const int settingsWidth = clampValue((shellWidth - s(44)) / 2, s(280), s(420));
            const int rightColumnLeft = shellLeft + s(28) + settingsWidth;
            placeFixedWindow(state.menuVolumeButton, shellLeft + s(16), buttonTop, settingsWidth, s(38));
            placeFixedWindow(state.menuDifficultyButton, rightColumnLeft, buttonTop, settingsWidth, s(38));
            placeFixedWindow(state.menuSpeedButton, shellLeft + s(16), buttonTop + s(46), settingsWidth, s(38));
            placeFixedWindow(state.menuSimulationButton, rightColumnLeft, buttonTop + s(46), settingsWidth, s(38));
            placeFixedWindow(state.menuLanguageButton, shellLeft + s(16), buttonTop + s(92), settingsWidth, s(38));
            placeFixedWindow(state.menuTextSpeedButton, rightColumnLeft, buttonTop + s(92), settingsWidth, s(38));
            placeFixedWindow(state.menuVisualButton, shellLeft + s(16), buttonTop + s(138), settingsWidth, s(38));
            placeFixedWindow(state.menuMusicModeButton, rightColumnLeft, buttonTop + s(138), settingsWidth, s(38));
            placeFixedWindow(state.menuAudioFadeButton, shellLeft + s(16), buttonTop + s(184), settingsWidth, s(38));
            placeFixedWindow(state.menuApplySettingsButton, rightColumnLeft, buttonTop + s(184), settingsWidth, s(38));
            placeFixedWindow(state.menuResetSettingsButton, shellLeft + s(16), buttonTop + s(230), settingsWidth, s(36));
            placeFixedWindow(state.menuBackButton, rightColumnLeft, buttonTop + s(230), settingsWidth, s(36));
            setControlVisibility(state, state.menuContinueButton, false);
            setControlVisibility(state, state.menuPlayButton, false);
            setControlVisibility(state, state.menuSettingsButton, false);
            setControlVisibility(state, state.menuLoadButton, false);
            setControlVisibility(state, state.menuDeleteSaveButton, false);
            setControlVisibility(state, state.menuCreditsButton, false);
            setControlVisibility(state, state.menuExitButton, false);
            setControlVisibility(state, state.menuBackButton, true);
            setControlVisibility(state, state.menuVolumeButton, true);
            setControlVisibility(state, state.menuDifficultyButton, true);
            setControlVisibility(state, state.menuSpeedButton, true);
            setControlVisibility(state, state.menuSimulationButton, true);
            setControlVisibility(state, state.menuLanguageButton, true);
            setControlVisibility(state, state.menuTextSpeedButton, true);
            setControlVisibility(state, state.menuVisualButton, true);
            setControlVisibility(state, state.menuMusicModeButton, true);
            setControlVisibility(state, state.menuAudioFadeButton, true);
            setControlVisibility(state, state.menuApplySettingsButton, true);
            setControlVisibility(state, state.menuResetSettingsButton, true);
        } else {
            placeFixedWindow(state.menuBackButton, shellLeft + s(16), buttonTop, s(220), s(38));
            setControlVisibility(state, state.menuContinueButton, false);
            setControlVisibility(state, state.menuPlayButton, false);
            setControlVisibility(state, state.menuSettingsButton, false);
            setControlVisibility(state, state.menuLoadButton, false);
            setControlVisibility(state, state.menuDeleteSaveButton, false);
            setControlVisibility(state, state.menuCreditsButton, false);
            setControlVisibility(state, state.menuExitButton, false);
            setControlVisibility(state, state.menuBackButton, true);
            setControlVisibility(state, state.menuVolumeButton, false);
            setControlVisibility(state, state.menuDifficultyButton, false);
            setControlVisibility(state, state.menuSpeedButton, false);
            setControlVisibility(state, state.menuSimulationButton, false);
            setControlVisibility(state, state.menuLanguageButton, false);
            setControlVisibility(state, state.menuTextSpeedButton, false);
            setControlVisibility(state, state.menuVisualButton, false);
            setControlVisibility(state, state.menuMusicModeButton, false);
            setControlVisibility(state, state.menuAudioFadeButton, false);
            setControlVisibility(state, state.menuApplySettingsButton, false);
            setControlVisibility(state, state.menuResetSettingsButton, false);
        }

        state.layout.statusBar = makeRect(padding,
                                          static_cast<int>(client.bottom) - s(kStatusHeight),
                                          std::max(0, static_cast<int>(client.right) - padding * 2),
                                          s(22));
        placeFixedWindow(state.statusLabel,
                         state.layout.statusBar.left,
                         state.layout.statusBar.top,
                         rectWidth(state.layout.statusBar),
                         s(20));
        applyEditInteriorPadding(state, state.summaryEdit, 10, 8);
        applyEditInteriorPadding(state, state.detailEdit, 10, 8);
        applyEditInteriorPadding(state, state.managerEdit, 8, 0);
        if (syncScrollState()) return;
        return;
    }

    if (dashboardLayout && state.career.myTeam) {
        layoutCareerDashboard(state, client);
        applyEditInteriorPadding(state, state.summaryEdit, 10, 8);
        applyEditInteriorPadding(state, state.detailEdit, 10, 8);
        if (state.simulationProgressActive) hideSimulationProgressCoveredControls(state);
        if (syncScrollState()) return;
        return;
    }

    for (HWND hwnd : topControls) setControlVisibility(state, hwnd, true);
    for (HWND hwnd : topLabels) setControlVisibility(state, hwnd, true);
    for (HWND hwnd : navButtons) setControlVisibility(state, hwnd, true);
    for (HWND hwnd : headerButtons) setControlVisibility(state, hwnd, true);
    setControlVisibility(state, state.validateButton, true);
    setControlVisibility(state, state.breadcrumbLabel, true);
    setControlVisibility(state, state.pageTitleLabel, true);
    setControlVisibility(state, state.infoLabel, true);
    setMenuButtonsVisible(state, false);

    const int primaryButtonHeight = s(kHeaderButtonHeight);
    const int buttonGap = s(10);
    int buttonRight = client.right - padding;
    int buttonTop = s(header.buttonsTop);
    int topRowLeftmostButton = client.right - padding;
    auto placeHeaderButton = [&](HWND hwnd, int width) {
        if (!header.shareTopRow && buttonRight - width < padding) {
            buttonRight = client.right - padding;
            buttonTop += primaryButtonHeight + s(kHeaderRowGap);
        }
        buttonRight -= width;
        placeFixedWindow(hwnd, buttonRight, buttonTop, width, primaryButtonHeight);
        if (buttonTop == s(header.buttonsTop)) topRowLeftmostButton = std::min(topRowLeftmostButton, buttonRight);
        buttonRight -= buttonGap;
    };
    placeHeaderButton(state.validateButton, s(94));
    placeHeaderButton(state.displayModeButton, s(154));
    placeHeaderButton(state.frontMenuButton, s(138));
    placeHeaderButton(state.simulateButton, s(126));
    placeHeaderButton(state.saveButton, s(112));
    placeHeaderButton(state.loadButton, s(112));
    placeHeaderButton(state.newCareerButton, s(156));
    const int headerRightLimit = header.shareTopRow ? topRowLeftmostButton - s(12) : client.right - padding;

    const int fieldTop = s(16);
    const int fieldHeight = s(kHeaderFieldHeight);
    const int hintHeight = s(kHeaderHintHeight);
    const int blockGap = s(20);
    const int rowTwoTop = fieldTop + fieldHeight + s(kHeaderRowGap);
    const int rowThreeTop = rowTwoTop + fieldHeight + s(kHeaderRowGap);
    const int controlsLeft = s(20);
    const int controlsAvailableWidth = std::max(s(320), headerRightLimit - controlsLeft);
    const int maxControlRowWidth = header.stackedControls ? s(520) : (header.splitControls ? s(820) : s(1120));
    const int headerAvailableWidth = std::max(s(320), std::min(maxControlRowWidth, controlsAvailableWidth));
    int managerFieldLeft = s(20);
    int managerFieldTop = fieldTop;
    int managerFieldWidth = s(240);

    auto placeFieldBlock = [&](HWND label, int labelWidth, HWND field, int inputOffset, int x, int y, int width, bool comboField) {
        int fieldWidth = std::max(s(150), width - inputOffset);
        placeFixedWindow(label, x, y + s(4), labelWidth, s(22));
        RECT fieldRect = makeRect(x + inputOffset, y, fieldWidth, fieldHeight);
        if (comboField) {
            RECT comboRect = controlRectForCombo(fieldRect, s(kComboPopupHeight));
            placeFixedWindow(field, comboRect.left, comboRect.top, rectWidth(comboRect), rectHeight(comboRect));
        } else {
            placeFixedWindow(field, fieldRect.left, fieldRect.top, rectWidth(fieldRect), rectHeight(fieldRect));
        }
        if (field == state.managerEdit) {
            managerFieldLeft = fieldRect.left;
            managerFieldTop = fieldRect.top;
            managerFieldWidth = fieldWidth;
        }
    };

    if (header.stackedControls) {
        placeFieldBlock(state.divisionLabel, s(78), state.divisionCombo, s(90), s(20), fieldTop, headerAvailableWidth, true);
        placeFieldBlock(state.teamLabel, s(46), state.teamCombo, s(56), s(20), rowTwoTop, headerAvailableWidth, true);
        placeFieldBlock(state.managerLabel, s(78), state.managerEdit, s(86), s(20), rowThreeTop, std::min(s(430), headerAvailableWidth), false);
    } else if (header.splitControls) {
        int rowOneWidth = headerAvailableWidth - blockGap;
        int divisionBlockWidth = std::max(s(250), rowOneWidth * 40 / 100);
        int teamBlockWidth = rowOneWidth - divisionBlockWidth;
        placeFieldBlock(state.divisionLabel, s(78), state.divisionCombo, s(90), s(20), fieldTop, divisionBlockWidth, true);
        placeFieldBlock(state.teamLabel, s(46), state.teamCombo, s(56), s(20) + divisionBlockWidth + blockGap, fieldTop, teamBlockWidth, true);
        placeFieldBlock(state.managerLabel, s(78), state.managerEdit, s(86), s(20), rowTwoTop, std::min(s(440), headerAvailableWidth), false);
    } else {
        int rowOneWidth = headerAvailableWidth - blockGap * 2;
        int divisionBlockWidth = clampValue(rowOneWidth * 28 / 100, s(300), s(360));
        int managerBlockWidth = clampValue(rowOneWidth * 32 / 100, s(380), s(460));
        int teamBlockWidth = rowOneWidth - divisionBlockWidth - managerBlockWidth;
        int divisionX = s(20);
        int teamX = divisionX + divisionBlockWidth + blockGap;
        int managerX = teamX + teamBlockWidth + blockGap;
        placeFieldBlock(state.divisionLabel, s(78), state.divisionCombo, s(90), divisionX, fieldTop, divisionBlockWidth, true);
        placeFieldBlock(state.teamLabel, s(46), state.teamCombo, s(56), teamX, fieldTop, teamBlockWidth, true);
        placeFieldBlock(state.managerLabel, s(78), state.managerEdit, s(86), managerX, fieldTop, managerBlockWidth, false);
    }
    placeFixedWindow(state.managerHelpLabel,
                managerFieldLeft,
                managerFieldTop + fieldHeight + s(3),
                managerFieldWidth,
                hintHeight);
    applyEditInteriorPadding(state, state.managerEdit, 8, 0);

    state.layout.topBar = makeRect(0, 0, static_cast<int>(client.right), topBarHeight);
    state.layout.statusBar = makeRect(padding,
                                      static_cast<int>(client.bottom) - s(kStatusHeight),
                                      std::max(0, static_cast<int>(client.right) - padding * 2),
                                      s(22));
    const int chromeBottom = std::max(topBarHeight + s(180), static_cast<int>(state.layout.statusBar.top) - s(10));
    state.layout.sideMenu = makeRect(padding, topBarHeight, sideWidth, chromeBottom - topBarHeight);
    state.layout.sideMenuTitle = makeRect(state.layout.sideMenu.left + s(12),
                                          state.layout.sideMenu.top + s(10),
                                          std::max(0, rectWidth(state.layout.sideMenu) - s(24)),
                                          s(24));
    state.layout.contentShell = makeRect(state.layout.sideMenu.right + s(kContentGap),
                                         topBarHeight,
                                         std::max(0, static_cast<int>(client.right) - (static_cast<int>(state.layout.sideMenu.right) + s(kContentGap)) - padding),
                                         chromeBottom - topBarHeight);
    state.layout.shellInner = shrinkRect(state.layout.contentShell, s(kShellPadding), s(kShellPadding));

    const int sideX = state.layout.sideMenu.left;
    int navY = state.layout.sideMenuTitle.bottom + s(12);
    int navButtonHeight = s(40);
    int navGap = s(8);
    const int navAvailableHeight = std::max(s(220), static_cast<int>(state.layout.sideMenu.bottom) - navY - s(12));
    if (!navButtons.empty()) {
        const int minButtonHeight = s(32);
        navGap = std::max(s(4),
                          std::min(navGap,
                                   std::max(0, navAvailableHeight - static_cast<int>(navButtons.size()) * minButtonHeight) /
                                       std::max(1, static_cast<int>(navButtons.size()) - 1)));
        navButtonHeight = std::max(minButtonHeight,
                                   (navAvailableHeight - navGap * std::max(0, static_cast<int>(navButtons.size()) - 1)) /
                                       std::max(1, static_cast<int>(navButtons.size())));
    }
    std::array<HWND, 10> pages = {
        state.dashboardButton, state.squadButton, state.tacticsButton, state.calendarButton, state.leagueButton,
        state.transfersButton, state.financesButton, state.youthButton, state.boardButton, state.newsButton
    };
    for (HWND button : pages) {
        placeFixedWindow(button, sideX, navY, rectWidth(state.layout.sideMenu), navButtonHeight);
        navY += navButtonHeight + navGap;
    }

    RectCursor shellCursor{state.layout.shellInner};
    const bool filterVisible = state.filterCombo && IsWindowVisible(state.filterCombo);
    const bool stackHeaderFilter = filterVisible && rectWidth(state.layout.shellInner) < s(1040);
    const int pageHeaderHeight = s(kPageHeaderHeight) + (filterVisible && stackHeaderFilter ? s(kHeaderFieldHeight + 18) : 0);
    state.layout.pageHeader = takeTop(shellCursor, pageHeaderHeight, s(kPageSectionGap));
    state.layout.headerTextArea = state.layout.pageHeader;
    if (filterVisible) {
        if (stackHeaderFilter) {
            const int filterAreaHeight = s(kHeaderFieldHeight + 10);
            state.layout.headerFilterArea = makeRect(state.layout.pageHeader.left,
                                                     state.layout.pageHeader.bottom - filterAreaHeight,
                                                     rectWidth(state.layout.pageHeader),
                                                     filterAreaHeight);
            state.layout.headerTextArea.bottom = std::max(state.layout.headerTextArea.top,
                                                          state.layout.headerFilterArea.top - s(8));
        } else {
            RectCursor headerCursor{state.layout.pageHeader};
            const int minFilterWidth = std::min(rectWidth(state.layout.pageHeader), s(250));
            const int preferredFilterWidth = std::max(minFilterWidth, rectWidth(state.layout.pageHeader) / 3);
            const int maxFilterWidth = std::max(minFilterWidth, rectWidth(state.layout.pageHeader) - s(320));
            const int filterWidth = clampValue(preferredFilterWidth, minFilterWidth, maxFilterWidth);
            state.layout.headerFilterArea = takeRight(headerCursor, filterWidth, s(kPanelGap));
            state.layout.headerTextArea = headerCursor.remaining;
        }
    }

    RectCursor headerTextCursor{state.layout.headerTextArea};
    state.layout.breadcrumb = takeTop(headerTextCursor, s(kBreadcrumbHeight), s(4));
    state.layout.pageTitle = takeTop(headerTextCursor, s(kPageTitleHeight), s(6));
    state.layout.infoLine = takeTop(headerTextCursor, s(kPageInfoHeight), 0);
    placeFixedWindow(state.breadcrumbLabel,
                     state.layout.breadcrumb.left,
                     state.layout.breadcrumb.top,
                     rectWidth(state.layout.breadcrumb),
                     rectHeight(state.layout.breadcrumb));
    placeFixedWindow(state.pageTitleLabel,
                     state.layout.pageTitle.left,
                     state.layout.pageTitle.top,
                     rectWidth(state.layout.pageTitle),
                     rectHeight(state.layout.pageTitle));
    placeFixedWindow(state.infoLabel,
                     state.layout.infoLine.left,
                     state.layout.infoLine.top,
                     rectWidth(state.layout.infoLine),
                     rectHeight(state.layout.infoLine));
    if (filterVisible) {
        const int filterLabelWidth = s(56);
        const int filterTop = state.layout.headerFilterArea.top +
                              std::max(0, (rectHeight(state.layout.headerFilterArea) - fieldHeight) / 2);
        state.layout.filterLabel = makeRect(state.layout.headerFilterArea.left,
                                            filterTop + s(4),
                                            filterLabelWidth,
                                            s(20));
        RECT filterFieldRect = makeRect(state.layout.headerFilterArea.left + filterLabelWidth + s(10),
                                        filterTop,
                                        std::max(s(140), rectWidth(state.layout.headerFilterArea) - filterLabelWidth - s(10)),
                                        fieldHeight);
        state.layout.filterField = controlRectForCombo(filterFieldRect, s(kComboPopupHeight));
        placeFixedWindow(state.filterLabel,
                         state.layout.filterLabel.left,
                         state.layout.filterLabel.top,
                         rectWidth(state.layout.filterLabel),
                         rectHeight(state.layout.filterLabel));
        placeFixedWindow(state.filterCombo,
                         state.layout.filterField.left,
                         state.layout.filterField.top,
                         rectWidth(state.layout.filterField),
                         rectHeight(state.layout.filterField));
    }

    showActionButtonsForPage(state);
    std::vector<ActionButtonRef> visibleButtons = {
        {state.scoutActionButton, 92}, {state.shortlistButton, 92}, {state.followShortlistButton, 98},
        {state.buyButton, 92}, {state.preContractButton, 102}, {state.loanButton, 96}, {state.renewButton, 92},
        {state.sellButton, 92}, {state.planButton, 92}, {state.instructionButton, 112},
        {state.youthUpgradeButton, 94}, {state.trainingUpgradeButton, 96},
        {state.scoutingUpgradeButton, 94}, {state.stadiumUpgradeButton, 96}
    };

    std::vector<std::vector<ActionButtonRef> > actionRows;
    std::vector<ActionButtonRef> currentActionRow;
    const int actionAvailableWidth = std::max(s(140), rectWidth(shellCursor.remaining));
    const int actionGap = s(10);
    const int actionRowGap = s(kActionRowGap);
    const int actionButtonHeight = s(kActionButtonHeight);
    int currentActionWidth = 0;
    for (const auto& action : visibleButtons) {
        if (!action.hwnd || !IsWindowVisible(action.hwnd)) continue;
        const int width = s(action.width);
        const int nextWidth = currentActionRow.empty() ? width : currentActionWidth + actionGap + width;
        if (!currentActionRow.empty() && nextWidth > actionAvailableWidth) {
            actionRows.push_back(currentActionRow);
            currentActionRow.clear();
            currentActionWidth = 0;
        }
        currentActionRow.push_back({action.hwnd, width});
        currentActionWidth = currentActionRow.size() == 1 ? width : currentActionWidth + actionGap + width;
    }
    if (!currentActionRow.empty()) actionRows.push_back(currentActionRow);

    if (!actionRows.empty()) {
        const int actionStripHeight = static_cast<int>(actionRows.size()) * actionButtonHeight +
                                      std::max(0, static_cast<int>(actionRows.size()) - 1) * actionRowGap;
        state.layout.actionStrip = takeTop(shellCursor, actionStripHeight, s(kPageSectionGap));
        int rowTop = state.layout.actionStrip.top;
        for (const auto& row : actionRows) {
            int x = state.layout.actionStrip.left;
            for (const auto& action : row) {
                placeFixedWindow(action.hwnd, x, rowTop, action.width, actionButtonHeight);
                x += action.width + actionGap;
            }
            rowTop += actionButtonHeight + actionRowGap;
        }
    }

    state.layout.scrollViewport = shellCursor.remaining;
    state.layout.mainArea = state.layout.scrollViewport;
    scrollViewportTop = state.layout.scrollViewport.top;
    scrollViewportBottom = state.layout.scrollViewport.bottom;
    scrollClipViewport = state.layout.scrollViewport;

    const bool contextualInsightStrip = pageUsesInsightStrip(state) && state.currentPage != GuiPage::Dashboard;
    const int dashboardSpotlightReserve = dashboardLayout
        ? s((client.right - client.left) < s(1380) ? kDashboardSpotlightCompactReserve : kDashboardSpotlightWideReserve)
        : 0;
    const int contextualInsightReserve = contextualInsightStrip
        ? s((client.right - client.left) < s(1380) ? kInsightStripCompactReserve : kInsightStripWideReserve)
        : 0;
    int panelsTop = scrollViewportTop;
    const int insightReserve = dashboardSpotlightReserve + contextualInsightReserve;
    if (insightReserve > 0) {
        RECT spotlightDoc{contentLeft,
                          panelsTop,
                          contentLeft + availableMainWidth,
                          panelsTop + insightReserve};
        state.layout.spotlightBand = viewportRect(state, spotlightDoc, true);
        panelsTop = spotlightDoc.bottom + s(kPanelGap);
    }
    const int footerHeight = std::max(s(pageLayout.footerMinHeight), s(150));
    auto placeScrollablePanel = [&](PanelBounds& snapshotPanel, HWND label, HWND body, const RECT& docOuter) {
        const bool labelVisible = label && IsWindowVisible(label);
        const int titleHeight = labelVisible ? s(kPanelTitleHeight) : 0;
        const int titleGap = labelVisible ? s(kPanelHeaderGap) : 0;
        PanelBounds docPanel = buildPanelBounds(docOuter, titleHeight, s(kPanelContentPadding), titleGap);
        snapshotPanel = projectPanelBounds(state, docPanel, true);
        if (label) {
            placeScrollableWindow(label,
                                  docPanel.title.left,
                                  docPanel.title.top,
                                  rectWidth(docPanel.title),
                                  rectHeight(docPanel.title));
        }
        if (body) {
            placeScrollableWindow(body,
                                  docPanel.body.left,
                                  docPanel.body.top,
                                  rectWidth(docPanel.body),
                                  rectHeight(docPanel.body));
        }
        recordBottom(docOuter.top, rectHeight(docOuter));
        return docPanel;
    };

    if (dashboardEmptyState) {
        const int consumedBySetupStrip = std::max(0, panelsTop - scrollViewportTop);
        const int visibleSetupHeight = rectHeight(state.layout.scrollViewport) - consumedBySetupStrip - s(18);
        const int summaryHeight = std::max(s(380), visibleSetupHeight);
        const int summaryWidth = stackedMainColumns ? availableMainWidth : contentWidth;
        const int rightColumnWidth = stackedMainColumns ? availableMainWidth : infoWidth;
        const int rightColumnX = stackedMainColumns ? contentLeft : infoLeft;
        const int rightColumnTop = stackedMainColumns ? panelsTop + summaryHeight + s(kPanelGap) : panelsTop;
        const int sideHeight = std::max(s(170), (summaryHeight - s(34)) / 2);
        RECT summaryDoc{contentLeft, panelsTop, contentLeft + summaryWidth, panelsTop + summaryHeight};
        PanelBounds summaryDocPanel = placeScrollablePanel(state.layout.summaryPanel, state.summaryLabel, nullptr, summaryDoc);
        RECT summaryEditDoc = summaryDocPanel.body;
        placeScrollableWindow(state.summaryEdit,
                              summaryEditDoc.left,
                              summaryEditDoc.top,
                              rectWidth(summaryEditDoc),
                              rectHeight(summaryEditDoc));

        RECT detailDoc{rightColumnX, rightColumnTop, rightColumnX + rightColumnWidth, rightColumnTop + sideHeight};
        PanelBounds detailDocPanel = placeScrollablePanel(state.layout.detailPanel, state.detailLabel, state.detailEdit, detailDoc);
        const int newsTop = detailDoc.bottom + s(kPanelGap);
        RECT newsDoc{rightColumnX,
                     newsTop,
                     rightColumnX + rightColumnWidth,
                     newsTop + std::max(s(188), summaryHeight - sideHeight - s(28))};
        PanelBounds newsDocPanel = placeScrollablePanel(state.layout.newsPanel, state.newsLabel, state.newsList, newsDoc);
        (void)detailDocPanel;
        (void)newsDocPanel;

        setControlVisibility(state, state.emptyNewButton, false);
        setControlVisibility(state, state.emptyLoadButton, false);
        setControlVisibility(state, state.emptyValidateButton, false);

        state.layout.centerColumn = projectPanelBounds(state, buildPanelBounds(summaryDoc, 0, 0, 0), true).outer;
        RECT rightColumnDoc{rightColumnX,
                            rightColumnTop,
                            rightColumnX + rightColumnWidth,
                            newsDoc.bottom};
        state.layout.rightColumn = viewportRect(state, rightColumnDoc, true);

        placeFixedWindow(state.statusLabel,
                         state.layout.statusBar.left,
                         state.layout.statusBar.top,
                         rectWidth(state.layout.statusBar),
                         s(20));
        applyEditInteriorPadding(state, state.summaryEdit, 10, 8);
        applyEditInteriorPadding(state, state.detailEdit, 10, 8);
        if (state.simulationProgressActive) hideSimulationProgressCoveredControls(state);
        if (syncScrollState()) return;
        return;
    }

    setControlVisibility(state, state.emptyNewButton, false);
    setControlVisibility(state, state.emptyLoadButton, false);
    setControlVisibility(state, state.emptyValidateButton, false);

    const bool stackTopPanels = stackedMainColumns || contentWidth < s(pageLayout.summaryMinWidth + 280 + kPanelGap);
    RECT summaryDoc{};
    RECT primaryDoc{};
    if (stackTopPanels) {
        summaryDoc = RECT{contentLeft, panelsTop, contentLeft + contentWidth, panelsTop + topPanelHeight};
        primaryDoc = RECT{contentLeft, summaryDoc.bottom + s(kPanelGap), contentLeft + contentWidth, summaryDoc.bottom + s(kPanelGap) + topPanelHeight};
    } else {
        summaryDoc = RECT{contentLeft, panelsTop, contentLeft + summaryWidth, panelsTop + topPanelHeight};
        primaryDoc = RECT{summaryDoc.right + s(kPanelGap), panelsTop, summaryDoc.right + s(kPanelGap) + tableWidth, panelsTop + topPanelHeight};
    }

    PanelBounds summaryDocPanel = placeScrollablePanel(state.layout.summaryPanel, state.summaryLabel, state.summaryEdit, summaryDoc);
    PanelBounds primaryDocPanel = placeScrollablePanel(state.layout.primaryPanel, state.tableLabel, state.tableList, primaryDoc);
    (void)summaryDocPanel;
    (void)primaryDocPanel;

    const int secondTop = primaryDoc.bottom + s(kPanelGap);
    RECT secondaryDoc{contentLeft, secondTop, contentLeft + contentWidth, secondTop + midPanelHeight};
    PanelBounds secondaryDocPanel = placeScrollablePanel(state.layout.secondaryPanel, state.squadLabel, state.squadList, secondaryDoc);
    (void)secondaryDocPanel;

    const int footerTop = secondaryDoc.bottom + s(kPanelGap);
    RECT footerDoc{contentLeft, footerTop, contentLeft + contentWidth, footerTop + footerHeight};
    PanelBounds footerDocPanel = placeScrollablePanel(state.layout.footerPanel, state.transferLabel, state.transferList, footerDoc);
    (void)footerDocPanel;

    const int rightColumnX = stackedMainColumns ? contentLeft : infoLeft;
    const int rightColumnWidth = stackedMainColumns ? availableMainWidth : infoWidth;
    const int rightStartTop = stackedMainColumns ? footerDoc.bottom + s(kPanelGap) : (dashboardLayout ? secondTop : panelsTop);
    RECT detailDoc{rightColumnX,
                   rightStartTop,
                   rightColumnX + rightColumnWidth,
                   rightStartTop + (dashboardLayout ? s(pageLayout.dashboardDetailHeight)
                                                    : topPanelHeight + s(pageLayout.nonDashboardDetailExtra))};
    PanelBounds detailDocPanel = placeScrollablePanel(state.layout.detailPanel, state.detailLabel, state.detailEdit, detailDoc);
    (void)detailDocPanel;
    const int newsHeight = std::max(s(210),
                                    dashboardLayout ? midPanelHeight : topPanelHeight);
    RECT newsDoc{rightColumnX,
                 detailDoc.bottom + s(kPanelGap),
                 rightColumnX + rightColumnWidth,
                 detailDoc.bottom + s(kPanelGap) + newsHeight};
    PanelBounds newsDocPanel = placeScrollablePanel(state.layout.newsPanel, state.newsLabel, state.newsList, newsDoc);
    (void)newsDocPanel;

    state.layout.centerColumn = viewportRect(state,
                                             RECT{contentLeft, panelsTop, contentLeft + contentWidth, footerDoc.bottom},
                                             true);
    state.layout.rightColumn = viewportRect(state,
                                            RECT{rightColumnX, rightStartTop, rightColumnX + rightColumnWidth, newsDoc.bottom},
                                            true);
    const int contextCardBottomLimit = rightStartTop - s(kPanelGap);
    const int contextCardHeight = std::min(s(kContextCardHeight),
                                           std::max(0, contextCardBottomLimit - panelsTop));
    if (currentContextTeam(state) && !stackedMainColumns && contextCardHeight >= s(96)) {
        RECT contextDoc{rightColumnX,
                        panelsTop,
                        rightColumnX + rightColumnWidth,
                        panelsTop + contextCardHeight};
        state.layout.contextCard = viewportRect(state, contextDoc, true);
    }

    placeFixedWindow(state.statusLabel,
                     state.layout.statusBar.left,
                     state.layout.statusBar.top,
                     rectWidth(state.layout.statusBar),
                     s(20));
    applyEditInteriorPadding(state, state.summaryEdit, 10, 8);
    applyEditInteriorPadding(state, state.detailEdit, 10, 8);
    if (state.simulationProgressActive) hideSimulationProgressCoveredControls(state);
    if (syncScrollState()) return;
}

void initializeInterface(AppState& state) {
    rebuildFonts(state);
    state.backgroundBrush = CreateSolidBrush(kThemeBg);
    state.panelBrush = CreateSolidBrush(kThemePanel);
    state.headerBrush = CreateSolidBrush(kThemeHeader);
    state.topBarBrush = CreateSolidBrush(kThemeTopBarPanel);
    state.shellBrush = CreateSolidBrush(kThemeShell);
    state.inputBrush = CreateSolidBrush(kThemeInput);

    const DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW;

    state.divisionLabel = createControl(state, 0, L"STATIC", L"Division", WS_CHILD | WS_VISIBLE, 18, 18, 82, 20, state.window, 0);
    state.teamLabel = createControl(state, 0, L"STATIC", L"Club", WS_CHILD | WS_VISIBLE, 354, 18, 72, 20, state.window, 0);
    state.managerLabel = createControl(state, 0, L"STATIC", L"Manager", WS_CHILD | WS_VISIBLE, 702, 18, 82, 20, state.window, 0);
    state.managerHelpLabel = createControl(state,
                                           0,
                                           L"STATIC",
                                           L"Completa el nombre del manager.",
                                           WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS,
                                           676,
                                           44,
                                           220,
                                           18,
                                           state.window,
                                           0);
    state.filterLabel = createControl(state, 0, L"STATIC", L"Filtro", WS_CHILD | WS_VISIBLE, 0, 0, 50, 20, state.window, 0);

    state.divisionCombo = createControl(state, 0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 92, 14, 184, 300, state.window, IDC_DIVISION_COMBO);
    state.teamCombo = createControl(state, 0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 360, 14, 224, 300, state.window, IDC_TEAM_COMBO);
    state.managerEdit = createControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 676, 14, 188, 24, state.window, IDC_MANAGER_EDIT);
    state.filterCombo = createControl(state, 0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 180, 260, state.window, IDC_FILTER_COMBO);
    SendMessageW(state.managerEdit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Ingresa nombre del manager"));
    SendMessageW(state.managerEdit, EM_LIMITTEXT, 48, 0);

    state.newCareerButton = createControl(state, 0, L"BUTTON", L"Nueva carrera", buttonStyle, 0, 0, 100, 28, state.window, IDC_NEW_CAREER_BUTTON);
    state.loadButton = createControl(state, 0, L"BUTTON", L"Cargar", buttonStyle, 0, 0, 86, 28, state.window, IDC_LOAD_BUTTON);
    state.saveButton = createControl(state, 0, L"BUTTON", L"Guardar", buttonStyle, 0, 0, 86, 28, state.window, IDC_SAVE_BUTTON);
    state.simulateButton = createControl(state, 0, L"BUTTON", L"Simular", buttonStyle, 0, 0, 126, 28, state.window, IDC_SIMULATE_BUTTON);
    state.validateButton = createControl(state, 0, L"BUTTON", L"Auditar", buttonStyle, 0, 0, 92, 28, state.window, IDC_VALIDATE_BUTTON);
    state.displayModeButton = createControl(state, 0, L"BUTTON", L"Pantalla F11", buttonStyle, 0, 0, 154, 28, state.window, IDC_DISPLAY_MODE_BUTTON);
    state.frontMenuButton = createControl(state, 0, L"BUTTON", L"Menu principal", buttonStyle, 0, 0, 138, 28, state.window, IDC_FRONT_MENU_BUTTON);
    state.menuContinueButton = createControl(state, 0, L"BUTTON", L"Continuar", buttonStyle, 0, 0, 180, 36, state.window, IDC_MENU_CONTINUE_BUTTON);
    state.menuPlayButton = createControl(state, 0, L"BUTTON", L"Jugar", buttonStyle, 0, 0, 180, 36, state.window, IDC_MENU_PLAY_BUTTON);
    state.menuSettingsButton = createControl(state, 0, L"BUTTON", L"Configuraciones", buttonStyle, 0, 0, 180, 36, state.window, IDC_MENU_SETTINGS_BUTTON);
    state.menuLoadButton = createControl(state, 0, L"BUTTON", L"Cargar guardado", buttonStyle, 0, 0, 180, 36, state.window, IDC_MENU_LOAD_BUTTON);
    state.menuDeleteSaveButton = createControl(state, 0, L"BUTTON", L"Borrar guardado", buttonStyle, 0, 0, 180, 36, state.window, IDC_MENU_DELETE_SAVE_BUTTON);
    state.menuCreditsButton = createControl(state, 0, L"BUTTON", L"Creditos", buttonStyle, 0, 0, 180, 36, state.window, IDC_MENU_CREDITS_BUTTON);
    state.menuExitButton = createControl(state, 0, L"BUTTON", L"Salir", buttonStyle, 0, 0, 180, 36, state.window, IDC_MENU_EXIT_BUTTON);
    state.menuBackButton = createControl(state, 0, L"BUTTON", L"Volver", buttonStyle, 0, 0, 140, 34, state.window, IDC_MENU_BACK_BUTTON);
    state.menuVolumeButton = createControl(state, 0, L"BUTTON", L"Volumen: 70%", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_VOLUME_BUTTON);
    state.menuDifficultyButton = createControl(state, 0, L"BUTTON", L"Dificultad: Normal", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_DIFFICULTY_BUTTON);
    state.menuSpeedButton = createControl(state, 0, L"BUTTON", L"Velocidad: Normal", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_SPEED_BUTTON);
    state.menuSimulationButton = createControl(state, 0, L"BUTTON", L"Simulacion: Detallado", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_SIMULATION_BUTTON);
    state.menuLanguageButton = createControl(state, 0, L"BUTTON", L"Idioma: Espanol", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_LANGUAGE_BUTTON);
    state.menuTextSpeedButton = createControl(state, 0, L"BUTTON", L"Texto: Normal", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_TEXT_SPEED_BUTTON);
    state.menuVisualButton = createControl(state, 0, L"BUTTON", L"Visual: Editorial", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_VISUAL_BUTTON);
    state.menuMusicModeButton = createControl(state, 0, L"BUTTON", L"Musica: Solo portada", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_MUSICMODE_BUTTON);
    state.menuAudioFadeButton = createControl(state, 0, L"BUTTON", L"Audio: Fade activo", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_AUDIOFADE_BUTTON);
    state.menuApplySettingsButton = createControl(state, 0, L"BUTTON", L"Aplicar ajustes", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_APPLY_SETTINGS_BUTTON);
    state.menuResetSettingsButton = createControl(state, 0, L"BUTTON", L"Restaurar", buttonStyle, 0, 0, 280, 34, state.window, IDC_MENU_RESET_SETTINGS_BUTTON);
    state.emptyNewButton = createControl(state, 0, L"BUTTON", L"Crear carrera", buttonStyle, 0, 0, 140, 30, state.window, IDC_EMPTY_NEW_BUTTON);
    state.emptyLoadButton = createControl(state, 0, L"BUTTON", L"Abrir guardado", buttonStyle, 0, 0, 140, 30, state.window, IDC_EMPTY_LOAD_BUTTON);
    state.emptyValidateButton = createControl(state, 0, L"BUTTON", L"Validar datos", buttonStyle, 0, 0, 140, 30, state.window, IDC_EMPTY_VALIDATE_BUTTON);
    setControlVisibility(state, state.emptyNewButton, false);
    setControlVisibility(state, state.emptyLoadButton, false);
    setControlVisibility(state, state.emptyValidateButton, false);
    ShowWindow(state.menuContinueButton, SW_HIDE);
    ShowWindow(state.menuBackButton, SW_HIDE);
    ShowWindow(state.menuLoadButton, SW_HIDE);
    ShowWindow(state.menuDeleteSaveButton, SW_HIDE);
    ShowWindow(state.menuCreditsButton, SW_HIDE);
    ShowWindow(state.menuExitButton, SW_HIDE);
    ShowWindow(state.menuVolumeButton, SW_HIDE);
    ShowWindow(state.menuDifficultyButton, SW_HIDE);
    ShowWindow(state.menuSpeedButton, SW_HIDE);
    ShowWindow(state.menuSimulationButton, SW_HIDE);
    ShowWindow(state.menuLanguageButton, SW_HIDE);
    ShowWindow(state.menuTextSpeedButton, SW_HIDE);
    ShowWindow(state.menuVisualButton, SW_HIDE);
    ShowWindow(state.menuMusicModeButton, SW_HIDE);
    ShowWindow(state.menuAudioFadeButton, SW_HIDE);
    ShowWindow(state.menuApplySettingsButton, SW_HIDE);
    ShowWindow(state.menuResetSettingsButton, SW_HIDE);

    state.dashboardButton = createControl(state, 0, L"BUTTON", L"Inicio", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_DASHBOARD_BUTTON);
    state.squadButton = createControl(state, 0, L"BUTTON", L"Plantilla", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_SQUAD_BUTTON);
    state.tacticsButton = createControl(state, 0, L"BUTTON", L"Tacticas", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_TACTICS_BUTTON);
    state.calendarButton = createControl(state, 0, L"BUTTON", L"Calendario", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_CALENDAR_BUTTON);
    state.leagueButton = createControl(state, 0, L"BUTTON", L"Liga", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_LEAGUE_BUTTON);
    state.transfersButton = createControl(state, 0, L"BUTTON", L"Fichajes", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_TRANSFERS_BUTTON);
    state.financesButton = createControl(state, 0, L"BUTTON", L"Finanzas", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_FINANCES_BUTTON);
    state.youthButton = createControl(state, 0, L"BUTTON", L"Cantera", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_YOUTH_BUTTON);
    state.boardButton = createControl(state, 0, L"BUTTON", L"Directiva", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_BOARD_BUTTON);
    state.newsButton = createControl(state, 0, L"BUTTON", L"Noticias", buttonStyle, 0, 0, 132, 34, state.window, IDC_PAGE_NEWS_BUTTON);

    state.scoutActionButton = createControl(state, 0, L"BUTTON", L"Otear", buttonStyle, 0, 0, 92, 26, state.window, IDC_SCOUT_BUTTON);
    state.shortlistButton = createControl(state, 0, L"BUTTON", L"Shortlist", buttonStyle, 0, 0, 92, 26, state.window, IDC_SHORTLIST_BUTTON);
    state.followShortlistButton = createControl(state, 0, L"BUTTON", L"Actualizar", buttonStyle, 0, 0, 98, 26, state.window, IDC_FOLLOW_SHORTLIST_BUTTON);
    state.buyButton = createControl(state, 0, L"BUTTON", L"Comprar", buttonStyle, 0, 0, 92, 26, state.window, IDC_BUY_BUTTON);
    state.preContractButton = createControl(state, 0, L"BUTTON", L"Precontrato", buttonStyle, 0, 0, 102, 26, state.window, IDC_PRECONTRACT_BUTTON);
    state.loanButton = createControl(state, 0, L"BUTTON", L"Cesion", buttonStyle, 0, 0, 96, 26, state.window, IDC_LOAN_BUTTON);
    state.renewButton = createControl(state, 0, L"BUTTON", L"Renovar", buttonStyle, 0, 0, 92, 26, state.window, IDC_RENEW_BUTTON);
    state.sellButton = createControl(state, 0, L"BUTTON", L"Vender", buttonStyle, 0, 0, 92, 26, state.window, IDC_SELL_BUTTON);
    state.planButton = createControl(state, 0, L"BUTTON", L"Plan", buttonStyle, 0, 0, 92, 26, state.window, IDC_PLAN_BUTTON);
    state.instructionButton = createControl(state, 0, L"BUTTON", L"Instruccion", buttonStyle, 0, 0, 112, 26, state.window, IDC_INSTRUCTION_BUTTON);
    state.youthUpgradeButton = createControl(state, 0, L"BUTTON", L"Cantera+", buttonStyle, 0, 0, 94, 26, state.window, IDC_YOUTH_UPGRADE_BUTTON);
    state.trainingUpgradeButton = createControl(state, 0, L"BUTTON", L"Entreno+", buttonStyle, 0, 0, 96, 26, state.window, IDC_TRAINING_UPGRADE_BUTTON);
    state.scoutingUpgradeButton = createControl(state, 0, L"BUTTON", L"Scout+", buttonStyle, 0, 0, 94, 26, state.window, IDC_SCOUTING_UPGRADE_BUTTON);
    state.stadiumUpgradeButton = createControl(state, 0, L"BUTTON", L"Estadio+", buttonStyle, 0, 0, 96, 26, state.window, IDC_STADIUM_UPGRADE_BUTTON);

    state.breadcrumbLabel = createControl(state, 0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 320, 18, state.window, 0);
    state.pageTitleLabel = createControl(state, 0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 360, 24, state.window, 0);
    state.infoLabel = createControl(state, 0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 400, 22, state.window, 0);
    state.summaryLabel = createControl(state, 0, L"STATIC", L"Proximo partido", WS_CHILD | WS_VISIBLE, 0, 0, 240, 18, state.window, 0);
    state.summaryEdit = createControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 300, 180, state.window, IDC_SUMMARY_EDIT);
    state.tableLabel = createControl(state, 0, L"STATIC", L"Tabla de liga", WS_CHILD | WS_VISIBLE, 0, 0, 240, 18, state.window, 0);
    state.tableList = createControl(state, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, 0, 0, 300, 180, state.window, IDC_TABLE_LIST);
    state.squadLabel = createControl(state, 0, L"STATIC", L"Estado del equipo", WS_CHILD | WS_VISIBLE, 0, 0, 240, 18, state.window, 0);
    state.squadList = createControl(state, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, 0, 0, 300, 180, state.window, IDC_SQUAD_LIST);
    state.transferLabel = createControl(state, 0, L"STATIC", L"Lesiones", WS_CHILD | WS_VISIBLE, 0, 0, 260, 18, state.window, 0);
    state.transferList = createControl(state, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, 0, 0, 300, 180, state.window, IDC_TRANSFER_LIST);
    state.detailLabel = createControl(state, 0, L"STATIC", L"Ultimo resultado", WS_CHILD | WS_VISIBLE, 0, 0, 220, 18, state.window, 0);
    state.detailEdit = createControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 280, 240, state.window, IDC_DETAIL_EDIT);
    state.newsLabel = createControl(state, 0, L"STATIC", L"Noticias", WS_CHILD | WS_VISIBLE, 0, 0, 240, 18, state.window, 0);
    state.newsList = createControl(state, WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY, 0, 0, 280, 220, state.window, IDC_NEWS_LIST);
    state.statusLabel = createControl(state, 0, L"STATIC", L"Interfaz lista.", WS_CHILD | WS_VISIBLE, 0, 0, 420, 18, state.window, 0);

    applyInterfaceFonts(state);

    DWORD styles = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES;
    ListView_SetExtendedListViewStyle(state.tableList, styles);
    ListView_SetExtendedListViewStyle(state.squadList, styles);
    ListView_SetExtendedListViewStyle(state.transferList, styles);
    ListView_SetBkColor(state.tableList, kThemeInput);
    ListView_SetBkColor(state.squadList, kThemeInput);
    ListView_SetBkColor(state.transferList, kThemeInput);
    ListView_SetTextBkColor(state.tableList, kThemeInput);
    ListView_SetTextBkColor(state.squadList, kThemeInput);
    ListView_SetTextBkColor(state.transferList, kThemeInput);
    ListView_SetTextColor(state.tableList, kThemeText);
    ListView_SetTextColor(state.squadList, kThemeText);
    ListView_SetTextColor(state.transferList, kThemeText);
    rebuildTeamLogoImageList(state);
    state.career.initializeLeague();
    fillDivisionCombo(state, state.gameSetup.division);
    fillTeamCombo(state, state.gameSetup.division, state.gameSetup.club);
    setCurrentPage(state, GuiPage::MainMenu);
    layoutWindow(state);
    refreshAll(state);
    setStatus(state, "Todo listo. Crea una carrera o continua tu ultima partida.");
    if (state.menuPlayButton) SetFocus(state.menuPlayButton);
}

void drawSimulationProgressOverlay(AppState& state, HDC hdc, const RECT& client) {
    if (!state.simulationProgressActive) return;

    const auto s = [&](int value) { return scaleByDpi(state, value); };
    const int clientWidth = static_cast<int>(client.right - client.left);
    const int clientHeight = static_cast<int>(client.bottom - client.top);
    const int panelWidth = clampValue(clientWidth - s(160), s(420), s(760));
    const int panelHeight = s(214);
    const int panelLeft = client.left + (clientWidth - panelWidth) / 2;
    const int panelTop = client.top + std::max(s(92), (clientHeight - panelHeight) / 2 - s(72));
    RECT panel{panelLeft, panelTop, panelLeft + panelWidth, panelTop + panelHeight};

    drawRoundedPanel(hdc, panel, RGB(10, 24, 32), RGB(77, 130, 166), s(18));

    RECT titleRect{panel.left + s(24), panel.top + s(16), panel.right - s(24), panel.top + s(42)};
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(242, 247, 249));
    HGDIOBJ oldFont = SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
    DrawTextW(hdc, L"Simulando semana", -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT phaseRect{panel.left + s(24), panel.top + s(48), panel.right - s(24), panel.top + s(74)};
    const std::string phase = state.simulationProgressPhase.empty() ? "Procesando" : state.simulationProgressPhase;
    const std::wstring phaseText = utf8ToWide(phase + " | " + std::to_string(clampValue(state.simulationProgressPercent, 0, 100)) + "%");
    SetTextColor(hdc, kThemeAccent);
    DrawTextW(hdc, phaseText.c_str(), -1, &phaseRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT detailRect{panel.left + s(24), panel.top + s(74), panel.right - s(24), panel.top + s(100)};
    SetTextColor(hdc, RGB(195, 213, 224));
    const std::wstring detailText = utf8ToWide(state.simulationProgressDetail.empty()
                                                  ? std::string("Actualizando el mundo de la carrera.")
                                                  : state.simulationProgressDetail);
    DrawTextW(hdc, detailText.c_str(), -1, &detailRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT barBg{panel.left + s(24), panel.top + s(108), panel.right - s(24), panel.top + s(122)};
    drawRoundedPanel(hdc, barBg, RGB(16, 35, 45), RGB(45, 76, 94), s(7));
    RECT barFill = barBg;
    barFill.right = barFill.left + std::max(s(8), rectWidth(barBg) * clampValue(state.simulationProgressPercent, 0, 100) / 100);
    HBRUSH fillBrush = CreateSolidBrush(kThemeAccentGreen);
    FillRect(hdc, &barFill, fillBrush);
    DeleteObject(fillBrush);

    const std::array<std::pair<const wchar_t*, int>, 4> steps = {{
        {L"Autosave", 10},
        {L"Partidos", 35},
        {L"Tabla", 75},
        {L"Final", 100}
    }};
    const int chipGap = s(8);
    const int chipTop = panel.top + s(128);
    const int chipWidth = (panelWidth - s(48) - chipGap * 3) / 4;
    for (size_t i = 0; i < steps.size(); ++i) {
        RECT chip{
            panel.left + s(24) + static_cast<int>(i) * (chipWidth + chipGap),
            chipTop,
            panel.left + s(24) + static_cast<int>(i) * (chipWidth + chipGap) + chipWidth,
            chipTop + s(18)
        };
        const bool reached = state.simulationProgressPercent >= steps[i].second;
        drawRoundedPanel(hdc,
                         chip,
                         reached ? RGB(25, 78, 58) : RGB(18, 36, 46),
                         reached ? kThemeAccentGreen : RGB(49, 74, 88),
                         s(9));
        RECT chipText = chip;
        chipText.left += s(8);
        chipText.right -= s(8);
        SetTextColor(hdc, reached ? RGB(232, 247, 239) : kThemeMuted);
        DrawTextW(hdc, steps[i].first, -1, &chipText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    RECT eventsTitle{panel.left + s(24), panel.top + s(154), panel.right - s(24), panel.top + s(176)};
    SetTextColor(hdc, kThemeAccent);
    DrawTextW(hdc, L"Eventos recientes", -1, &eventsTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SetTextColor(hdc, RGB(205, 220, 228));
    const int eventLineHeight = s(18);
    const int firstEventTop = panel.top + s(178);
    if (state.simulationProgressEvents.empty()) {
        RECT eventRect{panel.left + s(24), firstEventTop, panel.right - s(24), firstEventTop + eventLineHeight};
        DrawTextW(hdc,
                  L"Esperando eventos de la simulacion...",
                  -1,
                  &eventRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        const size_t maxVisibleEvents = std::min<size_t>(state.simulationProgressEvents.size(), 2);
        const size_t firstEvent = state.simulationProgressEvents.size() - maxVisibleEvents;
        for (size_t i = 0; i < maxVisibleEvents; ++i) {
            RECT eventRect{
                panel.left + s(24),
                firstEventTop + static_cast<int>(i) * eventLineHeight,
                panel.right - s(24),
                firstEventTop + static_cast<int>(i + 1) * eventLineHeight
            };
            const std::wstring eventText = utf8ToWide("- " + state.simulationProgressEvents[firstEvent + i]);
            DrawTextW(hdc, eventText.c_str(), -1, &eventRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    SelectObject(hdc, oldFont);
}

void paintWindowChrome(AppState& state, HDC hdc) {
    RECT client{};
    GetClientRect(state.window, &client);
    const auto s = [&](int value) { return scaleByDpi(state, value); };
    FillRect(hdc, &client, state.backgroundBrush ? state.backgroundBrush : static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    state.insightHotspots.clear();

    if (state.currentPage == GuiPage::MainMenu) {
        const bool highContrast = state.settings.visualProfile == VisualProfile::HighContrast;

        const COLORREF bgDeep = highContrast ? RGB(4, 12, 18) : RGB(5, 14, 21);
        const COLORREF bgMid = highContrast ? RGB(7, 25, 34) : RGB(7, 22, 31);
        const COLORREF panelFill = highContrast ? RGB(9, 30, 39) : RGB(10, 27, 36);
        const COLORREF panelBorder = highContrast ? RGB(94, 151, 177) : RGB(38, 71, 87);

        HBRUSH deepBrush = CreateSolidBrush(bgDeep);
        FillRect(hdc, &client, deepBrush);
        DeleteObject(deepBrush);
        drawMainMenuBackground(state, hdc, client);

        RECT topShade = client;
        topShade.bottom = client.top + std::max(s(220), rectHeight(client) * 38 / 100);
        HBRUSH midBrush = CreateSolidBrush(bgMid);
        FrameRect(hdc, &topShade, midBrush);
        DeleteObject(midBrush);

        RECT shell{s(26), s(24), client.right - s(26), client.bottom - s(42)};
        drawRoundedPanel(hdc, shell, RGB(7, 19, 27), RGB(28, 56, 70), s(26));

        const int headerHeight = rectHeight(client) < s(760) ? s(148) : s(166);
        RECT header{shell.left + s(14), shell.top + s(14), shell.right - s(14), shell.top + headerHeight};
        drawRoundedPanel(hdc, header, panelFill, panelBorder, s(20));

        RECT accent{header.left + s(28), header.top + s(22), header.left + s(138), header.top + s(28)};
        HBRUSH accentBrush = CreateSolidBrush(kThemeAccent);
        FillRect(hdc, &accent, accentBrush);
        DeleteObject(accentBrush);

        SetBkMode(hdc, TRANSPARENT);

        RECT kicker{header.left + s(28), header.top + s(36), header.right - s(28), header.top + s(58)};
        SetTextColor(hdc, kThemeAccent);
        HGDIOBJ oldFont = SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        DrawTextW(hdc,
                  L"SIMULADOR DE GESTION FUTBOLISTICA CHILENA",
                  -1,
                  &kicker,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT title{header.left + s(28), header.top + s(60), header.right - s(28), header.top + s(112)};
        SelectObject(hdc, state.heroFont ? state.heroFont : state.titleFont);
        SetTextColor(hdc, RGB(245, 248, 250));
        DrawTextW(hdc,
                  L"Chilean Footballito",
                  -1,
                  &title,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT tagline{header.left + s(28), header.top + s(114), header.right - s(28), header.bottom - s(18)};
        SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetTextColor(hdc, RGB(181, 204, 216));
        DrawTextW(hdc,
                  L"Construye tu club. Define tu estilo. Deja tu legado.",
                  -1,
                  &tagline,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT versionBadge{header.right - s(164), header.top + s(26), header.right - s(26), header.top + s(78)};
        drawRoundedPanel(hdc, versionBadge, RGB(9, 31, 40), RGB(43, 85, 102), s(12));
        RECT versionTop{versionBadge.left + s(10), versionBadge.top + s(5), versionBadge.right - s(10), versionBadge.top + s(25)};
        RECT versionBottom{versionBadge.left + s(10), versionBadge.top + s(25), versionBadge.right - s(10), versionBadge.bottom - s(4)};
        SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        SetTextColor(hdc, RGB(73, 210, 145));
        DrawTextW(hdc, L"v0.1.0", -1, &versionTop, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetTextColor(hdc, kThemeMuted);
        DrawTextW(hdc, L"EN DESARROLLO", -1, &versionBottom, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const int contentTop = header.bottom + s(18);
        const int contentBottom = shell.bottom - s(18);
        const int splitX = shell.left + std::max(s(520), rectWidth(shell) * 57 / 100);

        RECT actionCard{shell.left + s(18), contentTop, splitX - s(9), contentBottom};
        RECT infoCard{splitX + s(9), contentTop, shell.right - s(18), contentBottom};
        drawRoundedPanel(hdc, actionCard, RGB(8, 22, 30), RGB(31, 63, 78), s(20));
        drawRoundedPanel(hdc, infoCard, RGB(9, 24, 33), RGB(38, 72, 88), s(20));

        RECT actionHeading{actionCard.left + s(26), actionCard.top + s(18), actionCard.right - s(20), actionCard.top + s(46)};
        SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        SetTextColor(hdc, RGB(235, 240, 243));
        DrawTextW(hdc, L"BIENVENIDO, ENTRENADOR", -1, &actionHeading, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT actionHint{actionCard.left + s(26), actionCard.top + s(46), actionCard.right - s(20), actionCard.top + s(78)};
        SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetTextColor(hdc, kThemeMuted);
        DrawTextW(hdc,
                  L"Elige como quieres comenzar tu historia.",
                  -1,
                  &actionHint,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT infoHeading{infoCard.left + s(24), infoCard.top + s(18), infoCard.right - s(20), infoCard.top + s(46)};
        SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        SetTextColor(hdc, kThemeAccent);
        DrawTextW(hdc, L"CENTRO DEL MANAGER", -1, &infoHeading, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const bool hasCareer = state.career.myTeam != nullptr;
        const bool canContinue = state.menuContinueButton && IsWindowEnabled(state.menuContinueButton);

        std::wstring clubLine = L"Club: Sin carrera activa";
        std::wstring managerLine = L"Manager: pendiente";
        std::wstring seasonLine = L"Temporada: lista para comenzar";
        std::wstring saveLine = canContinue ? L"Guardado: disponible" : L"Guardado: sin partida";
        COLORREF saveAccent = canContinue ? RGB(73, 210, 145) : RGB(156, 169, 176);

        if (hasCareer) {
            clubLine = L"Club: " + utf8ToWide(state.career.myTeam->name);
            std::string managerName = state.gameSetup.manager.empty() ? std::string("Manager") : state.gameSetup.manager;
            managerLine = L"Manager: " + utf8ToWide(managerName);
            seasonLine = L"Temporada " + std::to_wstring(state.career.currentSeason) +
                         L"  |  Semana " + std::to_wstring(state.career.currentWeek);
            saveLine = L"Estado: carrera activa";
            saveAccent = RGB(73, 210, 145);
        }

        RECT profile{infoCard.left + s(24), infoCard.top + s(56), infoCard.right - s(24), infoCard.top + s(190)};
        drawRoundedPanel(hdc, profile, RGB(11, 31, 41), RGB(44, 82, 99), s(15));

        const int profileLeft = profile.left + s(18);
        const int profileRight = profile.right - s(14);
        RECT line1{profileLeft, profile.top + s(10), profileRight, profile.top + s(34)};
        RECT line2{profileLeft, profile.top + s(38), profileRight, profile.top + s(62)};
        RECT line3{profileLeft, profile.top + s(66), profileRight, profile.top + s(90)};
        RECT line4{profileLeft, profile.top + s(96), profileRight, profile.bottom - s(8)};

        SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetTextColor(hdc, RGB(233, 239, 242));
        DrawTextW(hdc, clubLine.c_str(), -1, &line1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(hdc, RGB(181, 202, 212));
        DrawTextW(hdc, managerLine.c_str(), -1, &line2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextW(hdc, seasonLine.c_str(), -1, &line3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(hdc, saveAccent);
        DrawTextW(hdc, saveLine.c_str(), -1, &line4, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT settingsTitle{infoCard.left + s(24), infoCard.top + s(208), infoCard.right - s(20), infoCard.top + s(236)};
        SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        SetTextColor(hdc, RGB(235, 240, 243));
        DrawTextW(hdc, L"CONFIGURACION ACTUAL", -1, &settingsTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT settingsCard{infoCard.left + s(24), infoCard.top + s(242), infoCard.right - s(24), infoCard.top + s(366)};
        drawRoundedPanel(hdc, settingsCard, RGB(10, 29, 39), RGB(37, 73, 89), s(14));

        const std::wstring difficulty =
            L"Dificultad   " + utf8ToWide(game_settings::difficultyLabel(state.settings.difficulty));
        const std::wstring simulation =
            L"Simulacion   " + utf8ToWide(game_settings::simulationModeLabel(state.settings.simulationMode));
        const std::wstring speed =
            L"Velocidad    " + utf8ToWide(game_settings::simulationSpeedLabel(state.settings.simulationSpeed));
        const std::wstring music =
            L"Musica       " + utf8ToWide(game_settings::menuMusicModeLabel(state.settings.menuMusicMode));

        RECT setting1{settingsCard.left + s(16), settingsCard.top + s(8), settingsCard.right - s(12), settingsCard.top + s(34)};
        RECT setting2{settingsCard.left + s(16), settingsCard.top + s(36), settingsCard.right - s(12), settingsCard.top + s(62)};
        RECT setting3{settingsCard.left + s(16), settingsCard.top + s(64), settingsCard.right - s(12), settingsCard.top + s(90)};
        RECT setting4{settingsCard.left + s(16), settingsCard.top + s(92), settingsCard.right - s(12), settingsCard.bottom - s(6)};

        SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetTextColor(hdc, RGB(167, 192, 203));
        DrawTextW(hdc, difficulty.c_str(), -1, &setting1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextW(hdc, simulation.c_str(), -1, &setting2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextW(hdc, speed.c_str(), -1, &setting3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextW(hdc, music.c_str(), -1, &setting4, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (infoCard.bottom - infoCard.top >= s(500)) {
            RECT shortcutTitle{infoCard.left + s(24), infoCard.top + s(386), infoCard.right - s(20), infoCard.top + s(414)};
            SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
            SetTextColor(hdc, RGB(235, 240, 243));
            DrawTextW(hdc, L"ATAJOS", -1, &shortcutTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT shortcutCard{
                infoCard.left + s(24),
                infoCard.top + s(420),
                infoCard.right - s(24),
                std::min(infoCard.bottom - s(20), infoCard.top + s(500))
            };
            drawRoundedPanel(hdc, shortcutCard, RGB(10, 28, 37), RGB(35, 69, 84), s(14));

            RECT shortcutText{
                shortcutCard.left + s(16),
                shortcutCard.top + s(8),
                shortcutCard.right - s(12),
                shortcutCard.bottom - s(8)
            };
            SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
            SetTextColor(hdc, RGB(154, 181, 193));
            DrawTextW(hdc,
                      L"1-6  Acciones del menu    Enter  Seleccionar\nF11  Pantalla              Esc    Volver",
                      -1,
                      &shortcutText,
                      DT_LEFT | DT_VCENTER | DT_WORDBREAK);
        }

        // Separador vertical suave entre ambas zonas.
        HPEN dividerPen = CreatePen(PS_SOLID, 1, RGB(28, 55, 68));
        HGDIOBJ oldPen = SelectObject(hdc, dividerPen);
        MoveToEx(hdc, splitX, contentTop + s(20), nullptr);
        LineTo(hdc, splitX, contentBottom - s(20));
        SelectObject(hdc, oldPen);
        DeleteObject(dividerPen);

        // Barra inferior minimalista.
        RECT footer{shell.left + s(22), shell.bottom - s(38), shell.right - s(22), shell.bottom - s(12)};
        SetTextColor(hdc, RGB(128, 160, 175));
        SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        DrawTextW(hdc,
                  L"Chilean Footballito  |  Desarrollo activo  |  Temporada 2026",
                  -1,
                  &footer,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(hdc, oldFont);
        drawSimulationProgressOverlay(state, hdc, client);
        return;
    }

    if (isFrontMenuPage(state.currentPage)) {
        const bool highContrastFrontend = state.settings.visualProfile == VisualProfile::HighContrast;
        RECT hero{ s(18), s(18), client.right - s(18), client.bottom - s(44) };
        drawRoundedPanel(hdc,
                         hero,
                         highContrastFrontend ? RGB(6, 18, 25) : RGB(9, 22, 30),
                         highContrastFrontend ? RGB(86, 132, 158) : RGB(36, 62, 76),
                         s(28));

        RECT titleBand{hero.left + s(14), hero.top + s(14), hero.right - s(14), hero.top + s(156)};
        drawRoundedPanel(hdc,
                         titleBand,
                         highContrastFrontend ? RGB(8, 28, 38) : RGB(11, 28, 37),
                         highContrastFrontend ? RGB(104, 164, 196) : RGB(44, 76, 94),
                         s(20));

        RECT accentLine{titleBand.left + s(22), titleBand.top + s(18), titleBand.left + s(118), titleBand.top + s(24)};
        HBRUSH accentBrush = CreateSolidBrush(kThemeAccent);
        FillRect(hdc, &accentLine, accentBrush);
        DeleteObject(accentBrush);

        RECT kickerRect{titleBand.left + s(22), titleBand.top + s(30), titleBand.right - s(24), titleBand.top + s(52)};
        RECT nameRect{titleBand.left + s(22), titleBand.top + s(48), titleBand.right - s(24), titleBand.top + s(100)};
        RECT subtitleRect{titleBand.left + s(22), titleBand.top + s(102), titleBand.right - s(24), titleBand.top + s(128)};
        RECT navRect{titleBand.left + s(22), titleBand.bottom - s(34), titleBand.right - s(22), titleBand.bottom - s(12)};

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kThemeAccent);
        HGDIOBJ oldFont = SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        DrawTextW(hdc,
                  L"SIMULADOR DE GESTION FUTBOLISTICA CHILENA",
                  -1,
                  &kickerRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, state.heroFont ? state.heroFont : state.titleFont);
        SetTextColor(hdc, RGB(244, 247, 249));
        DrawTextW(hdc,
                  L"Chilean Footballito",
                  -1,
                  &nameRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetTextColor(hdc, RGB(188, 209, 220));
        const wchar_t* subtitleText = state.currentPage == GuiPage::MainMenu
            ? L"Portada del manager: continua, abre guardados, configura o entra al juego real desde un solo frontend."
            : (state.currentPage == GuiPage::Settings
                ? L"Cabina de configuracion: audio, accesibilidad, timing y perfil visual comparten persistencia."
                : (state.currentPage == GuiPage::Saves
                    ? L"Gestor de guardados: elige una carrera, revisa su detalle y decide si cargarla o borrarla."
                    : L"Creditos y hoja tecnica del proyecto, con la misma identidad visual del frontend principal."));
        DrawTextW(hdc, subtitleText, -1, &subtitleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        auto drawChip = [&](const RECT& area, const std::wstring& label, COLORREF fill, COLORREF border) {
            drawRoundedPanel(hdc, area, fill, border, s(12));
            RECT textRect = area;
            textRect.left += s(12);
            textRect.right -= s(12);
            SetTextColor(hdc, RGB(233, 239, 242));
            DrawTextW(hdc, label.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        };

        const int chipGap = s(10);
        const int chipHeight = s(24);
        const int chipWidth = std::max(s(140), static_cast<int>((navRect.right - navRect.left - chipGap * 3) / 4));
        std::vector<std::pair<std::wstring, COLORREF> > chips = {
            {utf8ToWide("Dificultad " + game_settings::difficultyLabel(state.settings.difficulty)), kThemeAccent},
            {utf8ToWide("Velocidad " + game_settings::simulationSpeedLabel(state.settings.simulationSpeed)), kThemeWarning},
            {utf8ToWide("Modo " + game_settings::simulationModeLabel(state.settings.simulationMode)), kThemeAccentBlue},
            {utf8ToWide("Musica " + game_settings::menuMusicModeLabel(state.settings.menuMusicMode)), kThemeAccentGreen}
        };
        for (size_t i = 0; i < chips.size(); ++i) {
            RECT chipRect{
                navRect.left + static_cast<int>(i) * (chipWidth + chipGap),
                navRect.top,
                navRect.left + static_cast<int>(i) * (chipWidth + chipGap) + chipWidth,
                navRect.top + chipHeight
            };
            drawChip(chipRect, chips[i].first, RGB(15, 32, 43), chips[i].second);
        }

        RECT summaryCard = expandedRect(childRectOnParent(state.summaryEdit, state.window), s(8), s(24));
        RECT detailCard = expandedRect(childRectOnParent(state.detailEdit, state.window), s(8), s(24));
        RECT newsCard = expandedRect(childRectOnParent(state.newsList, state.window), s(8), s(24));
        RECT statusCard = expandedRect(childRectOnParent(state.statusLabel, state.window), s(6), s(8));
        if (IsWindowVisible(state.summaryEdit)) drawRoundedPanel(hdc, summaryCard, kThemePanel, RGB(40, 64, 79), s(18));
        if (IsWindowVisible(state.detailEdit)) drawRoundedPanel(hdc, detailCard, RGB(15, 27, 37), RGB(44, 72, 90), s(18));
        if (IsWindowVisible(state.newsList)) drawRoundedPanel(hdc, newsCard, RGB(15, 27, 37), RGB(44, 72, 90), s(18));
        drawRoundedPanel(hdc, statusCard, RGB(11, 23, 31), RGB(39, 65, 79), s(12));
        SelectObject(hdc, oldFont);
        drawSimulationProgressOverlay(state, hdc, client);
        return;
    }

    if (state.layout.dashboardPage && !state.layout.dashboardEmptyState) {
        if (rectHasArea(state.layout.statusBar)) {
            drawRoundedPanel(hdc, state.layout.statusBar, RGB(11, 23, 31), RGB(39, 65, 79), s(12));
        }
        drawSimulationProgressOverlay(state, hdc, client);
        return;
    }

    RECT topBar = state.layout.topBar;
    FillRect(hdc, &topBar, state.headerBrush ? state.headerBrush : state.backgroundBrush);
    drawRoundedPanel(hdc,
                     RECT{s(8), s(8), client.right - s(8), std::max(s(24), static_cast<int>(state.layout.topBar.bottom) - s(12))},
                     kThemeTopBarPanel,
                     RGB(28, 53, 65),
                     s(18));
    drawTopMetrics(state, hdc, client);

    drawRoundedPanel(hdc, state.layout.sideMenu, RGB(12, 23, 31), RGB(34, 57, 70), s(18));
    drawRoundedPanel(hdc, state.layout.contentShell, kThemeShell, RGB(32, 53, 66), s(22));
    if (rectHasArea(state.layout.statusBar)) {
        drawRoundedPanel(hdc, state.layout.statusBar, RGB(11, 23, 31), RGB(39, 65, 79), s(12));
    }

    auto drawScrollableChrome = [&]() {
        if (rectHasArea(state.layout.summaryPanel.outer) && IsWindowVisible(state.summaryEdit)) {
            drawRoundedPanel(hdc, state.layout.summaryPanel.outer, kThemePanel, RGB(40, 64, 79), s(16));
        }
        if (rectHasArea(state.layout.primaryPanel.outer) && (IsWindowVisible(state.tableList) || state.currentPage == GuiPage::Tactics)) {
            drawRoundedPanel(hdc, state.layout.primaryPanel.outer, kThemePanel, RGB(40, 64, 79), s(16));
        }
        if (rectHasArea(state.layout.secondaryPanel.outer) && IsWindowVisible(state.squadList)) {
            drawRoundedPanel(hdc, state.layout.secondaryPanel.outer, kThemePanel, RGB(40, 64, 79), s(16));
        }
        if (rectHasArea(state.layout.footerPanel.outer) && IsWindowVisible(state.transferList)) {
            drawRoundedPanel(hdc, state.layout.footerPanel.outer, kThemePanel, RGB(40, 64, 79), s(16));
        }
        if (rectHasArea(state.layout.detailPanel.outer) && IsWindowVisible(state.detailEdit)) {
            drawRoundedPanel(hdc, state.layout.detailPanel.outer, RGB(15, 27, 37), RGB(44, 72, 90), s(16));
        }
        if (rectHasArea(state.layout.newsPanel.outer) && IsWindowVisible(state.newsList)) {
            drawRoundedPanel(hdc, state.layout.newsPanel.outer, RGB(15, 27, 37), RGB(44, 72, 90), s(16));
        }
        if (state.currentPage == GuiPage::Dashboard && rectHasArea(state.layout.spotlightBand)) {
            drawDashboardSpotlights(state, hdc, state.layout.spotlightBand);
        } else if (pageUsesInsightStrip(state) && rectHasArea(state.layout.spotlightBand)) {
            drawContextSpotlights(state, hdc, state.layout.spotlightBand);
        }
        if (rectHasArea(state.layout.contextCard)) {
            drawContextTeamLogo(state, hdc);
        }
    };

    if (rectHasArea(state.layout.scrollViewport)) {
        ScopedClipRect scrollClip(hdc, state.layout.scrollViewport);
        drawScrollableChrome();
    } else {
        drawScrollableChrome();
    }

    RECT menuTitle = state.layout.sideMenuTitle;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kThemeMuted);
    HGDIOBJ oldFont = SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
    DrawTextW(hdc, L"Secciones", -1, &menuTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    if (state.currentPage == GuiPage::Tactics) {
        RECT boardRect = shrinkRect(state.layout.primaryPanel.body, s(2), s(2));
        if (rectHasArea(boardRect)) {
            ScopedClipRect boardClip(hdc, state.layout.primaryPanel.body);
            drawTacticsBoard(state, hdc, boardRect);
        }
    }
    drawSimulationProgressOverlay(state, hdc, client);
}

}  // namespace gui_win32

#endif
