#include "career/career_service.h"
#include "career/career_reports.h"
#include "career/career_runtime.h"
#include "career/staff_service.h"
#include "career/career_support.h"
#include "career/dressing_room_service.h"
#include "engine/models.h"
#include "transfers/negotiation_system.h"
#include "utils.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace std;
void CareerService::generateDevelopmentReports() {
    if (!career_.myTeam) return;

    const CareerReport report = buildClubReport(career_);
    for (const auto& block : report.blocks) {
        if (block.title.find("Entrenamiento") == std::string::npos &&
            block.title.find("Cantera") == std::string::npos &&
            block.title.find("Desarrollo") == std::string::npos) {
            continue;
        }
        for (std::size_t i = 0; i < block.lines.size() && i < 2; ++i) {
            career_.addInboxItem(block.title + " | " + block.lines[i], "Desarrollo");
        }
    }
}

void CareerService::generateWeeklyNarrative() {
    const CareerReport report = buildWeeklyDashboardReport(career_);
    int added = 0;
    for (const auto& block : report.blocks) {
        for (const auto& line : block.lines) {
            if (line.empty()) continue;
            career_.addNews(block.title + ": " + line);
            if (++added >= 3) return;
        }
    }
}

void CareerService::dispatchStaffBriefing() {
    for (const auto& line : staff_service::buildWeeklyStaffBriefingLines(career_, 4)) {
        career_.addInboxItem(line, "Staff");
    }
}

void CareerService::addSquadAlerts() {
    if (!career_.myTeam) return;

    int injuredPlayers = 0;
    int lowFitnessPlayers = 0;
    int expiringContracts = 0;
    for (const auto& player : career_.myTeam->players) {
        if (player.injured || player.injuryWeeks > 0) injuredPlayers++;
        if (player.fitness < 55) lowFitnessPlayers++;
        if (player.contractWeeks > 0 && player.contractWeeks <= 8) expiringContracts++;
    }

    if (injuredPlayers > 0) {
        career_.addInboxItem("Plantel | " + std::to_string(injuredPlayers) +
                                 " jugador(es) lesionado(s) requieren seguimiento.",
                             "Medical");
    }
    if (lowFitnessPlayers >= 3) {
        career_.addInboxItem("Plantel | " + std::to_string(lowFitnessPlayers) +
                                 " jugador(es) llegan con condicion baja.",
                             "Staff");
    }
    if (expiringContracts > 0) {
        career_.addInboxItem("Contratos | " + std::to_string(expiringContracts) +
                                 " renovacion(es) entran en zona urgente.",
                             "Directiva");
    }
}

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

void CareerService::updateWeeklyManagerReputation() {
    Career& career = career_;
    if (!career.myTeam) return;
    int rank = career.currentCompetitiveRank();
    if (rank > 0) {
        if (rank <= max(1, career.boardExpectedFinish - 1)) {
            career.managerReputation = clampInt(career.managerReputation + 2, 1, 100);
        } else if (rank > career.boardExpectedFinish + 2) {
            career.managerReputation = clampInt(career.managerReputation - 1, 1, 100);
        }
    }
    if ((career.myTeam->tactics == "Pressing" || career.myTeam->matchInstruction == "Juego directo") &&
        career.myTeam->goalsFor >= max(4, career.currentWeek * 2)) {
        career.managerReputation = clampInt(career.managerReputation + 1, 1, 100);
    }
    int promiseWarnings = 0;
    int youthContributors = 0;
    for (const auto& player : career.myTeam->players) {
        if (promiseAtRisk(player, career.currentWeek)) promiseWarnings++;
        if (player.age <= 21 && player.matchesPlayed >= 4) youthContributors++;
    }
    if (youthContributors >= 2) {
        career.managerReputation = clampInt(career.managerReputation + 1, 1, 100);
    }
    DressingRoomSnapshot dressing = dressing_room_service::buildSnapshot(*career.myTeam, career.currentWeek);
    if (dressing.socialTension <= 2 && career.myTeam->morale >= 68) {
        career.managerReputation = clampInt(career.managerReputation + 1, 1, 100);
    } else if (dressing.socialTension >= 5) {
        career.managerReputation = clampInt(career.managerReputation - 1, 1, 100);
    }
    if (career.boardConfidence <= 25 && promiseWarnings >= 2) {
        career.managerReputation = clampInt(career.managerReputation - 1, 1, 100);
    }
}

void CareerService::handleWeeklyManagerStatus() {
    Career& career = career_;
    if (!career.myTeam) return;
    if (career.boardConfidence >= 20 && career.boardWarningWeeks < 6) return;
    emitUiMessage("");
    emitUiMessage("[Directiva] " + career.myTeam->name + " decide despedirte.");
    career.addNews(career.managerName + " fue despedido de " + career.myTeam->name + ".");
    career.managerReputation = clampInt(career.managerReputation - 8, 10, 100);

    vector<Team*> jobs = ::buildJobMarket(career, true);
    if (jobs.empty()) {
        for (auto& team : career.allTeams) {
            if (&team != career.myTeam) jobs.push_back(&team);
        }
    }
    if (jobs.empty()) return;

    emitUiMessage("Debes elegir nuevo club:");
    for (size_t i = 0; i < jobs.size(); ++i) {
        emitUiMessage(to_string(i + 1) + ". " + jobs[i]->name + " (" + divisionDisplay(jobs[i]->division) + ")");
    }

    int choice = 1;
    if (managerJobSelectionCallback()) {
        int selected = managerJobSelectionCallback()(career, jobs);
        if (selected >= 0 && selected < static_cast<int>(jobs.size())) {
            choice = selected + 1;
        }
    }
    takeManagerJob(career, jobs[static_cast<size_t>(choice - 1)], "Llega tras un despido reciente.");
}

void CareerService::generateWeeklyManagerCareerEvents() {
    Career& career = career_;
    if (!career.myTeam) return;
    int rank = career.currentCompetitiveRank();
    int field = max(1, career.currentCompetitiveFieldSize());
    if (rank > 0 && rank <= max(2, field / 4) && randInt(1, 100) <= 18) {
        career.addNews("Entrevista: la prensa describe a " + career.managerName + " como un DT " +
                       managerStyleLabel(*career.myTeam) + ".");
    }
    int youthContributors = 0;
    for (const auto& player : career.myTeam->players) {
        if (player.age <= 21 && player.matchesPlayed >= 4) youthContributors++;
    }
    if (youthContributors >= 2 && randInt(1, 100) <= 16) {
        career.addNews("Perfil de manager: la prensa valora la apuesta juvenil de " + career.managerName + ".");
    }
    if (career.managerReputation >= 58 && rank > 0 && rank <= career.boardExpectedFinish &&
        randInt(1, 100) <= 12) {
        vector<Team*> jobs = ::buildJobMarket(career, false);
        if (!jobs.empty()) {
            career.addNews("Rumor de banquillo: " + jobs.front()->name + " sigue a " + career.managerName + ".");
        }
    }
}
