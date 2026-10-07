#include "career/career_service.h"

#include "career/week_simulation.h"
#include "transfers/transfer_market.h"

#include <algorithm>
#include <stdexcept>

namespace {

bool hasValidActiveTeamAccess(const Career& career) {
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        if (!career.getActiveTeamAt(i)) return false;
    }
    return true;
}

bool containsActiveTeam(const Career& career, const Team* target) {
    const TeamId targetId = career.getTeamIdFor(target);
    if (targetId == kInvalidTeamId) return false;
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        if (career.getActiveTeamIdAt(i) == targetId) return true;
    }
    return false;
}

}  // namespace

// ============================================================================
// CareerService Implementation
// ============================================================================
// This service layer wraps existing game systems and adds safety checks
// and event dispatching without reimplementing existing logic





std::vector<Team*> CareerService::buildJobMarket(bool includeRelegated) {
    (void)includeRelegated;
    std::vector<Team*> jobs;
    if (!career_.myTeam) return jobs;
    
    for (auto& team : career_.allTeams) {
        if (team.name != career_.myTeam->name) {
            jobs.push_back(&team);
        }
    }
    
    return jobs;
}


Team* CareerService::findTeamByNameSafe(const std::string& name) {
    auto it = std::find_if(
        career_.allTeams.begin(),
        career_.allTeams.end(),
        [&name](const Team& t) { return t.name == name; }
    );
    
    if (it != career_.allTeams.end()) {
        return &(*it);
    }
    
    return nullptr;
}

bool CareerService::validateCareerState() {
    // Validate all critical pointers
    if (!career_.myTeam) return false;
    
    if (!containsActiveTeam(career_, career_.myTeam)) return false;
    
    // Validate all active teams
    if (!hasValidActiveTeamAccess(career_)) {
        return false;
    }
    
    return true;
}
