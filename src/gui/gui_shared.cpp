#include "gui/gui_internal.h"

#ifdef _WIN32

#include "utils/utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace gui_win32 {

namespace {

constexpr int kHiddenControlOrigin = -32000;

void clearWindowRegion(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    SetWindowRgn(hwnd, nullptr, TRUE);
}

bool isComboBoxControl(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    wchar_t className[32]{};
    GetClassNameW(hwnd, className, static_cast<int>(sizeof(className) / sizeof(className[0])));
    return lstrcmpiW(className, L"COMBOBOX") == 0;
}

}  // namespace

int scaleByDpi(const AppState& state, int value) {
    UINT dpi = state.dpi == 0 ? 96 : state.dpi;
    return MulDiv(value, static_cast<int>(dpi), 96);
}

RECT childRectOnParent(HWND child, HWND parent) {
    RECT rect{};
    if (!child || !parent) return rect;
    GetWindowRect(child, &rect);
    MapWindowPoints(nullptr, parent, reinterpret_cast<LPPOINT>(&rect), 2);
    return rect;
}

RECT expandedRect(RECT rect, int dx, int dy) {
    rect.left -= dx;
    rect.top -= dy;
    rect.right += dx;
    rect.bottom += dy;
    return rect;
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return std::wstring();
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0) return std::wstring(text.begin(), text.end());
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], length);
    if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
    return wide;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) return std::string(text.begin(), text.end());
    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &utf8[0], length, nullptr, nullptr);
    if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
    return utf8;
}

std::string toEditText(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) out += '\r';
        out += text[i];
    }
    return out;
}

void setWindowTextUtf8(HWND hwnd, const std::string& text) {
    std::wstring wide = utf8ToWide(toEditText(text));
    SetWindowTextW(hwnd, wide.c_str());
}

std::string getWindowTextUtf8(HWND hwnd) {
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return std::string();
    std::wstring buffer(static_cast<size_t>(length + 1), L'\0');
    GetWindowTextW(hwnd, &buffer[0], length + 1);
    buffer.resize(static_cast<size_t>(length));
    return trim(wideToUtf8(buffer));
}

void updateDynamicStaticText(AppState& state, HWND hwnd, const std::string& text) {
    if (!hwnd || !IsWindow(hwnd)) return;

    RECT oldRect = childRectOnParent(hwnd, state.window);
    setWindowTextUtf8(hwnd, text);
    RECT newRect = childRectOnParent(hwnd, state.window);

    const int padX = scaleByDpi(state, 10);
    const int padY = scaleByDpi(state, 8);
    InflateRect(&oldRect, padX, padY);
    InflateRect(&newRect, padX, padY);
    if (state.window && IsWindow(state.window)) {
        InvalidateRect(state.window, &oldRect, TRUE);
        InvalidateRect(state.window, &newRect, TRUE);
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

void hideControlAndInvalidate(AppState& state, HWND hwnd, int padX, int padY) {
    if (!hwnd || !IsWindow(hwnd)) return;

    if (isComboBoxControl(hwnd)) {
        SendMessageW(hwnd, CB_SHOWDROPDOWN, FALSE, 0);
    }
    RECT rect = childRectOnParent(hwnd, state.window);
    clearWindowRegion(hwnd);
    const bool wasVisible = IsWindowVisible(hwnd);
    if (wasVisible) ShowWindow(hwnd, SW_HIDE);
    RECT hiddenRect = childRectOnParent(hwnd, state.window);
    if (hiddenRect.left != kHiddenControlOrigin || hiddenRect.top != kHiddenControlOrigin) {
        SetWindowPos(hwnd,
                     nullptr,
                     kHiddenControlOrigin,
                     kHiddenControlOrigin,
                     0,
                     0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    }
    if (state.window && IsWindow(state.window)) {
        InflateRect(&rect, scaleByDpi(state, padX), scaleByDpi(state, padY));
        InvalidateRect(state.window, &rect, TRUE);
    }
}

void showControlAndInvalidate(AppState& state, HWND hwnd, int padX, int padY) {
    if (!hwnd || !IsWindow(hwnd)) return;
    clearWindowRegion(hwnd);
    if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOW);
    RECT rect = childRectOnParent(hwnd, state.window);
    if (state.window && IsWindow(state.window)) {
        InflateRect(&rect, scaleByDpi(state, padX), scaleByDpi(state, padY));
        InvalidateRect(state.window, &rect, TRUE);
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

void setControlVisibility(AppState& state, HWND hwnd, bool visible, int padX, int padY) {
    if (visible) {
        showControlAndInvalidate(state, hwnd, padX, padY);
    } else {
        hideControlAndInvalidate(state, hwnd, padX, padY);
    }
}

void moveControlAndInvalidate(AppState& state, HWND hwnd, int x, int y, int width, int height, int padX, int padY) {
    if (!hwnd || !IsWindow(hwnd)) return;

    RECT oldRect = childRectOnParent(hwnd, state.window);
    if (oldRect.left == x &&
        oldRect.top == y &&
        oldRect.right - oldRect.left == width &&
        oldRect.bottom - oldRect.top == height) {
        return;
    }
    MoveWindow(hwnd, x, y, width, height, TRUE);
    RECT newRect = childRectOnParent(hwnd, state.window);

    if (state.window && IsWindow(state.window)) {
        InflateRect(&oldRect, scaleByDpi(state, padX), scaleByDpi(state, padY));
        InflateRect(&newRect, scaleByDpi(state, padX), scaleByDpi(state, padY));
        InvalidateRect(state.window, &oldRect, TRUE);
        InvalidateRect(state.window, &newRect, TRUE);
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

void applyControlViewportClip(AppState& state, HWND hwnd, const RECT* clipRect) {
    if (!hwnd || !IsWindow(hwnd)) return;
    if (!clipRect || !state.window || !IsWindow(state.window)) {
        clearWindowRegion(hwnd);
        return;
    }

    RECT windowRect = childRectOnParent(hwnd, state.window);
    RECT visibleRect{};
    if (!IntersectRect(&visibleRect, &windowRect, clipRect)) {
        HRGN emptyRegion = CreateRectRgn(0, 0, 0, 0);
        if (SetWindowRgn(hwnd, emptyRegion, TRUE) == 0) {
            DeleteObject(emptyRegion);
        }
        return;
    }

    const int left = static_cast<int>(std::max<LONG>(0, visibleRect.left - windowRect.left));
    const int top = static_cast<int>(std::max<LONG>(0, visibleRect.top - windowRect.top));
    const int right = std::max(left, static_cast<int>(visibleRect.right - windowRect.left));
    const int bottom = std::max(top, static_cast<int>(visibleRect.bottom - windowRect.top));
    const bool fullWindowVisible = left == 0 &&
                                   top == 0 &&
                                   right >= (windowRect.right - windowRect.left) &&
                                   bottom >= (windowRect.bottom - windowRect.top);
    if (fullWindowVisible) {
        clearWindowRegion(hwnd);
        return;
    }

    HRGN clipRegion = CreateRectRgn(left, top, right, bottom);
    if (SetWindowRgn(hwnd, clipRegion, TRUE) == 0) {
        DeleteObject(clipRegion);
    }
}

void applyEditInteriorPadding(AppState& state, HWND hwnd, int horizontalPadding, int verticalPadding) {
    if (!hwnd || !IsWindow(hwnd)) return;

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const int hPad = std::max(0, scaleByDpi(state, horizontalPadding));
    const int vPad = std::max(0, scaleByDpi(state, verticalPadding));
    if ((style & ES_MULTILINE) != 0) {
        RECT client{};
        GetClientRect(hwnd, &client);
        RECT textRect{
            client.left + hPad,
            client.top + vPad,
            std::max(client.left + hPad, client.right - hPad),
            std::max(client.top + vPad, client.bottom - vPad)
        };
        SendMessageW(hwnd, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&textRect));
    } else {
        SendMessageW(hwnd,
                     EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(hPad, hPad));
    }
}

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
                   int id) {
    HWND hwnd = CreateWindowExW(exStyle,
                                className,
                                text,
                                style,
                                x,
                                y,
                                width,
                                height,
                                parent,
                                reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
                                state.instance,
                                nullptr);
    if (hwnd && state.font) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(state.font), TRUE);
    }
    return hwnd;
}

void addComboItem(HWND combo, const std::string& text) {
    std::wstring wide = utf8ToWide(text);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
}

void resetComboItems(HWND combo, const std::vector<std::string>& items) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (const auto& item : items) {
        addComboItem(combo, item);
    }
    if (!items.empty()) {
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    }
}

int comboIndex(HWND combo) {
    return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
}

std::string comboText(HWND combo) {
    int index = comboIndex(combo);
    if (index < 0) return std::string();
    int length = static_cast<int>(SendMessageW(combo, CB_GETLBTEXTLEN, index, 0));
    std::wstring buffer(static_cast<size_t>(length + 1), L'\0');
    SendMessageW(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(buffer.data()));
    buffer.resize(static_cast<size_t>(length));
    return wideToUtf8(buffer);
}

int selectedListViewRow(HWND list) {
    return ListView_GetNextItem(list, -1, LVNI_SELECTED);
}

std::string listViewText(HWND list, int row, int subItem) {
    if (row < 0) return std::string();
    wchar_t buffer[512]{};
    LVITEMW item{};
    item.iSubItem = subItem;
    item.pszText = buffer;
    item.cchTextMax = static_cast<int>(sizeof(buffer) / sizeof(buffer[0]));
    SendMessageW(list, LVM_GETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&item));
    return wideToUtf8(buffer);
}

