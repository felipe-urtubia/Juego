#include "career/career_service.h"

#include <algorithm>

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
