#include "career/board_objective.h"
#include "engine/models.h"

#include "competition/competition.h"
#include "competition/league_registry.h"
#include "io/io.h"
#include "utils/utils.h"
#include "validators/validators.h"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

using namespace std;

namespace {

vector<vector<pair<int, int>>> buildRoundRobinSchedule(const vector<int>& teamIdx, bool doubleRound) {
    vector<vector<pair<int, int>>> out;
    if (teamIdx.size() < 2) return out;

    vector<int> idx = teamIdx;
    if (idx.size() % 2 == 1) idx.push_back(-1);

    int size = static_cast<int>(idx.size());
    int rounds = size - 1;
    for (int round = 0; round < rounds; ++round) {
        vector<pair<int, int>> matches;
        for (int i = 0; i < size / 2; ++i) {
            int a = idx[static_cast<size_t>(i)];
            int b = idx[static_cast<size_t>(size - 1 - i)];
            if (a == -1 || b == -1) continue;
            matches.push_back(round % 2 == 0 ? pair<int, int>{a, b} : pair<int, int>{b, a});
        }
        out.push_back(matches);
        int last = idx.back();
        for (int i = size - 1; i > 1; --i) idx[static_cast<size_t>(i)] = idx[static_cast<size_t>(i - 1)];
        idx[1] = last;
    }

    if (doubleRound) {
        int base = static_cast<int>(out.size());
        for (int i = 0; i < base; ++i) {
            vector<pair<int, int>> reverseMatches;
            for (const auto& match : out[static_cast<size_t>(i)]) {
                reverseMatches.push_back({match.second, match.first});
            }
            out.push_back(reverseMatches);
        }
    }
    return out;
}

LeagueTable buildBoardTable(const vector<Team*>& teams, const string& ruleId) {
    LeagueTable table;
    table.ruleId = ruleId;
    for (Team* team : teams) {
        if (team) table.addTeam(team);
    }
    table.sortTable();
    return table;
}

Team* activeTeamForLegacyTable(const Career& career, int index) {
    return const_cast<Team*>(career.getActiveTeamAt(index));
}

bool activeTeamIdsAreSynced(const Career& career) {
    if (career.activeTeamIds.empty()) return false;
    if (career.activeTeamIds.size() != career.activeTeams.size()) return false;
    for (size_t i = 0; i < career.activeTeamIds.size(); ++i) {
        const TeamId id = career.activeTeamIds[i];
        if (id == kInvalidTeamId) return false;
        if (career.getTeamById(id) != career.activeTeams[i]) return false;
    }
    return true;
}

vector<Team*> boardRelevantTeams(const Career& career) {
    vector<Team*> teams;
    if (!career.myTeam) return teams;
    if (career.usesGroupFormat()) {
        const auto& idx = [&career]() -> const vector<int>& {
            for (int i : career.groupNorthIdx) {
                if (activeTeamForLegacyTable(career, i) == career.myTeam) {
                    return career.groupNorthIdx;
                }
            }
            return career.groupSouthIdx;
        }();
        for (int i : idx) {
            if (Team* team = activeTeamForLegacyTable(career, i)) teams.push_back(team);
        }
        if (!teams.empty()) return teams;
    }
    teams.reserve(static_cast<size_t>(career.getActiveTeamCount()));
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        if (Team* team = activeTeamForLegacyTable(career, i)) teams.push_back(team);
    }
    return teams;
}

int youthPlayersUsed(const Team& team) {
    int count = 0;
    for (const auto& player : team.players) {
        if (player.age <= 20 && player.matchesPlayed > 0) count++;
    }
    return count;
}

bool promiseCurrentlyAtRisk(const Player& player, int currentWeek) {
    if (player.promisedRole == "Titular") {
        return player.startsThisSeason + 2 < max(2, currentWeek * 2 / 3);
    }
    if (player.promisedRole == "Rotacion") {
        return player.startsThisSeason + 1 < max(1, currentWeek / 3);
    }
    if (player.promisedRole == "Proyecto") {
        return player.age <= 22 && player.startsThisSeason < max(1, currentWeek / 4);
    }
    return false;
}

int promisesAtRiskForBoard(const Team& team, int currentWeek) {
    int total = 0;
    for (const auto& player : team.players) {
        if (promiseCurrentlyAtRisk(player, currentWeek)) total++;
    }
    return total;
}

}  // namespace

