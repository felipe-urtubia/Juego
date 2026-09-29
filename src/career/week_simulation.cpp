#include "career/week_simulation.h"
#include "career/career_service.h"
#include "week_match_helpers.h"

#include "career/app_services.h"
#include "career/career_modules.h"
#include "career/gameplay_reports.h"
#include "ai/team_ai.h"
#include "career/match_analysis_store.h"
#include "career/dressing_room_service.h"
#include "career/career_reports.h"
#include "career/career_runtime.h"
#include "career/season_transition.h"
#include "career/career_support.h"
#include "career/player_development.h"
#include "career/staff_service.h"
#include "career/team_management.h"
#include "career/world_state_service.h"
#include "career/game_events_system.h"
#include "competition.h"
#include "development/monthly_development.h"
#include "simulation.h"
#include "simulation/match_center.h"
#include "transfers/negotiation_system.h"
#include "transfers/transfer_market.h"
#include "engine/social_system.h"
#include "engine/rival_ai.h"
#include "engine/rivalry_system.h"
#include "engine/manager_stress.h"
#include "engine/debt_system.h"
#include "engine/facilities_system.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

using namespace std;

namespace {



TeamId safeActiveTeamIdAt(const Career& career, size_t index) {
    if (index > static_cast<size_t>(numeric_limits<int>::max())) return kInvalidTeamId;
    return career.getActiveTeamIdAt(static_cast<int>(index));
}







void weeklyDashboard(const Career& career) {
    if (!career.myTeam) return;
    emitUiMessage("");
    for (const string& line : formatCareerReportLines(buildWeeklyDashboardReport(career))) {
        emitUiMessage(line);
    }
    emitUiMessage("");
    emitUiMessage("--- CENTRO SEMANAL DE DECISIONES ---");
    for (const string& line : buildWeeklyDecisionOptions(career)) {
        emitUiMessage(line);
    }
    
    // === Mostrar Sistemas de Gameplay ===
    emitUiMessage("");
    emitUiMessage("--- SISTEMAS DE JUGABILIDAD ---");
    
    // Reportes de vestuario
    auto dressingRoomBrief = gameplay_reports::getDressingRoomBrief(career.dressingRoomDynamics);
    for (const auto& line : dressingRoomBrief) {
        emitUiMessage(line);
    }
    
    // Alertas del manager
    auto managerAlerts = gameplay_reports::getManagerCriticalAlerts(career.managerStress);
    if (!managerAlerts.empty()) {
        emitUiMessage("");
        for (const auto& alert : managerAlerts) {
            emitUiMessage(alert);
        }
    }
    
    // Alertas de deuda
    auto debtAlerts = gameplay_reports::getDebtAlerts(career.debtStatus);
    if (!debtAlerts.empty()) {
        emitUiMessage("");
        for (const auto& alert : debtAlerts) {
            emitUiMessage(alert);
        }
    }
    
    // Rivalidades destacadas
    auto rivalryHighlights = gameplay_reports::getRivalryHighlights(career.rivalryDynamics, career.myTeam->name);
    if (!rivalryHighlights.empty()) {
        emitUiMessage("");
        for (const auto& highlight : rivalryHighlights) {
            emitUiMessage(highlight);
        }
    }
    
    // Sugerencias de instalaciones
    auto facilitySuggestions = gameplay_reports::getFacilitySuggestions(career.infrastructure, career.myTeam->budget);
    if (!facilitySuggestions.empty()) {
        emitUiMessage("");
        for (const auto& suggestion : facilitySuggestions) {
            emitUiMessage(suggestion);
        }
    }
}

void applyClubEvent(Career& career) {
    if (!career.myTeam || randInt(1, 100) > 15) return;
    int event = randInt(1, 6);
    if (event == 1) {
        long long bonus = 50000 + randInt(0, 30000);
        career.myTeam->budget += bonus;
        career.addNews("Nuevo patrocinio para " + career.myTeam->name + " por $" + to_string(bonus) + ".");
        emitUiMessage("[Evento] Patrocinio sorpresa: +" + to_string(bonus));
        return;
    }
    if (event == 2) {
        career.myTeam->morale = clampInt(career.myTeam->morale - 5, 0, 100);
        career.addNews("La hinchada presiona a " + career.myTeam->name + " tras los ultimos resultados.");
        emitUiMessage("[Evento] Protesta de hinchas: moral -5.");
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::MoraleAlert,
            "Protesta de hinchas",
            "Baja moral en el equipo. La hinchada presiona tras los ultimos resultados (-5 moral)."
        );
        return;
    }
    if (event == 3) {
        if (career.myTeam->players.empty()) return;
        int index = randInt(0, static_cast<int>(career.myTeam->players.size()) - 1);
        Player& player = career.myTeam->players[static_cast<size_t>(index)];
        player.injured = true;
        player.injuryType = "Leve";
        player.injuryWeeks = randInt(1, 2);
        player.injuryHistory++;
        career.addNews(player.name + " sufre una lesion leve en entrenamiento.");
        emitUiMessage("[Evento] Accidente en entrenamiento: " + player.name +
                      " fuera " + to_string(player.injuryWeeks) + " semanas.");
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::CriticalInjury,
            "Lesión en entrenamiento",
            player.name + " sufre una lesión leve. Baja estimada: " + to_string(player.injuryWeeks) + " semanas."
        );
        return;
    }
    if (event == 4) {
        for (auto& player : career.myTeam->players) {
            if (player.age > 29 || player.startsThisSeason > 1) continue;
            player.happiness = clampInt(player.happiness - 5, 1, 99);
            player.unhappinessWeeks = clampInt(player.unhappinessWeeks + 1, 0, 52);
            player.socialGroup = "Frustrados";
            career.addNews("Vestuario: " + player.name + " pide una conversacion por falta de minutos.");
            emitUiMessage("[Evento] Vestuario tenso: " + player.name + " reclama mas protagonismo.");
            career_events::EventNotificationSystem::recordEvent(
                career_events::EventType::ManagerAlert,
                "Jugador frustrado",
                player.name + " reclama más minutos. ¡Atiende al vestuario!"
            );
            return;
        }
    }
    if (event == 5) {
        const Player* best = nullptr;
        for (const auto& player : career.myTeam->players) {
            if (player.injured) continue;
            if (!best || player.skill > best->skill) best = &player;
        }
        if (best) {
            int idx = static_cast<int>(best - &career.myTeam->players[0]);
            Player& player = career.myTeam->players[static_cast<size_t>(idx)];
            player.wantsToLeave = player.ambition >= 60;
            player.happiness = clampInt(player.happiness - 3, 1, 99);
            career.addNews("Mercado: un club grande empieza a seguir a " + player.name + ".");
            emitUiMessage("[Evento] Mercado: aumenta el interes externo por " + player.name + ".");
            career_events::EventNotificationSystem::recordEvent(
                career_events::EventType::PlayerOffered,
                "Interés de mercado",
                "Un club grande sigue a " + player.name + ". ¡Prepárate para una posible oferta!"
            );
            return;
        }
    }

    int maxSquad = getCompetitionConfig(career.myTeam->division).maxSquadSize;
    if (maxSquad > 0 && static_cast<int>(career.myTeam->players.size()) >= maxSquad) return;
    int minSkill = 0;
    int maxSkill = 0;
    getDivisionSkillRange(career.myTeam->division, minSkill, maxSkill);
    int youthBoost = max(0, career.myTeam->youthFacilityLevel - 1);
    Player youth = makeRandomPlayer("MED", minSkill + youthBoost, maxSkill + youthBoost, 16, 18);
    youth.potential = clampInt(youth.skill + randInt(8 + youthBoost, 15 + youthBoost), youth.skill, 99);
    career.myTeam->addPlayer(youth);
    career.addNews("La cantera promociona a " + youth.name + " en " + career.myTeam->name + ".");
    emitUiMessage("[Evento] Cantera: se unio " + youth.name + " (pot " + to_string(youth.potential) + ").");
}