void clearListView(HWND list) {
    ListView_DeleteAllItems(list);
}

void addListViewRow(HWND list, const std::vector<std::string>& values) {
    if (values.empty()) return;
    std::vector<std::wstring> wideValues;
    wideValues.reserve(values.size());
    for (const auto& value : values) {
        wideValues.push_back(utf8ToWide(value));
    }

    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(list);
    item.pszText = const_cast<LPWSTR>(wideValues[0].c_str());
    SendMessageW(list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));

    for (size_t i = 1; i < wideValues.size(); ++i) {
        LVITEMW subItem{};
        subItem.iSubItem = static_cast<int>(i);
        subItem.pszText = const_cast<LPWSTR>(wideValues[i].c_str());
        SendMessageW(list, LVM_SETITEMTEXTW, item.iItem, reinterpret_cast<LPARAM>(&subItem));
    }
}

void resetListViewColumns(HWND list, const std::vector<std::pair<std::wstring, int> >& columns) {
    HWND header = ListView_GetHeader(list);
    if (header) {
        int count = Header_GetItemCount(header);
        for (int i = count - 1; i >= 0; --i) {
            ListView_DeleteColumn(list, i);
        }
    }
    for (size_t i = 0; i < columns.size(); ++i) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<LPWSTR>(columns[i].first.c_str());
        column.cx = columns[i].second;
        column.iSubItem = static_cast<int>(i);
        SendMessageW(list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&column));
    }
}

