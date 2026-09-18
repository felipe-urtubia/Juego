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

string nextDevelopmentPlan(const string& current) {
    static const vector<string> plans = {"Equilibrado", "Fisico", "Defensa", "Creatividad", "Finalizacion", "Liderazgo"};
    for (size_t i = 0; i < plans.size(); ++i) {
        if (plans[i] == current) return plans[(i + 1) % plans.size()];
    }
    return plans.front();
}

string nextInstructionForPlayer(const Player& player) {
    static const vector<string> goalkeepers = {"Libre", "Conservar posicion", "Abrir campo", "Descanso medico"};
    static const vector<string> defenders = {"Libre", "Cerrar por dentro", "Marcar fuerte", "Abrir campo", "Conservar posicion", "Descanso medico"};
    static const vector<string> midfielders = {"Libre", "Arriesgar pase", "Abrir campo", "Conservar posicion", "Marcar fuerte", "Descanso medico"};
    static const vector<string> forwards = {"Libre", "Atacar espalda", "Abrir campo", "Conservar posicion", "Marcar fuerte", "Descanso medico"};
    const vector<string>* options = &midfielders;
    const string pos = normalizePosition(player.position);
    if (pos == "ARQ") options = &goalkeepers;
    else if (pos == "DEF") options = &defenders;
    else if (pos == "DEL") options = &forwards;
    for (size_t i = 0; i < options->size(); ++i) {
        if ((*options)[i] == player.individualInstruction) return (*options)[(i + 1) % options->size()];
    }
    return options->front();
}

void consumeMatchingInboxEntry(vector<string>& inbox, const string& text) {
    for (auto it = inbox.rbegin(); it != inbox.rend(); ++it) {
        if (*it != text) continue;
        inbox.erase(next(it).base());
        return;
    }
}

string latestWeeklyDigestInboxEntry(const Career& career) {
    for (auto it = career.managerInbox.rbegin(); it != career.managerInbox.rend(); ++it) {
        if (it->find("[Centro semanal]") != string::npos) return *it;
    }
    return {};
}

ClubUpgrade staffUpgradeForRole(const string& role) {
    const string lower = toLower(trim(role));
    if (lower.find("asistente") != string::npos) return ClubUpgrade::AssistantCoach;
    if (lower.find("preparador") != string::npos) return ClubUpgrade::FitnessCoach;
    if (lower.find("scouting") != string::npos) return ClubUpgrade::Scouting;
    if (lower.find("juveniles") != string::npos) return ClubUpgrade::YouthCoach;
    if (lower.find("medico") != string::npos) return ClubUpgrade::Medical;
    if (lower.find("arquero") != string::npos) return ClubUpgrade::GoalkeepingCoach;
    return ClubUpgrade::PerformanceAnalyst;
}

string nextMatchInstruction(const string& current) {
    static const vector<string> instructions = {"Equilibrado", "Laterales altos", "Bloque bajo", "Balon parado",
                                                "Presion final", "Por bandas", "Juego directo",
                                                "Contra-presion", "Pausar juego"};
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i] == current) return instructions[(i + 1) % instructions.size()];
    }
    return instructions.front();
}

string nextTrainingFocus(const string& current) {
    static const vector<string> focuses = {"Balanceado", "Recuperacion", "Ataque", "Defensa",
                                           "Resistencia", "Tecnico", "Tactico", "Preparacion partido"};
    for (size_t i = 0; i < focuses.size(); ++i) {
        if (focuses[i] == current) return focuses[(i + 1) % focuses.size()];
    }
    return focuses.front();
}

string weeklyDecisionLabel(WeeklyDecision decision) {
    switch (decision) {
        case WeeklyDecision::Auto: return "Decision automatica del staff";
        case WeeklyDecision::Recovery: return "Recuperar plantel";
        case WeeklyDecision::HighIntensityTraining: return "Entrenar fuerte";
        case WeeklyDecision::DressingRoom: return "Ordenar vestuario";
        case WeeklyDecision::MatchPreparation: return "Preparar rival";
        case WeeklyDecision::FinancialControl: return "Control financiero";
        case WeeklyDecision::YouthPathway: return "Impulsar juveniles";
        case WeeklyDecision::ManagerRest: return "Descanso del manager";
    }
    return "Decision semanal";
}

int countFatiguedPlayers(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.fitness < 62 || player.fatigueLoad >= 58 || player.injured) ++total;
    }
    return total;
}

int countLowMoralePlayers(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.happiness < 48 || player.wantsToLeave || player.unhappinessWeeks >= 2) ++total;
    }
    return total;
}