void maybeInvokeIdle() {
    if (IdleCallback callback = idleCallback()) {
        callback();
    }
}



void updateSquadDynamics(Career& career, int pointsDelta) {
    DressingRoomSnapshot snapshot = dressing_room_service::applyWeeklyUpdate(career, pointsDelta);
    if (!snapshot.summary.empty()) {
        emitUiMessage("[Vestuario] " + snapshot.summary);
    }
    for (size_t i = 0; i < snapshot.alerts.size() && i < 2; ++i) {
        emitUiMessage("[Vestuario] " + snapshot.alerts[i]);
    }
}

void runMonthlyDevelopment(Career& career) {
    if (career.currentWeek <= 0 || career.currentWeek % 4 != 0) return;
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        Team* team = career.getActiveTeamAt(i);
        if (!team) continue;
        const development::MonthlyDevelopmentSummary summary =
            development::runMonthlyDevelopmentCycle(*team, career.currentWeek);
        if (team == career.myTeam && summary.improvedPlayers > 0) {
            career.addNews("Informe juvenil: " + to_string(summary.improvedPlayers) +
                           " jugador(es) joven(es) mejoran este mes.");
            if (summary.acceleratedProspects > 0) {
                career.addNews("Informe de cantera: " + to_string(summary.acceleratedProspects) +
                               " prospecto(s) muestran una aceleracion especial.");
            }
        }
        if (team == career.myTeam && summary.newYouthPlayers > 0) {
            career.addNews("La cantera suma " + to_string(summary.newYouthPlayers) +
                           " nuevo(s) prospecto(s) desde la region " + team->youthRegion + ".");
        }
    }
}

