#include "career/career_service.h"
#include "week_match_helpers.h"

#include "career/career_runtime.h"
#include "career/match_analysis_store.h"
#include "simulation.h"
#include "utils.h"

#include <string>
#include <vector>

using namespace std;

namespace {
struct TeamTableSnapshot {
    int points;
    int goalsFor;
    int goalsAgainst;
    int awayGoals;
    int wins;
    int draws;
    int losses;
    int yellowCards;
    int redCards;
    vector<HeadToHeadRecord> headToHead;
};

TeamTableSnapshot captureTableState(const Team& team) {
    return {team.points, team.goalsFor, team.goalsAgainst, team.awayGoals, team.wins, team.draws,
            team.losses, team.yellowCards, team.redCards, team.headToHead};
}

void restoreTableState(Team& team, const TeamTableSnapshot& snapshot) {
    team.points = snapshot.points;
    team.goalsFor = snapshot.goalsFor;
    team.goalsAgainst = snapshot.goalsAgainst;
    team.awayGoals = snapshot.awayGoals;
    team.wins = snapshot.wins;
    team.draws = snapshot.draws;
    team.losses = snapshot.losses;
    team.yellowCards = snapshot.yellowCards;
    team.redCards = snapshot.redCards;
    team.headToHead = snapshot.headToHead;
}

void maybeInvokeIdle() {
    if (IdleCallback callback = idleCallback()) {
        callback();
    }
}

}  // namespace

void CareerService::simulateSeasonCupRound() {
    Career& career = career_;
    if (!career.cupActive) return;
    vector<Team*> alive;
    for (const auto& name : career.cupRemainingTeams) {
        Team* team = career.findTeamByName(name);
        if (team && team->division == career.activeDivision) alive.push_back(team);
    }
    if (alive.size() <= 1) {
        career.cupActive = false;
        if (!alive.empty()) {
            career.cupChampion = alive.front()->name;
            career.addNews("Copa de temporada: " + career.cupChampion + " se consagra campeon.");
        }
        return;
    }

    career.cupRound++;
    emitUiMessage("");
    emitUiMessage("--- Copa de temporada: ronda " + to_string(career.cupRound) + " ---");
    vector<string> nextRound;
    if (alive.size() % 2 == 1) {
        Team* bye = alive.back();
        nextRound.push_back(bye->name);
        alive.pop_back();
        emitUiMessage("Pase libre: " + bye->name);
    }

    for (size_t i = 0; i < alive.size(); i += 2) {
        maybeInvokeIdle();
        Team* home = alive[i];
        Team* away = alive[i + 1];
        TeamTableSnapshot homeSnap = captureTableState(*home);
        TeamTableSnapshot awaySnap = captureTableState(*away);
        bool verbose =
            (home == career.myTeam || away == career.myTeam) &&
            weekSimulationPresentation() ==
                WeekSimulationPresentation::Detailed;
        emitUiMessage(home->name + " vs " + away->name);
        MatchResult result = verbose ? playMatch(&career, *home, *away, true, true, true)
                                     : playMatch(*home, *away, false, true, true);
        restoreTableState(*home, homeSnap);
        restoreTableState(*away, awaySnap);
        career_match_analysis::storeMatchAnalysis(career, *home, *away, result, true);
        career_week_matches::updateRivalMemoryForUserMatch(career, *home, *away, result);

        Team* winner = home;
        if (result.awayGoals > result.homeGoals) {
            winner = away;
        } else if (result.homeGoals == result.awayGoals) {
            winner = (teamPenaltyStrength(*home) >= teamPenaltyStrength(*away)) ? home : away;
            emitUiMessage("Gana por penales: " + winner->name);
        }
        nextRound.push_back(winner->name);
    }

    career.cupRemainingTeams = nextRound;
    if (career.cupRemainingTeams.size() == 1) {
        career.cupActive = false;
        career.cupChampion = career.cupRemainingTeams.front();
        career.addNews("Copa de temporada: " + career.cupChampion + " se consagra campeon.");
        emitUiMessage("Campeon de la copa: " + career.cupChampion);
    }
}
