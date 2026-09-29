#include "career/career_service.h"
#include "career/career_runtime.h"
#include "career/world_state_service.h"
#include "competition.h"
#include "simulation.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

void maybeInvokeIdle() {
    if (IdleCallback callback = idleCallback()) {
        callback();
    }
}
vector<vector<pair<int, int>>> buildRoundRobinIndexSchedule(int teamCount, bool doubleRound) {
    vector<vector<pair<int, int>>> out;
    if (teamCount < 2) return out;
    vector<int> indices;
    indices.reserve(teamCount + 1);
    for (int i = 0; i < teamCount; ++i) indices.push_back(i);
    if (indices.size() % 2 == 1) indices.push_back(-1);

    int size = static_cast<int>(indices.size());
    int rounds = size - 1;
    for (int round = 0; round < rounds; ++round) {
        vector<pair<int, int>> matches;
        for (int i = 0; i < size / 2; ++i) {
            int a = indices[static_cast<size_t>(i)];
            int b = indices[static_cast<size_t>(size - 1 - i)];
            if (a == -1 || b == -1) continue;
            if (round % 2 == 0) matches.push_back({a, b});
            else matches.push_back({b, a});
        }
        out.push_back(matches);
        int last = indices.back();
        for (int i = size - 1; i > 1; --i) indices[static_cast<size_t>(i)] = indices[static_cast<size_t>(i - 1)];
        indices[1] = last;
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

}  // namespace

void CareerService::simulateBackgroundDivisionWeek(const string& divisionId) {
    Career& career = career_;
    if (divisionId.empty() || divisionId == career.activeDivision) return;
    vector<Team*> teams = career.getDivisionTeams(divisionId);
    if (teams.size() < 2) return;
    sort(teams.begin(), teams.end(), [](Team* left, Team* right) {
        if (left->tiebreakerSeed != right->tiebreakerSeed) return left->tiebreakerSeed < right->tiebreakerSeed;
        return left->name < right->name;
    });
    auto schedule = buildRoundRobinIndexSchedule(static_cast<int>(teams.size()), true);
    int round = career.currentWeek - 1;
    if (round < 0 || round >= static_cast<int>(schedule.size())) return;

    int headlineMargin = -1;
    string headline;
    for (const auto& match : schedule[static_cast<size_t>(round)]) {
        maybeInvokeIdle();
        Team* home = teams[static_cast<size_t>(match.first)];
        Team* away = teams[static_cast<size_t>(match.second)];
        MatchResult result = playMatch(*home, *away, false, false);
        int margin = abs(result.homeGoals - result.awayGoals);
        if (margin > headlineMargin) {
            headlineMargin = margin;
            headline = home->name + " " + to_string(result.homeGoals) + "-" + to_string(result.awayGoals) + " " +
                       away->name;
        }
    }

    LeagueTable table;
    table.title = divisionDisplay(divisionId);
    table.ruleId = divisionId;
    for (Team* team : teams) table.addTeam(team);
    table.sortTable();
    Team* leader = table.teams.empty() ? nullptr : table.teams.front();
    Team* bottom = table.teams.empty() ? nullptr : table.teams.back();

    if (!headline.empty() && (career.currentWeek % 4 == 0 || headlineMargin >= 3)) {
        string news = "[Mundo] " + divisionDisplay(divisionId) + ": " + headline;
        if (leader) news += " | Lider " + leader->name + " (" + to_string(leader->points) + " pts)";
        career.addNews(news);
    }
    if (leader && randInt(1, 100) <= world_state_service::worldRuleValue("background_leader_story_chance", 14)) {
        career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + leader->name +
                       " instala una historia de temporada con estilo " + leader->clubStyle + ".");
    }
    if (bottom && randInt(1, 100) <= world_state_service::worldRuleValue("background_pressure_story_chance", 10)) {
        career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": crece la presion en " + bottom->name +
                       " por su mala racha.");
    }
    if (bottom && randInt(1, 100) <= world_state_service::worldRuleValue("background_manager_review_chance", 8)) {
        ensureTeamIdentity(*bottom);
        bottom->morale = clampInt(bottom->morale - 3, 0, 100);
        bottom->jobSecurity = clampInt(bottom->jobSecurity - 6, 5, 92);
        career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + bottom->name +
                       " entra en revision de banquillo tras otra semana bajo presion con " + bottom->headCoachName + ".");
    }
    if (leader && leader->youthFacilityLevel + leader->youthCoach >= 130 && randInt(1, 100) <= world_state_service::worldRuleValue("background_youth_promotion_chance", 12)) {
        const int maxSquad = getCompetitionConfig(divisionId).maxSquadSize;
        if (maxSquad <= 0 || static_cast<int>(leader->players.size()) < maxSquad) {
            const string youthPosition = leader->goalsAgainst > leader->goalsFor ? "DEF" : "MED";
            Player promoted = makeRandomPlayer(youthPosition,
                                               max(40, leader->getAverageSkill() - 18),
                                               max(leader->getAverageSkill() - 4, 45),
                                               17,
                                               19);
            promoted.potential = clampInt(promoted.skill + randInt(8, 16), promoted.skill, 95);
            promoted.contractWeeks = 156;
            promoted.wage = max(2500LL, promoted.wage / 2);
            leader->addPlayer(promoted);
            career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + leader->name +
                           " promociona al juvenil " + promoted.name + ".");
        }
    }
    if (leader && randInt(1, 100) <= world_state_service::worldRuleValue("background_injury_story_chance", 8)) {
        Player* keyPlayer = nullptr;
        for (auto& player : leader->players) {
            if (player.injured) continue;
            if (!keyPlayer || player.skill > keyPlayer->skill) keyPlayer = &player;
        }
        if (keyPlayer) {
            keyPlayer->injured = true;
            keyPlayer->injuryType = randInt(0, 1) == 0 ? "Sobrecarga" : "Muscular";
            keyPlayer->injuryWeeks = randInt(1, 3);
            keyPlayer->injuryHistory++;
            career.addNews("[Mundo] " + divisionDisplay(divisionId) + ": " + leader->name +
                           " pierde por lesion a " + keyPlayer->name + " durante " +
                           to_string(keyPlayer->injuryWeeks) + " semana(s).");
        }
    }
}