void progressScoutingAssignments(Career& career) {
    if (!career.myTeam || career.scoutingAssignments.empty()) return;

    Team& team = *career.myTeam;
    for (size_t i = 0; i < career.scoutingAssignments.size();) {
        ScoutingAssignment& assignment = career.scoutingAssignments[i];
        assignment.weeksRemaining = max(0, assignment.weeksRemaining - 1);
        assignment.knowledgeLevel = clampInt(assignment.knowledgeLevel + team.scoutingChief / 5 + max(0, team.performanceAnalyst - 50) / 6, 0, 100);

        if (assignment.weeksRemaining <= 0) {
            const ScoutingSessionResult session = runScoutingSessionService(career, assignment.region, assignment.focusPosition);
            if (session.service.ok) {
                const string lead = session.candidates.empty()
                    ? string("sin candidato destacado")
                    : session.candidates.front().playerName + " (" + session.candidates.front().clubName + ")";
                career.addInboxItem("Asignacion cerrada en " + assignment.region + " | foco " + assignment.focusPosition +
                                    " | radar " + lead + ".", "Scouting");
            } else {
                career.addInboxItem("Asignacion pausada en " + assignment.region + ": " + session.service.messages.front(), "Scouting");
            }
            career.scoutingAssignments.erase(career.scoutingAssignments.begin() + static_cast<long long>(i));
            continue;
        }

        if (assignment.weeksRemaining == 1 || assignment.knowledgeLevel >= 72) {
            career.addInboxItem("Seguimiento en curso: " + assignment.region + " | foco " + assignment.focusPosition +
                                " | conocimiento " + to_string(assignment.knowledgeLevel) + "%.", "Scouting");
        }
        ++i;
    }
}

void updateShortlistAlerts(Career& career) {
    if (!career.myTeam || career.scoutingShortlist.empty() || career.currentWeek % 4 != 0) return;

    vector<string> active;
    for (const auto& item : career.scoutingShortlist) {
        auto parts = splitByDelimiter(item, '|');
        if (parts.size() < 2) continue;
        Team* seller = career.findTeamByName(parts[0]);
        if (!seller) continue;
        int index = team_mgmt::playerIndexByName(*seller, parts[1]);
        if (index < 0) continue;
        const Player& player = seller->players[static_cast<size_t>(index)];
        active.push_back(item);
        if (player.contractWeeks <= 12) {
            career.addNews("Alerta de shortlist: " + player.name + " entra en ventana de precontrato con " +
                           seller->name + ".");
        } else if (player.value <= player.releaseClause * 60 / 100) {
            career.addNews("Alerta de shortlist: " + player.name + " mantiene un costo accesible en " +
                           seller->name + ".");
        }
    }
    career.scoutingShortlist = active;
}

