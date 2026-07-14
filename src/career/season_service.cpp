#include "career/season_service.h"

#include "career/career_runtime.h"
#include "career/week_simulation.h"

#include <iostream>
#include <sstream>
#include <streambuf>
#include <utility>
#include <vector>

using namespace std;

namespace {

thread_local vector<string>* g_seasonMessages = nullptr;
thread_local UiMessageCallback g_forwardSeasonMessage = nullptr;

void collectSeasonMessage(const string& message) {
    if (g_seasonMessages && !message.empty()) {
        g_seasonMessages->push_back(message);
    }

    if (g_forwardSeasonMessage && g_forwardSeasonMessage != collectSeasonMessage) {
        g_forwardSeasonMessage(message);
    }
}

vector<string> splitOutputLines(const string& text) {
    vector<string> lines;
    istringstream stream(text);
    string line;

    while (getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    return lines;
}

class StdoutCapture {
public:
    StdoutCapture()
        : original_(cout.rdbuf(buffer_.rdbuf())) {
    }

    ~StdoutCapture() {
        cout.rdbuf(original_);
    }

    string str() const {
        return buffer_.str();
    }

private:
    ostringstream buffer_;
    streambuf* original_;
};

} // namespace


SeasonStepResult SeasonService::simulateWeek(Career& career,
                                             IncomingOfferDecisionCallback offerDecision,
                                             ContractRenewalDecisionCallback renewDecision,
                                             ManagerJobSelectionCallback managerDecision,
                                             IdleCallback idleCallback) const {
    SeasonStepResult result;

    WeekSimulationResult week;
    week.weekBefore = career.currentWeek;
    week.seasonBefore = career.currentSeason;
    week.boardConfidenceBefore = career.boardConfidence;

    vector<string> messages;

    g_seasonMessages = &messages;

    StdoutCapture stdoutCapture;

    CareerRuntimeContext runtimeContext = currentCareerRuntimeContext();

    g_forwardSeasonMessage = runtimeContext.uiMessage;

    runtimeContext.uiMessage = collectSeasonMessage;
    runtimeContext.incomingOfferDecision = offerDecision;
    runtimeContext.contractRenewalDecision = renewDecision;
    runtimeContext.managerJobSelection = managerDecision;
    runtimeContext.idle = idleCallback;

    {
        ScopedCareerRuntimeContext callbackScope(runtimeContext);
        simulateCareerWeek(career);
    }

    g_seasonMessages = nullptr;
    g_forwardSeasonMessage = nullptr;

    week.ok = true;
    week.weekAfter = career.currentWeek;
    week.seasonAfter = career.currentSeason;
    week.boardConfidenceAfter = career.boardConfidence;

    week.seasonTransitionTriggered =
        week.seasonAfter != week.seasonBefore ||
        (week.weekBefore > 0 && week.weekAfter < week.weekBefore);

    week.lastMatchAnalysis = career.lastMatchAnalysis;

    vector<string> legacyLines = splitOutputLines(stdoutCapture.str());

    messages.insert(
        messages.end(),
        legacyLines.begin(),
        legacyLines.end()
    );

    week.messages = std::move(messages);

    if (week.messages.empty()) {
        week.messages.push_back("Semana simulada.");
    }

    result.ok = true;
    result.week = std::move(week);

    return result;
}