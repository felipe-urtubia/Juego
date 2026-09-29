#include "career/career_service.h"
#include "career/career_runtime.h"
#include "career/staff_service.h"
#include "career/career_support.h"
#include "transfers/negotiation_system.h"
#include "utils.h"

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

namespace {
const Player* leadingForward(const Team& team) {
    const Player* best = nullptr;
    for (const auto& player : team.players) {
        if (normalizePosition(player.position) != "DEL") continue;
        if (!best || player.skill > best->skill) best = &player;
    }
    return best;
}

} // namespace

void CareerService::addWeeklySquadNewsAlerts() {
    Career& career = career_;
    if (!career.myTeam) return;
    int defenseFitness = averageFitnessForLine(*career.myTeam, "DEF");
    int midfieldFitness = averageFitnessForLine(*career.myTeam, "MED");
    int attackFitness = averageFitnessForLine(*career.myTeam, "DEL");
    if (defenseFitness < 58 || midfieldFitness < 58 || attackFitness < 58) {
        string line = (defenseFitness <= midfieldFitness && defenseFitness <= attackFitness)
                          ? "la linea defensiva"
                          : (midfieldFitness <= attackFitness ? "el mediocampo" : "el frente de ataque");
        career.addNews("Alerta fisica: " + line + " llega exigida a la proxima fecha.");
    }
    const Player* forward = leadingForward(*career.myTeam);
    if (forward && forward->matchesPlayed >= 5 && forward->goals == 0) {
        career.addNews("Alerta ofensiva: " + forward->name + " ya suma " +
                       to_string(forward->matchesPlayed) + " partido(s) sin marcar.");
    }
    int promiseWarnings = 0;
    for (const auto& player : career.myTeam->players) {
        if (promiseAtRisk(player, career.currentWeek)) promiseWarnings++;
    }
    if (promiseWarnings >= 2) {
        career.addNews("Alerta de vestuario: hay " + to_string(promiseWarnings) +
                       " promesa(s) contractuales bajo revision.");
    }
}
