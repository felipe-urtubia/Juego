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

}  // namespace

ServiceResult startCareerService(Career& career,
                                 const string& divisionId,
                                 const string& teamName,
                                 const string& managerName) {
    ServiceResult result;
    career.initializeLeague(true);
    if (career.divisions.empty()) {
        result.messages.push_back("No se encontraron divisiones disponibles.");
        return result;
    }
    career.setActiveDivision(divisionId);
    if (career.getActiveTeamCount() == 0) {
        result.messages.push_back("La division seleccionada no tiene equipos.");
        return result;
    }
    Team* selectedTeam = career.getActiveTeamAt(0);
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        Team* team = career.getActiveTeamAt(i);
        if (team && team->name == teamName) {
            selectedTeam = team;
            break;
        }
    }
    career.myTeam = selectedTeam;
    career.managerName = managerName.empty() ? "Manager" : managerName;
    career.managerReputation = 50;
    career.clearHumanManagers();
    career.addHumanManager(career.managerName, career.myTeam ? career.myTeam->name : string(), career.managerReputation, true);
    career.newsFeed.clear();
    career.managerInbox.clear();
    career.scoutInbox.clear();
    career.scoutingShortlist.clear();
    career.scoutingAssignments.clear();
    career.history.clear();
    career.activePromises.clear();
    career.historicalRecords.clear();
    career.pendingTransfers.clear();
    career.achievements.clear();
    career.currentSeason = 1;
    career.currentWeek = 1;
    career.resetSeason();
    
    // === Inicializar Nuevos Sistemas de Gameplay ===
    vector<string> playerNames;
    for (const auto& player : career.myTeam->players) {
        playerNames.push_back(player.name);
    }
    career.dressingRoomDynamics = initializeDressingRoom(playerNames);
    
    // Inicializar IA rival para todos los equipos
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        Team* team = career.getActiveTeamAt(i);
        if (team != career.myTeam && team) {
            career.rivalAIMap[team->name] = createRivalAI(*team);
        }
    }
    
    // Inicializar rivalidades
    vector<string> teamNames;
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        Team* team = career.getActiveTeamAt(i);
        if (team) teamNames.push_back(team->name);
    }
    initializeRivalries(teamNames, career.rivalryDynamics);
    
    // Deuda inicial
    career.debtStatus = calculateDebtStatus(
        career.myTeam->budget,
        0,  // Sin deuda inicial
        career.myTeam->budget / 10  // Ingresos aproximados semanales
    );
    syncInfrastructureFromTeam(career, *career.myTeam);
    // === Fin InicializaciÃ³n Sistemas ===
    
    world_state_service::seedSeasonPromises(career);
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
    return controller.simulateWeek(autoOfferDecision, autoRenewDecision, autoManagerJobDecision, idleCallback);
}

ServiceResult simulateCareerWeekService(Career& career, IdleCallback idleCallback) {
    ServiceResult result = toServiceResult(simulateSeasonStepService(career, idleCallback));
    appendPostWeekActionDigest(career, result);
    return result;
}