int countYouthCandidates(const Team& team) {
    int total = 0;
    for (const auto& player : team.players) {
        if (player.age <= 21 && player.potential >= player.skill + 6) ++total;
    }
    return total;
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

WeeklyDecision decisionFromLastMatchCenter(const Career& career, int fatiguedPlayers) {
    const MatchCenterSnapshot& match = career.lastMatchCenter;
    if (match.opponentName.empty()) return WeeklyDecision::Auto;

    const bool lost = match.myGoals < match.oppGoals;
    const bool avoidedLoss = match.myGoals >= match.oppGoals;
    const int xgGap = match.myExpectedGoalsTenths - match.oppExpectedGoalsTenths;
    const int shotGap = match.myShots - match.oppShots;
    const int shotOnTargetGap = match.myShotsOnTarget - match.oppShotsOnTarget;
    const bool attackStalled = match.myExpectedGoalsTenths <= 10 || match.myShots <= 8 ||
                               (match.myGoals == 0 && match.myExpectedGoalsTenths <= 12);
    const bool defenseExposed = match.oppExpectedGoalsTenths >= 16 || shotOnTargetGap <= -3 || shotGap <= -6;

    if (avoidedLoss && xgGap >= 4 && fatiguedPlayers >= 2) {
        return WeeklyDecision::Recovery;
    }
    if ((lost && attackStalled) || (lost && defenseExposed) || defenseExposed || attackStalled) {
        return WeeklyDecision::HighIntensityTraining;
    }
    return WeeklyDecision::Auto;
}

WeeklyDecision chooseAutomaticWeeklyDecision(const Career& career) {
    if (!career.myTeam) return WeeklyDecision::Auto;
    const Team& team = *career.myTeam;
    const int fatigued = countFatiguedPlayers(team);
    const int lowMorale = countLowMoralePlayers(team);
    const int youth = countYouthCandidates(team);
    const bool objectiveYouth =
        career.boardMonthlyObjective.find("titularidades") != string::npos &&
        career.boardMonthlyProgress < career.boardMonthlyTarget;
    const bool objectiveBudget =
        career.boardMonthlyObjective.find("presupuesto") != string::npos &&
        career.boardMonthlyProgress < career.boardMonthlyTarget;

    if (career.debtStatus.debtSeverity >= 55 || objectiveBudget ||
        team.debt > team.sponsorWeekly * 16 || team.budget < max(120000LL, team.sponsorWeekly * 3)) {
        return WeeklyDecision::FinancialControl;
    }
    if (career.managerStress.stressLevel >= 78 || career.managerStress.energy <= 28) {
        return WeeklyDecision::ManagerRest;
    }
    if (fatigued >= 4) return WeeklyDecision::Recovery;
    if (lowMorale >= 3 || team.morale < 48) return WeeklyDecision::DressingRoom;
    const WeeklyDecision matchDecision = decisionFromLastMatchCenter(career, fatigued);
    if (matchDecision != WeeklyDecision::Auto) return matchDecision;
    if ((objectiveYouth || career.boardYouthTarget > 0) && youth >= 2) return WeeklyDecision::YouthPathway;
    if (!career.lastMatchCenter.opponentName.empty() || nextOpponent(career) != nullptr) {
        return WeeklyDecision::MatchPreparation;
    }
    return WeeklyDecision::HighIntensityTraining;
}

string debtRestrictionMessage(const Career& career, long long transferCost, long long playerWage) {
    if (!career.myTeam) return "No hay una carrera activa.";
    if (!career.debtStatus.canBuyPlayers) {
        return "La deuda bloquea fichajes: la directiva exige control financiero antes de comprar.";
    }
    if (!canAffordTransfer(career.debtStatus, transferCost, playerWage)) {
        return "La deuda restringe esta operacion: costo y salario superan el margen permitido.";
    }
    if (!career.debtStatus.canOfferHighSalaries &&
        playerWage > max(12000LL, career.myTeam->sponsorWeekly / 2)) {
        return "La deuda activa un techo salarial: no puedes ofrecer ese salario semanal.";
    }
    return "";
}

string nextStaffHireName(const Team& team, const string& roleLabel, int currentLevel) {
    static const vector<string> firstNames = {"Oscar", "German", "Fabian", "Leonardo", "Victor", "Damian", "Ignacio", "Tomas"};
    static const vector<string> lastNames = {"Navarro", "Rivera", "Carrasco", "Saavedra", "Abarca", "Henriquez", "Vera", "Olivares"};
    int hash = currentLevel;
    const string key = normalizeTeamId(team.name) + normalizeTeamId(roleLabel);
    for (char ch : key) hash += static_cast<unsigned char>(ch);
    return firstNames[static_cast<size_t>(hash % static_cast<int>(firstNames.size()))] + " " +
           lastNames[static_cast<size_t>((hash / 7) % static_cast<int>(lastNames.size()))];
}

long long totalNegotiationCommitment(const NegotiationState& state) {
    return state.agreedFee + state.agreedBonus + state.agreedAgentFee + state.agreedLoyaltyBonus;
}

string describeContractExtras(const NegotiationState& state) {
    return string("firma ") + formatMoneyValue(state.agreedBonus) +
           " | agente " + formatMoneyValue(state.agreedAgentFee) +
           " | fidelidad " + formatMoneyValue(state.agreedLoyaltyBonus) +
           " | bonus por partido " + formatMoneyValue(state.agreedAppearanceBonus);
}

void eraseNamedSelection(vector<string>& values, const string& name) {
    values.erase(remove(values.begin(), values.end(), name), values.end());
}

long long upgradeCost(const Team& team, ClubUpgrade upgrade) {
    switch (upgrade) {
        case ClubUpgrade::Stadium: return 60000LL * (team.stadiumLevel + 1);
        case ClubUpgrade::Youth: return 50000LL * (team.youthFacilityLevel + 1);
        case ClubUpgrade::Training: return 55000LL * (team.trainingFacilityLevel + 1);
        case ClubUpgrade::Scouting: return 36000LL + static_cast<long long>(team.scoutingChief) * 1300LL;
        case ClubUpgrade::Medical: return 33000LL + static_cast<long long>(team.medicalTeam) * 1200LL;
        case ClubUpgrade::AssistantCoach: return 35000LL + static_cast<long long>(team.assistantCoach) * 1200LL;
        case ClubUpgrade::FitnessCoach: return 32000LL + static_cast<long long>(team.fitnessCoach) * 1200LL;
        case ClubUpgrade::YouthCoach: return 34000LL + static_cast<long long>(team.youthCoach) * 1200LL;
        case ClubUpgrade::GoalkeepingCoach: return 31000LL + static_cast<long long>(team.goalkeepingCoach) * 1100LL;
        case ClubUpgrade::PerformanceAnalyst: return 30000LL + static_cast<long long>(team.performanceAnalyst) * 1100LL;
    }
    return 0;
}

string upgradeLabel(ClubUpgrade upgrade) {
    switch (upgrade) {
        case ClubUpgrade::Stadium: return "estadio";
        case ClubUpgrade::Youth: return "cantera";
        case ClubUpgrade::Training: return "entrenamiento";
        case ClubUpgrade::Scouting: return "scouting";
        case ClubUpgrade::Medical: return "medico";
        case ClubUpgrade::AssistantCoach: return "asistente tecnico";
        case ClubUpgrade::FitnessCoach: return "preparador fisico";
        case ClubUpgrade::YouthCoach: return "jefe de juveniles";
        case ClubUpgrade::GoalkeepingCoach: return "entrenador de arqueros";
        case ClubUpgrade::PerformanceAnalyst: return "analista de rendimiento";
    }
    return "club";
}

bool isFacilityUpgrade(ClubUpgrade upgrade) {
    return upgrade == ClubUpgrade::Stadium ||
           upgrade == ClubUpgrade::Youth ||
           upgrade == ClubUpgrade::Training ||
           upgrade == ClubUpgrade::Medical;
}

ServiceResult failure(const string& message) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(message);
    return result;
}

ServiceResult failureFromNegotiation(const NegotiationState& state, const string& fallback) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(state.status.empty() ? fallback : state.status);
    result.messages.insert(result.messages.end(), state.roundSummaries.begin(), state.roundSummaries.end());
    return result;
}

ServiceResult transferWindowClosedFailure(const Career& career, const string& operation) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back("Mercado cerrado: no puedes cerrar " + operation + " en la semana " +
                              to_string(career.currentWeek) + ".");
    result.messages.push_back(transfer_market::transferWindowLabel(career));
    result.messages.push_back("Puedes avanzar scouting, renovar contratos o firmar precontratos elegibles.");
    return result;
}

void appendNegotiationMessages(ServiceResult& result, const NegotiationState& state) {
    result.messages.insert(result.messages.end(), state.roundSummaries.begin(), state.roundSummaries.end());
}

