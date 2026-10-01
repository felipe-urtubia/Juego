#include "gui/gui_view_builders.h"

#ifdef _WIN32

namespace gui_win32 {

GuiPageModel buildModel(AppState& state) {
    switch (state.currentPage) {
        case GuiPage::MainMenu: return buildMainMenuModel(state);
        case GuiPage::Settings: return buildSettingsPageModel(state);
        case GuiPage::Credits: return buildCreditsPageModel(state);
        case GuiPage::Saves: return buildSavesPageModel(state);
        case GuiPage::NewCareer: return buildDashboardModel(state);
        case GuiPage::Dashboard: return buildDashboardModel(state);
        case GuiPage::Squad: return buildSquadModel(state, false);
        case GuiPage::Tactics: return buildTacticsModel(state);
        case GuiPage::Calendar: return buildCalendarModel(state);
        case GuiPage::League: return buildLeagueModel(state);
        case GuiPage::Transfers: return buildTransfersModel(state);
        case GuiPage::Finances: return buildFinancesModel(state);
        case GuiPage::Youth: return buildSquadModel(state, true);
        case GuiPage::Board: return buildBoardModel(state);
        case GuiPage::News: return buildNewsModel(state);
    }
    return buildDashboardModel(state);
}

}  // namespace gui_win32

#endif
