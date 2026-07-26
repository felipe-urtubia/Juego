#pragma once

#include "simulation/match_types.h"
#include "engine/social_system.h"
#include "engine/manager_stress.h"
#include "engine/rival_ai.h"
#include "engine/rivalry_system.h"
#include "engine/facilities_system.h"
#include "engine/debt_system.h"

#include <deque>
#include <string>
#include <utility>
#include <vector>

struct Player {
    std::string name;
    std::string position;
    std::string preferredFoot;
    std::vector<std::string> secondaryPositions;
    int attack = 0;
    int defense = 0;
    int stamina = 0;
    int fitness = 0;
    int skill = 0;
    int potential = 0;
    int age = 0;
    long long value = 0;
    long long wage = 0;
    long long releaseClause = 0;
    int setPieceSkill = 0;
    int leadership = 0;
    int professionalism = 0;
    int ambition = 0;
    int consistency = 0;
    int bigMatches = 0;
    int currentForm = 0;
    int tacticalDiscipline = 0;
    std::string personality;
    int discipline = 0;
    int injuryProneness = 0;
    int adaptation = 0;
    int versatility = 0;
    int happiness = 0;
    int chemistry = 0;
    int moraleMomentum = 0;
    int fatigueLoad = 0;
    int unhappinessWeeks = 0;
    int desiredStarts = 0;
    int startsThisSeason = 0;
    bool wantsToLeave = false;
    bool onLoan = false;
    std::string parentClub;
    int loanWeeksRemaining = 0;
    int contractWeeks = 0;
    bool injured = false;
    std::string injuryType;
    int injuryWeeks = 0;
    int injuryHistory = 0;
    int yellowAccumulation = 0;
    int seasonYellowCards = 0;
    int seasonRedCards = 0;
    int matchesSuspended = 0;
    int goals = 0;
    int assists = 0;
    int matchesPlayed = 0;
    int lastTrainedSeason = -1;
    int lastTrainedWeek = -1;
    std::string role;
    std::string roleDuty;
    std::string individualInstruction;
    std::string developmentPlan;
    std::string promisedRole;
    std::string promisedPosition;
    std::string socialGroup;
    std::vector<std::string> traits;
};

struct HeadToHeadRecord {
    std::string opponent;
    int points;
};

struct SeasonHistoryEntry {
    int season;
    std::string division;
    std::string club;
    int finish;
    std::string champion;
    std::string promoted;
    std::string relegated;
    std::string note;
};

struct HumanManagerProfile {
    std::string managerName;
    std::string teamName;
    int reputation = 50;
};

struct PendingTransfer {
    std::string playerName;
    std::string fromTeam;
    std::string toTeam;
    int effectiveSeason;
    int loanWeeks;
    long long fee;
    long long wage;
    int contractWeeks;
    bool preContract;
    bool loan;
    std::string promisedRole;
};

struct ScoutingAssignment {
    std::string region;
    std::string focusPosition;
    std::string priority;
    int weeksRemaining = 0;
    int knowledgeLevel = 0;
};

struct SquadPromise {
    std::string subjectName;
    std::string category;
    std::string target;
    int issuedWeek = 0;
    int deadlineWeek = 0;
    int progress = 0;
    bool fulfilled = false;
    bool failed = false;
};

struct HistoricalRecord {
    std::string category;
    std::string holderName;
    std::string teamName;
    int season = 0;
    int value = 0;
    std::string note;
};

struct MatchCenterSnapshot {
    std::string competitionLabel;
    std::string opponentName;
    std::string venueLabel;
    int myGoals = 0;
    int oppGoals = 0;
    int myShots = 0;
    int oppShots = 0;
    int myShotsOnTarget = 0;
    int oppShotsOnTarget = 0;
    int myPossession = 50;
    int oppPossession = 50;
    int myCorners = 0;
    int oppCorners = 0;
    int mySubstitutions = 0;
    int oppSubstitutions = 0;
    int myExpectedGoalsTenths = 0;
    int oppExpectedGoalsTenths = 0;
    std::string weather;
    std::string dominanceSummary;
    std::string tacticalSummary;
    std::string fatigueSummary;
    std::string postMatchImpact;
    std::vector<std::string> phaseSummaries;
};

class Team;