void registerNegotiatedPromise(Career& career, const Player& player, NegotiationPromise promise) {
    if (!career.myTeam || promise == NegotiationPromise::None) return;
    const int seasonDeadline = max(career.currentWeek, static_cast<int>(career.schedule.size()));
    career.activePromises.push_back({
        player.name,
        "Minutos",
        promiseLabel(promise),
        career.currentWeek,
        min(seasonDeadline, career.currentWeek + 8),
        player.startsThisSeason,
        false,
        false,
    });
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
    // === Fin Inicialización Sistemas ===
    
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

bool consumeLatestWeeklyDigestService(Career& career) {
    for (auto it = career.managerInbox.rbegin(); it != career.managerInbox.rend(); ++it) {
        if (it->find("[Centro semanal]") == string::npos) continue;
        career.managerInbox.erase(next(it).base());
        return true;
    }
    return false;
}

ServiceResult upgradeClubService(Career& career, ClubUpgrade upgrade) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    if (isFacilityUpgrade(upgrade) && career.debtStatus.debtSeverity >= 70) {
        return failure("La deuda bloquea inversiones de infraestructura hasta recuperar estabilidad financiera.");
    }
    long long cost = upgradeCost(team, upgrade);
    if (team.budget < cost) return failure("Presupuesto insuficiente para mejorar " + upgradeLabel(upgrade) + ".");
    team.budget -= cost;
    string message;
    switch (upgrade) {
        case ClubUpgrade::Stadium:
            team.stadiumLevel++;
            team.fanBase += 3;
            team.sponsorWeekly += 5000;
            message = team.name + " amplia su estadio.";
            break;
        case ClubUpgrade::Youth:
            team.youthFacilityLevel++;
            message = team.name + " mejora su cantera.";
            break;
        case ClubUpgrade::Training:
            team.trainingFacilityLevel++;
            team.goalkeepingCoach = clampInt(team.goalkeepingCoach + 3, 1, 99);
            if (team.goalkeepingCoachName.empty()) team.goalkeepingCoachName = nextStaffHireName(team, "arquero", team.goalkeepingCoach);
            message = team.name + " mejora su centro de entrenamiento.";
            break;
        case ClubUpgrade::Scouting:
            team.scoutingChief = clampInt(team.scoutingChief + 5, 1, 99);
            team.scoutingChiefName = nextStaffHireName(team, "scouting", team.scoutingChief);
            for (const auto& regionName : world_state_service::listConfiguredScoutingRegions()) {
                const bool coveredRegion =
                    regionName.empty() || regionName == "Todas" ||
                    find(team.scoutingRegions.begin(), team.scoutingRegions.end(), regionName) != team.scoutingRegions.end();
                if (!coveredRegion) {
                    team.scoutingRegions.push_back(regionName);
                    break;
                }
            }
            message = team.name + " fortalece su red de scouting con " + team.scoutingChiefName + ".";
            break;
        case ClubUpgrade::Medical:
            team.medicalTeam = clampInt(team.medicalTeam + 5, 1, 99);
            team.medicalChiefName = nextStaffHireName(team, "medico", team.medicalTeam);
            message = team.name + " fortalece el cuerpo medico con " + team.medicalChiefName + ".";
            break;
        case ClubUpgrade::AssistantCoach:
            team.assistantCoach = clampInt(team.assistantCoach + 5, 1, 99);
            team.performanceAnalyst = clampInt(team.performanceAnalyst + 3, 1, 99);
            team.assistantCoachName = nextStaffHireName(team, "assistant", team.assistantCoach);
            if (team.performanceAnalystName.empty()) team.performanceAnalystName = nextStaffHireName(team, "analyst", team.performanceAnalyst);
            message = team.name + " refuerza su cuerpo tecnico con " + team.assistantCoachName + ".";
            break;
        case ClubUpgrade::FitnessCoach:
            team.fitnessCoach = clampInt(team.fitnessCoach + 5, 1, 99);
            team.fitnessCoachName = nextStaffHireName(team, "fitness", team.fitnessCoach);
            message = team.name + " mejora su preparacion fisica con " + team.fitnessCoachName + ".";
            break;
        case ClubUpgrade::YouthCoach:
            team.youthCoach = clampInt(team.youthCoach + 5, 1, 99);
            team.youthCoachName = nextStaffHireName(team, "youth", team.youthCoach);
            message = team.name + " mejora la conduccion de juveniles con " + team.youthCoachName + ".";
            break;
        case ClubUpgrade::GoalkeepingCoach:
            team.goalkeepingCoach = clampInt(team.goalkeepingCoach + 5, 1, 99);
            team.goalkeepingCoachName = nextStaffHireName(team, "goalkeeping", team.goalkeepingCoach);
            message = team.name + " incorpora a " + team.goalkeepingCoachName + " para el trabajo de arqueros.";
            break;
        case ClubUpgrade::PerformanceAnalyst:
            team.performanceAnalyst = clampInt(team.performanceAnalyst + 5, 1, 99);
            team.performanceAnalystName = nextStaffHireName(team, "analyst", team.performanceAnalyst);
            message = team.name + " suma al analista " + team.performanceAnalystName + ".";
            break;
    }
    ensureTeamIdentity(team);
    syncInfrastructureFromTeam(career, team);
    career.infrastructure.upgradesThisSeason++;
    career.addNews(message);
    ServiceResult result;
    result.ok = true;
    result.messages.push_back(message + " Inversion " + formatMoneyValue(cost) + ".");
    return result;
}

ServiceResult reviewStaffStructureService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    const string weakestRole = staff_service::weakestStaffRole(team);
    ServiceResult result = upgradeClubService(career, staffUpgradeForRole(weakestRole));
    if (result.ok) {
        result.messages.insert(result.messages.begin(), "Revision de staff: se prioriza reforzar " + weakestRole + ".");
    } else {
        result.messages.insert(result.messages.begin(), "Revision de staff: el area mas debil hoy es " + weakestRole + ".");
    }
    return result;
}

ServiceResult changeYouthRegionService(Career& career, const string& region) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    static const vector<string> regions = {"Metropolitana", "Norte", "Centro", "Sur", "Patagonia"};
    if (find(regions.begin(), regions.end(), region) == regions.end()) {
        return failure("La region juvenil indicada no es valida.");
    }
    Team& team = *career.myTeam;
    if (team.youthRegion == region) return failure("Esa region juvenil ya esta activa.");
    const long long cost = 12000LL;
    if (team.budget < cost) return failure("Presupuesto insuficiente para reorientar la captacion juvenil.");
    team.budget -= cost;
    team.youthRegion = region;
    ensureTeamIdentity(team);
    career.addNews(team.name + " reorienta su captacion juvenil hacia " + team.youthRegion + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Nueva region juvenil: " + region + ". Inversion " + formatMoneyValue(cost) + ".");
    return result;
}

ServiceResult takeManagerJobService(Career& career, const string& teamName, const string& reason) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team* team = career.findTeamByName(teamName);
    if (!team) return failure("No se encontro el club seleccionado.");
    if (team == career.myTeam) return failure("Ya diriges ese club.");
    takeManagerJob(career, team, reason.empty() ? string("Cambio de club voluntario.") : reason);
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Nuevo destino: " + career.myTeam->name + ".");
    return result;
}

