#include "ui/economy_fairplay.h"

#include "engine/models.h"
#include "finance/finance_system.h"

#include <algorithm>

namespace economy_fairplay {

// Fair Play rules for each division
FairPlayRules EconomyFairPlaySystem::g_rules[3];
bool EconomyFairPlaySystem::g_initialized = false;

void EconomyFairPlaySystem::initialize(Career& career) {
    (void)career;
    // Primera Division - Stricter rules
    g_rules[0].division = "Primera";
    g_rules[0].maxSalaryCap = 50000000LL;        // $50M salary cap
    g_rules[0].minBudgetReserve = 5000000LL;     // $5M minimum reserve
    g_rules[0].maxSalaryPercentage = 0.72f;      // 72% of revenue can be salary
    g_rules[0].maxForeignPlayers = 8;            // Max 8 foreign players
    g_rules[0].minYouthPercentage = 0.15f;       // Min 15% youth (under 23)
    
    // Primera B - Medium rules
    g_rules[1].division = "Primera B";
    g_rules[1].maxSalaryCap = 20000000LL;
    g_rules[1].minBudgetReserve = 2000000LL;
    g_rules[1].maxSalaryPercentage = 0.75f;
    g_rules[1].maxForeignPlayers = 6;
    g_rules[1].minYouthPercentage = 0.20f;
    
    // Segunda - Looser rules
    g_rules[2].division = "Segunda";
    g_rules[2].maxSalaryCap = 10000000LL;
    g_rules[2].minBudgetReserve = 1000000LL;
    g_rules[2].maxSalaryPercentage = 0.78f;
    g_rules[2].maxForeignPlayers = 5;
    g_rules[2].minYouthPercentage = 0.25f;
    
    g_initialized = true;
}

bool EconomyFairPlaySystem::checkSalaryCompliance(const Team& team, Career& career) {
    
    long long totalSalary = 0;
    for (const auto& player : team.players) {
        totalSalary += player.wage;
    }
    
    return totalSalary <= getMaxAllowedSalary(team, career);
}

bool EconomyFairPlaySystem::checkBudgetCompliance(const Team& team) {
    // Budget must not go negative (maintains minimum reserve)
    return team.budget >= 0;
}

bool EconomyFairPlaySystem::checkForeignPlayerCompliance(const Team& team) {
    FairPlayRules rules = getRulesForDivision(team.division);
    
    // Simplified: count players by potential foreign indicator
    // In a full implementation, this would check player.nationality
    int foreignCount = 0;
    for (const auto& player : team.players) {
        // Estimate: players with lower potential/marketvalue in foreign clubs
        // For now, just use a simplified check based on other characteristics
        if (player.potential > 85 || player.value > 5000000LL) {
            foreignCount++;  // Rough estimation
        }
    }
    
    // Cap to reasonable foreign player count
    return foreignCount <= rules.maxForeignPlayers;
}

bool EconomyFairPlaySystem::checkYouthQuotaCompliance(const Team& team) {
    FairPlayRules rules = getRulesForDivision(team.division);
    
    if (team.players.empty()) return true;
    
    int youthCount = 0;
    for (const auto& player : team.players) {
        if (player.age <= 23) {
            youthCount++;
        }
    }
    
    float youthPercentage = static_cast<float>(youthCount) / static_cast<float>(team.players.size());
    return youthPercentage >= rules.minYouthPercentage;
}

FairPlayRules EconomyFairPlaySystem::getRulesForDivision(const std::string& division) {
    if (division.find("Primera B") != std::string::npos) {
        return g_rules[1];
    } else if (division.find("Segunda") != std::string::npos) {
        return g_rules[2];
    }
    return g_rules[0];  // Default to Primera
}

std::vector<FairPlayViolation> EconomyFairPlaySystem::getTeamViolations(const Team& team, Career& career) {
    std::vector<FairPlayViolation> violations;
    
    // Check salary cap
    if (!checkSalaryCompliance(team, career)) {
        const long long allowedSalary = getMaxAllowedSalary(team, career);
        long long totalSalary = 0;
        for (const auto& player : team.players) {
            totalSalary += player.wage;
        }
        long long excess = totalSalary - allowedSalary;
        
        FairPlayViolation v;
        v.type = "salary_cap";
        v.description = "Salary cap exceeded by $" + std::to_string(excess);
        v.severityLevel = (excess > allowedSalary / 2) ? 3 : 2;
        v.penaltyAmount = excess / 10;  // 10% of excess as fine
        v.isActive = true;
        violations.push_back(v);
    }
    
    // Check budget
    if (!checkBudgetCompliance(team)) {
        FairPlayViolation v;
        v.type = "budget";
        v.description = "Negative budget. Financial instability detected.";
        v.severityLevel = 3;
        v.penaltyAmount = 500000LL;
        v.isActive = true;
        violations.push_back(v);
    }
    
    // Check foreign players
    if (!checkForeignPlayerCompliance(team)) {
        FairPlayRules rules = getRulesForDivision(team.division);
        int foreignCount = 0;
        for (const auto& player : team.players) {
            if (player.potential > 85 || player.value > 5000000LL) {
                foreignCount++;
            }
        }
        
        FairPlayViolation v;
        v.type = "foreign_players";
        v.description = "Too many foreign players (" + std::to_string(foreignCount) + "/" + 
                       std::to_string(rules.maxForeignPlayers) + ").";
        v.severityLevel = 2;
        v.penaltyAmount = 250000LL;
        v.isActive = true;
        violations.push_back(v);
    }
    
    // Check youth quota
    if (!checkYouthQuotaCompliance(team)) {
        FairPlayRules rules = getRulesForDivision(team.division);
        int youthCount = 0;
        for (const auto& player : team.players) {
            if (player.age <= 23) youthCount++;
        }
        float youthPercentage = static_cast<float>(youthCount) / static_cast<float>(team.players.size());
        
        FairPlayViolation v;
        v.type = "youth_quota";
        v.description = "Insufficient youth players (" + std::to_string(youthCount) + "/" + 
                       std::to_string(team.players.size()) + ", " + 
                       std::to_string(static_cast<int>(youthPercentage * 100)) + "%).";
        v.severityLevel = 1;
        v.penaltyAmount = 100000LL;
        v.isActive = true;
        violations.push_back(v);
    }
    
    return violations;
}

void EconomyFairPlaySystem::applyFairPlayPenalties(Team& team, const std::vector<FairPlayViolation>& violations) {
    for (const auto& violation : violations) {
        if (violation.isActive && violation.penaltyAmount > 0) {
            team.budget = std::max(0LL, team.budget - violation.penaltyAmount);
            team.clubPrestige = std::max(0, team.clubPrestige - violation.severityLevel * 2);
            team.morale = std::max(0, team.morale - violation.severityLevel);
        }
    }
}

long long EconomyFairPlaySystem::getMaxAllowedSalary(const Team& team, Career& career) {
    (void)career;
    FairPlayRules rules = getRulesForDivision(team.division);
    
    const WeeklyFinanceReport finance = finance_system::projectWeeklyReport(team);
    const long long weeklyRevenue = finance.sponsorIncome + finance.matchdayIncome +
                                    finance.merchandisingIncome + finance.bonusIncome;
    long long capByRevenue = static_cast<long long>(weeklyRevenue * rules.maxSalaryPercentage);
    long long capByRule = rules.maxSalaryCap;
    
    return std::min(capByRevenue, capByRule);
}

void EconomyFairPlaySystem::weeklyComplianceCheck(Team& team, Career& career) {
    if (!g_initialized) {
        initialize(career);
    }
    
    auto violations = getTeamViolations(team, career);
    applyFairPlayPenalties(team, violations);
}

}  // namespace economy_fairplay
