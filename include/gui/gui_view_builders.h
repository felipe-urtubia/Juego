#pragma once

#ifdef _WIN32

#include "ai/ai_transfer_manager.h"
#include "gui/gui_internal.h"

#include <cstddef>
#include <string>
#include <vector>

namespace gui_win32 {

struct TransferPreviewItem {
    std::string player;
    std::string position;
    int age = 0;
    int skill = 0;
    int potential = 0;
    std::string skillLabel;
    std::string potentialLabel;
    long long expectedFee = 0;
    long long expectedWage = 0;
    long long expectedAgentFee = 0;
    std::string expectedFeeLabel;
    std::string expectedWageLabel;
    std::string expectedAgentFeeLabel;
    std::string club;
    std::string scoutingLabel;
    std::string marketLabel;
    std::string expectedRole;
    std::string scoutingNote;
    std::string competitionLabel;
    std::string actionLabel;
    std::string packageLabel;
    bool onShortlist = false;
    bool urgentNeed = false;
    int scoutingConfidence = 0;
    int affordabilityScore = 0;
    double totalScore = 0.0;
};

std::string pageTitleFor(GuiPage page);
std::string breadcrumbFor(GuiPage page);
bool isCongestedWeek(const Career& career);
std::string trainingSchedulePreview(const Team& team, bool congestedWeek, std::size_t limit = 3);
std::string boardStatusLabel(int confidence);
std::vector<DashboardMetric> buildMetrics(const AppState& state, const std::vector<std::string>& alerts);
LeagueTable selectedLeagueTable(const AppState& state);
std::string findNextMatchLine(const Career& career);
std::vector<std::vector<std::string> > buildFixtureRows(const Career& career, int limit);
std::vector<std::string> buildAlertLines(const Career& career);
std::string teamConditionLabel(int averageFitness);
std::string teamMoraleLabel(int morale);
std::string severityLabel(int value, int high, int medium);
bool newsMatchesFilter(const std::string& line, const std::string& filter);
std::string lastMatchPanelText(const Career& career, std::size_t maxReportLines, std::size_t maxEvents);
std::string dressingRoomPanelText(const Career& career, std::size_t maxAlerts);
std::vector<std::string> buildFeedLines(const Career& career, const std::string& filter, std::size_t limit = 18);
std::string playerStatus(const Player& player);
int playerSortMetric(const Player& player, int column);
bool playerMatchesFilter(const Team& team, const Player& player, const std::string& filter);
bool youthMatchesFilter(const Player& player, const std::string& filter);
std::string expectedRoleLabel(const Team& team, const Player& player);
std::string playerInterestLabel(const Career& career, const Team& seller, const Player& player);
std::string sellerInterestLabel(const Team& seller, const Player& player);
std::string transferRadarLabel(const TransferTarget& target);
std::string transferMarketLabel(const TransferTarget& target);
std::string scoutingRiskLabel(int confidence);
std::string scoutingNextStepLabel(const TransferPreviewItem& target);
const Player* findPlayerByName(const Team& team, const std::string& name);
std::string buildPlayerProfile(const Team& team, const Player* player);
ListPanelModel buildTeamStatusModel(const Career& career);
ListPanelModel buildLeagueTableModel(const Career& career, const std::string& filter);
ListPanelModel buildInjuryModel(const Career& career);
ListPanelModel buildFixtureModel(const Career& career, int limit);
ListPanelModel buildSquadUnitModel(const Team& team);
ListPanelModel buildPlayerTableModel(AppState& state, bool youthOnly);
ListPanelModel buildComparisonModel(const Career& career, const Player* selected);
ListPanelModel buildTransferPipelineModel(const Career& career);
std::vector<TransferPreviewItem> buildTransferTargets(const Career& career, const std::string& filter, const std::string& searchQuery = std::string());

GuiPageModel buildDashboardModel(AppState& state);
GuiPageModel buildMainMenuModel(AppState& state);
GuiPageModel buildSettingsPageModel(AppState& state);
GuiPageModel buildCreditsPageModel(AppState& state);
GuiPageModel buildSavesPageModel(AppState& state);
GuiPageModel buildSquadModel(AppState& state, bool youthOnly);
GuiPageModel buildTacticsModel(AppState& state);
GuiPageModel buildCalendarModel(AppState& state);
GuiPageModel buildLeagueModel(AppState& state);
GuiPageModel buildTransfersModel(AppState& state);
GuiPageModel buildFinancesModel(AppState& state);
GuiPageModel buildBoardModel(AppState& state);
GuiPageModel buildNewsModel(AppState& state);
GuiPageModel buildModel(AppState& state);

}  // namespace gui_win32

#endif