ServiceResult buyTransferTargetService(Career& career,
                                       const string& sellerTeamName,
                                       const string& playerName,
                                       NegotiationProfile profile,
                                       NegotiationPromise promise) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    if (!transfer_market::isTransferWindowOpen(career)) {
        return transferWindowClosedFailure(career, "fichajes inmediatos");
    }
    Team* seller = career.findTeamByName(sellerTeamName);
    if (!seller || seller == career.myTeam) return failure("No se encontro el club vendedor.");
    int sellerIdx = team_mgmt::playerIndexByName(*seller, playerName);
    if (sellerIdx < 0) return failure("No se encontro el jugador seleccionado.");
    int maxSquadSize = getCompetitionConfig(career.myTeam->division).maxSquadSize;
    if (maxSquadSize > 0 && static_cast<int>(career.myTeam->players.size()) >= maxSquadSize) {
        return failure("Tu plantel ya alcanzo el maximo permitido para la division.");
    }
    if (seller->players.size() <= 18) return failure("El club vendedor no libera jugadores con un plantel tan corto.");
    ensureTeamIdentity(*career.myTeam);
    ensureTeamIdentity(*seller);
    Player player = seller->players[static_cast<size_t>(sellerIdx)];
    if (player.onLoan) return failure("El jugador esta a prestamo y no esta disponible.");

    NegotiationState negotiation = runTransferNegotiation(career, *career.myTeam, *seller, player, profile, promise);
    if (!negotiation.clubAccepted || !negotiation.playerAccepted) {
        return failureFromNegotiation(negotiation, "La negociacion no pudo cerrarse.");
    }
    const long long totalCost = totalNegotiationCommitment(negotiation);
    const string debtRestriction = debtRestrictionMessage(career, totalCost, negotiation.agreedWage);
    if (!debtRestriction.empty()) {
        return failureFromNegotiation(negotiation, debtRestriction);
    }
    if (career.myTeam->budget < totalCost) {
        return failureFromNegotiation(negotiation, "Presupuesto insuficiente para cerrar la operacion.");
    }

    player.wage = negotiation.agreedWage;
    player.contractWeeks = negotiation.agreedContractWeeks;
    player.releaseClause = negotiation.agreedClause;
    player.onLoan = false;
    player.parentClub.clear();
    player.loanWeeksRemaining = 0;
    player.wantsToLeave = false;
    player.happiness = clampInt(player.happiness + 4, 1, 99);
    applyNegotiatedPromise(player, promise);
    player.promisedPosition = normalizePosition(player.position);
    career.myTeam->budget -= totalCost;
    career.myTeam->addPlayer(player);
    registerNegotiatedPromise(career, player, promise);
    seller->budget += negotiation.agreedFee;
    team_mgmt::detachPlayerFromSelections(*seller, player.name);
    eraseNamedSelection(career.scoutingShortlist, seller->name + "|" + player.name);
    seller->players.erase(seller->players.begin() + sellerIdx);
    ensureTeamIdentity(*career.myTeam);
    ensureTeamIdentity(*seller);
    career.addNews(player.name + " firma con " + career.myTeam->name + " desde " + seller->name +
                   " con un paquete contractual que incluye agente y bonus de fidelidad.");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Fichaje completado: " + player.name + " llega desde " + seller->name +
                              " por " + formatMoneyValue(negotiation.agreedFee) +
                              " | salario " + formatMoneyValue(negotiation.agreedWage) +
                              " | " + describeContractExtras(negotiation) +
                              " | contrato " + to_string(negotiation.agreedContractWeeks) +
                              " sem | promesa " + negotiation.agreedPromisedRole + ".");
    appendNegotiationMessages(result, negotiation);
    return result;
}

ServiceResult triggerReleaseClauseService(Career& career,
                                          const string& sellerTeamName,
                                          const string& playerName,
                                          NegotiationProfile profile,
                                          NegotiationPromise promise) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    if (!transfer_market::isTransferWindowOpen(career)) {
        return transferWindowClosedFailure(career, "clausulas de salida");
    }
    Team* seller = career.findTeamByName(sellerTeamName);
    if (!seller || seller == career.myTeam) return failure("No se encontro el club vendedor.");
    int sellerIdx = team_mgmt::playerIndexByName(*seller, playerName);
    if (sellerIdx < 0) return failure("No se encontro el jugador seleccionado.");
    int maxSquadSize = getCompetitionConfig(career.myTeam->division).maxSquadSize;
    if (maxSquadSize > 0 && static_cast<int>(career.myTeam->players.size()) >= maxSquadSize) {
        return failure("Tu plantel ya alcanzo el maximo permitido para la division.");
    }

    ensureTeamIdentity(*career.myTeam);
    ensureTeamIdentity(*seller);
    Player player = seller->players[static_cast<size_t>(sellerIdx)];
    if (player.onLoan) return failure("El jugador esta a prestamo y no esta disponible.");

    NegotiationState negotiation =
        runReleaseClauseNegotiation(career, *career.myTeam, *seller, player, profile, promise);
    if (!negotiation.clubAccepted || !negotiation.playerAccepted) {
        return failureFromNegotiation(negotiation, "No se pudo cerrar la clausula.");
    }
    const long long totalCost = totalNegotiationCommitment(negotiation);
    const string debtRestriction = debtRestrictionMessage(career, totalCost, negotiation.agreedWage);
    if (!debtRestriction.empty()) {
        return failureFromNegotiation(negotiation, debtRestriction);
    }
    if (career.myTeam->budget < totalCost) {
        return failureFromNegotiation(negotiation, "Presupuesto insuficiente para ejecutar la clausula.");
    }

    player.wage = negotiation.agreedWage;
    player.contractWeeks = negotiation.agreedContractWeeks;
    player.releaseClause = negotiation.agreedClause;
    player.onLoan = false;
    player.parentClub.clear();
    player.loanWeeksRemaining = 0;
    player.wantsToLeave = false;
    player.happiness = clampInt(player.happiness + 4, 1, 99);
    applyNegotiatedPromise(player, promise);
    player.promisedPosition = normalizePosition(player.position);

    career.myTeam->budget -= totalCost;
    career.myTeam->addPlayer(player);
    registerNegotiatedPromise(career, player, promise);
    seller->budget += negotiation.agreedFee;
    team_mgmt::detachPlayerFromSelections(*seller, player.name);
    eraseNamedSelection(career.scoutingShortlist, seller->name + "|" + player.name);
    seller->players.erase(seller->players.begin() + sellerIdx);
    ensureTeamIdentity(*career.myTeam);
    ensureTeamIdentity(*seller);
    career.addNews(player.name + " llega a " + career.myTeam->name +
                   " tras ejecutar su clausula con un contrato reforzado por agente y bonus de fidelidad.");

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Clausula ejecutada: " + player.name + " llega desde " + seller->name +
                              " por " + formatMoneyValue(negotiation.agreedFee) +
                              " | salario " + formatMoneyValue(negotiation.agreedWage) +
                              " | " + describeContractExtras(negotiation) +
                              " | promesa " + negotiation.agreedPromisedRole + ".");
    appendNegotiationMessages(result, negotiation);
    return result;
}

