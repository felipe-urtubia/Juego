#include "career/season_transition.h"

#include "ai/team_ai.h"
#include "career/career_reports.h"
#include "career/player_development.h"
#include "career/world_state_service.h"
#include "competition/competition.h"
#include "simulation/simulation.h"
#include "utils/utils.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>

using namespace std;

namespace {

string joinTeamNames(const vector<Team*>& teams) {
    string out;
    for (Team* team : teams) {
        if (!team) continue;
        if (!out.empty()) out += ", ";
        out += team->name;
    }
    return out;
}

vector<string> teamNames(const vector<Team*>& teams) {
    vector<string> names;
    for (Team* team : teams) {
        if (team) names.push_back(team->name);
    }
    return names;
}

void addLine(SeasonTransitionSummary& summary, const string& line) {
    if (!line.empty()) summary.lines.push_back(line);
}

int teamRank(const LeagueTable& table, const Team* team) {
    for (size_t i = 0; i < table.teams.size(); ++i) {
        if (table.teams[i] == team) return static_cast<int>(i) + 1;
    }
    return -1;
}

void recordSeasonHistory(Career& career,
                         const string& champion,
                         const vector<Team*>& promoted,
                         const vector<Team*>& relegated,
                         const string& note) {
    if (!career.myTeam) return;
    SeasonHistoryEntry entry;
    entry.season = career.currentSeason;
    entry.division = career.activeDivision;
    entry.club = career.myTeam->name;
    entry.finish = career.currentCompetitiveRank();
    entry.champion = champion;
    entry.promoted = joinTeamNames(promoted);
    entry.relegated = joinTeamNames(relegated);
    entry.note = note;
    career.history.push_back(entry);
    if (career.history.size() > 30) {
        career.history.erase(career.history.begin(),
                             career.history.begin() + static_cast<long long>(career.history.size() - 30));
    }
    if (!champion.empty()) {
        career.historicalRecords.push_back({
            "Campeon " + divisionDisplay(career.activeDivision) + " T" + to_string(career.currentSeason),
            champion,
            champion,
            career.currentSeason,
            1,
            note.empty() ? "Campeon de temporada" : note
        });
    }
    career.historicalRecords.push_back({
        "Tabla historica " + divisionDisplay(career.activeDivision) + " T" + to_string(career.currentSeason),
        career.myTeam->name,
        career.myTeam->name,
        career.currentSeason,
        entry.finish,
        "Puesto final del club controlado"
    });
    if (career.historicalRecords.size() > 60) {
        career.historicalRecords.erase(career.historicalRecords.begin(),
                                       career.historicalRecords.begin() +
                                           static_cast<long long>(career.historicalRecords.size() - 60));
    }
}

int updateWorldDivisionRecords(Career& career, SeasonTransitionSummary& summary) {
    int totalUpdates = 0;
    for (const auto& division : career.divisions) {
        vector<Team*> teams = career.getDivisionTeams(division.id);
        if (teams.empty()) continue;
        LeagueTable table;
        table.title = division.display.empty() ? divisionDisplay(division.id) : division.display;
        table.ruleId = division.id;
        for (Team* team : teams) {
            if (team) table.addTeam(team);
        }
        table.sortTable();
        totalUpdates += world_state_service::updateSeasonRecords(career, table);
    }
    if (totalUpdates > 0) addLine(summary, "Records historicos actualizados: " + to_string(totalUpdates));
    return totalUpdates;
}

void awardSeasonPrizeMoney(Career& career, const LeagueTable& table, SeasonTransitionSummary& summary) {
    if (!career.myTeam) return;
    int rank = teamRank(table, career.myTeam);
    if (rank <= 0) return;
    int size = max(1, static_cast<int>(table.teams.size()));
    long long prize = max(10000LL, static_cast<long long>(size - rank + 1) * 12000LL);
    career.myTeam->budget += prize;
    career.addNews(career.myTeam->name + " recibe $" + to_string(prize) + " por su ubicacion final.");
    addLine(summary, "Premio de temporada: $" + to_string(prize));
}

void advanceToNextSeason(Career& career, SeasonTransitionSummary& summary) {
    const int resolvedPromises = world_state_service::resolveSeasonCarryover(career, &summary.lines);
    if (resolvedPromises > 0) {
        addLine(summary, "Promesas cerradas al final del curso: " + to_string(resolvedPromises));
    }
    career.currentSeason++;
    career.currentWeek = 1;
    career.agePlayers();
    career.executePendingTransfers();
    if (career.myTeam) {
        career.setActiveDivision(career.myTeam->division);
    } else {
        career.setActiveDivision(career.activeDivision);
    }
    career.resetSeason();
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        if (Team* team = career.getActiveTeamAt(i)) {
            player_dev::addYouthPlayers(*team, 1);
        }
    }
    world_state_service::seedSeasonPromises(career);
    addLine(summary, "Nueva temporada: " + to_string(career.currentSeason) +
                         " | Division activa: " + divisionDisplay(career.activeDivision));
}

