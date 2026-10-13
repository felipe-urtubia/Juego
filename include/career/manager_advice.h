#pragma once

#include "engine/models.h"
#include "transfers/transfer_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace manager_advice {

std::vector<std::string> buildManagerActionLines(const Career& career, std::size_t limit = 5);
std::vector<std::string> buildCareerStorylines(const Career& career, std::size_t limit = 5);
std::string buildSuggestedBoardObjective(const Career& career);
std::string buildSuggestedBoardObjectiveReason(const Career& career);
bool adoptSuggestedBoardObjective(Career& career);

std::string buildTransferCompetitionLabel(const Career& career,
                                          const Team& seller,
                                          const Player& player,
                                          const TransferTarget& target);
std::string buildTransferActionLabel(const Career& career,
                                     const Team& seller,
                                     const Player& player,
                                     const TransferTarget& target);
std::string buildTransferPackageLabel(const TransferTarget& target);

}  // namespace manager_advice
