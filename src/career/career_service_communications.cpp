#include "career/career_service.h"
#include "career/career_runtime.h"
#include "career/staff_service.h"

#include <string>
#include <vector>

using namespace std;
void CareerService::dispatchWeeklyStaffBriefing() {
    Career& career = career_;
    if (!career.myTeam) return;
    const auto recommendations = staff_service::buildStaffRecommendations(career, 4);
    if (recommendations.empty()) return;

    for (size_t i = 0; i < recommendations.size() && i < 2; ++i) {
        const auto& recommendation = recommendations[i];
        career.addInboxItem(recommendation.staffRole + " | " + recommendation.severity + " | " +
                                recommendation.summary + " | Accion: " + recommendation.suggestedAction,
                            "Staff");
    }

    const auto& headline = recommendations.front();
    if (headline.urgency >= 48) {
        career.addNews("Mesa del staff: " + headline.staffRole + " avisa que " + headline.summary +
                       " Accion sugerida: " + headline.suggestedAction);
    }
    emitUiMessage("[Staff] " + headline.staffRole + " | " + headline.summary);
}
