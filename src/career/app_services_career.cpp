#include "app_services.h"

#include "ai/ai_transfer_manager.h"
#include "career/season_flow_controller.h"
#include "career/career_reports.h"
#include "career/career_runtime.h"
#include "career/career_support.h"
#include "career/match_center_service.h"
#include "career/weekly_focus_service.h"
#include "career/world_state_service.h"
#include "career/week_simulation.h"
#include "career/team_management.h"
#include "career/dressing_room_service.h"
#include "career/inbox_service.h"
#include "career/medical_service.h"
#include "career/staff_service.h"
#include "competition.h"
#include "development/training_impact_system.h"
#include "simulation/player_condition.h"
#include "transfers/negotiation_system.h"
#include "transfers/transfer_market.h"
#include "engine/social_system.h"
#include "engine/rival_ai.h"
#include "engine/rivalry_system.h"
#include "engine/debt_system.h"
#include "engine/facilities_system.h"
#include "utils.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace {

IncomingOfferDecision autoOfferDecision(const Career& career,
                                        const Player& player,
                                        const Team&,
                                        long long offer,
                                        long long maxOffer) {
    IncomingOfferDecision decision;
    size_t squadSize = career.myTeam ? career.myTeam->players.size() : 0;
    if (player.wantsToLeave && offer >= player.value) {
        decision.action = 1;
        return decision;
    }
    if (squadSize > 20 && offer >= max(player.value * 12 / 10, maxOffer * 90 / 100)) {
        decision.action = 1;
        return decision;
    }
    decision.action = (offer >= maxOffer) ? 1 : 3;
    return decision;
}

bool autoRenewDecision(const Career&,
                       const Team& team,
                       const Player& player,
                       long long demandedWage,
                       int,
                       long long) {
    if (team.budget < demandedWage * 6) return false;
    if (player.wantsToLeave && player.happiness < 45) return false;
    return player.skill >= team.getAverageSkill() - 5 || team.players.size() <= 18;
}

int autoManagerJobDecision(const Career&, const vector<Team*>& jobs) {
    return jobs.empty() ? -1 : 0;
}

ServiceResult toServiceResult(const SeasonStepResult& step) {
    ServiceResult result;
    result.ok = step.ok;
    result.messages = step.week.messages;
    if (result.messages.empty()) result.messages.push_back("Semana simulada.");
    return result;
}

void syncInfrastructureFromTeam(Career& career, const Team& team) {
    career.infrastructure.levels.trainingGround = clampInt(team.trainingFacilityLevel, 1, 5);
    career.infrastructure.levels.youthAcademy = clampInt(team.youthFacilityLevel, 1, 5);
    career.infrastructure.levels.medical = clampInt(1 + team.medicalTeam / 25, 1, 5);
    career.infrastructure.levels.stadium = clampInt(team.stadiumLevel, 1, 5);
    career.infrastructure.levels.facilities = clampInt(1 + team.assistantCoach / 30, 1, 5);
}

void syncTeamFromInfrastructure(const Career& career, Team& team) {
    team.trainingFacilityLevel = max(team.trainingFacilityLevel, career.infrastructure.levels.trainingGround);
    team.youthFacilityLevel = max(team.youthFacilityLevel, career.infrastructure.levels.youthAcademy);
    team.stadiumLevel = max(team.stadiumLevel, career.infrastructure.levels.stadium);
    team.medicalTeam = max(team.medicalTeam, 45 + career.infrastructure.levels.medical * 8);
    team.assistantCoach = max(team.assistantCoach, 42 + career.infrastructure.levels.facilities * 7);
}

string recommendedWeeklyDecisionSummary(const Career& career) {
    const vector<string> options = buildWeeklyDecisionOptions(career);
    if (options.empty()) return "revisar el centro semanal antes de avanzar";

    string recommendation = options.front();
    const string marker = "El staff recomienda:";
    const size_t markerPos = recommendation.find(marker);
    if (markerPos != string::npos) {
        recommendation = trim(recommendation.substr(markerPos + marker.size()));
    }
    if (recommendation.empty()) recommendation = options.front();
    return recommendation;
}