ServiceResult signPreContractService(Career& career,
                                     const string& sellerTeamName,
                                     const string& playerName,
                                     NegotiationProfile profile,
                                     NegotiationPromise promise) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team* seller = career.findTeamByName(sellerTeamName);
    if (!seller || seller == career.myTeam) return failure("No se encontro el club del jugador.");
    int sellerIdx = team_mgmt::playerIndexByName(*seller, playerName);
    if (sellerIdx < 0) return failure("No se encontro el jugador seleccionado.");
    const Player& player = seller->players[static_cast<size_t>(sellerIdx)];
    if (player.onLoan) return failure("No se puede firmar precontrato con un jugador cedido.");
    if (player.contractWeeks > 12) return failure("El jugador aun no es elegible para precontrato.");
    ensureTeamIdentity(*career.myTeam);
    ensureTeamIdentity(*seller);
    for (const auto& move : career.pendingTransfers) {
        if (move.playerName == player.name && move.toTeam == career.myTeam->name && move.preContract) {
            return failure("Ese jugador ya tiene un precontrato pendiente con tu club.");
        }
    }

    NegotiationState negotiation =
        runPreContractNegotiation(career, *career.myTeam, *seller, player, profile, promise);
    if (!negotiation.playerAccepted) {
        return failureFromNegotiation(negotiation, "El precontrato no pudo cerrarse.");
    }
    const long long upfrontCost = negotiation.agreedBonus + negotiation.agreedAgentFee + negotiation.agreedLoyaltyBonus;
    const string debtRestriction = debtRestrictionMessage(career, upfrontCost, negotiation.agreedWage);
    if (!debtRestriction.empty()) {
        return failureFromNegotiation(negotiation, debtRestriction);
    }
    if (career.myTeam->budget < upfrontCost) {
        return failureFromNegotiation(negotiation, "Presupuesto insuficiente para el paquete de firma.");
    }
    career.myTeam->budget -= upfrontCost;
    eraseNamedSelection(career.scoutingShortlist, seller->name + "|" + player.name);
    career.pendingTransfers.push_back({player.name,
                                       seller->name,
                                       career.myTeam->name,
                                       career.currentSeason + 1,
                                       0,
                                       upfrontCost,
                                       negotiation.agreedWage,
                                       negotiation.agreedContractWeeks,
                                       true,
                                       false,
                                       negotiation.agreedPromisedRole});
    career.addNews(player.name + " firma un precontrato con " + career.myTeam->name +
                   " tras acordar un paquete completo con agente y bonus.");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Precontrato firmado para la temporada siguiente. " +
                              describeContractExtras(negotiation) +
                              " | salario " + formatMoneyValue(negotiation.agreedWage) +
                              " | promesa " + negotiation.agreedPromisedRole + ".");
    appendNegotiationMessages(result, negotiation);
    return result;
}

ServiceResult renewPlayerContractService(Career& career,
                                         const string& playerName,
                                         NegotiationProfile profile,
                                         NegotiationPromise promise) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    Player& player = team.players[static_cast<size_t>(index)];
    ensureTeamIdentity(team);

    NegotiationState negotiation =
        runRenewalNegotiation(career, team, player, profile, promise, career.currentWeek);
    if (!negotiation.playerAccepted) {
        return failureFromNegotiation(negotiation, "La renovacion no pudo cerrarse.");
    }
    const long long upfrontCost = negotiation.agreedBonus + negotiation.agreedAgentFee + negotiation.agreedLoyaltyBonus;
    const string debtRestriction = debtRestrictionMessage(career, upfrontCost, negotiation.agreedWage);
    if (!debtRestriction.empty()) {
        return failureFromNegotiation(negotiation, debtRestriction);
    }
    if (team.budget < negotiation.agreedWage * 6 + upfrontCost) {
        return failureFromNegotiation(negotiation, "Presupuesto insuficiente para renovar al jugador.");
    }

    team.budget -= upfrontCost;
    player.wage = negotiation.agreedWage;
    player.contractWeeks = negotiation.agreedContractWeeks;
    player.releaseClause = negotiation.agreedClause;
    player.wantsToLeave = false;
    player.happiness = clampInt(player.happiness + 6, 1, 99);
    applyNegotiatedPromise(player, promise);
    player.promisedPosition = normalizePosition(player.position);
    registerNegotiatedPromise(career, player, promise);
    ensureTeamIdentity(team);
    career.addNews(player.name + " renueva con " + team.name +
                   " despues de una negociacion con agente, fidelidad y bonus por partido.");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Contrato renovado: " + player.name + " | salario " +
                              formatMoneyValue(negotiation.agreedWage) +
                              " | contrato " + to_string(negotiation.agreedContractWeeks) +
                              " sem | " + describeContractExtras(negotiation) +
                              " | promesa " + negotiation.agreedPromisedRole + ".");
    appendNegotiationMessages(result, negotiation);
    return result;
}

ServiceResult sellPlayerService(Career& career, const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    if (!transfer_market::isTransferWindowOpen(career)) {
        return transferWindowClosedFailure(career, "ventas inmediatas");
    }
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    if (team.players.size() <= 18) return failure("Debes mantener al menos 18 jugadores en plantel.");
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    const Player& player = team.players[static_cast<size_t>(index)];
    if (player.onLoan && !player.parentClub.empty()) return failure("No puedes vender un jugador cedido.");
    long long transferFee = max(10000LL, player.value * 105 / 100);
    team.budget += transferFee;
    team_mgmt::detachPlayerFromSelections(team, player.name);
    team_mgmt::applyDepartureShock(team, player);
    career.addNews(player.name + " sale de " + team.name + " por " + formatMoneyValue(transferFee) + ".");
    team.players.erase(team.players.begin() + index);
    ensureTeamIdentity(team);
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Venta completada: " + player.name + " deja el club por " + formatMoneyValue(transferFee) + ".");
    return result;
}

ServiceResult loanInPlayerService(Career& career,
                                  const string& sellerTeamName,
                                  const string& playerName,
                                  int loanWeeks) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    if (!transfer_market::isTransferWindowOpen(career)) {
        return transferWindowClosedFailure(career, "prestamos entrantes");
    }
    Team* seller = career.findTeamByName(sellerTeamName);
    if (!seller || seller == career.myTeam) return failure("No se encontro el club de origen.");
    int sellerIdx = team_mgmt::playerIndexByName(*seller, playerName);
    if (sellerIdx < 0) return failure("No se encontro el jugador seleccionado.");
    int maxSquadSize = getCompetitionConfig(career.myTeam->division).maxSquadSize;
    if (maxSquadSize > 0 && static_cast<int>(career.myTeam->players.size()) >= maxSquadSize) {
        return failure("Tu plantel ya alcanzo el maximo permitido para la division.");
    }

    Player player = seller->players[static_cast<size_t>(sellerIdx)];
    if (player.onLoan) return failure("El jugador ya esta cedido.");
    if (player.contractWeeks <= 12) return failure("El jugador no esta disponible para prestamo por su situacion contractual.");

    loanWeeks = clampInt(loanWeeks, 8, 26);
    long long fee = max(15000LL, player.value / 10);
    long long wageShare = max(player.wage / 2, wageDemandFor(player) * 55 / 100);
    const string debtRestriction = debtRestrictionMessage(career, fee, wageShare);
    if (!debtRestriction.empty()) return failure(debtRestriction);
    if (career.myTeam->budget < fee) return failure("Presupuesto insuficiente para el cargo de prestamo.");

    player.onLoan = true;
    player.parentClub = seller->name;
    player.loanWeeksRemaining = loanWeeks;
    player.wage = wageShare;
    career.myTeam->budget -= fee;
    seller->budget += fee;
    seller->players.erase(seller->players.begin() + sellerIdx);
    career.myTeam->addPlayer(player);
    career.addNews(player.name + " llega a prestamo desde " + seller->name + ".");

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Prestamo cerrado: " + player.name + " llega desde " + seller->name +
                              " | cargo " + formatMoneyValue(fee) +
                              " | salario semanal " + formatMoneyValue(wageShare) +
                              " | duracion " + to_string(loanWeeks) + " semanas.");
    return result;
}