void applyPositionStats(Player& p);
std::string defaultRoleForPosition(const std::string& position);
std::string defaultDutyForPosition(const std::string& position);
std::string defaultDutyForRole(const std::string& role, const std::string& position);
std::string defaultInstructionForPosition(const std::string& position);
std::string defaultDevelopmentPlanForPosition(const std::string& position);
void ensurePlayerProfile(Player& p, bool regenerateTraits = false);
std::vector<std::string> generatePlayerTraits(const Player& p, bool youth = false);
bool playerHasTrait(const Player& p, const std::string& trait);
int positionFitScore(const Player& p, const std::string& desiredPosition);
std::string playerReliabilityLabel(const Player& p);
std::string playerFormLabel(const Player& p);
void ensureTeamIdentity(Team& team);
int teamPrestigeScore(const Team& team);
bool areRivalClubs(const Team& a, const Team& b);
std::string teamExpectationLabel(const Team& team);
std::string joinStringValues(const std::vector<std::string>& values, const std::string& separator);
Player makeRandomPlayer(const std::string& position, int skillMin, int skillMax, int ageMin, int ageMax);

class Team {
public:
    std::string name;
    std::string division;
    std::vector<Player> players;
    std::string tactics;
    std::string formation;
    long long budget;
    int morale;
    std::string trainingFocus;
    int points;
    int goalsFor;
    int goalsAgainst;
    int awayGoals;
    int wins;
    int draws;
    int losses;
    int yellowCards;
    int redCards;
    int tiebreakerSeed;
    int pressingIntensity;
    int defensiveLine;
    int tempo;
    int width;
    std::string markingStyle;
    std::vector<std::string> preferredXI;
    std::vector<std::string> preferredBench;
    std::string captain;
    std::string penaltyTaker;
    std::string freeKickTaker;
    std::string cornerTaker;
    std::string rotationPolicy;
    int assistantCoach;
    int fitnessCoach;
    int scoutingChief;
    int youthCoach;
    int medicalTeam;
    int goalkeepingCoach;
    int performanceAnalyst;
    std::string assistantCoachName;
    std::string fitnessCoachName;
    std::string scoutingChiefName;
    std::string youthCoachName;
    std::string medicalChiefName;
    std::string goalkeepingCoachName;
    std::string performanceAnalystName;
    std::string youthRegion;
    long long debt;
    long long sponsorWeekly;
    int stadiumLevel;
    int youthFacilityLevel;
    int trainingFacilityLevel;
    int fanBase;
    int clubPrestige;
    std::string clubStyle;
    std::string youthIdentity;
    std::string primaryRival;
    std::string matchInstruction;
    int lastTacticalChangeMinute = -100;
    std::string headCoachName;
    int headCoachReputation;
    std::string headCoachStyle;
    int headCoachTenureWeeks;
    int jobSecurity;
    std::string transferPolicy;
    std::vector<std::string> scoutingRegions;
    std::vector<HeadToHeadRecord> headToHead;
    std::vector<std::string> achievements;

    Team(std::string n = "");

    void addPlayer(const Player& p);
    void addHeadToHeadPoints(const std::string& opponent, int points);
    int headToHeadPointsAgainst(const std::string& opponent) const;

    std::vector<int> getStartingXIIndices() const;
    std::vector<int> getBenchIndices(int count = 7) const;
    int getTotalAttack() const;
    int getTotalDefense() const;
    int getAverageSkill() const;
    int getAverageStamina() const;
    long long getSquadValue() const;

    void resetSeasonStats();
};

struct LeagueTable {
    std::vector<Team*> teams;
    std::string title;
    std::string ruleId;

    void clear();
    void addTeam(Team* team);
    void sortTable();
    std::vector<std::string> formatLines() const;
    void displayTable();
};

struct DivisionInfo {
    std::string id;
    std::string folder;
    std::string display;
};

extern const std::vector<DivisionInfo> kDivisions;

using TeamId = int;
constexpr TeamId kInvalidTeamId = -1;

struct Career {
    Team* myTeam;
    LeagueTable leagueTable;
    int currentSeason;
    int currentWeek;
    std::deque<Team> allTeams;
    std::vector<Team*> activeTeams;
    std::vector<TeamId> activeTeamIds;
    std::vector<std::vector<std::pair<int, int>>> schedule;
    std::vector<int> groupNorthIdx;
    std::vector<int> groupSouthIdx;
    std::vector<DivisionInfo> divisions;
    std::string activeDivision;
    std::string saveFile;
    std::vector<std::string> loadWarnings;
    std::vector<std::string> achievements;
    std::string managerName;
    int managerReputation;
    std::vector<HumanManagerProfile> humanManagers;
    int activeHumanManagerIndex;
    int boardConfidence;
    int boardExpectedFinish;
    long long boardBudgetTarget;
    int boardYouthTarget;
    int boardWarningWeeks;
    std::string boardMonthlyObjective;
    int boardMonthlyTarget;
    int boardMonthlyProgress;
    int boardMonthlyDeadlineWeek;
    std::vector<std::string> newsFeed;
    std::vector<std::string> managerInbox;
    std::vector<std::string> scoutInbox;
    std::vector<std::string> scoutingShortlist;
    std::vector<ScoutingAssignment> scoutingAssignments;
    std::vector<SeasonHistoryEntry> history;
    std::vector<SquadPromise> activePromises;
    std::vector<HistoricalRecord> historicalRecords;
    std::vector<PendingTransfer> pendingTransfers;
    bool cupActive;
    int cupRound;
    std::vector<std::string> cupRemainingTeams;
    std::string cupChampion;
    std::string lastMatchAnalysis;
    std::vector<std::string> lastMatchReportLines;
    std::vector<std::string> lastMatchEvents;
    std::string lastMatchPlayerOfTheMatch;
    MatchCenterSnapshot lastMatchCenter;
    
