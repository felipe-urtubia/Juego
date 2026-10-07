#include "career/career_service.h"

#include "career/career_reports.h"
#include "career/week_simulation.h"
#include "transfers/transfer_market.h"

#include <algorithm>
#include <cstddef>
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





void CareerService::updateSocialDynamics(int pointsDelta) {
    if (!career_.myTeam) return;
    
    // Update dressing room based on match result
    if (pointsDelta > 0) {
        career_.dressingRoomDynamics.overallCliqueMorale = std::min(100, career_.dressingRoomDynamics.overallCliqueMorale + 5);
    } else if (pointsDelta < 0) {
        career_.dressingRoomDynamics.overallCliqueMorale = std::max(0, career_.dressingRoomDynamics.overallCliqueMorale - 10);
    }
    
    // Dispatch event if dispatcher available
    if (eventDispatcher_) {
        Events::SeasonProgressedEvent evt;
        evt.season = career_.currentSeason;
        evt.week = career_.currentWeek;
        eventDispatcher_->publishSeason(evt);
    }
}

void CareerService::updateManagerStress(int performanceImpact) {
    if (performanceImpact < -50) {
        career_.managerStress.stressLevel = std::min(100, career_.managerStress.stressLevel + 15);
    } else if (performanceImpact > 50) {
        career_.managerStress.stressLevel = std::max(0, career_.managerStress.stressLevel - 10);
    }
    
    // Dispatch event if dispatcher available
    if (eventDispatcher_) {
        Events::ManagerStressChangedEvent evt;
        evt.newStress = career_.managerStress.stressLevel;
        evt.reason = performanceImpact < 0 ? "Poor results" : "Good results";
        eventDispatcher_->publishStress(evt);
    }
}

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

void CareerService::generateDevelopmentReports() {
    if (!career_.myTeam) return;

    const CareerReport report = buildClubReport(career_);
    for (const auto& block : report.blocks) {
        if (block.title.find("Entrenamiento") == std::string::npos &&
            block.title.find("Cantera") == std::string::npos &&
            block.title.find("Desarrollo") == std::string::npos) {
            continue;
        }
        for (std::size_t i = 0; i < block.lines.size() && i < 2; ++i) {
            career_.addInboxItem(block.title + " | " + block.lines[i], "Desarrollo");
        }
    }
}


void CareerService::generateWeeklyNarrative() {
    const CareerReport report = buildWeeklyDashboardReport(career_);
    int added = 0;
    for (const auto& block : report.blocks) {
        for (const auto& line : block.lines) {
            if (line.empty()) continue;
            career_.addNews(block.title + ": " + line);
            if (++added >= 3) return;
        }
    }
}

void CareerService::updateManagerReputation(int matchResult) {
    if (matchResult > 0) {
        career_.managerReputation = std::min(100, career_.managerReputation + 1);
    } else if (matchResult < 0) {
        career_.managerReputation = std::max(0, career_.managerReputation - 2);
    }
}

void CareerService::handleBoardStatus() {
    if (career_.boardConfidence < 30) {
        career_.boardWarningWeeks++;
    }
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