void updateManagerReputation(Career& career) {
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

void handleManagerStatus(Career& career) {
    if (!career.myTeam) return;
    if (career.boardConfidence >= 20 && career.boardWarningWeeks < 6) return;
    emitUiMessage("");
    emitUiMessage("[Directiva] " + career.myTeam->name + " decide despedirte.");
    career.addNews(career.managerName + " fue despedido de " + career.myTeam->name + ".");
    career.managerReputation = clampInt(career.managerReputation - 8, 10, 100);

    vector<Team*> jobs = buildJobMarket(career, true);
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



void emitSeasonTransitionSummary(const SeasonTransitionSummary& summary) {
    emitUiMessage("");
    emitUiMessage("--- Cierre de Temporada ---");
    for (const string& line : summary.lines) {
        emitUiMessage(line);
    }
}


}  // namespace

void checkAchievements(Career& career) {
    if (!career.myTeam) return;
    if (career.myTeam->wins >= 10 &&
        find(career.achievements.begin(), career.achievements.end(), "10 Victorias") == career.achievements.end()) {
        career.achievements.push_back("10 Victorias");
        emitUiMessage("Logro desbloqueado: 10 Victorias!");
    }
}

// === PHASE 2: SEPARATION OF CONCERNS (Internal Helpers) ===
// These functions extract logical sections of simulateCareerWeek to improve readability
// and reduce cyclomatic complexity of the main function

namespace {

// Process all matches scheduled for this week and return points delta for player team


// Update suspensions, injuries, fitness and training for all teams


void processTransfersPhase(Career& career) {
    maybeInvokeIdle();
    CareerService(career).processContractUpdates();
    maybeInvokeIdle();
    CareerService(career).processIncomingOffers();
    maybeInvokeIdle();
    transfer_market::processCpuTransfers(career);
    maybeInvokeIdle();
    transfer_market::processLoanReturns(career);
}

void applyFinancesPhase(Career& career, const unordered_map<TeamId, int>& pointsBefore) {
    maybeInvokeIdle();
    CareerService(career).processWeeklyFinances(pointsBefore);
    maybeInvokeIdle();
    career.leagueTable.sortTable();
    maybeInvokeIdle();
}

// Update manager-specific game state (reputation, objectives, etc.)
void updateManagerGameState(Career& career, int myTeamPointsDelta) {
    maybeInvokeIdle();
    if (career.myTeam) {
        if (career.boardMonthlyObjective.find("puntos") != string::npos) {
            career.boardMonthlyProgress += myTeamPointsDelta;
        }
        updateSquadDynamics(career, myTeamPointsDelta);
        maybeInvokeIdle();
        WorldPulseSummary worldPulse = world_state_service::processWeeklyWorldState(career);
        for (size_t i = 0; i < worldPulse.headlines.size() && i < 2; ++i) {
            emitUiMessage("[Mundo] " + worldPulse.headlines[i]);
        }
    }

    runMonthlyDevelopment(career);
    maybeInvokeIdle();
    progressScoutingAssignments(career);
    maybeInvokeIdle();
    updateShortlistAlerts(career);
    maybeInvokeIdle();
    career.updateDynamicObjectiveStatus();
    maybeInvokeIdle();
    career.updateBoardConfidence();
    maybeInvokeIdle();
    updateManagerReputation(career);
}

// Generate game narrative, events and communications for the week
void generateWeeklyNarrative(Career& career, int myTeamPointsDelta) {
    if (career.myTeam) {
        if (myTeamPointsDelta == 3) {
            career.addNews(career.myTeam->name + " gana en la fecha " + to_string(career.currentWeek) + ".");
        } else if (myTeamPointsDelta == 1) {
            career.addNews(career.myTeam->name + " empata en la fecha " + to_string(career.currentWeek) + ".");
        } else {
            career.addNews(career.myTeam->name + " pierde en la fecha " + to_string(career.currentWeek) + ".");
        }
        if (career.boardWarningWeeks >= 4) {
            career.addNews("La directiva aumenta la presion sobre " + career.myTeam->name + ".");
        }
        CareerService(career).generateWeeklyManagerCareerEvents();
        CareerService(career).generateWeeklyNarratives(myTeamPointsDelta);
    }
}

struct WeekSimulationSnapshots {
    vector<TeamId> activeTeamIds;
    unordered_map<TeamId, vector<int>> suspensionsBefore;
    unordered_map<TeamId, int> pointsBefore;
};

WeekSimulationSnapshots captureWeekSnapshots(const Career& career) {
    WeekSimulationSnapshots snapshots;
    const int activeTeamCount = career.getActiveTeamCount();
    snapshots.activeTeamIds.reserve(static_cast<size_t>(activeTeamCount));
    snapshots.suspensionsBefore.reserve(static_cast<size_t>(activeTeamCount));
    snapshots.pointsBefore.reserve(static_cast<size_t>(activeTeamCount));

    for (int i = 0; i < activeTeamCount; ++i) {
        const TeamId teamId = safeActiveTeamIdAt(career, i);
        const Team* team = career.getTeamById(teamId);
        if (!team) {
            continue;
        }

        snapshots.activeTeamIds.push_back(teamId);
        vector<int> suspensions;
        suspensions.reserve(team->players.size());
        for (const auto& player : team->players) suspensions.push_back(player.matchesSuspended);
        snapshots.suspensionsBefore.emplace(teamId, std::move(suspensions));
        snapshots.pointsBefore.emplace(teamId, team->points);
    }

    return snapshots;
}

void simulateMatchesPhase(Career& career,
                          const vector<pair<int, int>>& matches,
                          const unordered_map<TeamId, int>& pointsBefore,
                          int& myTeamPointsDelta,
                          bool& cupWeek) {
    CareerService(career).simulateWeekMatches(matches, pointsBefore, myTeamPointsDelta);
    maybeInvokeIdle();

    for (const auto& division : career.divisions) {
        CareerService(career).simulateBackgroundDivisionWeek(division.id);
        maybeInvokeIdle();
    }

    cupWeek = career.cupActive &&
              (career.currentWeek == 1 || career.currentWeek % 4 == 0 ||
               career.currentWeek == static_cast<int>(career.schedule.size()));
    if (cupWeek) {
        CareerService(career).simulateSeasonCupRound();
        maybeInvokeIdle();
    }
}

void updateFitnessPhase(Career& career,
                        const vector<TeamId>& activeTeamIds,
                        const unordered_map<TeamId, vector<int>>& suspensionsBefore,
                        bool cupWeek) {
    CareerService(career).updatePlayerPhysicalState(activeTeamIds, suspensionsBefore, cupWeek);
    maybeInvokeIdle();
}

void updateGameplaySystemsPhase(Career& career, const vector<pair<int, int>>& matches, int myTeamPointsDelta) {
    if (!career.myTeam) return;

    StressEvent stressEvent;
    if (myTeamPointsDelta >= 3) {
        stressEvent.type = "win";
        stressEvent.stressImpact = -10;
        stressEvent.description = "Victoria conseguida";
    } else if (myTeamPointsDelta == 1) {
        stressEvent.type = "draw";
        stressEvent.stressImpact = -3;
        stressEvent.description = "Empate";
    } else {
        stressEvent.type = "loss";
        stressEvent.stressImpact = +15;
        stressEvent.description = "Derrota sufrida";
    }
    updateManagerStress(career.managerStress, stressEvent);

    const bool hadWin = myTeamPointsDelta >= 3;
    const bool isKeyWeek = career.currentWeek % 4 == 0;
    updateCliqueDynamics(career.dressingRoomDynamics, hadWin, isKeyWeek);

    career.debtStatus = calculateDebtStatus(
        career.myTeam->budget,
        career.myTeam->debt,
        max(1LL, career.myTeam->sponsorWeekly + static_cast<long long>(career.myTeam->fanBase) * 2500LL));
    applyFinancialSanctions(career.debtStatus);

    const TeamId managedTeamId = career.getTeamIdFor(career.myTeam);
    for (const auto& match : matches) {
        const career_week_matches::ScheduledMatchRef fixture = career_week_matches::scheduledMatchRef(career, match);
        Team* home = fixture.home.team;
        Team* away = fixture.away.team;
        if (!fixture.valid()) continue;

        if (fixture.home.id == managedTeamId) {
            RivalryRecord* rivalryRec = getRivalryRecord(career.rivalryDynamics, home->name, away->name);
            if (rivalryRec) rivalryRec->lastMeetingWeek = career.currentWeek;
            const int intensity = getRivalryIntensity(career.rivalryDynamics, home->name, away->name);
            if (intensity > 70) {
                career.managerStress.pressureIntensity = min(100, career.managerStress.pressureIntensity + 3);
            }
        } else if (fixture.away.id == managedTeamId) {
            RivalryRecord* rivalryRec = getRivalryRecord(career.rivalryDynamics, away->name, home->name);
            if (rivalryRec) rivalryRec->lastMeetingWeek = career.currentWeek;
        }
    }
}

void generateNewsPhase(Career& career, const vector<pair<int, int>>& matches, int myTeamPointsDelta) {
    updateManagerGameState(career, myTeamPointsDelta);
    maybeInvokeIdle();

    updateGameplaySystemsPhase(career, matches, myTeamPointsDelta);

    if (career.myTeam) ensureTeamIdentity(*career.myTeam);
    CareerService(career).dispatchWeeklyStaffBriefing();
    maybeInvokeIdle();
    weeklyDashboard(career);
    maybeInvokeIdle();
    applyClubEvent(career);
    maybeInvokeIdle();

    generateWeeklyNarrative(career, myTeamPointsDelta);
    maybeInvokeIdle();
}

void advanceCalendarPhase(Career& career) {
    handleManagerStatus(career);
    career.currentWeek++;
    checkAchievements(career);
    career.syncActiveHumanManager();

    int milestoneWeek = -999;
    if (career_events::checkCareerMilestone(career, milestoneWeek)) {
        string description = career_events::GetMilestoneDescription(milestoneWeek, career);
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::CareerMilestone,
            "Hito alcanzado",
            description);
        emitUiMessage("[Hito] " + description);
    }

    if (career.currentWeek > static_cast<int>(career.schedule.size())) {
        emitSeasonTransitionSummary(endSeason(career));
    }
}

}  // anonymous namespace
// === END SEPARATION OF CONCERNS ===

