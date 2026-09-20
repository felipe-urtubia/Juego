#include "simulation/match_stats.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace match_stats {

void applyImpact(MatchStats& stats, const MatchEventImpact& impact) {
    stats.homeGoals += impact.homeGoalsDelta;
    stats.awayGoals += impact.awayGoalsDelta;
    stats.homeShots += impact.homeShotsDelta;
    stats.awayShots += impact.awayShotsDelta;
    stats.homeShotsOnTarget += impact.homeShotsOnTargetDelta;
    stats.awayShotsOnTarget += impact.awayShotsOnTargetDelta;
    stats.homeCorners += impact.homeCornersDelta;
    stats.awayCorners += impact.awayCornersDelta;
    stats.homeDangerousAttacks += impact.homeDangerousAttacksDelta;
    stats.awayDangerousAttacks += impact.awayDangerousAttacksDelta;
    stats.homeFouls += impact.homeFoulsDelta;
    stats.awayFouls += impact.awayFoulsDelta;
    stats.homeYellowCards += impact.homeYellowCardsDelta;
    stats.awayYellowCards += impact.awayYellowCardsDelta;
    stats.homeRedCards += impact.homeRedCardsDelta;
    stats.awayRedCards += impact.awayRedCardsDelta;
    stats.homeExpectedGoals += impact.homeExpectedGoalsDelta;
    stats.awayExpectedGoals += impact.awayExpectedGoalsDelta;
}

void pushEvent(MatchTimeline& timeline, MatchStats& stats, const MatchEvent& event) {
    timeline.events.push_back(event);
    applyImpact(stats, event.impact);
}

vector<string> buildLegacyTimeline(const MatchTimeline& timeline) {
    vector<MatchEvent> ordered = timeline.events;
    sort(ordered.begin(), ordered.end(), [](const MatchEvent& left, const MatchEvent& right) {
        if (left.minute != right.minute) return left.minute < right.minute;
        return static_cast<int>(left.type) < static_cast<int>(right.type);
    });

    vector<string> lines;
    for (const MatchEvent& event : ordered) {
        ostringstream out;
        out << event.minute << "' ";
        if (!event.teamName.empty()) out << event.teamName << ": ";
        if (!event.playerName.empty() && event.description.find(event.playerName) == string::npos) {
            out << event.playerName << " ";
        }
        out << event.description;
        lines.push_back(out.str());
    }
    return lines;
}

int countSubstitutions(const MatchTimeline& timeline, const string& teamName) {
    return static_cast<int>(count_if(timeline.events.begin(), timeline.events.end(), [&](const MatchEvent& event) {
        return event.type == MatchEventType::Substitution && event.teamName == teamName;
    }));
}

}  // namespace match_stats