void appendPostWeekActionDigest(Career& career, ServiceResult& result) {
    if (!result.ok || !career.myTeam) return;

    const weekly_focus_service::WeeklyFocusSnapshot focus =
        weekly_focus_service::buildWeeklyFocusSnapshot(career, 2, 2, 1);
    const MatchCenterView matchCenter = match_center_service::buildLastMatchCenter(career, 1, 2);
    const string decision = recommendedWeeklyDecisionSummary(career);

    vector<string> digest;
    digest.push_back("Post-semana: " + focus.headline);
    if (!focus.priorityLines.empty()) {
        digest.push_back("Prioridad 1: " + focus.priorityLines.front());
    }
    if (matchCenter.available && !matchCenter.recommendationLines.empty()) {
        digest.push_back("Ajuste del partido: " + matchCenter.recommendationLines.front());
    }
    digest.push_back("Decision sugerida: " + decision + ".");
    digest.push_back("Siguiente accion: abre Noticias y usa Instruccion para aplicar la decision semanal.");

    result.messages.insert(result.messages.end(), digest.begin(), digest.end());

    string inboxLine = focus.headline;
    if (!focus.priorityLines.empty()) inboxLine += " | " + focus.priorityLines.front();
    inboxLine += " | Decision: " + decision;
    career.addInboxItem(inboxLine, "Centro semanal");
    career.addNews("Centro semanal post-simulacion: " + decision + ".");
}

string careerSaveToken(const string& value) {
    string token;
    bool separatorPending = false;

    for (unsigned char ch : value) {
        char normalized = '\0';
        if (ch >= 'A' && ch <= 'Z') {
            normalized = static_cast<char>('a' + (ch - 'A'));
        } else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            normalized = static_cast<char>(ch);
        }

        if (normalized != '\0') {
            if (separatorPending && !token.empty() && token.back() != '_') token.push_back('_');
            token.push_back(normalized);
            separatorPending = false;
        } else if (!token.empty()) {
            separatorPending = true;
        }
    }

    while (!token.empty() && token.back() == '_') token.pop_back();
    if (token.empty()) token = "partida";

    constexpr size_t kMaxTokenLength = 28;
    if (token.size() > kMaxTokenLength) token.resize(kMaxTokenLength);
    while (!token.empty() && token.back() == '_') token.pop_back();
    if (token.empty()) token = "partida";
    return token;
}

string buildUniqueCareerSavePath(const string& managerName, const string& teamName) {
    const string baseName =
        "career_" + careerSaveToken(managerName) + "_" + careerSaveToken(teamName);

    auto isAvailable = [](const string& path) {
        return !pathExists(path) && !pathExists(path + ".bak");
    };

    string candidate = joinPath("saves", baseName + ".txt");
    if (isAvailable(candidate)) return candidate;

    for (int suffix = 2;; ++suffix) {
        candidate = joinPath("saves", baseName + "_" + to_string(suffix) + ".txt");
        if (isAvailable(candidate)) return candidate;
    }
}

}  // namespace