ServiceResult loanOutPlayerService(Career& career,
                                   const string& playerName,
                                   const string& destinationTeamName,
                                   int loanWeeks) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    if (!transfer_market::isTransferWindowOpen(career)) {
        return transferWindowClosedFailure(career, "prestamos salientes");
    }
    Team* receiver = career.findTeamByName(destinationTeamName);
    if (!receiver || receiver == career.myTeam) return failure("No se encontro el club destino.");
    Team& team = *career.myTeam;
    if (team.players.size() <= 18) return failure("Necesitas mantener al menos 18 jugadores en plantel.");

    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    const int maxSquad = getCompetitionConfig(receiver->division).maxSquadSize;
    if (maxSquad > 0 && static_cast<int>(receiver->players.size()) >= maxSquad) {
        return failure("El club destino no tiene cupo de plantel.");
    }

    Player player = team.players[static_cast<size_t>(index)];
    if (player.onLoan) return failure("No puedes ceder un jugador que ya esta prestado.");

    loanWeeks = clampInt(loanWeeks, 8, 26);
    long long fee = max(10000LL, player.value / 12);
    player.onLoan = true;
    player.parentClub = team.name;
    player.loanWeeksRemaining = loanWeeks;
    receiver->addPlayer(player);
    team.budget += fee;
    receiver->budget = max(0LL, receiver->budget - fee);
    team_mgmt::detachPlayerFromSelections(team, player.name);
    team.players.erase(team.players.begin() + index);
    career.addNews(player.name + " sale a prestamo hacia " + receiver->name + ".");

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Prestamo acordado: " + player.name + " va a " + receiver->name +
                              " | ingreso " + formatMoneyValue(fee) +
                              " | duracion " + to_string(loanWeeks) + " semanas.");
    return result;
}

ServiceResult cyclePlayerDevelopmentPlanService(Career& career, const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    Player& player = team.players[static_cast<size_t>(index)];
    player.developmentPlan = nextDevelopmentPlan(player.developmentPlan);
    career.addNews("Plan individual actualizado para " + player.name + ": " + player.developmentPlan + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back(player.name + " cambia su plan a " + player.developmentPlan + ".");
    return result;
}

ServiceResult cyclePlayerInstructionService(Career& career, const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    Player& player = team.players[static_cast<size_t>(index)];
    player.individualInstruction = nextInstructionForPlayer(player);
    career.addNews("Instruccion individual actualizada para " + player.name + ": " + player.individualInstruction + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back(player.name + " cambia su instruccion a " + player.individualInstruction + ".");
    return result;
}

ServiceResult holdTeamMeetingService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    const DressingRoomSnapshot before = dressing_room_service::buildSnapshot(team, career.currentWeek);

    int improvedPlayers = 0;
    for (auto& player : team.players) {
        int delta = 1;
        if (player.wantsToLeave || promiseAtRisk(player, career.currentWeek)) delta += 2;
        if (player.happiness <= 45 || player.socialGroup == "Frustrados") delta += 2;
        if (player.leadership >= 72 || playerHasTrait(player, "Lider")) delta += 1;
        player.happiness = clampInt(player.happiness + delta, 1, 99);
        player.chemistry = clampInt(player.chemistry + max(1, delta / 2), 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum + max(1, delta / 2), -25, 25);
        if (!promiseAtRisk(player, career.currentWeek) && player.happiness >= 52) {
            player.wantsToLeave = false;
        }
        if (delta >= 3) improvedPlayers++;
    }

    team.morale = clampInt(team.morale + 2 + before.leadershipSupport + (before.socialTension >= 5 ? 2 : 0), 0, 100);
    const DressingRoomSnapshot after = dressing_room_service::buildSnapshot(team, career.currentWeek);

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Reunion de plantel realizada. Mejoran el clima " + to_string(improvedPlayers) + " jugador(es).");
    result.messages.push_back("Tension social: " + to_string(before.socialTension) + " -> " + to_string(after.socialTension) +
                              " | Moral del equipo " + to_string(team.morale) + ".");
    career.addNews("El manager convoca una reunion de plantel para ordenar el clima interno en " + team.name + ".");
    return result;
}

ServiceResult talkToPlayerService(Career& career, const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    int index = team_mgmt::playerIndexByName(team, playerName);
    if (index < 0) return failure("No se encontro el jugador seleccionado.");
    Player& player = team.players[static_cast<size_t>(index)];

    const bool roleConflict = !player.promisedPosition.empty() &&
                              normalizePosition(player.promisedPosition) != normalizePosition(player.position);
    int openness = player.professionalism + player.happiness + player.leadership + team.morale / 2;
    if (player.wantsToLeave) openness -= 18;
    if (promiseAtRisk(player, career.currentWeek)) openness -= 12;
    if (roleConflict) openness -= 10;

    ServiceResult result;
    result.ok = true;
    if (openness >= 135) {
        player.happiness = clampInt(player.happiness + 6, 1, 99);
        player.chemistry = clampInt(player.chemistry + 3, 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum + 4, -25, 25);
        player.wantsToLeave = false;
        result.messages.push_back("La charla con " + player.name + " sale muy bien y baja su tension.");
    } else if (openness >= 110) {
        player.happiness = clampInt(player.happiness + 3, 1, 99);
        player.chemistry = clampInt(player.chemistry + 2, 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum + 2, -25, 25);
        result.messages.push_back(player.name + " acepta el mensaje y queda mas alineado con el plan del club.");
    } else {
        player.happiness = clampInt(player.happiness - 1, 1, 99);
        player.moraleMomentum = clampInt(player.moraleMomentum - 1, -25, 25);
        result.messages.push_back("La charla con " + player.name + " queda en observacion y no despeja del todo el malestar.");
    }
    if (roleConflict) {
        if (player.versatility >= 58 || player.professionalism >= 70) {
            player.position = normalizePosition(player.promisedPosition);
            player.roleDuty = defaultDutyForPosition(player.position);
            result.messages.push_back("Se realinea su uso con la posicion prometida (" + player.promisedPosition + ").");
        } else {
            result.messages.push_back("Sigue pendiente ordenar su rol de posicion prometida (" + player.promisedPosition + ").");
        }
    }
    career.addNews("El manager mantiene una charla individual con " + player.name + ".");
    return result;
}

ServiceResult cycleTrainingFocusService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    team.trainingFocus = nextTrainingFocus(team.trainingFocus);
    ensureTeamIdentity(team);

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Plan semanal actualizado: " + team.trainingFocus + ".");
    const vector<development::TrainingSessionPlan> schedule = development::buildWeeklyTrainingSchedule(team, false);
    for (size_t i = 0; i < schedule.size() && i < 3; ++i) {
        result.messages.push_back("- " + schedule[i].day + ": " + schedule[i].focus + " | " + schedule[i].note);
    }
    career.addNews("El cuerpo tecnico redefine el microciclo de " + team.name + " hacia un plan " + team.trainingFocus + ".");
    return result;
}