Career::Career()
    : myTeam(nullptr),
      currentSeason(1),
      currentWeek(1),
      saveFile("saves/career_save.txt"),
      managerName("Manager"),
      managerReputation(50),
      activeHumanManagerIndex(0),
      boardConfidence(60),
      boardExpectedFinish(0),
      boardBudgetTarget(0),
      boardYouthTarget(1),
      boardWarningWeeks(0),
      boardMonthlyTarget(0),
      boardMonthlyProgress(0),
      boardMonthlyDeadlineWeek(0),
      cupActive(false),
      cupRound(0),
      managerStress(initializeManagerStress()),
      initialized(false) {
    // Inicializar sistemas de gameplay
    infrastructure.levels.trainingGround = 2;
    infrastructure.levels.youthAcademy = 1;
    infrastructure.levels.medical = 2;
    infrastructure.levels.stadium = 2;
    infrastructure.levels.facilities = 1;
    infrastructure.totalUpgradeCapacity = 2000000;
}

Team* Career::getMyTeamSafe() {
    return myTeam;
}

const Team* Career::getMyTeamSafe() const {
    return myTeam;
}

Team* Career::getTeamAtIndexSafe(int index) {
    if (!isValidTeamIndex(index)) return nullptr;
    return &allTeams[static_cast<size_t>(index)];
}

const Team* Career::getTeamAtIndexSafe(int index) const {
    if (!isValidTeamIndex(index)) return nullptr;
    return &allTeams[static_cast<size_t>(index)];
}

int Career::getActiveTeamsCount() const {
    return getActiveTeamCount();
}

bool Career::isValidTeamIndex(int index) const {
    return index >= 0 && index < static_cast<int>(allTeams.size());
}

bool Career::usesSegundaFormat() const {
    const CompetitionConfig& config = getCompetitionConfig(activeDivision);
    return config.seasonHandler == CompetitionSeasonHandler::SegundaGroups &&
           (config.expectedTeamCount <= 0 || getActiveTeamCount() == config.expectedTeamCount);
}

bool Career::usesTerceraBFormat() const {
    const CompetitionConfig& config = getCompetitionConfig(activeDivision);
    return config.seasonHandler == CompetitionSeasonHandler::TerceraB &&
           (config.expectedTeamCount <= 0 || getActiveTeamCount() == config.expectedTeamCount);
}

bool Career::usesGroupFormat() const {
    return competitionUsesGroupStage(activeDivision, getActiveTeamCount());
}

void Career::buildSegundaGroups() {
    buildRegionalGroups();
}

void Career::buildRegionalGroups() {
    groupNorthIdx.clear();
    groupSouthIdx.clear();
    if (!usesGroupFormat()) return;
    const CompetitionConfig& config = getCompetitionConfig(activeDivision);
    int split = config.groups.groupSize;
    if (split <= 0) return;
    for (int i = 0; i < getActiveTeamCount(); ++i) {
        if (i < split) groupNorthIdx.push_back(i);
        else groupSouthIdx.push_back(i);
    }
}

void Career::initializeLeague(bool forceReload) {
    if (initialized && !forceReload) return;
    myTeam = nullptr;
    allTeams.clear();
    activeTeams.clear();
    activeTeamIds.clear();
    schedule.clear();
    leagueTable.clear();
    groupNorthIdx.clear();
    groupSouthIdx.clear();
    divisions.clear();
    activeDivision.clear();
    loadWarnings.clear();

    reloadCompetitionConfigs();
    const vector<string>& configWarnings = competitionConfigWarnings();
    loadWarnings.insert(loadWarnings.end(), configWarnings.begin(), configWarnings.end());

    for (const auto& div : kDivisions) {
        if (!isDirectory(div.folder)) continue;
        DivisionLoadResult divisionLoad = loadDivisionFromFolder(div.folder, div.id, allTeams);
        loadWarnings.insert(loadWarnings.end(), divisionLoad.warnings.begin(), divisionLoad.warnings.end());
        if (!divisionLoad.teams.empty()) {
            divisions.push_back(div);
        }
    }
    ExternalJsonLoadResult externalJson = loadExternalJsonData(allTeams);
    loadWarnings.insert(loadWarnings.end(), externalJson.warnings.begin(), externalJson.warnings.end());
    auto addDivisionIfMissing = [&](const DivisionInfo& division) {
        if (division.id.empty()) return;
        const bool exists = any_of(divisions.begin(), divisions.end(), [&](const DivisionInfo& current) {
            return current.id == division.id;
        });
        if (!exists) divisions.push_back(division);
    };
    for (const auto& division : externalJson.divisions) addDivisionIfMissing(division);
    for (const auto& team : allTeams) {
        if (team.division.empty()) continue;
        addDivisionIfMissing({team.division, "data/teams.json", divisionDisplay(team.division)});
    }
    for (auto& team : allTeams) ensureTeamIdentity(team);

    const StartupValidationSummary startupValidation = buildStartupValidationSummary(6);
    for (const string& line : startupValidation.lines) {
        if (line == "Auditoria automatica de datos externos") continue;
        loadWarnings.push_back(line);
    }

    const RuntimeValidationSummary validation = validateLoadedCareerData(*this, 10);
    for (const string& line : validation.lines) {
        if (line == "Validacion automatica de carga") continue;
        loadWarnings.push_back(line);
    }
    initialized = true;
}