namespace {

struct ColumnSizingProfile {
    int expansionWeight = 100;
    int minPercent = 72;
    int floorPercent = 58;
    int shrinkPriority = 1;
    bool compact = false;
};

struct ColumnContentDensity {
    int averageChars = 0;
    int maxChars = 0;
    int populatedRows = 0;
    bool compactValues = false;
    bool sparse = false;
};

bool containsAnyToken(const std::string& value, std::initializer_list<const char*> tokens) {
    for (const char* token : tokens) {
        if (!token) continue;
        if (value.find(token) != std::string::npos) return true;
    }
    return false;
}

std::string panelTitleForList(const AppState& state, HWND list) {
    if (list == state.tableList) return state.currentModel.primary.title;
    if (list == state.squadList) return state.currentModel.secondary.title;
    if (list == state.transferList) return state.currentModel.footer.title;
    return std::string();
}

std::string columnMemoryKey(const AppState& state,
                            HWND list,
                            const std::vector<std::pair<std::wstring, int> >& columns) {
    std::ostringstream key;
    key << static_cast<int>(state.currentPage) << "|" << panelTitleForList(state, list) << "|" << columns.size();
    for (const auto& column : columns) {
        key << "|" << toLower(trim(wideToUtf8(column.first)));
    }
    return key.str();
}

bool looksCompactCellValue(const std::string& raw) {
    const std::string value = trim(toLower(raw));
    if (value.empty()) return true;

    int alpha = 0;
    int digits = 0;
    int spaces = 0;
    for (unsigned char c : value) {
        if (std::isalpha(c)) ++alpha;
        else if (std::isdigit(c)) ++digits;
        else if (std::isspace(c)) ++spaces;
    }

    if (value.size() <= 4) return true;
    if (alpha == 0 && digits > 0) return true;
    if (digits >= alpha * 2 && value.size() <= 18) return true;
    if (containsAnyToken(value, {"$", "%", "sem", "/100", "/10", "pts", "dg"})) return value.size() <= 18;
    return spaces == 0 && value.size() <= 12;
}

std::vector<ColumnContentDensity> sampleColumnDensities(HWND list, size_t columnCount) {
    std::vector<ColumnContentDensity> densities(columnCount);
    if (!list || columnCount == 0) return densities;

    const int rowCount = ListView_GetItemCount(list);
    const int sampleRows = std::min(rowCount, 18);
    if (sampleRows <= 0) return densities;

    std::vector<int> compactVotes(columnCount, 0);
    for (int row = 0; row < sampleRows; ++row) {
        for (size_t col = 0; col < columnCount; ++col) {
            const std::string value = listViewText(list, row, static_cast<int>(col));
            const std::string trimmed = trim(value);
            if (trimmed.empty() || trimmed == "-") continue;

            ColumnContentDensity& density = densities[col];
            const int length = static_cast<int>(trimmed.size());
            density.averageChars += length;
            density.maxChars = std::max(density.maxChars, length);
            density.populatedRows += 1;
            if (looksCompactCellValue(trimmed)) compactVotes[col] += 1;
        }
    }

    for (size_t col = 0; col < columnCount; ++col) {
        ColumnContentDensity& density = densities[col];
        if (density.populatedRows > 0) {
            density.averageChars /= density.populatedRows;
        }
        density.compactValues = compactVotes[col] >= std::max(1, density.populatedRows * 2 / 3);
        density.sparse = density.populatedRows <= std::max(1, sampleRows / 3);
    }
    return densities;
}

void applyCompactProfile(ColumnSizingProfile& profile, int weight = 42, int minPercent = 86, int floorPercent = 74) {
    profile.expansionWeight = std::min(profile.expansionWeight, weight);
    profile.minPercent = std::max(profile.minPercent, minPercent);
    profile.floorPercent = std::max(profile.floorPercent, floorPercent);
    profile.shrinkPriority = 0;
    profile.compact = true;
}

void applyMediumTextProfile(ColumnSizingProfile& profile, int weight = 132, int minPercent = 68, int floorPercent = 56) {
    profile.expansionWeight = std::max(profile.expansionWeight, weight);
    profile.minPercent = std::min(profile.minPercent, minPercent);
    profile.floorPercent = std::min(profile.floorPercent, floorPercent);
    profile.shrinkPriority = std::max(profile.shrinkPriority, 2);
    profile.compact = false;
}

void applyWideTextProfile(ColumnSizingProfile& profile, int weight = 180, int minPercent = 60, int floorPercent = 48) {
    profile.expansionWeight = std::max(profile.expansionWeight, weight);
    profile.minPercent = std::min(profile.minPercent, minPercent);
    profile.floorPercent = std::min(profile.floorPercent, floorPercent);
    profile.shrinkPriority = std::max(profile.shrinkPriority, 3);
    profile.compact = false;
}

void mergeDensityIntoProfile(ColumnSizingProfile& profile, const ColumnContentDensity& density) {
    if (density.compactValues && density.maxChars <= 16) {
        applyCompactProfile(profile, 40, 88, 76);
    } else if (density.maxChars >= 28 || density.averageChars >= 18) {
        applyWideTextProfile(profile, 200, 56, 44);
    } else if (density.maxChars >= 18 || density.averageChars >= 12) {
        applyMediumTextProfile(profile, 146, 64, 52);
    }

    if (density.sparse) {
        profile.expansionWeight = std::max(32, profile.expansionWeight - 18);
    } else if (density.maxChars >= 24) {
        profile.expansionWeight += 12;
    }
}

ColumnSizingProfile buildColumnSizingProfile(const AppState& state,
                                             HWND list,
                                             size_t index,
                                             const std::wstring& headerText) {
    ColumnSizingProfile profile;
    const std::string panel = toLower(trim(panelTitleForList(state, list)));
    const std::string header = toLower(trim(wideToUtf8(headerText)));

    if (header.size() <= 3 ||
        containsAnyToken(header, {"pj", "pg", "pe", "pp", "gf", "gc", "dg", "pts", "pos", "hab", "gls", "ast",
                                  "xg", "xa", "min", "age", "pot", "fis", "med", "fit"})) {
        applyCompactProfile(profile);
    } else if (containsAnyToken(header, {"edad", "media", "potencial", "fisico", "forma", "moral", "valor",
                                         "salario", "costo", "precio", "riesgo", "tiempo", "semanas"})) {
        applyCompactProfile(profile, 48, 84, 70);
    } else if (containsAnyToken(header, {"jugador", "club", "equipo", "rival", "detalle", "lectura", "perfil",
                                         "competicion", "objetivo", "contexto", "mercado", "nota", "titular"})) {
        applyWideTextProfile(profile);
    } else if (containsAnyToken(header, {"estado", "tipo", "linea", "area", "rol", "encaje", "plan", "accion"})) {
        applyMediumTextProfile(profile);
    }

    if (panel == "leaguetableview" || panel == "leaguepositionwidget" || panel == "competitionsummary" || panel == "racecontext") {
        if (index == 1 || containsAnyToken(header, {"club", "equipo"})) {
            applyWideTextProfile(profile, 220, 56, 44);
        } else {
            applyCompactProfile(profile, 36, 88, 76);
        }
    } else if (panel == "fixturelistview" || panel == "competitionreport" || panel == "seasonhistory") {
        if (containsAnyToken(header, {"rival", "competicion", "detalle", "contexto"})) {
            applyWideTextProfile(profile, 188, 58, 46);
        } else if (containsAnyToken(header, {"estado", "tipo"})) {
            applyMediumTextProfile(profile, 140, 66, 54);
        } else {
            applyCompactProfile(profile, 48, 84, 70);
        }
    } else if (panel == "playertableview" || panel == "youthplayertableview") {
        if (index == 0 || containsAnyToken(header, {"jugador"})) {
            applyWideTextProfile(profile, 212, 56, 44);
        } else if (containsAnyToken(header, {"club", "estado", "rol", "contrato", "situacion"})) {
            applyMediumTextProfile(profile, 148, 64, 52);
        } else {
            applyCompactProfile(profile, 44, 86, 72);
        }
    } else if (panel == "transfermarketview" || panel == "transferpipeline" || panel == "transfertargetcard" ||
               state.currentPage == GuiPage::Transfers) {
        if (containsAnyToken(header, {"jugador", "club", "detalle", "lectura", "mercado", "perfil", "recomendacion"})) {
            applyWideTextProfile(profile, 196, 56, 42);
        } else if (containsAnyToken(header, {"estado", "tipo", "riesgo", "encaje", "rol"})) {
            applyMediumTextProfile(profile, 150, 64, 52);
        } else {
            applyCompactProfile(profile, 40, 86, 74);
        }
    } else if (panel == "newscardlist" || panel == "alertpanel" || panel == "alertpanel / newsfeedpanel" ||
               state.currentPage == GuiPage::News) {
        if (containsAnyToken(header, {"titular", "detalle", "lectura", "contexto", "competicion", "mercado", "club"})) {
            applyWideTextProfile(profile, 204, 54, 42);
        } else if (containsAnyToken(header, {"tipo", "estado"})) {
            applyMediumTextProfile(profile, 144, 64, 52);
        } else {
            applyCompactProfile(profile, 46, 84, 70);
        }
    } else if (panel == "injurylistwidget") {
        if (containsAnyToken(header, {"jugador", "detalle", "nota"})) {
            applyWideTextProfile(profile, 182, 58, 46);
        } else if (containsAnyToken(header, {"tipo", "estado"})) {
            applyMediumTextProfile(profile, 136, 66, 54);
        } else {
            applyCompactProfile(profile, 48, 84, 70);
        }
    } else if (panel == "salarytable" || panel == "boardobjectivetable") {
        if (containsAnyToken(header, {"jugador", "objetivo", "detalle", "concepto"})) {
            applyWideTextProfile(profile, 190, 56, 44);
        } else if (containsAnyToken(header, {"estado", "avance"})) {
            applyMediumTextProfile(profile, 138, 64, 52);
        } else {
            applyCompactProfile(profile, 44, 86, 72);
        }
    }

    return profile;
}

}  // namespace