ServiceResult cycleMatchInstructionService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    team.matchInstruction = nextMatchInstruction(team.matchInstruction);
    ensureTeamIdentity(team);
    career.addNews("Nueva instruccion de partido en " + team.name + ": " + team.matchInstruction + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Instruccion de partido actual: " + team.matchInstruction + ".");
    return result;
}

ServiceResult applyWeeklyDecisionService(Career& career, WeeklyDecision decision) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    syncTeamFromInfrastructure(career, team);
    if (decision == WeeklyDecision::Auto) {
        decision = chooseAutomaticWeeklyDecision(career);
    }

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Decision semanal aplicada: " + weeklyDecisionLabel(decision) + ".");

    switch (decision) {
        case WeeklyDecision::Auto:
            break;
        case WeeklyDecision::Recovery: {
            team.trainingFocus = "Recuperacion";
            int protectedPlayers = 0;
            for (auto& player : team.players) {
                if (player.fitness >= 66 && player.fatigueLoad < 55 && !player.injured) continue;
                player.individualInstruction = "Descanso medico";
                player.fatigueLoad = clampInt(player.fatigueLoad - 10, 0, 100);
                player.fitness = clampInt(player.fitness + 4 + max(0, team.medicalTeam - 60) / 18, 15, player.stamina);
                if (player.injured && team.medicalTeam >= 68) {
                    player.injuryWeeks = max(0, player.injuryWeeks - 1);
                    if (player.injuryWeeks == 0) {
                        player.injured = false;
                        player.injuryType.clear();
                    }
                }
                if (++protectedPlayers >= 4) break;
            }
            reduceStressWithRest(career.managerStress, 1);
            result.messages.push_back("Plan semanal: Recuperacion. Protegidos " + to_string(protectedPlayers) + " jugador(es).");
            break;
        }
        case WeeklyDecision::HighIntensityTraining: {
            const bool needsGoals = career.lastMatchCenter.myGoals <= career.lastMatchCenter.oppGoals &&
                                    career.lastMatchCenter.myExpectedGoalsTenths <= 11;
            const bool concededChances = career.lastMatchCenter.oppExpectedGoalsTenths >= 14;
            team.trainingFocus = needsGoals ? "Ataque" : (concededChances ? "Defensa" : "Tactico");
            career.managerStress.energy = clampInt(career.managerStress.energy - 5, 0, 100);
            career.managerStress.stressLevel = clampInt(career.managerStress.stressLevel + 2, 0, 100);
            team.morale = clampInt(team.morale + 1, 0, 100);
            result.messages.push_back("Plan semanal: " + team.trainingFocus + ". Suben automatismos, pero baja energia del manager.");
            break;
        }
        case WeeklyDecision::DressingRoom: {
            ServiceResult meeting = holdTeamMeetingService(career);
            result.messages.insert(result.messages.end(), meeting.messages.begin(), meeting.messages.end());
            career.managerStress.stressLevel = clampInt(career.managerStress.stressLevel - 3, 0, 100);
            break;
        }
        case WeeklyDecision::MatchPreparation: {
            const Team* opponent = nextOpponent(career);
            team.trainingFocus = "Preparacion partido";
            if (opponent) {
                if (opponent->defensiveLine >= 4) team.matchInstruction = "Juego directo";
                else if (opponent->width <= 2) team.matchInstruction = "Por bandas";
                else if (opponent->pressingIntensity >= 4) team.matchInstruction = "Pausar juego";
                else team.matchInstruction = "Contra-presion";
            } else {
                team.matchInstruction = "Equilibrado";
            }
            int tunedPlayers = 0;
            for (auto& player : team.players) {
                if (player.injured || player.matchesSuspended > 0) continue;
                player.tacticalDiscipline = clampInt(player.tacticalDiscipline + 1 + max(0, team.performanceAnalyst - 60) / 20, 1, 99);
                if (++tunedPlayers >= 11) break;
            }
            career.managerStress.energy = clampInt(career.managerStress.energy - 4, 0, 100);
            result.messages.push_back("Preparacion rival: " +
                                      string(opponent ? opponent->name : "sin rival confirmado") +
                                      " | instruccion " + team.matchInstruction + ".");
            break;
        }
        case WeeklyDecision::FinancialControl: {
            team.transferPolicy = "Vender antes de comprar";
            long long payment = 0;
            if (team.debt > 0 && team.budget > team.sponsorWeekly * 2) {
                payment = min(team.debt, max(0LL, team.budget / 12));
                team.budget -= payment;
                team.debt -= payment;
            }
            career.debtStatus = calculateDebtStatus(
                team.budget,
                team.debt,
                max(1LL, team.sponsorWeekly + static_cast<long long>(team.fanBase) * 2500LL));
            applyFinancialSanctions(career.debtStatus);
            career.boardConfidence = clampInt(career.boardConfidence + (payment > 0 ? 1 : 0), 0, 100);
            result.messages.push_back("Politica de mercado: Vender antes de comprar" +
                                      string(payment > 0 ? " | amortizacion " + formatMoneyValue(payment) : " | sin amortizacion posible") + ".");
            break;
        }
        case WeeklyDecision::YouthPathway: {
            team.trainingFocus = "Tecnico";
            int promotedFocus = 0;
            for (auto& player : team.players) {
                if (player.age > 21 || player.potential < player.skill + 6) continue;
                player.developmentPlan = normalizePosition(player.position) == "DEL" ? "Finalizacion"
                                      : normalizePosition(player.position) == "DEF" ? "Defensa"
                                      : "Creatividad";
                player.happiness = clampInt(player.happiness + 4, 1, 99);
                player.moraleMomentum = clampInt(player.moraleMomentum + 2, -25, 25);
                ++promotedFocus;
                if (promotedFocus >= 3) break;
            }
            if (career.boardMonthlyObjective.find("titularidades") != string::npos && promotedFocus > 0) {
                career.addInboxItem("Plan juvenil listo: alinea un sub-20 para avanzar el objetivo mensual.", "Directiva");
            }
            result.messages.push_back("Cantera priorizada: " + to_string(promotedFocus) + " proyecto(s) reciben foco individual.");
            break;
        }
        case WeeklyDecision::ManagerRest: {
            const int previousStress = career.managerStress.stressLevel;
            reduceStressWithRest(career.managerStress, 2);
            team.trainingFocus = "Recuperacion";
            team.morale = clampInt(team.morale + 1, 0, 100);
            result.messages.push_back("Descanso del manager: estres " + to_string(previousStress) + " -> " +
                                      to_string(career.managerStress.stressLevel) +
                                      " | energia " + to_string(career.managerStress.energy) + ".");
            break;
        }
    }

    syncInfrastructureFromTeam(career, team);
    career.addNews("Centro semanal: " + weeklyDecisionLabel(decision) + " en " + team.name + ".");
    return result;
}