    // === NUEVOS SISTEMAS DE GAMEPLAY ===
    // Dinámicas Sociales
    DressingRoomDynamics dressingRoomDynamics;
    
    // Estrés del Manager
    ManagerStressState managerStress;
    
    // IA Rival
    std::unordered_map<std::string, RivalAI> rivalAIMap;
    
    // Rivalidades entre equipos
    RivalryDynamics rivalryDynamics;
    
    // Infraestructura
    InfrastructureSystem infrastructure;
    
    // Sistema de Deuda
    DebtStatus debtStatus;
    
    // === FIN NUEVOS SISTEMAS ===
    bool initialized;

    Career();

    bool usesSegundaFormat() const;
    bool usesTerceraBFormat() const;
    bool usesGroupFormat() const;
    void buildSegundaGroups();
    void buildRegionalGroups();
    void initializeLeague(bool forceReload = false);
    std::vector<Team*> getDivisionTeams(const std::string& id);
    void syncActiveTeamIds();
    bool hasSyncedActiveTeamIds() const;
    void rebuildActiveLeagueTable();
    void refreshActiveDivisionTeamLinks();
    void refreshActiveDivisionTeamLinks(const std::string& id);
    void buildSchedule();
    void setActiveDivision(const std::string& id);
    void resetSeason();
    void agePlayers();
    Team* findTeamByName(const std::string& name);
    const Team* findTeamByName(const std::string& name) const;
    TeamId getTeamIdFor(const Team* team) const;
    TeamId getTeamIdByName(const std::string& name) const;
    Team* getTeamById(TeamId id);
    const Team* getTeamById(TeamId id) const;
    TeamId getActiveTeamIdAt(int index) const;
    void addNews(const std::string& item);
    void addInboxItem(const std::string& item, const std::string& channel = "");
    void executePendingTransfers();
    void initializeSeasonCup();
    void initializeDynamicObjective();
    void updateDynamicObjectiveStatus();
    int currentCompetitiveRank() const;
    int currentCompetitiveFieldSize() const;
    void initializeBoardObjectives();
    void updateBoardConfidence();
    void clearHumanManagers();
    void syncActiveHumanManager();
    void applyActiveHumanManager();
    bool addHumanManager(const std::string& managerName,
                         const std::string& teamName,
                         int reputation = 50,
                         bool makeActive = true);
    bool switchActiveHumanManager(int index);
    int humanManagerCount() const;
    std::vector<std::string> humanControlledTeamNames() const;
    bool isHumanControlledTeam(const Team& team) const;
    int getActiveTeamCount() const;
    Team* getActiveTeamAt(int index);
    const Team* getActiveTeamAt(int index) const;
    int getAllTeamCount() const;
    Team* getTeamAt(int index);
    const Team* getTeamAt(int index) const;
    Team* setMyTeamByName(const std::string& teamName);
    bool saveCareer();
    bool loadCareer();

    // === SAFE ACCESS METHODS (Phase 1 Refactoring) ===
    // Provide safe access to teams without relying on raw pointers
    // These methods validate bounds and prevent dangling pointer issues
    Team* getMyTeamSafe();
    const Team* getMyTeamSafe() const;
    Team* getTeamAtIndexSafe(int index);
    const Team* getTeamAtIndexSafe(int index) const;
    int getActiveTeamsCount() const;
    bool isValidTeamIndex(int index) const;
    // === END SAFE ACCESS METHODS ===

};

struct MatchResult {
    int homeGoals;
    int awayGoals;
    int homeShots;
    int awayShots;
    int homePossession;
    int awayPossession;
    int homeSubstitutions;
    int awaySubstitutions;
    int homeCorners;
    int awayCorners;
    std::string weather;
    std::vector<std::string> warnings;
    std::vector<std::string> reportLines;
    std::vector<std::string> events;
    std::string verdict;
    MatchContext context;
    MatchStats stats;
    MatchTimeline timeline;
    MatchReport report;
};

struct TeamStrength {
    std::vector<int> xi;
    int attack;
    int defense;
    int avgSkill;
    int avgStamina;
};