void autosizeListViewColumns(AppState& state, HWND list, const std::vector<std::pair<std::wstring, int> >& columns) {
    if (!list || columns.empty() || !IsWindow(list)) return;
    RECT client{};
    GetClientRect(list, &client);
    int availableWidth = std::max(0, static_cast<int>(client.right - client.left - 4));
    if (availableWidth <= 0) return;

    std::vector<int> widths(columns.size(), 0);
    std::vector<int> minWidths(columns.size(), 0);
    std::vector<int> floorWidths(columns.size(), 0);
    std::vector<int> hardFloorWidths(columns.size(), 0);
    std::vector<ColumnSizingProfile> profiles(columns.size());
    const std::vector<ColumnContentDensity> densities = sampleColumnDensities(list, columns.size());
    const std::string memoryKey = columnMemoryKey(state, list, columns);
    const auto remembered = state.columnWidthMemory.find(memoryKey);
    const bool hasRememberedWidths = remembered != state.columnWidthMemory.end() &&
                                     remembered->second.size() == columns.size();
    int totalWidth = 0;

    for (size_t i = 0; i < columns.size(); ++i) {
        profiles[i] = buildColumnSizingProfile(state, list, i, columns[i].first);
        if (i < densities.size()) mergeDensityIntoProfile(profiles[i], densities[i]);
        SendMessageW(list, LVM_SETCOLUMNWIDTH, static_cast<WPARAM>(i), LVSCW_AUTOSIZE);
        int contentWidth = ListView_GetColumnWidth(list, static_cast<int>(i));
        SendMessageW(list, LVM_SETCOLUMNWIDTH, static_cast<WPARAM>(i), LVSCW_AUTOSIZE_USEHEADER);
        int headerWidth = ListView_GetColumnWidth(list, static_cast<int>(i));
        int autoWidth = std::max(contentWidth, headerWidth);
        int preferredWidth = scaleByDpi(state, columns[i].second);
        int densityWidth = preferredWidth;
        if (i < densities.size() && densities[i].maxChars > 0) {
            const int perChar = scaleByDpi(state, profiles[i].compact ? 5 : 7);
            densityWidth = scaleByDpi(state, 28) + std::min(40, densities[i].maxChars) * perChar;
        }
        int width = std::max(std::max(autoWidth, preferredWidth), std::min(availableWidth, densityWidth));
        if (hasRememberedWidths) {
            width = std::max(scaleByDpi(state, profiles[i].compact ? 44 : 76),
                             (width * 7 + remembered->second[i] * 5) / 12);
        }
        int minWidth = clampValue(preferredWidth * profiles[i].minPercent / 100,
                                  scaleByDpi(state, profiles[i].compact ? 54 : 92),
                                  width);
        int floorWidth = clampValue(preferredWidth * profiles[i].floorPercent / 100,
                                    scaleByDpi(state, profiles[i].compact ? 42 : 76),
                                    minWidth);
        int hardFloor = scaleByDpi(state, profiles[i].compact ? 36 : 68);
        widths[i] = width;
        minWidths[i] = minWidth;
        floorWidths[i] = floorWidth;
        hardFloorWidths[i] = std::min(floorWidth, hardFloor);
        totalWidth += width;
    }

    auto orderedIndices = [&](int minShrinkPriority) {
        std::vector<size_t> order;
        for (size_t i = 0; i < widths.size(); ++i) {
            if (profiles[i].shrinkPriority >= minShrinkPriority) order.push_back(i);
        }
        std::stable_sort(order.begin(), order.end(), [&](size_t lhs, size_t rhs) {
            if (profiles[lhs].shrinkPriority != profiles[rhs].shrinkPriority) {
                return profiles[lhs].shrinkPriority > profiles[rhs].shrinkPriority;
            }
            return profiles[lhs].expansionWeight > profiles[rhs].expansionWeight;
        });
        return order;
    };

    auto shrinkTowards = [&](const std::vector<int>& targets, int minShrinkPriority, int& overflow) {
        std::vector<size_t> order = orderedIndices(minShrinkPriority);
        while (overflow > 0 && !order.empty()) {
            bool changed = false;
            for (size_t index : order) {
                if (overflow <= 0) break;
                int reducible = widths[index] - targets[index];
                if (reducible <= 0) continue;
                int step = std::min(overflow,
                                    std::max(1, std::min(reducible,
                                                         profiles[index].shrinkPriority >= 2 ? 3 : 2)));
                widths[index] -= step;
                overflow -= step;
                changed = true;
            }
            if (!changed) break;
        }
    };

    if (totalWidth < availableWidth && !widths.empty()) {
        int extra = availableWidth - totalWidth;
        std::vector<size_t> order;
        for (size_t i = 0; i < widths.size(); ++i) order.push_back(i);
        std::stable_sort(order.begin(), order.end(), [&](size_t lhs, size_t rhs) {
            return profiles[lhs].expansionWeight > profiles[rhs].expansionWeight;
        });
        while (extra > 0 && !order.empty()) {
            bool changed = false;
            for (size_t index : order) {
                if (extra <= 0) break;
                int grant = std::min(extra, std::max(1, profiles[index].expansionWeight / 60));
                widths[index] += grant;
                extra -= grant;
                changed = true;
            }
            if (!changed) break;
        }
    } else if (totalWidth > availableWidth) {
        int overflow = totalWidth - availableWidth;
        shrinkTowards(minWidths, 2, overflow);
        shrinkTowards(minWidths, 1, overflow);
        shrinkTowards(minWidths, 0, overflow);
        shrinkTowards(floorWidths, 0, overflow);
        shrinkTowards(hardFloorWidths, 0, overflow);

        if (overflow > 0) {
            int currentWidth = 0;
            for (int width : widths) currentWidth += width;
            if (currentWidth > 0) {
                int adjustedTotal = 0;
                for (size_t i = 0; i < widths.size(); ++i) {
                    int scaledWidth = std::max(hardFloorWidths[i], widths[i] * availableWidth / currentWidth);
                    widths[i] = scaledWidth;
                    adjustedTotal += scaledWidth;
                }
                if (!widths.empty() && adjustedTotal != availableWidth) {
                    widths.back() = std::max(hardFloorWidths.back(), widths.back() + (availableWidth - adjustedTotal));
                }
            }
        }
    }

    for (size_t i = 0; i < widths.size(); ++i) {
        ListView_SetColumnWidth(list, static_cast<int>(i), widths[i]);
    }
    state.columnWidthMemory[memoryKey] = widths;
}

void drawRoundedPanel(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawPitchOverlay(HDC hdc, const RECT& rect) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(42, 88, 68));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    int midX = (rect.left + rect.right) / 2;
    int midY = (rect.top + rect.bottom) / 2;
    MoveToEx(hdc, midX, rect.top, nullptr);
    LineTo(hdc, midX, rect.bottom);
    int radius = std::min((rect.right - rect.left) / 8, (rect.bottom - rect.top) / 4);
    Ellipse(hdc, midX - radius, midY - radius, midX + radius, midY + radius);

    int boxWidth = (rect.right - rect.left) / 6;
    int boxHeight = (rect.bottom - rect.top) / 3;
    Rectangle(hdc, rect.left, midY - boxHeight / 2, rect.left + boxWidth, midY + boxHeight / 2);
    Rectangle(hdc, rect.right - boxWidth, midY - boxHeight / 2, rect.right, midY + boxHeight / 2);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void drawSectionHeader(HDC hdc, const RECT& rect, const std::wstring& title) {
    RECT lineRect = rect;
    lineRect.top += 14;
    lineRect.left += 110;
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(52, 78, 98));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, lineRect.left, lineRect.top, nullptr);
    LineTo(hdc, lineRect.right, lineRect.top);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kThemeAccent);
    DrawTextW(hdc, title.c_str(), -1, const_cast<RECT*>(&rect), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawStatBar(HDC hdc, const RECT& rect, const std::wstring& label, int value, int maxValue, COLORREF accent) {
    int safeMax = std::max(1, maxValue);
    int pct = clampValue(value * 100 / safeMax, 0, 100);

    RECT track = rect;
    track.top += 18;
    drawRoundedPanel(hdc, track, RGB(20, 33, 44), RGB(42, 63, 79), 10);

    RECT fill = track;
    fill.right = fill.left + (track.right - track.left) * pct / 100;
    fill.left += 1;
    fill.top += 1;
    fill.bottom -= 1;
    fill.right = std::max(fill.left + 4, fill.right - 1);
    drawRoundedPanel(hdc, fill, accent, accent, 8);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kThemeText);
    DrawTextW(hdc, label.c_str(), -1, const_cast<RECT*>(&rect), DT_LEFT | DT_TOP | DT_SINGLELINE);
    std::wostringstream valueStream;
    valueStream << value;
    std::wstring valueText = valueStream.str();
    DrawTextW(hdc, valueText.c_str(), -1, const_cast<RECT*>(&rect), DT_RIGHT | DT_TOP | DT_SINGLELINE);
}

GuiPage pageForControlId(int id) {
    switch (id) {
        case IDC_PAGE_DASHBOARD_BUTTON: return GuiPage::Dashboard;
        case IDC_PAGE_SQUAD_BUTTON: return GuiPage::Squad;
        case IDC_PAGE_TACTICS_BUTTON: return GuiPage::Tactics;
        case IDC_PAGE_CALENDAR_BUTTON: return GuiPage::Calendar;
        case IDC_PAGE_LEAGUE_BUTTON: return GuiPage::League;
        case IDC_PAGE_TRANSFERS_BUTTON: return GuiPage::Transfers;
        case IDC_PAGE_FINANCES_BUTTON: return GuiPage::Finances;
        case IDC_PAGE_YOUTH_BUTTON: return GuiPage::Youth;
        case IDC_PAGE_BOARD_BUTTON: return GuiPage::Board;
        case IDC_PAGE_NEWS_BUTTON: return GuiPage::News;
        default: return GuiPage::Dashboard;
    }
}

