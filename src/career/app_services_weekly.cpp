#include "career/app_services.h"

#include "career/career_reports.h"
#include "career/career_support.h"
#include "career/inbox_service.h"
#include "career/medical_service.h"
#include "career/team_management.h"
#include "engine/debt_system.h"
#include "engine/manager_stress.h"
#include "engine/models.h"
#include "utils/utils.h"

#include <algorithm>
#include <iterator>

using namespace std;

namespace {
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


ServiceResult failure(const string& message) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(message);
    return result;
}
}  // namespace

bool consumeLatestWeeklyDigestService(Career& career) {
    for (auto it = career.managerInbox.rbegin(); it != career.managerInbox.rend(); ++it) {
        if (it->find("[Centro semanal]") == string::npos) continue;
        career.managerInbox.erase(next(it).base());
        return true;
    }
    return false;
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