vector<Team*> Career::getDivisionTeams(const string& id) {
    vector<Team*> out;
    const string canonicalId = canonicalDivisionId(id);
    for (auto& team : allTeams) {
        if (canonicalDivisionId(team.division) == canonicalId) out.push_back(&team);
    }
    return out;
}

void Career::syncActiveTeamIds() {
    activeTeamIds.clear();
    activeTeamIds.reserve(activeTeams.size());
    for (const Team* team : activeTeams) {
        activeTeamIds.push_back(getTeamIdFor(team));
    }
}

bool Career::hasSyncedActiveTeamIds() const {
    return activeTeamIdsAreSynced(*this);
}

void Career::rebuildActiveLeagueTable() {
    leagueTable.clear();
    leagueTable.title = divisionDisplay(activeDivision);
    leagueTable.ruleId = activeDivision;
    for (int i = 0; i < getActiveTeamCount(); ++i) {
        if (Team* team = getActiveTeamAt(i)) leagueTable.addTeam(team);
    }
    leagueTable.sortTable();
}

void Career::refreshActiveDivisionTeamLinks() {
    refreshActiveDivisionTeamLinks(activeDivision);
}

void Career::refreshActiveDivisionTeamLinks(const string& id) {
    activeDivision = canonicalDivisionId(id);
    activeTeams = activeDivision.empty() ? vector<Team*>() : getDivisionTeams(activeDivision);
    syncActiveTeamIds();
    rebuildActiveLeagueTable();
}

void Career::buildSchedule() {
    schedule.clear();
    int n = getActiveTeamCount();
    if (n < 2) return;

    if (usesGroupFormat()) {
        const CompetitionConfig& config = getCompetitionConfig(activeDivision);
        if (groupNorthIdx.empty() || groupSouthIdx.empty()) buildRegionalGroups();
        auto north = buildRoundRobinSchedule(groupNorthIdx, config.groups.doubleRoundRobin);
        auto south = buildRoundRobinSchedule(groupSouthIdx, config.groups.doubleRoundRobin);
        int rounds = static_cast<int>(max(north.size(), south.size()));
        for (int r = 0; r < rounds; ++r) {
            vector<pair<int, int>> matches;
            if (r < static_cast<int>(north.size())) {
                matches.insert(matches.end(), north[static_cast<size_t>(r)].begin(), north[static_cast<size_t>(r)].end());
            }
            if (r < static_cast<int>(south.size())) {
                matches.insert(matches.end(), south[static_cast<size_t>(r)].begin(), south[static_cast<size_t>(r)].end());
            }
            schedule.push_back(matches);
        }
        return;
    }

    vector<int> idx;
    idx.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) idx.push_back(i);
    schedule = buildRoundRobinSchedule(idx, true);
}

void Career::setActiveDivision(const string& id) {
    refreshActiveDivisionTeamLinks(id);
    groupNorthIdx.clear();
    groupSouthIdx.clear();
    buildRegionalGroups();
    buildSchedule();
}

