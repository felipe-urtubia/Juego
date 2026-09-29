#pragma once

#include "engine/models.h"
#include "utils/event_system.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// ============================================================================
// PHASE 2-3: Career Service Layer
// ============================================================================
// Consolidates free functions into cohesive service object
// Enables dependency injection and testability
// Reduces god object anti-pattern

class CareerService {
private:
    Career& career_;
    std::shared_ptr<Events::EventDispatcher> eventDispatcher_;

public:
    explicit CareerService(Career& career, std::shared_ptr<Events::EventDispatcher> dispatcher = nullptr)
        : career_(career), eventDispatcher_(dispatcher ? dispatcher : std::make_shared<Events::EventDispatcher>()) {}

    // ========== WEEK SIMULATION SERVICES ==========
    
    // Simulate all matches for current week
    void simulateWeekMatches(const std::vector<std::pair<int, int>>& matches, const std::unordered_map<TeamId, int>& pointsBefore, int& outMyTeamPointsDelta);

    // Simulate one round of the season cup
    void simulateSeasonCupRound();

    // Simulate matches and world events in a background division
    void simulateBackgroundDivisionWeek(const std::string& divisionId);
    
    // Update physical state of all players (injuries, fitness, training effects)
    void updatePlayerPhysicalState(const std::vector<TeamId>& activeTeamIds, const std::unordered_map<TeamId, std::vector<int>>& suspensionsBefore, bool cupWeek);
    
    // Process financial operations (wages, sponsorships, facility costs)
    void processWeeklyFinances(const std::unordered_map<TeamId, int>& pointsBefore);
    
    // Update team social dynamics based on match results
    void updateSocialDynamics(int pointsDelta);
    
    // Update manager stress levels
    void updateManagerStress(int performanceImpact);
    
    // ========== SQUAD MANAGEMENT SERVICES ==========
    
    // Build job market (available team offers)
    std::vector<Team*> buildJobMarket(bool includeRelegated = false);
    
    // Update contract statuses and handle expirations
    void processContractUpdates();
    
    // Generate team development proposals
    void generateDevelopmentReports();
    
    // ========== TRANSFER SERVICES ==========
    
    // Process incoming transfer offers for squad players
    void processIncomingOffers();
    
    // Update pending transfers status
    void updatePendingTransfers();
    
    // ========== NEWS & COMMUNICATION SERVICES ==========
    
    // Generate weekly narrative (match reports, team news)
    void generateWeeklyNarrative();
    
    // Dispatch staff briefings and tactical advice
    void dispatchStaffBriefing();

    // Preserve weekly staff news, inbox messages and UI briefing
    void dispatchWeeklyStaffBriefing();

    // Weekly physical, offensive and contractual squad news
    void addWeeklySquadNewsAlerts();

    // Generate weekly league, squad and opponent news
    void generateWeeklyNarratives(int myTeamPointsDelta);

    // Weekly interviews, youth recognition and manager job rumors
    void generateWeeklyManagerCareerEvents();
    void updateWeeklyManagerReputation();
    void handleWeeklyManagerStatus();
    
    // Add squad-related alerts and notifications
    void addSquadAlerts();
    
    // ========== REPUTATION & BOARD SERVICES ==========
    
    // Update manager reputation based on recent performance
    void updateManagerReputation(int matchResult);
    
    // Handle board confidence and warnings
    void handleBoardStatus();
    
    // ========== UTILITY SERVICES ==========
    
    // Safely get team by name with bounds checking
    Team* findTeamByNameSafe(const std::string& name);
    
    // Validate career state integrity
    bool validateCareerState();
    
    // Get underlying event dispatcher
    std::shared_ptr<Events::EventDispatcher> getEventDispatcher() const {
        return eventDispatcher_;
    }
};
