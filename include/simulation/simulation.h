#pragma once

#include "engine/models.h"
#include "simulation/match_engine.h"

#include <string>
#include <vector>

TeamStrength computeStrength(Team& team);
bool hasInjuredInXI(const Team& team, const std::vector<int>& xi);
void applyTactics(const Team& team, int& attack, int& defense);
double calcLambda(int attack, int defense);
int samplePoisson(double lambda);
bool simulateInjury(Player& player, const std::string& tactics, bool verbose, std::vector<std::string>* events);
void healInjuries(Team& team, bool verbose);
void recoverFitness(Team& team, int days);
void assignGoalsAndAssists(Team& team, int goals, const std::vector<int>& xi, const std::string& teamName, std::vector<std::string>* events);
int teamPenaltyStrength(const Team& team);
MatchResult simulateMatch(Team& home, Team& away, bool keyMatch = false, bool neutralVenue = false);
MatchResult simulateMatch(Career* career, Team& home, Team& away, bool keyMatch = false, bool neutralVenue = false);
MatchResult simulateInteractiveMatch(
    Career* career,
    Team& home,
    Team& away,
    bool userControlsHome,
    const match_engine::ManagerDecisionCallback& decisionCallback,
    bool keyMatch = false,
    bool neutralVenue = false);
MatchResult playMatch(Team& home, Team& away, bool verbose, bool keyMatch = false, bool neutralVenue = false);
MatchResult playMatch(Career* career, Team& home, Team& away, bool verbose, bool keyMatch = false, bool neutralVenue = false);