ServiceResult applyMatchPreparationPlanService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    const Team* opponent = nextOpponent(career);
    const vector<string> planLines = buildNextOpponentPlanLines(career, 5);

    ServiceResult result = applyWeeklyDecisionService(career, WeeklyDecision::MatchPreparation);
    if (!result.ok) return result;

    Team& team = *career.myTeam;
    string headline = "Plan de partido aplicado: " +
                      string(opponent ? opponent->name : "sin rival confirmado") +
                      " | entrenamiento " + team.trainingFocus +
                      " | instruccion " + team.matchInstruction + ".";
    result.messages.insert(result.messages.begin() + min<size_t>(1, result.messages.size()), headline);

    if (!planLines.empty()) {
        result.messages.push_back("Checklist del asistente:");
        for (const string& line : planLines) {
            result.messages.push_back("- " + line);
        }
    }

    career.addNews("Plan de partido preparado para " +
                   string(opponent ? opponent->name : "la proxima fecha") +
                   ": " + team.matchInstruction + ".");
    return result;
}

vector<string> buildWeeklyDecisionOptions(const Career& career) {
    vector<string> lines;
    const WeeklyDecision autoDecision = chooseAutomaticWeeklyDecision(career);
    lines.push_back("Auto | El staff recomienda: " + weeklyDecisionLabel(autoDecision));
    lines.push_back("Recuperar plantel | Baja fatiga, protege lesionados y orienta el microciclo a recuperacion.");
    lines.push_back("Entrenar fuerte | Sube foco tecnico/tactico segun el ultimo partido, con coste de energia.");
    lines.push_back("Ordenar vestuario | Reunion de plantel para mejorar moral, quimica y promesas en riesgo.");
    lines.push_back("Preparar rival | Ajusta instruccion de partido y disciplina tactica para el proximo rival.");
    lines.push_back("Control financiero | Reduce deuda cuando hay caja y activa politica vender antes de comprar.");
    lines.push_back("Impulsar juveniles | Enfoca proyectos sub-21 y prepara el objetivo de minutos juveniles.");
    lines.push_back("Descanso manager | Reduce estres y recupera energia antes de decisiones importantes.");
    return lines;
}

ServiceResult resolveInboxDecisionService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);

    const auto actionableEntries = inbox_service::buildActionableInbox(career, 8);
    const auto inboxEntries = inbox_service::buildCombinedInbox(career, 8);
    const auto medicalStatuses = medical_service::buildMedicalStatuses(team);
    const bool hasRealActionableEntry =
        !actionableEntries.empty() &&
        !(actionableEntries.size() == 1 && actionableEntries.front().urgency <= 12 &&
          actionableEntries.front().text.find("Inbox limpio") != string::npos);
    if (!hasRealActionableEntry && inboxEntries.empty() && medicalStatuses.empty()) {
        return failure("No hay decisiones urgentes en el inbox.");
    }

    string latestText;
    string latestCommand;
    string latestDestination;
    string latestChannel;
    bool scoutingEntry = false;
    const string weeklyDigestText = latestWeeklyDigestInboxEntry(career);
    if (!weeklyDigestText.empty()) {
        latestText = weeklyDigestText;
        latestChannel = "Centro semanal";
    } else if (hasRealActionableEntry) {
        latestText = actionableEntries.front().text;
        latestCommand = actionableEntries.front().command;
        latestDestination = actionableEntries.front().destination;
        latestChannel = actionableEntries.front().channel;
        scoutingEntry = actionableEntries.front().scouting;
    } else if (!inboxEntries.empty()) {
        latestText = inboxEntries.back().text;
        latestChannel = inboxEntries.back().channel;
        scoutingEntry = inboxEntries.back().scouting;
    }
    const string lower = toLower(latestChannel + " " + latestDestination + " " + latestCommand + " " + latestText);

    ServiceResult result;
    const bool weeklyDigestEntry =
        lower.find("centro semanal") != string::npos ||
        (lower.find("cockpit semanal") != string::npos && lower.find("decision:") != string::npos) ||
        lower.find("decision sugerida") != string::npos;
    const bool needsRecovery = lower.find("lesion") != string::npos ||
                               lower.find("fatiga") != string::npos ||
                               lower.find("fisico") != string::npos ||
                               lower.find("carga") != string::npos ||
                               lower.find("recuperacion") != string::npos ||
                               lower.find("medico") != string::npos ||
                               (!medicalStatuses.empty() && lower.empty());
    if (weeklyDigestEntry) {
        result = applyWeeklyDecisionService(career, WeeklyDecision::Auto);
        if (result.ok) {
            result.messages.insert(result.messages.begin(),
                                   "Decision de inbox: se aplica la recomendacion del cierre semanal.");
        }
    } else if (needsRecovery) {
        team.trainingFocus = "Recuperacion";
        int protectedPlayers = 0;
        for (const auto& status : medicalStatuses) {
            int index = team_mgmt::playerIndexByName(team, status.playerName);
            if (index < 0) continue;
            Player& player = team.players[static_cast<size_t>(index)];
            player.individualInstruction = "Descanso medico";
            player.fatigueLoad = clampInt(player.fatigueLoad - 6, 0, 100);
            protectedPlayers++;
            if (protectedPlayers >= 2) break;
        }
        result.ok = true;
        result.messages.push_back("Decision de inbox: se prioriza recuperacion y se protegen " + to_string(protectedPlayers) + " jugador(es)." );
        result.messages.push_back("Plan semanal actualizado a Recuperacion.");
    } else if (lower.find("contrato") != string::npos || lower.find("renov") != string::npos) {
        string renewalTarget;
        for (const auto& player : team.players) {
            if (player.contractWeeks > 0 && player.contractWeeks <= 12) {
                renewalTarget = player.name;
                break;
            }
        }
        if (!renewalTarget.empty()) {
            result = renewPlayerContractService(career, renewalTarget, NegotiationProfile::Balanced, NegotiationPromise::None);
            if (result.ok) result.messages.insert(result.messages.begin(), "Decision de inbox: se atiende renovacion prioritaria.");
        } else {
            result = applyWeeklyDecisionService(career, WeeklyDecision::MatchPreparation);
        }
    } else if (lower.find("deuda") != string::npos || lower.find("caja") != string::npos ||
               lower.find("presupuesto") != string::npos || lower.find("salario") != string::npos ||
               lower.find("finanz") != string::npos) {
        result = applyWeeklyDecisionService(career, WeeklyDecision::FinancialControl);
    } else if (lower.find("staff") != string::npos) {
        result = reviewStaffStructureService(career);
    } else if (scoutingEntry || lower.find("scouting") != string::npos || lower.find("mercado") != string::npos || lower.find("shortlist") != string::npos) {
        if (!career.scoutingShortlist.empty()) result = followShortlistService(career);
        else result = createScoutingAssignmentService(career, "", detectScoutingNeed(team), 3);
    } else if (lower.find("promesa") != string::npos || lower.find("directiva") != string::npos || lower.find("vestuario") != string::npos) {
        result = holdTeamMeetingService(career);
    } else if (lower.find("rival") != string::npos || lower.find("partido") != string::npos ||
               lower.find("tact") != string::npos || lower.find("instruccion") != string::npos) {
        result = applyWeeklyDecisionService(career, WeeklyDecision::MatchPreparation);
    } else {
        result = cycleMatchInstructionService(career);
    }

    if (result.ok && !latestText.empty()) {
        consumeMatchingInboxEntry(career.managerInbox, latestText);
        consumeMatchingInboxEntry(career.scoutInbox, latestText);
    }
    return result;
}