void Career::resetSeason() {
    activePromises.clear();
    scoutingAssignments.clear();
    for (auto& team : allTeams) {
        team.resetSeasonStats();
        team.morale = 50;
        for (auto& player : team.players) {
            player.injured = false;
            player.injuryWeeks = 0;
            player.injuryType.clear();
            player.fitness = player.stamina;
            player.yellowAccumulation = 0;
            player.seasonYellowCards = 0;
            player.seasonRedCards = 0;
            player.matchesSuspended = 0;
            player.goals = 0;
            player.assists = 0;
            player.matchesPlayed = 0;
            player.startsThisSeason = 0;
            player.wantsToLeave = false;
            player.moraleMomentum = 0;
            player.fatigueLoad = 0;
            player.unhappinessWeeks = 0;
            player.happiness = clampInt(player.happiness + 5, 1, 99);
            player.currentForm = clampInt(44 + player.professionalism / 4 + randInt(-8, 8), 20, 88);
            ensurePlayerProfile(player, false);
        }
    }
    buildSchedule();
    initializeBoardObjectives();
    initializeSeasonCup();
    initializeDynamicObjective();
    lastMatchAnalysis.clear();
    lastMatchReportLines.clear();
    lastMatchEvents.clear();
    lastMatchPlayerOfTheMatch.clear();
    lastMatchCenter = MatchCenterSnapshot{};
    scoutInbox.clear();
}

void Career::agePlayers() {
    for (auto& team : allTeams) {
        for (auto& player : team.players) {
            player.age++;
            if (player.age > 30) {
                player.skill = max(1, player.skill - 1);
                player.stamina = max(1, player.stamina - 1);
            }
            if (player.fitness > player.stamina) player.fitness = player.stamina;
            if (player.potential < player.skill) player.potential = player.skill;
            player.moraleMomentum = clampInt(player.moraleMomentum / 2, -25, 25);
            player.fatigueLoad = max(0, player.fatigueLoad - 8);
            player.unhappinessWeeks = 0;
            player.currentForm = clampInt((player.currentForm + 50) / 2 + randInt(-4, 4), 15, 90);
            ensurePlayerProfile(player, false);
        }
    }
}

Team* Career::findTeamByName(const string& name) {
    for (auto& team : allTeams) {
        if (team.name == name) return &team;
    }
    return nullptr;
}

const Team* Career::findTeamByName(const string& name) const {
    for (const auto& team : allTeams) {
        if (team.name == name) return &team;
    }
    return nullptr;
}

TeamId Career::getTeamIdFor(const Team* team) const {
    if (!team) return kInvalidTeamId;
    for (size_t i = 0; i < allTeams.size(); ++i) {
        if (&allTeams[i] == team) return static_cast<TeamId>(i);
    }
    return kInvalidTeamId;
}

TeamId Career::getTeamIdByName(const string& name) const {
    for (size_t i = 0; i < allTeams.size(); ++i) {
        if (allTeams[i].name == name) return static_cast<TeamId>(i);
    }
    return kInvalidTeamId;
}

Team* Career::getTeamById(TeamId id) {
    if (id < 0 || id >= static_cast<TeamId>(allTeams.size())) return nullptr;
    return &allTeams[static_cast<size_t>(id)];
}

const Team* Career::getTeamById(TeamId id) const {
    if (id < 0 || id >= static_cast<TeamId>(allTeams.size())) return nullptr;
    return &allTeams[static_cast<size_t>(id)];
}

TeamId Career::getActiveTeamIdAt(int index) const {
    if (index < 0) return kInvalidTeamId;
    const size_t activeIndex = static_cast<size_t>(index);
    if (hasSyncedActiveTeamIds() && activeIndex < activeTeamIds.size()) {
        const TeamId id = activeTeamIds[activeIndex];
        if (getTeamById(id)) return id;
    }
    if (activeIndex >= activeTeams.size()) return kInvalidTeamId;
    return getTeamIdFor(activeTeams[activeIndex]);
}

int Career::getActiveTeamCount() const {
    if (hasSyncedActiveTeamIds()) return static_cast<int>(activeTeamIds.size());
    return static_cast<int>(activeTeams.size());
}

Team* Career::getActiveTeamAt(int index) {
    const TeamId id = getActiveTeamIdAt(index);
    if (Team* team = getTeamById(id)) return team;
    if (index < 0 || index >= static_cast<int>(activeTeams.size())) return nullptr;
    return activeTeams[static_cast<size_t>(index)];
}

const Team* Career::getActiveTeamAt(int index) const {
    const TeamId id = getActiveTeamIdAt(index);
    if (const Team* team = getTeamById(id)) return team;
    if (index < 0 || index >= static_cast<int>(activeTeams.size())) return nullptr;
    return activeTeams[static_cast<size_t>(index)];
}

