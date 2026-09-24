#pragma once

#include "engine/models.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

struct MatchCenterMetric {
    std::string label;
    std::string myValue;
    std::string oppValue;
};

struct MatchCenterView {
    bool available = false;
    std::string headline;
    std::string scoreboard;
    std::string tacticalSummary;
    std::string fatigueSummary;
    std::string postMatchImpact;
    std::string playerOfTheMatch;
    std::vector<MatchCenterMetric> metrics;
    std::vector<std::string> phaseLines;
    std::vector<std::string> eventLines;
    std::array<int, 9> myHeatMap{};
    std::array<int, 9> oppHeatMap{};
    std::vector<std::string> playerStatLines;
    std::vector<std::string> timelineLines;
    std::vector<std::string> recommendationLines;
};

namespace match_center_service {

void captureLastMatchCenter(Career& career,
                            const Team& home,
                            const Team& away,
                            const MatchResult& result,
                            bool cupMatch);

MatchCenterView buildLastMatchCenter(const Career& career,
                                     std::size_t maxPhases = 4,
                                     std::size_t maxEvents = 6);

std::string formatLastMatchCenter(const Career& career,
                                  std::size_t maxPhases = 4,
                                  std::size_t maxEvents = 6);

}  // namespace match_center_service