bool isPageButtonId(int id) {
    return id >= IDC_PAGE_DASHBOARD_BUTTON && id <= IDC_PAGE_NEWS_BUTTON;
}

bool isPrimaryButtonId(int id) {
    return id == IDC_NEW_CAREER_BUTTON || id == IDC_SIMULATE_BUTTON || id == IDC_BUY_BUTTON ||
           id == IDC_RENEW_BUTTON || id == IDC_SCOUT_BUTTON || id == IDC_PAGE_DASHBOARD_BUTTON ||
           id == IDC_EMPTY_NEW_BUTTON || id == IDC_EMPTY_LOAD_BUTTON || id == IDC_EMPTY_VALIDATE_BUTTON ||
           id == IDC_MENU_CONTINUE_BUTTON || id == IDC_MENU_PLAY_BUTTON;
}

bool isUpgradeButtonId(int id) {
    return id == IDC_YOUTH_UPGRADE_BUTTON || id == IDC_TRAINING_UPGRADE_BUTTON ||
           id == IDC_SCOUTING_UPGRADE_BUTTON || id == IDC_STADIUM_UPGRADE_BUTTON;
}

bool isActionButtonId(int id) {
    return id == IDC_SCOUT_BUTTON || id == IDC_SHORTLIST_BUTTON || id == IDC_FOLLOW_SHORTLIST_BUTTON ||
           id == IDC_BUY_BUTTON || id == IDC_PRECONTRACT_BUTTON || id == IDC_LOAN_BUTTON || id == IDC_RENEW_BUTTON ||
           id == IDC_SELL_BUTTON || id == IDC_PLAN_BUTTON || id == IDC_INSTRUCTION_BUTTON ||
           id == IDC_YOUTH_UPGRADE_BUTTON || id == IDC_TRAINING_UPGRADE_BUTTON ||
           id == IDC_SCOUTING_UPGRADE_BUTTON || id == IDC_STADIUM_UPGRADE_BUTTON;
}

static bool isActivePageButton(const AppState& state, int id) {
    return isPageButtonId(id) && pageForControlId(id) == state.currentPage;
}

static bool usesButtonBadge(int id) {
    return isPageButtonId(id) || id == IDC_DISPLAY_MODE_BUTTON || id == IDC_FRONT_MENU_BUTTON || id == IDC_MENU_CONTINUE_BUTTON ||
           id == IDC_MENU_PLAY_BUTTON || id == IDC_MENU_LOAD_BUTTON || id == IDC_MENU_DELETE_SAVE_BUTTON || id == IDC_MENU_SETTINGS_BUTTON ||
           id == IDC_MENU_CREDITS_BUTTON || id == IDC_MENU_EXIT_BUTTON || id == IDC_MENU_BACK_BUTTON ||
           id == IDC_MENU_APPLY_SETTINGS_BUTTON || id == IDC_MENU_RESET_SETTINGS_BUTTON;
}

static DisplayMode displayModeForButton(const AppState& state) {
    if (state.isBorderlessFullscreen) return DisplayMode::BorderlessFullscreen;
    if (state.window && IsZoomed(state.window)) return DisplayMode::MaximizedWindow;
    return DisplayMode::RestoredWindow;
}

static bool titleMatches(const std::string& value, const char* expected) {
    return value == expected;
}

static bool isTeamLogoSubItem(const AppState& state, HWND list, int subItem) {
    const std::string title = panelTitleForList(state, list);
    if (title == "LeagueTableView") return subItem == 1;
    if (title == "FixtureListView") return subItem == 2;
    if (title == "RaceContext") return subItem == 1;
    if (title == "SeasonHistory" || title == "SeasonRecords") return subItem == 1;
    if (title == "TransferMarketView") return subItem == 10;
    return false;
}