// simulateCareerWeek is intentionally organized into discrete phases to keep weekly simulation
// behavior predictable and easy to maintain. Each phase has a single responsibility:
// 1. simulateMatchesPhase: resuelve los partidos de la jornada.
// 2. updateFitnessPhase: actualiza forma, lesiones y suspensiones.
// 3. processTransfersPhase: ejecuta ofertas y movimientos pendientes.
// 4. applyFinancesPhase: aplica ingresos y gastos semanales.
// 5. generateNewsPhase: crea noticias y eventos derivados de la semana.
// 6. advanceCalendarPhase: avanza la semana y maneja transicion de temporada.
//
// Esto es una mejora de claridad; no altera la lógica de simulacion ya existente.
void simulateCareerWeek(Career& career) {
    if (career.getActiveTeamCount() == 0 || career.schedule.empty()) {
        emitUiMessage("No hay calendario disponible.");
        return;
    }
    if (career.currentWeek < 1) {
        emitUiMessage("Semana inválida detectada; reiniciando a semana 1.");
        career.currentWeek = 1;
    }
    if (career.currentWeek > static_cast<int>(career.schedule.size())) {
        emitSeasonTransitionSummary(endSeason(career));
        return;
    }

    emitUiMessage("");
    emitUiMessage("Simulando semana " + to_string(career.currentWeek) + "...");
    career.leagueTable.sortTable();

    WeekSimulationSnapshots snapshots = captureWeekSnapshots(career);
    const auto& matches = career.schedule[static_cast<size_t>(career.currentWeek - 1)];
    int myTeamPointsDelta = 0;
    bool cupWeek = career.cupActive &&
                   (career.currentWeek == 1 || career.currentWeek % 4 == 0 ||
                    career.currentWeek == static_cast<int>(career.schedule.size()));

    simulateMatchesPhase(career, matches, snapshots.pointsBefore, myTeamPointsDelta, cupWeek);
    updateFitnessPhase(career, snapshots.activeTeamIds, snapshots.suspensionsBefore, cupWeek);
    processTransfersPhase(career);
    applyFinancesPhase(career, snapshots.pointsBefore);
    generateNewsPhase(career, matches, myTeamPointsDelta);
    advanceCalendarPhase(career);
}
