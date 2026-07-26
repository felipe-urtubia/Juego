#pragma once

#include "engine/models.h"

#include <string>
#include <vector>

namespace match_internal {

std::string compactToken(std::string value);

void applyRoleModifier(const Player& p, int& attack, int& defense);
void applyIndividualInstructionModifier(const Player& p, const Team& team, int& attack, int& defense);
void applyTraitModifier(const Player& p, int& attack, int& defense);
void applyPlayerStateModifier(const Player& p, const Team& team, int& attack, int& defense);
void applyMatchInstruction(const Team& team, int& attack, int& defense);
void applyFormationBias(const Team& team, const std::vector<int>& xi, int& attack, int& defense);
void applyStyleMatchup(const Team& home,
                       const Team& away,
                       int& homeAttack,
                       int& homeDefense,
                       int& awayAttack,
                       int& awayDefense);
int pressingRecoveryBonus(const Team& team, const std::vector<int>& xi);
int directPlayBonus(const Team& attacking, const Team& defending, const std::vector<int>& xi);
int crowdSupportBonus(const Team& home, const Team& away, bool neutralVenue);
int clutchModifier(const Team& team, const std::vector<int>& xi, bool keyMatch);
int bestBenchReplacement(const Team& team,
                         const std::vector<int>& activeXI,
                         const std::string& targetPos);

int bestBenchReplacement(const Team& team,
                         const std::vector<int>& activeXI,
                         const std::string& targetPos,
                         int minute,
                         int goalsFor,
                         int goalsAgainst,
                         int opponentAvailablePlayers);

void pushTacticalEvent(std::vector<std::string>* events, int minute, const std::string& text);
void applyMatchFatigue(Team& team, const std::vector<int>& xi, const std::string& tactics);
void assignGoalsAndAssists(Team& team,
                           int goals,
                           const std::vector<int>& xi,
                           const std::string& teamName,
                           std::vector<std::string>* events);
void applyIntensityInjuryRisk(Team& team, const std::vector<int>& participants, std::vector<std::string>* events);

}  // namespace match_internal