int Career::getAllTeamCount() const {
    return static_cast<int>(allTeams.size());
}

Team* Career::getTeamAt(int index) {
    if (index < 0 || index >= static_cast<int>(allTeams.size())) return nullptr;
    return &allTeams[static_cast<size_t>(index)];
}

const Team* Career::getTeamAt(int index) const {
    if (index < 0 || index >= static_cast<int>(allTeams.size())) return nullptr;
    return &allTeams[static_cast<size_t>(index)];
}

Team* Career::setMyTeamByName(const string& teamName) {
    myTeam = findTeamByName(teamName);
    return myTeam;
}

void Career::addInboxItem(const string& item, const string& channel) {
    if (item.empty()) return;
    string entry = channel.empty() ? item : ("[" + channel + "] " + item);
    managerInbox.push_back(entry);
    if (managerInbox.size() > 60) {
        managerInbox.erase(managerInbox.begin(), managerInbox.begin() + static_cast<long long>(managerInbox.size() - 60));
    }
}

void Career::addNews(const string& item) {
    if (item.empty()) return;
    string entry = "T" + to_string(currentSeason) + "-F" + to_string(currentWeek) + ": " + item;
    newsFeed.push_back(entry);
    if (newsFeed.size() > 40) {
        newsFeed.erase(newsFeed.begin(), newsFeed.begin() + static_cast<long long>(newsFeed.size() - 40));
    }
    const string lower = toLower(item);
    if (lower.find("[mundo]") != string::npos || lower.find("promesa") != string::npos || lower.find("fich") != string::npos ||
        lower.find("lesion") != string::npos || lower.find("directiva") != string::npos || lower.find("record") != string::npos) {
        addInboxItem(entry, "Resumen");
    }
}

void Career::executePendingTransfers() {
    for (size_t i = 0; i < pendingTransfers.size();) {
        PendingTransfer& move = pendingTransfers[i];
        if (move.effectiveSeason > currentSeason) {
            ++i;
            continue;
        }
        Team* from = findTeamByName(move.fromTeam);
        Team* to = findTeamByName(move.toTeam);
        if (!from || !to) {
            pendingTransfers.erase(pendingTransfers.begin() + static_cast<long long>(i));
            continue;
        }
        int playerIdx = -1;
        for (size_t p = 0; p < from->players.size(); ++p) {
            if (from->players[p].name == move.playerName) {
                playerIdx = static_cast<int>(p);
                break;
            }
        }
        if (playerIdx < 0) {
            pendingTransfers.erase(pendingTransfers.begin() + static_cast<long long>(i));
            continue;
        }
        Player player = from->players[static_cast<size_t>(playerIdx)];
        if (move.preContract) {
            player.onLoan = false;
            player.parentClub.clear();
            player.loanWeeksRemaining = 0;
            if (move.wage > 0) player.wage = move.wage;
            if (move.contractWeeks > 0) player.contractWeeks = move.contractWeeks;
            player.releaseClause = max(player.value * 2, player.wage * 45);
            if (!move.promisedRole.empty()) {
                player.promisedRole = move.promisedRole;
                player.promisedPosition = player.position;
                if (move.promisedRole == "Titular") player.desiredStarts = max(player.desiredStarts, 4);
                else if (move.promisedRole == "Rotacion") player.desiredStarts = max(player.desiredStarts, 2);
                else if (move.promisedRole == "Proyecto") player.desiredStarts = max(player.desiredStarts, 2);
            }
            from->players.erase(from->players.begin() + playerIdx);
            to->addPlayer(player);
            addNews(player.name + " se incorpora a " + to->name + " por precontrato.");
        }
        pendingTransfers.erase(pendingTransfers.begin() + static_cast<long long>(i));
    }
}

void Career::initializeSeasonCup() {
    cupRemainingTeams.clear();
    cupChampion.clear();
    cupRound = 0;
    cupActive = getActiveTeamCount() >= 4;
    if (!cupActive) return;
    for (int i = 0; i < getActiveTeamCount(); ++i) {
        Team* team = getActiveTeamAt(i);
        if (team) cupRemainingTeams.push_back(team->name);
    }
}

