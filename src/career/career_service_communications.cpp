#include "career/career_service.h"
#include "career/career_runtime.h"
#include "career/staff_service.h"
#include "career/career_support.h"
#include "engine/models.h"
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

void CareerService::generateWeeklyNarratives(int myTeamPointsDelta) {
    Career& career = career_;
    if (!career.myTeam) return;
    int rank = career.currentCompetitiveRank();
    int field = max(1, career.currentCompetitiveFieldSize());
    if (rank > 0 && rank <= max(2, field / 4)) {
        career.addNews("La prensa destaca a " + career.myTeam->name + " por su presencia en la zona alta.");
    } else if (rank >= max(2, field - field / 4)) {
        career.addNews("La prensa pone a " + career.myTeam->name + " en la pelea por evitar el fondo.");
    }

    if (myTeamPointsDelta == 3 && career.myTeam->morale >= 65) {
        career.addNews("El vestuario de " + career.myTeam->name + " atraviesa un momento de confianza.");
    } else if (myTeamPointsDelta == 0 && career.boardConfidence <= 30) {
        career.addNews("Crece la tension institucional alrededor de " + career.myTeam->name + ".");
    }

    int promiseAlerts = 0;
    int leaders = 0;
    for (const auto& player : career.myTeam->players) {
        if (player.promisedRole == "Titular" && player.startsThisSeason + 2 < max(2, career.currentWeek * 2 / 3)) {
            promiseAlerts++;
        }
        if (player.promisedRole == "Rotacion" && player.startsThisSeason + 1 < max(1, career.currentWeek / 3)) {
            promiseAlerts++;
        }
        if ((player.leadership >= 72 || playerHasTrait(player, "Lider")) && player.happiness >= 55) {
            leaders++;
        }
    }
    if (promiseAlerts > 0) {
        career.addNews("Se acumulan " + to_string(promiseAlerts) + " promesa(s) de rol bajo presion en el plantel.");
    }
    if (career.myTeam->fanBase >= 65 && myTeamPointsDelta == 3) {
        career.addNews("La aficion responde con entusiasmo y empuja la recaudacion del club.");
    } else if (career.myTeam->fanBase >= 45 && myTeamPointsDelta == 0) {
        career.addNews("La prensa cuestiona la falta de resultados recientes de " + career.myTeam->name + ".");
    }
    if (leaders >= 3 && career.myTeam->morale >= 60) {
        career.addNews("Los lideres del vestuario sostienen un ambiente competitivo en " + career.myTeam->name + ".");
    }
    if (career.myTeam->youthIdentity == "Cantera estructurada") {
        int youthMinutes = 0;
        for (const auto& player : career.myTeam->players) {
            if (player.age <= 20 && player.matchesPlayed > 0) youthMinutes++;
        }
        if (youthMinutes >= 2) {
            career.addNews("La identidad de cantera de " + career.myTeam->name + " gana peso esta semana.");
        }
    }

    const Team* opponent = nextOpponent(career);
    if (opponent) {
        career.addNews("Informe previo: " + buildOpponentReport(career) + ".");
        if (areRivalClubs(*career.myTeam, *opponent)) {
            career.addNews("La semana queda marcada por un clasico ante " + opponent->name + ".");
        }
    }
    if (teamPrestigeScore(*career.myTeam) >= 68 && myTeamPointsDelta == 0) {
        career.addNews("La exigencia institucional aprieta: el entorno de " + career.myTeam->name + " esperaba mas.");
    }

    for (const auto& player : career.myTeam->players) {
        if (player.contractWeeks > 0 && player.contractWeeks <= 4) {
            career.addNews("Contrato al limite: " + player.name + " entra en sus ultimas " +
                           to_string(player.contractWeeks) + " semana(s).");
            break;
        }
    }
    addWeeklySquadNewsAlerts();
}