static LRESULT drawTeamLogoSubItem(AppState& state, NMLVCUSTOMDRAW* draw, HWND list) {
    if (!draw || !list || !isTeamLogoSubItem(state, list, draw->iSubItem)) return CDRF_DODEFAULT;

    LVITEMW item{};
    item.mask = LVIF_IMAGE;
    item.iItem = static_cast<int>(draw->nmcd.dwItemSpec);
    item.iSubItem = draw->iSubItem;
    item.iImage = -1;
    ListView_GetItem(list, &item);
    if (item.iImage < 0) return CDRF_DODEFAULT;

    RECT rect{};
    if (!ListView_GetSubItemRect(list, item.iItem, item.iSubItem, LVIR_BOUNDS, &rect)) return CDRF_DODEFAULT;

    HIMAGELIST images = ListView_GetImageList(list, LVSIL_SMALL);
    int iconWidth = 0;
    int iconHeight = 0;
    if (images) {
        ImageList_GetIconSize(images, &iconWidth, &iconHeight);
        const int iconX = rect.left + 5;
        const int iconY = static_cast<int>(rect.top) +
                          std::max(0, (static_cast<int>(rect.bottom - rect.top) - iconHeight) / 2);
        ImageList_Draw(images, item.iImage, draw->nmcd.hdc, iconX, iconY, ILD_TRANSPARENT);
    }

    RECT textRect = rect;
    textRect.left += std::max(24, iconWidth + 10);
    textRect.right -= 6;
    const std::wstring text = utf8ToWide(listViewText(list, item.iItem, item.iSubItem));
    SetBkMode(draw->nmcd.hdc, TRANSPARENT);
    SetTextColor(draw->nmcd.hdc, (draw->nmcd.uItemState & CDIS_SELECTED) ? RGB(255, 255, 255) : kThemeText);
    HGDIOBJ oldFont = SelectObject(draw->nmcd.hdc,
                                   state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    DrawTextW(draw->nmcd.hdc, text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(draw->nmcd.hdc, oldFont);
    return CDRF_SKIPDEFAULT;
}

static wchar_t pageButtonGlyph(int id) {
    switch (id) {
        case IDC_PAGE_DASHBOARD_BUTTON: return L'H';
        case IDC_PAGE_SQUAD_BUTTON: return L'P';
        case IDC_PAGE_TACTICS_BUTTON: return L'T';
        case IDC_PAGE_CALENDAR_BUTTON: return L'C';
        case IDC_PAGE_LEAGUE_BUTTON: return L'L';
        case IDC_PAGE_TRANSFERS_BUTTON: return L'$';
        case IDC_PAGE_FINANCES_BUTTON: return L'F';
        case IDC_PAGE_YOUTH_BUTTON: return L'Y';
        case IDC_PAGE_BOARD_BUTTON: return L'D';
        case IDC_PAGE_NEWS_BUTTON: return L'N';
        default: return L' ';
    }
}

static bool isFrontMenuButtonId(int id) {
    return id == IDC_MENU_CONTINUE_BUTTON || id == IDC_MENU_PLAY_BUTTON || id == IDC_MENU_LOAD_BUTTON || 
           id == IDC_MENU_DELETE_SAVE_BUTTON ||  id == IDC_MENU_SETTINGS_BUTTON || id == IDC_MENU_CREDITS_BUTTON || id == IDC_MENU_EXIT_BUTTON ||
           id == IDC_MENU_BACK_BUTTON || id == IDC_MENU_VOLUME_BUTTON || id == IDC_MENU_DIFFICULTY_BUTTON ||
           id == IDC_MENU_SPEED_BUTTON || id == IDC_MENU_SIMULATION_BUTTON || id == IDC_MENU_LANGUAGE_BUTTON ||
           id == IDC_MENU_TEXT_SPEED_BUTTON || id == IDC_MENU_VISUAL_BUTTON ||
           id == IDC_MENU_MUSICMODE_BUTTON || id == IDC_MENU_AUDIOFADE_BUTTON ||
           id == IDC_MENU_APPLY_SETTINGS_BUTTON || id == IDC_MENU_RESET_SETTINGS_BUTTON;
}

static bool isFrontMenuSettingButtonId(int id) {
    return id == IDC_MENU_VOLUME_BUTTON || id == IDC_MENU_DIFFICULTY_BUTTON ||
           id == IDC_MENU_SPEED_BUTTON || id == IDC_MENU_SIMULATION_BUTTON || id == IDC_MENU_LANGUAGE_BUTTON ||
           id == IDC_MENU_TEXT_SPEED_BUTTON || id == IDC_MENU_VISUAL_BUTTON ||
           id == IDC_MENU_MUSICMODE_BUTTON || id == IDC_MENU_AUDIOFADE_BUTTON;
}

static bool isFrontMenuMainActionButtonId(int id) {
    return id == IDC_MENU_CONTINUE_BUTTON || id == IDC_MENU_PLAY_BUTTON || id == IDC_MENU_LOAD_BUTTON ||
           id == IDC_MENU_DELETE_SAVE_BUTTON || id == IDC_MENU_SETTINGS_BUTTON || id == IDC_MENU_CREDITS_BUTTON || id == IDC_MENU_EXIT_BUTTON ||
           id == IDC_MENU_BACK_BUTTON || id == IDC_MENU_APPLY_SETTINGS_BUTTON || id == IDC_MENU_RESET_SETTINGS_BUTTON;
}

static bool shouldRenderButtonOnCurrentPage(const AppState& state, int id) {
    if (id == IDC_MENU_CONTINUE_BUTTON || id == IDC_MENU_PLAY_BUTTON || id == IDC_MENU_LOAD_BUTTON ||
        id == IDC_MENU_DELETE_SAVE_BUTTON || id == IDC_MENU_SETTINGS_BUTTON || id == IDC_MENU_CREDITS_BUTTON || id == IDC_MENU_EXIT_BUTTON) {
        if (id == IDC_MENU_LOAD_BUTTON || id == IDC_MENU_DELETE_SAVE_BUTTON) {
            return state.currentPage == GuiPage::MainMenu || state.currentPage == GuiPage::Saves;
        }
        return state.currentPage == GuiPage::MainMenu;
    }
    if (isFrontMenuSettingButtonId(id)) {
        return state.currentPage == GuiPage::Settings;
    }
    if (id == IDC_MENU_APPLY_SETTINGS_BUTTON || id == IDC_MENU_RESET_SETTINGS_BUTTON) {
        return state.currentPage == GuiPage::Settings;
    }
    if (id == IDC_MENU_BACK_BUTTON) {
        return state.currentPage == GuiPage::Settings || state.currentPage == GuiPage::Credits || state.currentPage == GuiPage::Saves;
    }
    if (id == IDC_NEW_CAREER_BUTTON || id == IDC_LOAD_BUTTON || id == IDC_SAVE_BUTTON ||
        id == IDC_SIMULATE_BUTTON || id == IDC_VALIDATE_BUTTON || id == IDC_DISPLAY_MODE_BUTTON ||
        id == IDC_FRONT_MENU_BUTTON || id == IDC_EMPTY_NEW_BUTTON || id == IDC_EMPTY_LOAD_BUTTON ||
        id == IDC_EMPTY_VALIDATE_BUTTON || isPageButtonId(id) || isActionButtonId(id)) {
        return !isFrontMenuPage(state.currentPage);
    }
    return true;
}

static wchar_t buttonBadgeGlyph(int id, DisplayMode displayMode) {
    if (id == IDC_DISPLAY_MODE_BUTTON) {
        return displayMode == DisplayMode::BorderlessFullscreen ? L'W'
             : (displayMode == DisplayMode::MaximizedWindow ? L'F' : L'M');
    }
    if (id == IDC_MENU_CONTINUE_BUTTON) return L'C';
    if (id == IDC_MENU_PLAY_BUTTON) return L'J';
    if (id == IDC_MENU_LOAD_BUTTON) return L'L';
    if (id == IDC_MENU_DELETE_SAVE_BUTTON) return L'B';
    if (id == IDC_FRONT_MENU_BUTTON) return L'M';
    if (id == IDC_MENU_SETTINGS_BUTTON) return L'A';
    if (id == IDC_MENU_CREDITS_BUTTON) return L'R';
    if (id == IDC_MENU_EXIT_BUTTON) return L'S';
    if (id == IDC_MENU_BACK_BUTTON) return L'V';
    if (id == IDC_MENU_APPLY_SETTINGS_BUTTON) return L'A';
    if (id == IDC_MENU_RESET_SETTINGS_BUTTON) return L'O';
    return pageButtonGlyph(id);
}

void drawThemedButton(AppState& state, const DRAWITEMSTRUCT* drawItem) {
    if (!drawItem) return;
    int id = static_cast<int>(drawItem->CtlID);
    if (!IsWindow(drawItem->hwndItem) || !IsWindowVisible(drawItem->hwndItem) ||
        !shouldRenderButtonOnCurrentPage(state, id)) {
        return;
    }
    HDC hdc = drawItem->hDC;
    RECT rect = drawItem->rcItem;
    bool pressed = (drawItem->itemState & ODS_SELECTED) != 0;
    bool disabled = (drawItem->itemState & ODS_DISABLED) != 0;
    bool activePage = isActivePageButton(state, id);
    bool focused = GetFocus() == drawItem->hwndItem || (drawItem->itemState & ODS_FOCUS) != 0;

    COLORREF fill = kThemePanelAlt;
    COLORREF border = RGB(40, 62, 77);
    COLORREF text = kThemeText;
    const DisplayMode displayMode = displayModeForButton(state);
    if (isPageButtonId(id)) {
        fill = RGB(14, 24, 32);
        border = RGB(35, 56, 69);
    } else if (id == IDC_MENU_CONTINUE_BUTTON) {
        fill = RGB(16, 67, 74);
        border = RGB(74, 184, 196);
    } else if (id == IDC_MENU_PLAY_BUTTON) {
        fill = RGB(18, 75, 56);
        border = RGB(71, 180, 128);
    } else if (id == IDC_MENU_LOAD_BUTTON) {
        fill = RGB(25, 41, 60);
        border = RGB(93, 139, 198);
    } else if (id == IDC_MENU_SETTINGS_BUTTON) {
        fill = RGB(20, 46, 78);
        border = RGB(90, 149, 224);
    } else if (id == IDC_MENU_CREDITS_BUTTON) {
        fill = RGB(57, 45, 20);
        border = kThemeAccent;
    } else if (id == IDC_MENU_APPLY_SETTINGS_BUTTON) {
        fill = RGB(18, 75, 56);
        border = RGB(71, 180, 128);
    } else if (id == IDC_MENU_RESET_SETTINGS_BUTTON) {
        fill = RGB(57, 45, 20);
        border = kThemeWarning;
    } else if (id == IDC_MENU_EXIT_BUTTON) {
        fill = RGB(55, 27, 32);
        border = RGB(186, 92, 104);
    } else if (id == IDC_MENU_BACK_BUTTON) {
        fill = RGB(29, 39, 48);
        border = RGB(88, 112, 128);
    } else if (isFrontMenuSettingButtonId(id)) {
        fill = RGB(15, 31, 42);
        if (id == IDC_MENU_DIFFICULTY_BUTTON) border = kThemeAccent;
        else if (id == IDC_MENU_SPEED_BUTTON || id == IDC_MENU_TEXT_SPEED_BUTTON) border = kThemeWarning;
        else if (id == IDC_MENU_SIMULATION_BUTTON || id == IDC_MENU_VISUAL_BUTTON) border = kThemeAccentBlue;
        else if (id == IDC_MENU_VOLUME_BUTTON || id == IDC_MENU_MUSICMODE_BUTTON) border = kThemeAccentGreen;
        else if (id == IDC_MENU_LANGUAGE_BUTTON) border = RGB(109, 175, 221);
        else if (id == IDC_MENU_AUDIOFADE_BUTTON) border = RGB(128, 150, 163);
    } else if (id == IDC_DISPLAY_MODE_BUTTON) {
        fill = displayMode == DisplayMode::BorderlessFullscreen
            ? RGB(24, 58, 80)
            : (displayMode == DisplayMode::MaximizedWindow ? RGB(32, 48, 66) : RGB(42, 53, 30));
        border = kThemeAccentBlue;
    } else if (id == IDC_FRONT_MENU_BUTTON) {
        fill = RGB(26, 44, 64);
        border = RGB(105, 156, 219);
    } else if (isPrimaryButtonId(id)) {
        fill = RGB(19, 63, 49);
        border = kThemeAccentGreen;
    } else if (isUpgradeButtonId(id)) {
        fill = RGB(50, 39, 19);
        border = kThemeAccent;
    } else if (id == IDC_VALIDATE_BUTTON) {
        fill = RGB(30, 43, 70);
        border = kThemeAccentBlue;
    } else if (id == IDC_SAVE_BUTTON || id == IDC_LOAD_BUTTON) {
        fill = RGB(28, 40, 53);
    }
    if (activePage) {
        fill = RGB(68, 54, 20);
        border = kThemeAccent;
    }
    if (pressed) {
        fill = RGB(std::max(0, static_cast<int>(GetRValue(fill)) - 12),
                   std::max(0, static_cast<int>(GetGValue(fill)) - 12),
                   std::max(0, static_cast<int>(GetBValue(fill)) - 12));
    }
    if (disabled) {
        fill = RGB(28, 32, 38);
        border = RGB(48, 55, 62);
        text = RGB(103, 111, 118);
    }
    if (focused && !disabled) {
        border = RGB(235, 221, 176);
    }

    drawRoundedPanel(hdc, rect, fill, border, isFrontMenuButtonId(id) ? 16 : 12);

    RECT accentRect = rect;
    accentRect.right = accentRect.left + (isFrontMenuButtonId(id) ? 6 : 4);
    COLORREF accentColor = activePage ? kThemeAccent : (isPrimaryButtonId(id) ? kThemeAccentGreen : border);
    if (id == IDC_MENU_CONTINUE_BUTTON) accentColor = RGB(87, 196, 204);
    if (id == IDC_FRONT_MENU_BUTTON) accentColor = RGB(105, 156, 219);
    if (id == IDC_MENU_SETTINGS_BUTTON) accentColor = kThemeAccentBlue;
    if (id == IDC_MENU_LOAD_BUTTON) accentColor = RGB(110, 157, 215);
    if (id == IDC_MENU_CREDITS_BUTTON) accentColor = kThemeAccent;
    if (id == IDC_MENU_APPLY_SETTINGS_BUTTON) accentColor = kThemeAccentGreen;
    if (id == IDC_MENU_RESET_SETTINGS_BUTTON) accentColor = kThemeWarning;
    if (id == IDC_MENU_EXIT_BUTTON) accentColor = RGB(207, 101, 114);
    if (id == IDC_MENU_BACK_BUTTON) accentColor = RGB(108, 131, 145);
    if (id == IDC_MENU_DIFFICULTY_BUTTON) accentColor = kThemeAccent;
    if (id == IDC_MENU_SPEED_BUTTON) accentColor = kThemeWarning;
    if (id == IDC_MENU_SIMULATION_BUTTON) accentColor = kThemeAccentBlue;
    if (id == IDC_MENU_LANGUAGE_BUTTON) accentColor = RGB(109, 175, 221);
    if (id == IDC_MENU_TEXT_SPEED_BUTTON) accentColor = kThemeWarning;
    if (id == IDC_MENU_VISUAL_BUTTON) accentColor = kThemeAccentBlue;
    if (id == IDC_MENU_MUSICMODE_BUTTON) accentColor = kThemeAccentGreen;
    if (id == IDC_MENU_AUDIOFADE_BUTTON) accentColor = RGB(128, 150, 163);
    HBRUSH accentBrush = CreateSolidBrush(accentColor);
    FillRect(hdc, &accentRect, accentBrush);
    DeleteObject(accentBrush);

    if (focused && !disabled) {
        RECT focusRect = rect;
        InflateRect(&focusRect, -4, -4);
        HPEN focusPen = CreatePen(PS_SOLID, 1, RGB(238, 224, 170));
        HGDIOBJ oldPen = SelectObject(hdc, focusPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(hdc, focusRect.left, focusRect.top, focusRect.right, focusRect.bottom, 12, 12);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(focusPen);
    }

    wchar_t textBuffer[128]{};
    GetWindowTextW(drawItem->hwndItem, textBuffer, static_cast<int>(sizeof(textBuffer) / sizeof(textBuffer[0])));
    RECT textRect = rect;
    if (usesButtonBadge(id)) {
        RECT badgeRect{rect.left + 14, rect.top + 8, rect.left + 38, rect.bottom - 8};
        COLORREF badgeFill = RGB(28, 46, 58);
        if (id == IDC_DISPLAY_MODE_BUTTON) badgeFill = kThemeAccentBlue;
        else if (id == IDC_FRONT_MENU_BUTTON) badgeFill = RGB(105, 156, 219);
        else if (activePage) badgeFill = kThemeAccent;
        else if (id == IDC_MENU_CONTINUE_BUTTON) badgeFill = RGB(87, 196, 204);
        else if (id == IDC_MENU_PLAY_BUTTON) badgeFill = kThemeAccentGreen;
        else if (id == IDC_MENU_LOAD_BUTTON) badgeFill = RGB(110, 157, 215);
        else if (id == IDC_MENU_SETTINGS_BUTTON) badgeFill = kThemeAccentBlue;
        else if (id == IDC_MENU_CREDITS_BUTTON) badgeFill = kThemeAccent;
        else if (id == IDC_MENU_APPLY_SETTINGS_BUTTON) badgeFill = kThemeAccentGreen;
        else if (id == IDC_MENU_RESET_SETTINGS_BUTTON) badgeFill = kThemeWarning;
        else if (id == IDC_MENU_EXIT_BUTTON) badgeFill = RGB(207, 101, 114);
        else if (id == IDC_MENU_BACK_BUTTON) badgeFill = RGB(108, 131, 145);
        HBRUSH badgeBrush = CreateSolidBrush(badgeFill);
        HGDIOBJ oldBrush = SelectObject(hdc, badgeBrush);
        HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, badgeRect.left, badgeRect.top, badgeRect.right, badgeRect.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(badgeBrush);

        RECT badgeText = badgeRect;
        SetTextColor(hdc,
                     id == IDC_DISPLAY_MODE_BUTTON ? RGB(10, 24, 35)
                                                   : (activePage ? RGB(34, 27, 8) : RGB(225, 234, 239)));
        HGDIOBJ oldBadgeFont = SelectObject(hdc, state.sectionFont ? state.sectionFont : state.font);
        const wchar_t glyph = buttonBadgeGlyph(id, displayMode);
        wchar_t iconText[2]{glyph, L'\0'};
        DrawTextW(hdc, iconText, -1, &badgeText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldBadgeFont);

        textRect.left += 46;
    } else {
        textRect.left += 6;
    }
    if (pressed) OffsetRect(&textRect, 0, 1);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text);
    HGDIOBJ oldFont = SelectObject(hdc,
                                   isFrontMenuMainActionButtonId(id)
                                       ? (state.sectionFont ? state.sectionFont : state.font)
                                       : (state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))));
    DrawTextW(hdc,
              textBuffer,
              -1,
              &textRect,
              (usesButtonBadge(id) ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
}

LRESULT handleListCustomDraw(AppState& state, LPNMHDR header) {
    if (header->code != NM_CUSTOMDRAW)
{
    return CDRF_DODEFAULT;
}
    HWND tableHeader = state.tableList ? ListView_GetHeader(state.tableList) : nullptr;
    HWND squadHeader = state.squadList ? ListView_GetHeader(state.squadList) : nullptr;
    HWND transferHeader = state.transferList ? ListView_GetHeader(state.transferList) : nullptr;

    if (header->hwndFrom == tableHeader || header->hwndFrom == squadHeader || header->hwndFrom == transferHeader) {
        NMCUSTOMDRAW* draw = reinterpret_cast<NMCUSTOMDRAW*>(header);
        if (draw->dwDrawStage == CDDS_PREPAINT) {
            return CDRF_NOTIFYITEMDRAW;
        }
        if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
            int item = static_cast<int>(draw->dwItemSpec);
            RECT rect{};
            Header_GetItemRect(header->hwndFrom, item, &rect);
            HBRUSH fillBrush = CreateSolidBrush(RGB(21, 35, 46));
            FillRect(draw->hdc, &rect, fillBrush);
            DeleteObject(fillBrush);

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(56, 79, 96));
            HGDIOBJ oldPen = SelectObject(draw->hdc, pen);
            MoveToEx(draw->hdc, rect.left, rect.bottom - 1, nullptr);
            LineTo(draw->hdc, rect.right, rect.bottom - 1);
            SelectObject(draw->hdc, oldPen);
            DeleteObject(pen);

            wchar_t buffer[160]{};
            HDITEMW itemData{};
            itemData.mask = HDI_TEXT;
            itemData.pszText = buffer;
            itemData.cchTextMax = static_cast<int>(sizeof(buffer) / sizeof(buffer[0]));
            SendMessageW(header->hwndFrom, HDM_GETITEMW, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&itemData));

            RECT textRect = rect;
            textRect.left += 10;
            textRect.right -= 8;
            SetBkMode(draw->hdc, TRANSPARENT);
            SetTextColor(draw->hdc, RGB(232, 238, 242));
            HGDIOBJ oldFont = SelectObject(draw->hdc, state.font ? state.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
            DrawTextW(draw->hdc, buffer, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(draw->hdc, oldFont);
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_DODEFAULT;
    }

    if (header->idFrom != IDC_TABLE_LIST && header->idFrom != IDC_SQUAD_LIST && header->idFrom != IDC_TRANSFER_LIST) {
        return CDRF_DODEFAULT;
    }

    auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }
    if (draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
        return drawTeamLogoSubItem(state, draw, header->hwndFrom);
    }
    if (draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT) {
        return CDRF_DODEFAULT;
    }

    int row = static_cast<int>(draw->nmcd.dwItemSpec);
    COLORREF bg = (row % 2 == 0) ? kThemeInput : RGB(14, 24, 33);
    COLORREF text = kThemeText;

    if (header->idFrom == IDC_TABLE_LIST) {
        std::string first = listViewText(state.tableList, row, 0);
        std::string second = listViewText(state.tableList, row, 1);
        if (titleMatches(state.currentModel.primary.title, "LeagueTableView")) {
            int totalRows = ListView_GetItemCount(state.tableList);
            int position = std::atoi(first.c_str());
            int relegationStart = std::max(1, totalRows - 1);
            int continentalSpots = std::max(1, totalRows / 4);
            if (second.find('*') != std::string::npos) {
                bg = RGB(24, 71, 54);
                text = RGB(242, 252, 247);
            } else if (position > 0 && position <= continentalSpots) {
                bg = RGB(20, 67, 49);
            } else if (position >= relegationStart) {
                bg = RGB(88, 34, 38);
                text = RGB(255, 237, 237);
            } else if (position > totalRows / 2) {
                bg = RGB(79, 64, 20);
            }
        } else if (second.find('*') != std::string::npos) {
            bg = RGB(22, 66, 51);
        } else if (first == "1" || row == 0) {
            bg = RGB(58, 48, 17);
            text = RGB(255, 244, 208);
        }
    } else if (header->idFrom == IDC_SQUAD_LIST) {
        if (titleMatches(state.currentModel.secondary.title, "TeamStatusPanel")) {
            std::string level = listViewText(state.squadList, row, 1);
            if (level == "Alta" && row >= 2) {
                bg = RGB(88, 34, 38);
                text = RGB(255, 237, 237);
            } else if (level == "Alta") {
                bg = RGB(21, 69, 52);
            } else if (level == "Media") {
                bg = RGB(84, 68, 24);
            } else if (level == "Baja" && row <= 1) {
                bg = RGB(88, 34, 38);
                text = RGB(255, 237, 237);
            } else if (level == "Baja") {
                bg = RGB(21, 69, 52);
            } else if (level == "Sin datos") {
                bg = RGB(33, 45, 56);
            }
        } else {
            std::string fitness = listViewText(state.squadList, row, 7);
            std::string morale = listViewText(state.squadList, row, 5);
            std::string status = listViewText(state.squadList, row, std::max(0, Header_GetItemCount(ListView_GetHeader(state.squadList)) - 1));
            int fitnessValue = fitness.empty() ? 80 : std::atoi(fitness.c_str());
            int moraleValue = morale.empty() ? 60 : std::atoi(morale.c_str());
            if (status.find("Les") != std::string::npos) {
                bg = RGB(82, 34, 36);
            } else if (fitnessValue < 62 || moraleValue < 45) {
                bg = RGB(82, 62, 20);
            } else if (fitnessValue >= 85 && moraleValue >= 65) {
                bg = RGB(19, 62, 47);
            }
        }
    } else if (header->idFrom == IDC_TRANSFER_LIST) {
        if (titleMatches(state.currentModel.footer.title, "InjuryListWidget")) {
            std::string issueType = listViewText(state.transferList, row, 1);
            std::string note = listViewText(state.transferList, row, 4);
            if (issueType == "-" && note.find("Plantilla completa") != std::string::npos) {
                bg = RGB(21, 69, 52);
            } else if (issueType.find("Fatiga") != std::string::npos) {
                bg = RGB(84, 68, 24);
            } else {
                bg = RGB(88, 34, 38);
                text = RGB(255, 237, 237);
            }
        } else {
            std::string type = listViewText(state.transferList, row, 0);
            if (type.find("Mercado") != std::string::npos || type.find("Objetivo") != std::string::npos) {
                bg = RGB(19, 61, 47);
            } else if (type.find("Contrato") != std::string::npos || type.find("Precontrato") != std::string::npos) {
                bg = RGB(66, 50, 19);
            } else if (type.find("Salida") != std::string::npos) {
                bg = RGB(78, 33, 35);
            } else if (type.find("Prestamo") != std::string::npos) {
                bg = RGB(24, 42, 64);
            }
        }
    }

    if ((draw->nmcd.uItemState & CDIS_SELECTED) != 0) {
        bg = kThemeSelection;
        text = RGB(255, 255, 255);
    }

    draw->clrText = text;
    draw->clrTextBk = bg;
    return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
}

}  // namespace gui_win32

#endif