int choosePhilosophyAwareObjectiveType(const Team& team) {
    int chance = randInt(1, 100);
    if (team.youthIdentity.find("Cantera") != string::npos) {
        if (chance <= 55) return 2;
        if (chance <= 85) return 1;
        return 3;
    }
    if (team.clubStyle == "Control de posesion") {
        if (chance <= 45) return 1;
        if (chance <= 80) return 3;
        return 2;
    }
    if (team.clubStyle == "Bloque ordenado") {
        if (chance <= 35) return 4;
        if (chance <= 70) return 3;
        return 1;
    }
    if (team.clubStyle == "Presion vertical") {
        if (chance <= 45) return 1;
        if (chance <= 80) return 3;
        return 4;
    }
    if (team.clubStyle == "Ataque por bandas") {
        if (chance <= 45) return 1;
        if (chance <= 80) return 3;
        return 2;
    }
    if (team.clubStyle == "Transicion directa") {
        if (chance <= 45) return 1;
        if (chance <= 80) return 3;
        return 2;
    }
    return randInt(1, 4);
}

void Career::initializeDynamicObjective() {
    if (!myTeam) return;
    int rank = currentCompetitiveRank();
    int field = max(1, currentCompetitiveFieldSize());
    boardMonthlyProgress = 0;
    boardMonthlyTarget = 0;
    boardMonthlyDeadlineWeek = min(static_cast<int>(schedule.size()), currentWeek + 3);
    if (boardMonthlyDeadlineWeek < currentWeek) boardMonthlyDeadlineWeek = currentWeek;

    int objectiveType = choosePhilosophyAwareObjectiveType(*myTeam);
    if (objectiveType == 1) {
        boardMonthlyTarget = 6;
        boardMonthlyObjective = "Sumar al menos 6 puntos en 4 semanas";
    } else if (objectiveType == 2) {
        boardMonthlyTarget = 2;
        boardMonthlyObjective = "Dar 2 titularidades a sub-20";
    } else if (objectiveType == 3 && rank > 1) {
        boardMonthlyTarget = max(1, rank - 1);
        boardMonthlyProgress = (rank > 0) ? rank : field;
        boardMonthlyObjective = "Mejorar la posicion liguera antes de 4 semanas";
    } else {
        boardMonthlyTarget = static_cast<int>(max(100000LL, myTeam->budget * 80 / 100));
        boardMonthlyObjective = "Mantener presupuesto por sobre $" + to_string(boardMonthlyTarget);
        boardMonthlyProgress = static_cast<int>(min<long long>(myTeam->budget, 2000000000LL));
    }
}

void Career::updateDynamicObjectiveStatus() {
    if (!myTeam || boardMonthlyObjective.empty()) return;

    const BoardObjectiveType objectiveType =
        boardObjectiveTypeFromText(boardMonthlyObjective);

    switch (objectiveType) {
        case BoardObjectiveType::YouthStarts: {
            int youthStarts = 0;
            for (const auto& player : myTeam->players) {
                if (player.age <= 20) {
                    youthStarts += player.startsThisSeason;
                }
            }
            boardMonthlyProgress = youthStarts;
            break;
        }

        case BoardObjectiveType::Position:
        case BoardObjectiveType::RelegationRisk:
            boardMonthlyProgress = currentCompetitiveRank();
            break;

        case BoardObjectiveType::Budget:
            boardMonthlyProgress = static_cast<int>(
                min<long long>(myTeam->budget, 2000000000LL));
            break;

        case BoardObjectiveType::Points:
        case BoardObjectiveType::Recovery:
        case BoardObjectiveType::Unknown:
            break;
    }

    if (currentWeek < boardMonthlyDeadlineWeek) return;

    bool success = false;

    switch (objectiveType) {
        case BoardObjectiveType::Points:
        case BoardObjectiveType::YouthStarts:
        case BoardObjectiveType::Budget:
            success = boardMonthlyProgress >= boardMonthlyTarget;
            break;

        case BoardObjectiveType::Position:
            success = boardMonthlyProgress > 0 &&
                      boardMonthlyProgress <= boardMonthlyTarget;
            break;

        case BoardObjectiveType::RelegationRisk: {
            const int safeRankLimit =
                max(1, currentCompetitiveFieldSize() * 3 / 4);

            success = boardMonthlyProgress > 0 &&
                      boardMonthlyProgress <= safeRankLimit;
            break;
        }

        case BoardObjectiveType::Recovery:
            success = boardMonthlyProgress >=
                      max(1, boardMonthlyTarget);
            break;

        case BoardObjectiveType::Unknown:
            success = false;
            break;
    }

    boardConfidence = clampInt(
        boardConfidence + (success ? 4 : -4), 0, 100);

    addNews(
        string("Objetivo mensual ") +
        (success ? "cumplido: " : "fallado: ") +
        boardMonthlyObjective
    );

    initializeDynamicObjective();
}