ServiceResult startCareerService(Career& career,
                                 const string& divisionId,
                                 const string& teamName,
                                 const string& managerName) {
    ServiceResult result;

    // Construir la nueva carrera fuera del estado activo. De esta forma,
    // cualquier error de validacion deja intacta la carrera que ya existe.
    Career candidate;
    candidate.initializeLeague(true);

    if (candidate.divisions.empty()) {
        result.messages.push_back("No se encontraron divisiones disponibles.");
        return result;
    }

    candidate.setActiveDivision(divisionId);
    if (candidate.getActiveTeamCount() == 0) {
        result.messages.push_back("La division seleccionada no tiene equipos.");
        return result;
    }

    Team* selectedTeam = nullptr;
    for (int i = 0; i < candidate.getActiveTeamCount(); ++i) {
        Team* team = candidate.getActiveTeamAt(i);
        if (team && team->name == teamName) {
            selectedTeam = team;
            break;
        }
    }

    if (!selectedTeam) {
        result.messages.push_back("No se encontro el club seleccionado en la division indicada.");
        return result;
    }

    candidate.myTeam = selectedTeam;
    candidate.managerName = managerName.empty() ? "Manager" : managerName;
    candidate.saveFile = buildUniqueCareerSavePath(candidate.managerName, candidate.myTeam->name);
    candidate.managerReputation = 50;
    candidate.clearHumanManagers();
    candidate.addHumanManager(candidate.managerName,
                              candidate.myTeam->name,
                              candidate.managerReputation,
                              true);
    candidate.newsFeed.clear();
    candidate.managerInbox.clear();
    candidate.scoutInbox.clear();
    candidate.scoutingShortlist.clear();
    candidate.scoutingAssignments.clear();
    candidate.history.clear();
    candidate.activePromises.clear();
    candidate.historicalRecords.clear();
    candidate.pendingTransfers.clear();
    candidate.achievements.clear();
    candidate.currentSeason = 1;
    candidate.currentWeek = 1;
    candidate.resetSeason();

    // === Inicializar Nuevos Sistemas de Gameplay ===
    vector<string> playerNames;
    for (const auto& player : candidate.myTeam->players) {
        playerNames.push_back(player.name);
    }
    candidate.dressingRoomDynamics = initializeDressingRoom(playerNames);

    // Inicializar IA rival para todos los equipos
    for (int i = 0; i < candidate.getActiveTeamCount(); ++i) {
        Team* team = candidate.getActiveTeamAt(i);
        if (team != candidate.myTeam && team) {
            candidate.rivalAIMap[team->name] = createRivalAI(*team);
        }
    }

    // Inicializar rivalidades
    vector<string> teamNames;
    for (int i = 0; i < candidate.getActiveTeamCount(); ++i) {
        Team* team = candidate.getActiveTeamAt(i);
        if (team) teamNames.push_back(team->name);
    }
    initializeRivalries(teamNames, candidate.rivalryDynamics);

    // Deuda inicial
    candidate.debtStatus = calculateDebtStatus(
        candidate.myTeam->budget,
        0,
        candidate.myTeam->budget / 10
    );
    syncInfrastructureFromTeam(candidate, *candidate.myTeam);
    // === Fin Inicializacion Sistemas ===

    world_state_service::seedSeasonPromises(candidate);

    const string committedDivision = candidate.activeDivision;
    const string committedTeamName = candidate.myTeam->name;

    // Commit atomico del nuevo estado. Career contiene punteros hacia allTeams,
    // por lo que despues de copiar reconstruimos todos los enlaces derivados.
    career = candidate;
    career.refreshActiveDivisionTeamLinks(committedDivision);
    career.setMyTeamByName(committedTeamName);

    if (!career.myTeam) {
        result.messages.push_back("No se pudo reconstruir el club de la nueva carrera.");
        return result;
    }

    result.ok = true;
    result.messages.push_back("Nueva carrera iniciada con " + career.myTeam->name + ".");
    return result;
}
ServiceResult loadCareerService(Career& career) {
    ServiceResult result;
    career.initializeLeague(true);
    result.ok = career.loadCareer();
    if (result.ok && career.activePromises.empty()) {
        world_state_service::seedSeasonPromises(career);
    }
    if (result.ok && career.myTeam) {
        syncTeamFromInfrastructure(career, *career.myTeam);
        career.debtStatus = calculateDebtStatus(
            career.myTeam->budget,
            career.myTeam->debt,
            max(1LL, career.myTeam->sponsorWeekly + static_cast<long long>(career.myTeam->fanBase) * 2500LL));
        applyFinancialSanctions(career.debtStatus);
    }
    result.messages.push_back(result.ok
                                  ? "Carrera cargada: " + (career.myTeam ? career.myTeam->name : string("Sin club")) + "."
                                  : "No se encontro una carrera guardada.");
    return result;
}

ServiceResult saveCareerService(Career& career) {
    ServiceResult result;
    result.ok = career.saveCareer();
    result.messages.push_back(result.ok
                                  ? "Carrera guardada en " + career.saveFile + "."
                                  : "No se pudo guardar la carrera en " + career.saveFile + ".");
    return result;
}

SeasonStepResult simulateSeasonStepService(Career& career, IdleCallback idleCallback) {
    SeasonFlowController controller(career);
    IncomingOfferDecisionCallback offerDecision = incomingOfferDecisionCallback();
    if (!offerDecision) {
        offerDecision = autoOfferDecision;
    }
    return controller.simulateWeek(offerDecision, autoRenewDecision, autoManagerJobDecision, idleCallback);
}

ServiceResult simulateCareerWeekService(Career& career, IdleCallback idleCallback) {
    ServiceResult result = toServiceResult(simulateSeasonStepService(career, idleCallback));
    appendPostWeekActionDigest(career, result);
    return result;
}

