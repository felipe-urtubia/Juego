#include "career/career_service.h"

#include "career/career_reports.h"
#include "career/staff_service.h"
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

void CareerService::simulateWeekMatches() {
    if (!career_.myTeam) {
        throw std::runtime_error("Career myTeam is null");
    }
    
    if (!hasValidActiveTeamAccess(career_)) {
        throw std::runtime_error("Invalid team pointers in activeTeams");
    }

    simulateCareerWeek(career_);
}

void CareerService::updatePlayerPhysicalState() {
    if (!career_.myTeam || career_.myTeam->players.empty()) {
        return;
    }
    
    // Update each player's condition and injuries
    for (auto& player : career_.myTeam->players) {
        player.fitness = std::max(0, std::min(player.fitness, player.stamina));
        if (player.injuryWeeks <= 0) {
            player.injured = false;
            player.injuryType.clear();
        }
    }
}

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

void CareerService::processContractUpdates() {
    if (!career_.myTeam) return;
    
    for (auto& player : career_.myTeam->players) {
        if (player.contractWeeks > 0) {
            player.contractWeeks--;
        }
    }
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

void CareerService::processIncomingOffers() {
    transfer_market::processCpuTransfers(career_);
}

void CareerService::updatePendingTransfers() {
    for (auto& transfer : career_.pendingTransfers) {
        if (transfer.loanWeeks > 0) {
            transfer.loanWeeks--;
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

void CareerService::dispatchStaffBriefing() {
    for (const auto& line : staff_service::buildWeeklyStaffBriefingLines(career_, 4)) {
        career_.addInboxItem(line, "Staff");
    }
}

void CareerService::addSquadAlerts() {
    if (!career_.myTeam) return;

    int injuredPlayers = 0;
    int lowFitnessPlayers = 0;
    int expiringContracts = 0;
    for (const auto& player : career_.myTeam->players) {
        if (player.injured || player.injuryWeeks > 0) injuredPlayers++;
        if (player.fitness < 55) lowFitnessPlayers++;
        if (player.contractWeeks > 0 && player.contractWeeks <= 8) expiringContracts++;
    }

    if (injuredPlayers > 0) {
        career_.addInboxItem("Plantel | " + std::to_string(injuredPlayers) +
                                 " jugador(es) lesionado(s) requieren seguimiento.",
                             "Medical");
    }
    if (lowFitnessPlayers >= 3) {
        career_.addInboxItem("Plantel | " + std::to_string(lowFitnessPlayers) +
                                 " jugador(es) llegan con condicion baja.",
                             "Staff");
    }
    if (expiringContracts > 0) {
        career_.addInboxItem("Contratos | " + std::to_string(expiringContracts) +
                                 " renovacion(es) entran en zona urgente.",
                             "Directiva");
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