int Career::currentCompetitiveRank() const {
    if (!myTeam) return -1;
    vector<Team*> teams = boardRelevantTeams(*this);
    LeagueTable table = buildBoardTable(teams, activeDivision);
    for (size_t i = 0; i < table.teams.size(); ++i) {
        if (table.teams[i] == myTeam) return static_cast<int>(i) + 1;
    }
    return -1;
}

int Career::currentCompetitiveFieldSize() const {
    return static_cast<int>(boardRelevantTeams(*this).size());
}

void Career::initializeBoardObjectives() {
    if (!myTeam) return;
    vector<Team*> teams = boardRelevantTeams(*this);
    if (teams.empty()) return;
    vector<Team*> byValue = teams;
    sort(byValue.begin(), byValue.end(), [](Team* left, Team* right) {
        if (left->getSquadValue() != right->getSquadValue()) return left->getSquadValue() > right->getSquadValue();
        return left->name < right->name;
    });
    int valueRank = static_cast<int>(byValue.size());
    for (size_t i = 0; i < byValue.size(); ++i) {
        if (byValue[i] == myTeam) {
            valueRank = static_cast<int>(i) + 1;
            break;
        }
    }

    int fieldSize = static_cast<int>(teams.size());
    boardExpectedFinish = clampInt(valueRank + 1, 1, fieldSize);
    boardBudgetTarget = max(150000LL, myTeam->budget * 60 / 100);
    boardYouthTarget = (fieldSize >= 14) ? 2 : 1;
    boardConfidence = clampInt(55 + max(0, boardExpectedFinish - valueRank) * 4, 45, 75);
    boardWarningWeeks = 0;
}

void Career::updateBoardConfidence() {
    if (!myTeam || boardExpectedFinish <= 0) return;
    int rank = currentCompetitiveRank();
    int delta = 0;
    int prestige = teamPrestigeScore(*myTeam);
    if (rank > 0) {
        if (rank <= boardExpectedFinish) delta += 3;
        else delta -= min(5, rank - boardExpectedFinish);
        if (prestige >= 70 && rank > boardExpectedFinish) delta--;
    }
    if (myTeam->budget >= boardBudgetTarget) delta += 1;
    else delta -= 2;
    if (myTeam->budget < boardBudgetTarget * 8 / 10) delta -= 1;
    if (myTeam->debt > myTeam->sponsorWeekly * 16) delta -= 2;

    int youthUsed = youthPlayersUsed(*myTeam);
    if (youthUsed >= boardYouthTarget) delta += 1;
    else if (currentWeek > max(3, static_cast<int>(schedule.size()) / 4)) delta -= 1;

    if (myTeam->morale >= 65) delta += 1;
    else if (myTeam->morale <= 35) delta -= 1;
    if (myTeam->fanBase >= 55 && myTeam->morale <= 42) delta -= 1;

    int promiseRisk = promisesAtRiskForBoard(*myTeam, currentWeek);
    if (promiseRisk >= 2) delta -= min(3, promiseRisk);

    boardConfidence = clampInt(boardConfidence + delta, 0, 100);
    if (boardConfidence < 35) boardWarningWeeks++;
    else boardWarningWeeks = 0;
}

void Career::clearHumanManagers() {
    humanManagers.clear();
    activeHumanManagerIndex = 0;
}

void Career::syncActiveHumanManager() {
    if (!myTeam && humanManagers.empty()) return;

    if (humanManagers.empty()) {
        HumanManagerProfile profile;
        profile.managerName = managerName.empty() ? "Manager" : managerName;
        profile.teamName = myTeam ? myTeam->name : "";
        profile.reputation = clampInt(managerReputation <= 0 ? 50 : managerReputation, 1, 100);
        humanManagers.push_back(std::move(profile));
        activeHumanManagerIndex = 0;
    }

    if (activeHumanManagerIndex < 0 || activeHumanManagerIndex >= static_cast<int>(humanManagers.size())) {
        activeHumanManagerIndex = 0;
    }

    HumanManagerProfile& active = humanManagers[static_cast<size_t>(activeHumanManagerIndex)];
    active.managerName = managerName.empty() ? (active.managerName.empty() ? "Manager" : active.managerName)
                                             : managerName;
    active.reputation = clampInt(managerReputation <= 0 ? active.reputation : managerReputation, 1, 100);
    if (myTeam) active.teamName = myTeam->name;
}