int divisionIndex(const string& id) {
    for (size_t i = 0; i < kDivisions.size(); ++i) {
        if (kDivisions[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

LeagueTable buildRankedTable(const vector<Team*>& teams, const string& title, const string& ruleId) {
    LeagueTable table;
    table.title = title.empty() ? divisionDisplay(ruleId) : title;
    table.ruleId = ruleId;
    for (Team* team : teams) {
        if (team) table.addTeam(team);
    }
    table.sortTable();
    return table;
}

vector<Team*> rankedDivisionTeams(Career& career, const string& divisionId) {
    return buildRankedTable(career.getDivisionTeams(divisionId), divisionDisplay(divisionId), divisionId).teams;
}

vector<Team*> topByStandings(Career& career, const string& divisionId, int count) {
    vector<Team*> teams = rankedDivisionTeams(career, divisionId);
    if (count < 0) count = 0;
    if (static_cast<int>(teams.size()) > count) teams.resize(count);
    return teams;
}

vector<Team*> bottomByStandings(Career& career, const string& divisionId, int count) {
    vector<Team*> ranked = rankedDivisionTeams(career, divisionId);
    if (count <= 0 || ranked.empty()) return {};
    vector<Team*> out;
    for (int i = 0; i < count && i < static_cast<int>(ranked.size()); ++i) {
        out.push_back(ranked[ranked.size() - 1 - static_cast<size_t>(i)]);
    }
    return out;
}

void addMovementLines(SeasonTransitionSummary& summary,
                      const string& promoteLabel,
                      const vector<Team*>& promoted,
                      const string& relegateLabel,
                      const vector<Team*>& relegated) {
    summary.promotedTeams = teamNames(promoted);
    summary.relegatedTeams = teamNames(relegated);
    if (!summary.promotedTeams.empty()) addLine(summary, promoteLabel + ": " + joinTeamNames(promoted));
    if (!summary.relegatedTeams.empty()) addLine(summary, relegateLabel + ": " + joinTeamNames(relegated));
}

void awardLegPoints(int goalsA, int goalsB, int& pointsA, int& pointsB) {
    if (goalsA > goalsB) {
        pointsA += 3;
    } else if (goalsB > goalsA) {
        pointsB += 3;
    } else {
        pointsA += 1;
        pointsB += 1;
    }
}

vector<vector<pair<int, int>>> buildRoundRobinIndexSchedule(int teamCount, bool doubleRound) {
    vector<vector<pair<int, int>>> out;
    if (teamCount < 2) return out;
    vector<int> idx;
    idx.reserve(static_cast<size_t>(teamCount + 1));
    for (int i = 0; i < teamCount; ++i) idx.push_back(i);
    if (idx.size() % 2 == 1) idx.push_back(-1);

    int size = static_cast<int>(idx.size());
    int rounds = size - 1;
    for (int round = 0; round < rounds; ++round) {
        vector<pair<int, int>> matches;
        for (int i = 0; i < size / 2; ++i) {
            int a = idx[i];
            int b = idx[size - 1 - i];
            if (a == -1 || b == -1) continue;
            matches.push_back(round % 2 == 0 ? pair<int, int>{a, b} : pair<int, int>{b, a});
        }
        out.push_back(matches);
        int last = idx.back();
        for (int i = size - 1; i > 1; --i) idx[i] = idx[i - 1];
        idx[1] = last;
    }

    if (doubleRound) {
        int base = static_cast<int>(out.size());
        for (int i = 0; i < base; ++i) {
            vector<pair<int, int>> rev;
            for (const auto& match : out[static_cast<size_t>(i)]) rev.push_back({match.second, match.first});
            out.push_back(rev);
        }
    }
    return out;
}

Team* simulateSingleLegKnockout(Team* home,
                                Team* away,
                                const string& label,
                                SeasonTransitionSummary& summary,
                                bool neutralVenue = false) {
    if (!home) return away;
    if (!away) return home;

    Team h1 = *home;
    Team a1 = *away;
    MatchResult result = playMatch(h1, a1, false, true, neutralVenue);
    addLine(summary, label + ": " + home->name + " " + to_string(result.homeGoals) + "-" +
                         to_string(result.awayGoals) + " " + away->name);
    if (result.homeGoals > result.awayGoals) return home;
    if (result.awayGoals > result.homeGoals) return away;

    Team* winner = (teamPenaltyStrength(*home) >= teamPenaltyStrength(*away)) ? home : away;
    addLine(summary, label + " definido por penales: " + winner->name);
    return winner;
}

Team* simulateTwoLegTie(Team* firstHome,
                        Team* firstAway,
                        const string& label,
                        bool extraTimeFinal,
                        SeasonTransitionSummary& summary) {
    if (!firstHome) return firstAway;
    if (!firstAway) return firstHome;

    Team h1 = *firstHome;
    Team a1 = *firstAway;
    MatchResult r1 = playMatch(h1, a1, false, true);
    Team h2 = *firstAway;
    Team a2 = *firstHome;
    MatchResult r2 = playMatch(h2, a2, false, true);

    int firstHomePoints = 0;
    int firstAwayPoints = 0;
    awardLegPoints(r1.homeGoals, r1.awayGoals, firstHomePoints, firstAwayPoints);
    awardLegPoints(r2.awayGoals, r2.homeGoals, firstHomePoints, firstAwayPoints);

    int firstHomeAgg = r1.homeGoals + r2.awayGoals;
    int firstAwayAgg = r1.awayGoals + r2.homeGoals;
    int firstHomeGD = firstHomeAgg - firstAwayAgg;
    int firstAwayGD = -firstHomeGD;

    addLine(summary, label + " ida: " + firstHome->name + " " + to_string(r1.homeGoals) + "-" +
                         to_string(r1.awayGoals) + " " + firstAway->name);
    addLine(summary, label + " vuelta: " + firstAway->name + " " + to_string(r2.homeGoals) + "-" +
                         to_string(r2.awayGoals) + " " + firstHome->name);
    addLine(summary, label + " global: " + firstHome->name + " " + to_string(firstHomeAgg) + "-" +
                         to_string(firstAwayAgg) + " " + firstAway->name);

    if (firstHomePoints > firstAwayPoints) return firstHome;
    if (firstAwayPoints > firstHomePoints) return firstAway;
    if (firstHomeGD > firstAwayGD) return firstHome;
    if (firstAwayGD > firstHomeGD) return firstAway;

    if (extraTimeFinal) {
        int etHome = randInt(0, 1);
        int etAway = randInt(0, 1);
        addLine(summary, label + " prorroga: " + firstAway->name + " " + to_string(etHome) + "-" +
                             to_string(etAway) + " " + firstHome->name);
        if (etHome > etAway) return firstAway;
        if (etAway > etHome) return firstHome;
    }

    Team* winner = (teamPenaltyStrength(*firstHome) >= teamPenaltyStrength(*firstAway)) ? firstHome : firstAway;
    addLine(summary, label + " definido por penales: " + winner->name);
    return winner;
}

Team* teamAtPos(const LeagueTable& table, int pos) {
    int idx = pos - 1;
    if (idx < 0 || idx >= static_cast<int>(table.teams.size())) return nullptr;
    return table.teams[static_cast<size_t>(idx)];
}

Team* loserOfTwoTeamTie(Team* winner, Team* a, Team* b) {
    if (!a) return b;
    if (!b) return a;
    return winner == a ? b : a;
}

Team* simulateTwoLegAggregateTie(Team* firstHome,
                                 Team* firstAway,
                                 const string& label,
                                 SeasonTransitionSummary& summary) {
    if (!firstHome) return firstAway;
    if (!firstAway) return firstHome;

    Team h1 = *firstHome;
    Team a1 = *firstAway;
    MatchResult r1 = playMatch(h1, a1, false, true);
    Team h2 = *firstAway;
    Team a2 = *firstHome;
    MatchResult r2 = playMatch(h2, a2, false, true);

    int firstHomeAgg = r1.homeGoals + r2.awayGoals;
    int firstAwayAgg = r1.awayGoals + r2.homeGoals;

    addLine(summary, label + " ida: " + firstHome->name + " " + to_string(r1.homeGoals) + "-" +
                         to_string(r1.awayGoals) + " " + firstAway->name);
    addLine(summary, label + " vuelta: " + firstAway->name + " " + to_string(r2.homeGoals) + "-" +
                         to_string(r2.awayGoals) + " " + firstHome->name);
    addLine(summary, label + " global: " + firstHome->name + " " + to_string(firstHomeAgg) + "-" +
                         to_string(firstAwayAgg) + " " + firstAway->name);

    if (firstHomeAgg > firstAwayAgg) return firstHome;
    if (firstAwayAgg > firstHomeAgg) return firstAway;

    Team* winner = (teamPenaltyStrength(*firstHome) >= teamPenaltyStrength(*firstAway)) ? firstHome : firstAway;
    addLine(summary, label + " definido por penales: " + winner->name);
    return winner;
}

struct TerceraBSeasonOutcome {
    vector<Team*> directPromoted;
    vector<Team*> promotionCandidates;
    Team* champion = nullptr;
};

struct TerceraARelegationOutcome {
    vector<Team*> promotionTeams;
    vector<Team*> directRelegated;
};

struct SegundaSeasonOutcome {
    Team* champion = nullptr;
    vector<Team*> relegated;
};

TerceraBSeasonOutcome resolveTerceraBSeason(const vector<Team*>& northRanked,
                                            const vector<Team*>& southRanked,
                                            bool usePlayoffLosersForPromotion,
                                            SeasonTransitionSummary& summary) {
    TerceraBSeasonOutcome out;
    Team* northChampion = northRanked.size() > 0 ? northRanked[0] : nullptr;
    Team* southChampion = southRanked.size() > 0 ? southRanked[0] : nullptr;

    if (northChampion) out.directPromoted.push_back(northChampion);
    if (southChampion &&
        find(out.directPromoted.begin(), out.directPromoted.end(), southChampion) == out.directPromoted.end()) {
        out.directPromoted.push_back(southChampion);
    }

    if (northChampion && southChampion) {
        out.champion = simulateSingleLegKnockout(northChampion, southChampion, "Final Tercera B", summary, true);
    } else {
        out.champion = northChampion ? northChampion : southChampion;
    }
    if (out.champion) addLine(summary, "Campeon Tercera B: " + out.champion->name);

    Team* south2 = southRanked.size() > 1 ? southRanked[1] : nullptr;
    Team* north3 = northRanked.size() > 2 ? northRanked[2] : nullptr;
    Team* north2 = northRanked.size() > 1 ? northRanked[1] : nullptr;
    Team* south3 = southRanked.size() > 2 ? southRanked[2] : nullptr;

    Team* tie1Winner = simulateTwoLegAggregateTie(south2, north3, "Cruce 1", summary);
    Team* tie2Winner = simulateTwoLegAggregateTie(north2, south3, "Cruce 2", summary);

    Team* candidate1 = usePlayoffLosersForPromotion ? loserOfTwoTeamTie(tie1Winner, south2, north3) : tie1Winner;
    Team* candidate2 = usePlayoffLosersForPromotion ? loserOfTwoTeamTie(tie2Winner, north2, south3) : tie2Winner;

    if (candidate1) out.promotionCandidates.push_back(candidate1);
    if (candidate2 &&
        find(out.promotionCandidates.begin(), out.promotionCandidates.end(), candidate2) == out.promotionCandidates.end()) {
        out.promotionCandidates.push_back(candidate2);
    }
    return out;
}

TerceraBSeasonOutcome inferTerceraBSeasonFromStandings(Career& career, SeasonTransitionSummary& summary) {
    vector<Team*> teams = career.getDivisionTeams("tercera division b");
    vector<Team*> north;
    vector<Team*> south;
    int groupSize = getCompetitionConfig("tercera division b").groups.groupSize;
    if (groupSize <= 0) groupSize = max(1, static_cast<int>(teams.size()) / 2);
    for (size_t i = 0; i < teams.size(); ++i) {
        if (static_cast<int>(i) < groupSize) north.push_back(teams[i]);
        else south.push_back(teams[i]);
    }

    LeagueTable northTable = buildRankedTable(north, competitionGroupTitle("tercera division b", true), "tercera division b");
    LeagueTable southTable = buildRankedTable(south, competitionGroupTitle("tercera division b", false), "tercera division b");
    return resolveTerceraBSeason(northTable.teams, southTable.teams, true, summary);
}

TerceraARelegationOutcome getInactiveTerceraARelegationByStandings(Career& career) {
    TerceraARelegationOutcome out;
    vector<Team*> ranked = rankedDivisionTeams(career, "tercera division a");
    const int teamCount = static_cast<int>(ranked.size());
    if (teamCount > 0) out.directRelegated.push_back(ranked[static_cast<size_t>(teamCount - 1)]);
    if (teamCount > 1) out.directRelegated.push_back(ranked[static_cast<size_t>(teamCount - 2)]);
    if (teamCount > 3) {
        out.promotionTeams.push_back(ranked[static_cast<size_t>(teamCount - 4)]);
        out.promotionTeams.push_back(ranked[static_cast<size_t>(teamCount - 3)]);
    } else if (teamCount > 2) {
        out.promotionTeams.push_back(ranked[static_cast<size_t>(teamCount - 3)]);
    }
    return out;
}

Team* simulateTerceraAPlayoff(const vector<Team*>& table, SeasonTransitionSummary& summary) {
    if (table.size() < 5) return nullptr;
    Team* s2 = table[1];
    Team* s3 = table[2];
    Team* s4 = table[3];
    Team* s5 = table[4];

    Team* sf1 = simulateTwoLegAggregateTie(s5, s2, "Semifinal 1", summary);
    Team* sf2 = simulateTwoLegAggregateTie(s4, s3, "Semifinal 2", summary);
    if (!sf1) return sf2;
    if (!sf2) return sf1;

    Team* winner = simulateSingleLegKnockout(sf1, sf2, "Final playoff Tercera A", summary, true);
    if (winner) addLine(summary, "Ganador playoff Tercera A: " + winner->name);
    return winner;
}

vector<Team*> resolveTerceraAPromotions(const vector<Team*>& table,
                                        SeasonTransitionSummary& summary) {
    vector<Team*> promoted;
    if (table.empty()) return promoted;

    Team* champion = table.front();
    if (champion) promoted.push_back(champion);

    Team* playoffWinner = simulateTerceraAPlayoff(table, summary);
    if (playoffWinner &&
        find(promoted.begin(), promoted.end(), playoffWinner) == promoted.end()) {
        promoted.push_back(playoffWinner);
    }

    return promoted;
}
Team* simulateSegundaPlayoff(const vector<Team*>& seeds, SeasonTransitionSummary& summary) {
    if (seeds.empty()) return nullptr;
    if (seeds.size() == 1) return seeds.front();

    Team* s1 = seeds.size() > 0 ? seeds[0] : nullptr;
    Team* s2 = seeds.size() > 1 ? seeds[1] : nullptr;
    Team* s3 = seeds.size() > 2 ? seeds[2] : nullptr;
    Team* s4 = seeds.size() > 3 ? seeds[3] : nullptr;
    Team* s5 = seeds.size() > 4 ? seeds[4] : nullptr;
    Team* s6 = seeds.size() > 5 ? seeds[5] : nullptr;
    Team* s7 = seeds.size() > 6 ? seeds[6] : nullptr;

    Team* q1 = simulateTwoLegAggregateTie(s2, s7, "Cuartos 1", summary);
    Team* q2 = simulateTwoLegAggregateTie(s3, s6, "Cuartos 2", summary);
    Team* q3 = simulateTwoLegAggregateTie(s4, s5, "Cuartos 3", summary);

    Team* semi1 = simulateTwoLegAggregateTie(s1, q3, "Semifinal 1", summary);
    Team* semi2 = simulateTwoLegAggregateTie(q1, q2, "Semifinal 2", summary);
    Team* champion = simulateTwoLegAggregateTie(semi1, semi2, "Final playoff", summary);
    if (champion) addLine(summary, "Ganador playoff Segunda: " + champion->name);
    return champion;
}

SegundaSeasonOutcome resolveSegundaSeason(Career& career,
                                          const LeagueTable& north,
                                          const LeagueTable& south,
                                          SeasonTransitionSummary& summary) {
    SegundaSeasonOutcome out;

    vector<Team*> playoffTeams;
    for (int pos = 1; pos <= 3; ++pos) {
        if (Team* team = teamAtPos(north, pos)) playoffTeams.push_back(team);
        if (Team* team = teamAtPos(south, pos)) playoffTeams.push_back(team);
    }

    Team* north4 = teamAtPos(north, 4);
    Team* south4 = teamAtPos(south, 4);
    Team* playoffExtra = nullptr;
    Team* descensoExtra = nullptr;

    if (north4 && south4) {
        Team* winner =
            simulateSingleLegKnockout(
                north4,
                south4,
                "Repechaje 4°",
                summary);

        playoffExtra = winner;
        descensoExtra = (winner == north4) ? south4 : north4;
    } else if (north4 || south4) {
        playoffExtra = north4 ? north4 : south4;
    }

    if (playoffExtra &&
        find(playoffTeams.begin(), playoffTeams.end(), playoffExtra) ==
            playoffTeams.end()) {
        playoffTeams.push_back(playoffExtra);
    }

    vector<Team*> descensoTeams;

    for (int pos = 5; pos <= 7; ++pos) {
        if (Team* team = teamAtPos(north, pos)) descensoTeams.push_back(team);
        if (Team* team = teamAtPos(south, pos)) descensoTeams.push_back(team);
    }

    if (descensoExtra) descensoTeams.push_back(descensoExtra);

    vector<Team*> playoffSeeds = playoffTeams;

    if (!playoffSeeds.empty()) {
        LeagueTable seedTable;

        for (Team* team : playoffSeeds) {
            seedTable.addTeam(team);
        }

        seedTable.sortTable();
        playoffSeeds = seedTable.teams;
    }

    for (Team* team : north.teams) {
        if (team) team->resetSeasonStats();
    }

    for (Team* team : south.teams) {
        if (team) team->resetSeasonStats();
    }

    out.champion =
        simulateSegundaPlayoff(playoffSeeds, summary);

    LeagueTable descensoTable;

    if (!descensoTeams.empty()) {
        for (Team* team : descensoTeams) {
            team->resetSeasonStats();
        }

        auto schedule =
            buildRoundRobinIndexSchedule(
                static_cast<int>(descensoTeams.size()),
                false);

        for (const auto& roundMatches : schedule) {
            for (const auto& match : roundMatches) {
                Team* home =
                    descensoTeams[
                        static_cast<size_t>(match.first)];

                Team* away =
                    descensoTeams[
                        static_cast<size_t>(match.second)];

                team_ai::adjustCpuTactics(
                    *home,
                    *away,
                    career.myTeam);

                team_ai::adjustCpuTactics(
                    *away,
                    *home,
                    career.myTeam);

                playMatch(*home, *away, false, true);
            }

            for (Team* team : descensoTeams) {
                healInjuries(*team, false);
                recoverFitness(*team, 7);
                player_dev::applyWeeklyTrainingPlan(*team);
            }
        }

        descensoTable.title = "Grupo Descenso";

        for (Team* team : descensoTeams) {
            descensoTable.addTeam(team);
        }

        descensoTable.sortTable();
    }

    if (!descensoTable.teams.empty()) {
        int count =
            min(
                2,
                static_cast<int>(
                    descensoTable.teams.size()));

        for (int i = 0; i < count; ++i) {
            out.relegated.push_back(
                descensoTable.teams[
                    descensoTable.teams.size() -
                    1 -
                    static_cast<size_t>(i)]);
        }
    }

    return out;
}

SegundaSeasonOutcome inferSegundaSeasonFromStandings(Career& career) {
    vector<Team*> teams =
        career.getDivisionTeams("segunda division");

    vector<Team*> northTeams;
    vector<Team*> southTeams;

    int groupSize =
        getCompetitionConfig(
            "segunda division").groups.groupSize;

    if (groupSize <= 0) {
        groupSize =
            max(
                1,
                static_cast<int>(teams.size()) / 2);
    }

    for (size_t i = 0; i < teams.size(); ++i) {
        if (static_cast<int>(i) < groupSize) {
            northTeams.push_back(teams[i]);
        } else {
            southTeams.push_back(teams[i]);
        }
    }

    LeagueTable north =
        buildRankedTable(
            northTeams,
            competitionGroupTitle(
                "segunda division",
                true),
            "segunda division");

    LeagueTable south =
        buildRankedTable(
            southTeams,
            competitionGroupTitle(
                "segunda division",
                false),
            "segunda division");

    SeasonTransitionSummary ignoredSummary;

    return resolveSegundaSeason(
        career,
        north,
        south,
        ignoredSummary);
}
Team* liguillaAscensoPrimeraB(const vector<Team*>& table, SeasonTransitionSummary& summary) {
    if (table.size() < 2) return table.empty() ? nullptr : table.front();

    unordered_map<Team*, int> seedPos;
    for (size_t i = 0; i < table.size(); ++i) seedPos[table[i]] = static_cast<int>(i) + 1;

    Team* s2 = table.size() > 1 ? table[1] : nullptr;
    Team* s3 = table.size() > 2 ? table[2] : nullptr;
    Team* s4 = table.size() > 3 ? table[3] : nullptr;
    Team* s5 = table.size() > 4 ? table[4] : nullptr;
    Team* s6 = table.size() > 5 ? table[5] : nullptr;
    Team* s7 = table.size() > 6 ? table[6] : nullptr;
    Team* s8 = table.size() > 7 ? table[7] : nullptr;

    auto seed = [&](Team* team) {
        auto it = seedPos.find(team);
        return it == seedPos.end() ? 99 : it->second;
    };

    Team* w1 = simulateTwoLegTie(s8, s3, "Primera ronda 1", false, summary);
    Team* w2 = simulateTwoLegTie(s7, s4, "Primera ronda 2", false, summary);
    Team* w3 = simulateTwoLegTie(s6, s5, "Primera ronda 3", false, summary);

    vector<Team*> winners;
    if (w1) winners.push_back(w1);
    if (w2) winners.push_back(w2);
    if (w3) winners.push_back(w3);

    Team* worst = nullptr;
    int worstSeed = -1;
    for (Team* team : winners) {
        int currentSeed = seed(team);
        if (currentSeed > worstSeed) {
            worstSeed = currentSeed;
            worst = team;
        }
    }

    Team* semiA = nullptr;
    if (s2 && worst) {
        semiA = simulateTwoLegTie(worst, s2, "Semifinal A", false, summary);
    } else {
        semiA = s2 ? s2 : worst;
    }

    vector<Team*> remaining;
    for (Team* team : winners) {
        if (team && team != worst) remaining.push_back(team);
    }

    Team* semiB = nullptr;
    if (remaining.size() == 2) {
        Team* a = remaining[0];
        Team* b = remaining[1];
        Team* firstHome = seed(a) > seed(b) ? a : b;
        Team* firstAway = (firstHome == a) ? b : a;
        semiB = simulateTwoLegTie(firstHome, firstAway, "Semifinal B", false, summary);
    } else if (remaining.size() == 1) {
        semiB = remaining[0];
    }

    if (!semiA) return semiB;
    if (!semiB) return semiA;

    Team* firstHome = seed(semiA) > seed(semiB) ? semiA : semiB;
    Team* firstAway = (firstHome == semiA) ? semiB : semiA;
    Team* winner = simulateTwoLegTie(firstHome, firstAway, "Final liguilla", true, summary);
    if (winner) addLine(summary, "Ganador liguilla: " + winner->name);
    return winner;
}

struct PrimeraBSeasonOutcome {
    Team* champion = nullptr;
    Team* liguillaWinner = nullptr;
    Team* relegated = nullptr;
};

PrimeraBSeasonOutcome resolvePrimeraBSeason(const vector<Team*>& table,
                                            SeasonTransitionSummary& summary) {
    PrimeraBSeasonOutcome outcome;
    vector<Team*> seeded = table;

    if (!table.empty()) {
        if (table.size() >= 2) {
            int topPts = table[0]->points;
            int tiedCount = 0;
            for (Team* team : table) {
                if (team->points == topPts) tiedCount++;
                else break;
            }

            if (tiedCount >= 2) {
                Team* a = table[0];
                Team* b = table[1];
                outcome.champion =
                    simulateSingleLegKnockout(a, b, "Final por el titulo", summary, true);

                if (outcome.champion && outcome.champion != seeded[0]) {
                    auto it = find(seeded.begin(), seeded.end(), outcome.champion);
                    if (it != seeded.end()) {
                        Team* oldFirst = seeded[0];
                        size_t pos = static_cast<size_t>(it - seeded.begin());
                        seeded[pos] = oldFirst;
                        seeded[0] = outcome.champion;
                    }
                }
            } else {
                outcome.champion = table[0];
            }
        } else {
            outcome.champion = table[0];
        }
    }

    if (outcome.champion) {
        addLine(summary, "Campeon fase regular Primera B: " + outcome.champion->name);
    }

    outcome.liguillaWinner = liguillaAscensoPrimeraB(seeded, summary);

    if (!table.empty()) {
        int n = static_cast<int>(table.size());
        int bottomPts = table[static_cast<size_t>(n - 1)]->points;
        int tiedCount = 0;

        for (int i = n - 1; i >= 0; --i) {
            if (table[static_cast<size_t>(i)]->points == bottomPts) tiedCount++;
            else break;
        }

        if (tiedCount >= 2 && n >= 2) {
            Team* a = table[static_cast<size_t>(n - 1)];
            Team* b = table[static_cast<size_t>(n - 2)];
            Team* winner =
                simulateSingleLegKnockout(a, b, "Definicion descenso Primera B", summary, true);
            outcome.relegated = (winner == a) ? b : a;
        } else {
            outcome.relegated = table[static_cast<size_t>(n - 1)];
        }
    }

    return outcome;
}

SeasonTransitionSummary endSeasonSegundaDivision(Career& career) {
    SeasonTransitionSummary summary;
    addLine(summary, "Fin de temporada (Segunda Division)");

    int idx = divisionIndex(career.activeDivision);
    string higher = (idx > 0) ? kDivisions[static_cast<size_t>(idx - 1)].id : "";
    string lower = (idx >= 0 && idx + 1 < static_cast<int>(kDivisions.size()))
                       ? kDivisions[static_cast<size_t>(idx + 1)].id
                       : "";

    const bool higherIsPrimeraB = higher == "primera b";
    PrimeraBSeasonOutcome higherPrimeraBOutcome;

    if (higherIsPrimeraB) {
        vector<Team*> higherTable = rankedDivisionTeams(career, higher);
        higherPrimeraBOutcome = resolvePrimeraBSeason(higherTable, summary);
    }

    if (career.groupNorthIdx.empty() || career.groupSouthIdx.empty()) {
        career.buildSegundaGroups();
    }

    LeagueTable north =
        buildCompetitionGroupTable(career, true);

    LeagueTable south =
        buildCompetitionGroupTable(career, false);

    SegundaSeasonOutcome segundaOutcome =
        resolveSegundaSeason(
            career,
            north,
            south,
            summary);

    Team* champion = segundaOutcome.champion;

    if (champion) {
        summary.champion = champion->name;
        addLine(summary, "Campeon playoff: " + champion->name);
    }

    vector<Team*> relegate =
        segundaOutcome.relegated;

    vector<Team*> promote;
    if (!higher.empty() && champion) promote.push_back(champion);

    vector<Team*> fromHigher;

    if (!higher.empty()) {
        if (higherIsPrimeraB) {
            if (higherPrimeraBOutcome.relegated && !promote.empty()) {
                fromHigher.push_back(higherPrimeraBOutcome.relegated);
            }
        } else {
            fromHigher =
                bottomByStandings(career, higher, static_cast<int>(promote.size()));
        }
    }

    vector<Team*> fromLower;
    if (!lower.empty()) {
        const CompetitionConfig& lowerConfig = getCompetitionConfig(lower);

        if (lowerConfig.seasonHandler == CompetitionSeasonHandler::TerceraA) {
            vector<Team*> lowerTable = rankedDivisionTeams(career, lower);
            fromLower = resolveTerceraAPromotions(lowerTable, summary);

            if (fromLower.size() > relegate.size()) {
                fromLower.resize(relegate.size());
            }
        } else {
            fromLower =
                topByStandings(career, lower, static_cast<int>(relegate.size()));
        }
    }

    for (Team* team : promote) {
        if (!higher.empty()) {
            team->division = higher;
            team->budget += 50000;
            team->morale = 60;
        }
    }
    for (Team* team : relegate) {
        if (!lower.empty()) {
            team->division = lower;
            team->budget = max(0LL, team->budget - 20000);
            team->morale = 40;
        }
    }
    for (Team* team : fromHigher) {
        team->division = career.activeDivision;

        if (higherIsPrimeraB) {
            team->budget = max(0LL, team->budget - 20000);
        }

        team->morale = 45;
    }
    for (Team* team : fromLower) {
        team->division = career.activeDivision;

        if (!lower.empty() &&
            getCompetitionConfig(lower).seasonHandler == CompetitionSeasonHandler::TerceraA) {
            team->budget += 40000;
        }

        team->morale = 55;
    }

    addMovementLines(summary, "Ascensos", promote, "Descensos", relegate);
    summary.note = "Temporada con playoff de ascenso y grupo de descenso.";
    awardSeasonPrizeMoney(career, buildRelevantCompetitionTable(career), summary);
    updateWorldDivisionRecords(career, summary);
    recordSeasonHistory(career, summary.champion, promote, relegate, summary.note);
    advanceToNextSeason(career, summary);
    return summary;
}

SeasonTransitionSummary endSeasonPrimeraB(Career& career) {
    SeasonTransitionSummary summary;
    addLine(summary, "Fin de temporada (Primera B)");

    vector<Team*> table = career.leagueTable.teams;
    PrimeraBSeasonOutcome primeraBOutcome = resolvePrimeraBSeason(table, summary);
    Team* champion = primeraBOutcome.champion;
    Team* liguillaWinner = primeraBOutcome.liguillaWinner;
    Team* relegated = primeraBOutcome.relegated;

    if (champion) {
        summary.champion = champion->name;
    }

    int idx = divisionIndex(career.activeDivision);
    string higher = (idx > 0) ? kDivisions[static_cast<size_t>(idx - 1)].id : "";
    string lower = (idx >= 0 && idx + 1 < static_cast<int>(kDivisions.size()))
                       ? kDivisions[static_cast<size_t>(idx + 1)].id
                       : "";

    const bool lowerIsSegunda = lower == "segunda division";
    SegundaSeasonOutcome lowerSegundaOutcome;

    if (lowerIsSegunda) {
        lowerSegundaOutcome = inferSegundaSeasonFromStandings(career);
    }

    vector<Team*> promote;
    if (!higher.empty() && champion) promote.push_back(champion);
    if (!higher.empty() && liguillaWinner &&
        find(promote.begin(), promote.end(), liguillaWinner) == promote.end()) {
        promote.push_back(liguillaWinner);
    }

    vector<Team*> relegate;
    if (!lower.empty() && relegated) relegate.push_back(relegated);

    vector<Team*> fromHigher =
        higher.empty() ? vector<Team*>() : bottomByStandings(career, higher, static_cast<int>(promote.size()));
    vector<Team*> fromLower;

    if (!lower.empty()) {
        if (lowerIsSegunda) {
            if (lowerSegundaOutcome.champion && !relegate.empty()) {
                fromLower.push_back(lowerSegundaOutcome.champion);
            }
        } else {
            fromLower =
                topByStandings(career, lower, static_cast<int>(relegate.size()));
        }
    }

    for (Team* team : promote) {
        if (!higher.empty()) {
            team->division = higher;
            team->budget += 50000;
            team->morale = 60;
        }
    }
    for (Team* team : relegate) {
        if (!lower.empty()) {
            team->division = lower;
            team->budget = max(0LL, team->budget - 20000);
            team->morale = 40;
        }
    }
    for (Team* team : fromHigher) {
        team->division = career.activeDivision;
        team->morale = 45;
    }
    for (Team* team : fromLower) {
        team->division = career.activeDivision;

        if (lowerIsSegunda) {
            team->budget += 50000;
        }

        team->morale = 55;
    }

    addMovementLines(summary, "Ascensos", promote, "Descenso", relegate);
    summary.note = "Campeon regular y liguilla de ascenso.";
    awardSeasonPrizeMoney(career, career.leagueTable, summary);
    updateWorldDivisionRecords(career, summary);
    recordSeasonHistory(career, summary.champion, promote, relegate, summary.note);
    advanceToNextSeason(career, summary);
    return summary;
}

SeasonTransitionSummary endSeasonTerceraA(Career& career) {
    SeasonTransitionSummary summary;
    addLine(summary, "Fin de temporada (Tercera Division A)");

    vector<Team*> table = career.leagueTable.teams;
    Team* champion = table.empty() ? nullptr : table.front();
    if (champion) {
        summary.champion = champion->name;
        addLine(summary, "Ascenso directo a Segunda: " + champion->name);
    }

    int idx = divisionIndex(career.activeDivision);
    string higher =
        (idx > 0)
            ? kDivisions[static_cast<size_t>(idx - 1)].id
            : "";

    string lower =
        (idx >= 0 &&
         idx + 1 < static_cast<int>(kDivisions.size()))
            ? kDivisions[static_cast<size_t>(idx + 1)].id
            : "";

    const bool higherIsSegunda =
        higher == "segunda division";

    SegundaSeasonOutcome higherSegundaOutcome;

    if (higherIsSegunda) {
        int segundaIdx = divisionIndex(higher);

        if (segundaIdx > 0) {
            const string& primeraBDivision =
                kDivisions[static_cast<size_t>(segundaIdx - 1)].id;

            if (getCompetitionConfig(primeraBDivision).seasonHandler ==
                CompetitionSeasonHandler::PrimeraB) {
                vector<Team*> primeraBTable =
                    rankedDivisionTeams(career, primeraBDivision);

                SeasonTransitionSummary ignoredPrimeraBSummary;
                resolvePrimeraBSeason(
                    primeraBTable,
                    ignoredPrimeraBSummary);
            }
        }

        higherSegundaOutcome =
            inferSegundaSeasonFromStandings(career);
    }

    vector<Team*> promotionOutcome =
        resolveTerceraAPromotions(table, summary);

    vector<Team*> promotionTeamsA;
    vector<Team*> directRelegated;
    if (table.size() >= 14) {
        promotionTeamsA.push_back(table[12]);
        promotionTeamsA.push_back(table[13]);
    } else if (table.size() >= 12) {
        promotionTeamsA.push_back(table[table.size() - 2]);
        promotionTeamsA.push_back(table.back());
    }
    if (table.size() >= 16) {
        directRelegated.push_back(table[14]);
        directRelegated.push_back(table[15]);
    } else if (table.size() >= 14) {
        directRelegated.push_back(table[table.size() - 2]);
        directRelegated.push_back(table.back());
    }

    TerceraBSeasonOutcome lowerOutcome = inferTerceraBSeasonFromStandings(career, summary);
    vector<Team*> promotedByPromotion;
    vector<Team*> relegatedByPromotion;
    int promotionTies =
        min(static_cast<int>(promotionTeamsA.size()), static_cast<int>(lowerOutcome.promotionCandidates.size()));
    for (int i = 0; i < promotionTies; ++i) {
        Team* aTeam = promotionTeamsA[static_cast<size_t>(i)];
        Team* bTeam = lowerOutcome.promotionCandidates[static_cast<size_t>(i)];
        Team* winner =
            simulateTwoLegAggregateTie(bTeam, aTeam, "Promocion Tercera A/B " + to_string(i + 1), summary);
        if (winner == bTeam) {
            promotedByPromotion.push_back(bTeam);
            relegatedByPromotion.push_back(aTeam);
        }
    }

    vector<Team*> promote;
    if (!higher.empty()) {
        promote = promotionOutcome;
    }

    vector<Team*> fromHigher;

    if (!higher.empty()) {
        if (higherIsSegunda) {
            fromHigher =
                higherSegundaOutcome.relegated;

            if (fromHigher.size() > promote.size()) {
                fromHigher.resize(promote.size());
            }
        } else {
            fromHigher =
                bottomByStandings(
                    career,
                    higher,
                    static_cast<int>(promote.size()));
        }
    }
    vector<Team*> fromLower = lowerOutcome.directPromoted;

    for (Team* team : promote) {
        if (!higher.empty()) {
            team->division = higher;
            team->budget += 40000;
            team->morale = 60;
        }
    }
    for (Team* team : fromHigher) {
        team->division = career.activeDivision;

        if (higherIsSegunda) {
            team->budget =
                max(
                    0LL,
                    team->budget - 20000);
        }

        team->morale = 45;
    }
    for (Team* team : directRelegated) {
        if (!lower.empty()) {
            team->division = lower;
            team->budget = max(0LL, team->budget - 15000);
            team->morale = 40;
        }
    }
    for (Team* team : fromLower) {
        if (!lower.empty()) {
            team->division = career.activeDivision;
            team->budget += 25000;
            team->morale = 58;
        }
    }
    for (Team* team : relegatedByPromotion) {
        if (!lower.empty()) {
            team->division = lower;
            team->budget = max(0LL, team->budget - 15000);
            team->morale = 42;
        }
    }
    for (Team* team : promotedByPromotion) {
        if (!lower.empty()) {
            team->division = career.activeDivision;
            team->morale = 58;
        }
    }

    vector<Team*> allRelegated = directRelegated;
    allRelegated.insert(allRelegated.end(), relegatedByPromotion.begin(), relegatedByPromotion.end());
    vector<Team*> allPromoted = promote;
    allPromoted.insert(allPromoted.end(), promotedByPromotion.begin(), promotedByPromotion.end());
    addMovementLines(summary, "Ascensos", allPromoted, "Descensos", allRelegated);

    summary.note = "Ascenso directo, playoff y promocion interdivisional.";
    awardSeasonPrizeMoney(career, career.leagueTable, summary);
    updateWorldDivisionRecords(career, summary);
    recordSeasonHistory(career, summary.champion, allPromoted, allRelegated, summary.note);
    advanceToNextSeason(career, summary);
    return summary;
}

SeasonTransitionSummary endSeasonTerceraB(Career& career) {
    SeasonTransitionSummary summary;
    addLine(summary, "Fin de temporada (Tercera Division B)");

    if (career.groupNorthIdx.empty() || career.groupSouthIdx.empty()) {
        career.buildRegionalGroups();
    }
    LeagueTable north = buildCompetitionGroupTable(career, true);
    LeagueTable south = buildCompetitionGroupTable(career, false);

    TerceraBSeasonOutcome outcome = resolveTerceraBSeason(north.teams, south.teams, true, summary);
    TerceraARelegationOutcome higherOutcome = getInactiveTerceraARelegationByStandings(career);

    vector<Team*> promotedByPromotion;
    vector<Team*> relegatedByPromotion;
    int promotionTies = min(static_cast<int>(higherOutcome.promotionTeams.size()),
                            static_cast<int>(outcome.promotionCandidates.size()));
    for (int i = 0; i < promotionTies; ++i) {
        Team* aTeam = higherOutcome.promotionTeams[static_cast<size_t>(i)];
        Team* bTeam = outcome.promotionCandidates[static_cast<size_t>(i)];
        Team* winner =
            simulateTwoLegAggregateTie(bTeam, aTeam, "Promocion Tercera A/B " + to_string(i + 1), summary);
        if (winner == bTeam) {
            promotedByPromotion.push_back(bTeam);
            relegatedByPromotion.push_back(aTeam);
        }
    }

    int idx = divisionIndex(career.activeDivision);
    string higher = (idx > 0) ? kDivisions[static_cast<size_t>(idx - 1)].id : "";

    for (Team* team : outcome.directPromoted) {
        if (!higher.empty()) {
            team->division = higher;
            team->budget += 25000;
            team->morale = 60;
        }
    }
    for (Team* team : higherOutcome.directRelegated) {
        team->division = career.activeDivision;
        team->budget = max(0LL, team->budget - 15000);
        team->morale = 45;
    }
    for (Team* team : promotedByPromotion) {
        if (!higher.empty()) {
            team->division = higher;
            team->morale = 58;
        }
    }
    for (Team* team : relegatedByPromotion) {
        team->division = career.activeDivision;
        team->budget = max(0LL, team->budget - 15000);
        team->morale = 45;
    }

    summary.champion = outcome.champion ? outcome.champion->name : "";
    vector<Team*> allPromoted = outcome.directPromoted;
    allPromoted.insert(allPromoted.end(), promotedByPromotion.begin(), promotedByPromotion.end());

    vector<Team*> allRelegated = higherOutcome.directRelegated;
    allRelegated.insert(allRelegated.end(), relegatedByPromotion.begin(), relegatedByPromotion.end());

    addMovementLines(summary, "Ascensos", allPromoted, "Descensos", allRelegated);

    summary.note = "Campeones zonales, final y promocion por playoff.";
    awardSeasonPrizeMoney(career, buildRelevantCompetitionTable(career), summary);
    updateWorldDivisionRecords(career, summary);
    recordSeasonHistory(career, summary.champion, allPromoted, allRelegated, summary.note);
    advanceToNextSeason(career, summary);
    return summary;
}

}  // namespace

SeasonTransitionSummary endSeason(Career& career) {
    const CompetitionConfig& config = getCompetitionConfig(career.activeDivision);
    switch (config.seasonHandler) {
        case CompetitionSeasonHandler::SegundaGroups:
            return endSeasonSegundaDivision(career);
        case CompetitionSeasonHandler::PrimeraB:
            return endSeasonPrimeraB(career);
        case CompetitionSeasonHandler::TerceraA:
            return endSeasonTerceraA(career);
        case CompetitionSeasonHandler::TerceraB:
            return endSeasonTerceraB(career);
        default:
            break;
    }

    SeasonTransitionSummary summary;
    addLine(summary, "Fin de temporada");

    vector<Team*> table = career.leagueTable.teams;
    Team* champion = table.empty() ? nullptr : table.front();

    if (config.seasonHandler == CompetitionSeasonHandler::PrimeraDivision &&
        table.size() >= 2 &&
        table[0]->points == table[1]->points) {
        champion =
            simulateSingleLegKnockout(table[0], table[1], "Final por el titulo", summary, true);
    }

    if (champion) {
        summary.champion = champion->name;
        addLine(summary, "Campeon: " + champion->name);
    }

    int idx = divisionIndex(career.activeDivision);
    string higher = (idx > 0) ? kDivisions[static_cast<size_t>(idx - 1)].id : "";
    string lower = (idx >= 0 && idx + 1 < static_cast<int>(kDivisions.size()))
                       ? kDivisions[static_cast<size_t>(idx + 1)].id
                       : "";

    int n = static_cast<int>(table.size());
    vector<Team*> promote;
    vector<Team*> relegate;
    int promoteSlots = higher.empty() ? 0 : 2;
    int relegateSlots = lower.empty() ? 0 : 2;

    if (promoteSlots > 0) {
        int count = min(promoteSlots, n);
        for (int i = 0; i < count; ++i) promote.push_back(table[static_cast<size_t>(i)]);
    }

    int actualPromote = static_cast<int>(promote.size());
    int relegateCount = min(relegateSlots, max(0, n - actualPromote));
    for (int i = 0; i < relegateCount; ++i) {
        relegate.push_back(table[static_cast<size_t>(n - 1 - i)]);
    }

    vector<Team*> fromHigher =
        higher.empty() ? vector<Team*>() : bottomByStandings(career, higher, actualPromote);
    vector<Team*> fromLower;
    vector<Team*> primeraBRelegatedToSegunda;
    vector<Team*> segundaPromotedToPrimeraB;
    string segundaDivision;

    if (!lower.empty() && config.seasonHandler == CompetitionSeasonHandler::PrimeraDivision &&
        getCompetitionConfig(lower).seasonHandler == CompetitionSeasonHandler::PrimeraB) {
        vector<Team*> pbTable = rankedDivisionTeams(career, lower);
        PrimeraBSeasonOutcome pbOutcome = resolvePrimeraBSeason(pbTable, summary);

        if (pbOutcome.champion) fromLower.push_back(pbOutcome.champion);
        if (pbOutcome.liguillaWinner &&
            pbOutcome.liguillaWinner != pbOutcome.champion) {
            fromLower.push_back(pbOutcome.liguillaWinner);
        }

        if (static_cast<int>(fromLower.size()) > relegateCount) {
            fromLower.resize(static_cast<size_t>(relegateCount));
        }

        int pbIdx = divisionIndex(lower);
        if (pbIdx >= 0 && pbIdx + 1 < static_cast<int>(kDivisions.size())) {
            segundaDivision = kDivisions[static_cast<size_t>(pbIdx + 1)].id;
        }

        if (!segundaDivision.empty() && pbOutcome.relegated) {
            primeraBRelegatedToSegunda.push_back(pbOutcome.relegated);

            SegundaSeasonOutcome segundaOutcome =
                inferSegundaSeasonFromStandings(career);

            if (segundaOutcome.champion) {
                segundaPromotedToPrimeraB.push_back(segundaOutcome.champion);
            }

            if (segundaPromotedToPrimeraB.size() >
                primeraBRelegatedToSegunda.size()) {
                segundaPromotedToPrimeraB.resize(
                    primeraBRelegatedToSegunda.size());
            }
        }
    } else {
        fromLower =
            lower.empty() ? vector<Team*>() : topByStandings(career, lower, relegateCount);
    }

    for (Team* team : promote) {
        if (!higher.empty()) {
            team->division = higher;
            team->morale = 60;
        }
    }
    for (Team* team : relegate) {
        if (!lower.empty()) {
            team->division = lower;
            team->morale = 40;
        }
    }
    for (Team* team : fromHigher) {
        team->division = career.activeDivision;
        team->morale = 45;
    }
    for (Team* team : fromLower) {
        team->division = career.activeDivision;
        if (!lower.empty() &&
            config.seasonHandler == CompetitionSeasonHandler::PrimeraDivision &&
            getCompetitionConfig(lower).seasonHandler == CompetitionSeasonHandler::PrimeraB) {
            team->budget += 50000;
        }
        team->morale = 55;
    }

    for (Team* team : primeraBRelegatedToSegunda) {
        if (!segundaDivision.empty()) {
            team->division = segundaDivision;
            team->budget = max(0LL, team->budget - 20000);
            team->morale = 40;
        }
    }

    for (Team* team : segundaPromotedToPrimeraB) {
        if (!lower.empty()) {
            team->division = lower;
            team->budget += 50000;
            team->morale = 55;
        }
    }

    if (!primeraBRelegatedToSegunda.empty()) {
        addLine(summary,
                "Descenso Primera B a Segunda: " +
                    joinTeamNames(primeraBRelegatedToSegunda));
    }
    if (!segundaPromotedToPrimeraB.empty()) {
        addLine(summary,
                "Ascenso Segunda a Primera B: " +
                    joinTeamNames(segundaPromotedToPrimeraB));
    }

    addMovementLines(summary, "Ascensos", promote, "Descensos", relegate);
    summary.note = "Cierre de temporada regular.";
    awardSeasonPrizeMoney(career, career.leagueTable, summary);
    updateWorldDivisionRecords(career, summary);
    recordSeasonHistory(career, summary.champion, promote, relegate, summary.note);
    advanceToNextSeason(career, summary);
    return summary;
}