void Career::applyActiveHumanManager() {
    if (humanManagers.empty()) {
        if (myTeam) {
            managerName = managerName.empty() ? "Manager" : managerName;
            managerReputation = clampInt(managerReputation <= 0 ? 50 : managerReputation, 1, 100);
        }
        return;
    }

    if (activeHumanManagerIndex < 0 || activeHumanManagerIndex >= static_cast<int>(humanManagers.size())) {
        activeHumanManagerIndex = 0;
    }

    HumanManagerProfile& active = humanManagers[static_cast<size_t>(activeHumanManagerIndex)];
    if (active.managerName.empty()) active.managerName = "Manager";
    if (active.reputation <= 0) active.reputation = 50;

    managerName = active.managerName;
    managerReputation = clampInt(active.reputation, 1, 100);
    myTeam = active.teamName.empty() ? nullptr : findTeamByName(active.teamName);

    if (!myTeam && !active.teamName.empty()) {
        for (size_t i = 0; i < humanManagers.size(); ++i) {
            if (humanManagers[i].teamName.empty()) continue;
            Team* fallback = findTeamByName(humanManagers[i].teamName);
            if (!fallback) continue;
            activeHumanManagerIndex = static_cast<int>(i);
            HumanManagerProfile& fallbackProfile = humanManagers[i];
            managerName = fallbackProfile.managerName.empty() ? "Manager" : fallbackProfile.managerName;
            managerReputation = clampInt(fallbackProfile.reputation <= 0 ? 50 : fallbackProfile.reputation, 1, 100);
            myTeam = fallback;
            break;
        }
    }

    if (myTeam) {
        refreshActiveDivisionTeamLinks(myTeam->division);
        humanManagers[static_cast<size_t>(activeHumanManagerIndex)].teamName = myTeam->name;
    }
}

bool Career::addHumanManager(const string& manager, const string& teamName, int reputation, bool makeActive) {
    if (teamName.empty() || !findTeamByName(teamName)) return false;

    syncActiveHumanManager();
    const string resolvedManager = manager.empty() ? "Manager" : manager;
    const int resolvedReputation = clampInt(reputation <= 0 ? 50 : reputation, 1, 100);
    for (size_t i = 0; i < humanManagers.size(); ++i) {
        if (humanManagers[i].teamName != teamName) continue;
        humanManagers[i].managerName = resolvedManager;
        humanManagers[i].reputation = resolvedReputation;
        if (makeActive) {
            activeHumanManagerIndex = static_cast<int>(i);
            applyActiveHumanManager();
        }
        return true;
    }

    humanManagers.push_back({resolvedManager, teamName, resolvedReputation});
    if (makeActive) {
        activeHumanManagerIndex = static_cast<int>(humanManagers.size() - 1);
        applyActiveHumanManager();
    }
    return true;
}

bool Career::switchActiveHumanManager(int index) {
    syncActiveHumanManager();
    if (index < 0 || index >= static_cast<int>(humanManagers.size())) return false;
    activeHumanManagerIndex = index;
    applyActiveHumanManager();
    if (myTeam) setActiveDivision(myTeam->division);
    return myTeam != nullptr;
}

int Career::humanManagerCount() const {
    if (!humanManagers.empty()) return static_cast<int>(humanManagers.size());
    return myTeam ? 1 : 0;
}

vector<string> Career::humanControlledTeamNames() const {
    vector<string> names;
    const auto pushUniqueName = [&names](const string& name) {
        if (name.empty()) return;
        if (find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    };

    for (const auto& profile : humanManagers) pushUniqueName(profile.teamName);
    if (names.empty() && myTeam) pushUniqueName(myTeam->name);
    return names;
}

bool Career::isHumanControlledTeam(const Team& team) const {
    const vector<string> names = humanControlledTeamNames();
    return find(names.begin(), names.end(), team.name) != names.end();
}
