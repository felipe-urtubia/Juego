#include "career/app_services.h"

#include "ai/ai_transfer_manager.h"
#include "career/team_management.h"
#include "career/career_reports.h"
#include "career/world_state_service.h"
#include "engine/models.h"
#include "simulation/player_condition.h"
#include "transfers/negotiation_system.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

string resolveAssignmentRegion(const Team& team, const string& region) {
    if (!region.empty()) return region;
    if (!team.scoutingRegions.empty()) return team.scoutingRegions.front();
    if (!team.youthRegion.empty()) return team.youthRegion;
    return "Todas";
}

string assignmentPriorityLabel(const Team& team, const string& focusPos) {
    return normalizePosition(focusPos) == detectScoutingNeed(team) ? "Urgente" : "Seguimiento";
}

ServiceResult failure(const string& message) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(message);
    return result;
}

bool hasScoutingCoverage(const Team& team, const string& region) {
    if (region.empty() || region == "Todas") return true;
    return find(team.scoutingRegions.begin(), team.scoutingRegions.end(), region) != team.scoutingRegions.end();
}

string scoutingCoverageLabel(const Team& team, const string& region) {
    if (region.empty() || region == "Todas") return "Cobertura abierta";
    return hasScoutingCoverage(team, region) ? "Region cubierta" : "Cobertura baja";
}

string availabilityLabel(const Player& player) {
    if (player.contractWeeks <= 12) return "Contrato corto";
    if (player.wantsToLeave || player.happiness <= 42) return "Abierto a salir";
    if (player.releaseClause <= player.value * 2) return "Clausula accesible";
    return "Negociacion dura";
}

string agentProfileLabel(const Player& player) {
    const int difficulty = agentDifficulty(player);
    if (difficulty >= 72) return "Agente duro";
    if (difficulty >= 58) return "Agente exigente";
    return "Agente manejable";
}

string scoutingReportStage(int confidence, bool coveredRegion) {
    if (confidence >= 84 && coveredRegion) return "Informe completo";
    if (confidence >= 68) return coveredRegion ? "Seguimiento avanzado" : "Seguimiento parcial";
    return coveredRegion ? "Primer vistazo" : "Radar lejano";
}

string scoutingHiddenRiskLabel(const Player& player, int confidence) {
    int risk = 0;
    if (player.consistency <= 48) risk += 2;
    if (player.professionalism <= 45) risk += 2;
    if (player.injuryHistory >= 2) risk += 2;
    if (player.bigMatches <= 45) risk += 1;
    if (confidence < 60) risk += 1;
    if (risk >= 5) return "Riesgo alto";
    if (risk >= 3) return "Riesgo medio";
    return "Riesgo controlado";
}

int scoutingAssignmentBoost(const Career& career, const Team& seller, const string& focusPos) {
    int boost = 0;
    const string normalizedFocus = normalizePosition(focusPos);
    for (const auto& assignment : career.scoutingAssignments) {
        if (!assignment.region.empty() && assignment.region != "Todas" && assignment.region != seller.youthRegion) continue;
        if (!assignment.focusPosition.empty() && normalizePosition(assignment.focusPosition) != normalizedFocus) continue;
        boost = max(boost, assignment.knowledgeLevel / 5 + max(0, 4 - assignment.weeksRemaining) * 2);
    }
    return clampInt(boost, 0, 24);
}

void appendScoutInbox(Career& career, const string& note, const string& inboxLine) {
    career.scoutInbox.push_back(note);
    if (career.scoutInbox.size() > 40) {
        career.scoutInbox.erase(career.scoutInbox.begin(),
                                career.scoutInbox.begin() + static_cast<long long>(career.scoutInbox.size() - 40));
    }
    career.addInboxItem(inboxLine, "Scouting");
}

}  // namespace

ScoutingSessionResult runScoutingSessionService(Career& career, const string& region, const string& focusPos) {
    ScoutingSessionResult session;
    if (!career.myTeam) {
        session.service = failure("No hay una carrera activa.");
        return session;
    }
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    const int analystBonus = max(0, team.performanceAnalyst - 55) / 6;
    long long scoutCost = max(3000LL, 9000LL - team.scoutingChief * 50LL - analystBonus * 120LL);
    if (team.budget < scoutCost) {
        session.service = failure("Presupuesto insuficiente para ojeo.");
        return session;
    }
    session.resolvedRegion = region.empty() ? (team.scoutingRegions.empty() ? string("Todas") : team.scoutingRegions.front()) : region;
    session.resolvedFocusPosition = normalizePosition(focusPos);
    if (session.resolvedFocusPosition == "N/A" || session.resolvedFocusPosition.empty()) {
        session.resolvedFocusPosition = detectScoutingNeed(team);
    }

    vector<pair<Team*, int>> reports;
    for (auto& club : career.allTeams) {
        if (&club == career.myTeam) continue;
        ensureTeamIdentity(club);
        const bool coveredRegion = hasScoutingCoverage(team, club.youthRegion);
        if (session.resolvedRegion != "Todas" && club.youthRegion != session.resolvedRegion) continue;
        if (session.resolvedRegion == "Todas" && !coveredRegion && team.scoutingChief < 62) continue;
        for (size_t i = 0; i < club.players.size(); ++i) {
            const Player& player = club.players[i];
            if (player.onLoan) continue;
            if (positionFitScore(player, session.resolvedFocusPosition) < 70) continue;
            reports.push_back({&club, static_cast<int>(i)});
        }
    }
    if (reports.empty()) {
        session.service = failure("No se encontraron jugadores para ese informe.");
        return session;
    }
    sort(reports.begin(), reports.end(), [&](const auto& left, const auto& right) {
        const Team& clubA = *left.first;
        const Team& clubB = *right.first;
        const Player& a = clubA.players[static_cast<size_t>(left.second)];
        const Player& b = clubB.players[static_cast<size_t>(right.second)];
        int fitA = a.potential + a.professionalism / 2 + a.currentForm / 2 +
                   positionFitScore(a, session.resolvedFocusPosition) + (hasScoutingCoverage(team, clubA.youthRegion) ? 6 : 0) +
                   scoutingAssignmentBoost(career, clubA, session.resolvedFocusPosition);
        int fitB = b.potential + b.professionalism / 2 + b.currentForm / 2 +
                   positionFitScore(b, session.resolvedFocusPosition) + (hasScoutingCoverage(team, clubB.youthRegion) ? 6 : 0) +
                   scoutingAssignmentBoost(career, clubB, session.resolvedFocusPosition);
        if (fitA != fitB) return fitA > fitB;
        return a.skill > b.skill;
    });
    if (reports.size() > 5) reports.resize(5);
    int error = clampInt(14 - team.scoutingChief / 10 - analystBonus / 2, 2, 10);
    team.budget -= scoutCost;
    session.scoutingCost = scoutCost;
    session.service.ok = true;
    session.service.messages.push_back("Scouting completado en " + session.resolvedRegion +
                                       " con foco " + session.resolvedFocusPosition +
                                       ". Costo " + formatMoneyValue(scoutCost) + ".");
    for (const auto& report : reports) {
        Team* club = report.first;
        const Player& player = club->players[static_cast<size_t>(report.second)];
        int estSkillLo = clampInt(player.skill - error, 1, 99);
        int estSkillHi = clampInt(player.skill + error, 1, 99);
        int estPotLo = clampInt(player.potential - error, player.skill, 99);
        int estPotHi = clampInt(player.potential + error, player.skill, 99);
        int fitScore = positionFitScore(player, session.resolvedFocusPosition);
        string fitLabel = fitScore >= 90 ? "ajuste alto" : (fitScore >= 75 ? "ajuste medio" : "ajuste parcial");
        const bool coveredRegion = hasScoutingCoverage(team, club->youthRegion);
        const int regionalFamiliarity = club->youthRegion == team.youthRegion ? 8 : 0;
        const int networkBonus = coveredRegion ? world_state_service::worldRuleValue("scouting_network_bonus", 8) : -6;
        const int assignmentBoost = scoutingAssignmentBoost(career, *club, session.resolvedFocusPosition);
        const int confidence =
            clampInt(26 + team.scoutingChief / 3 + analystBonus + (12 - error) * 3 + regionalFamiliarity + networkBonus +
                         max(0, fitScore - 70) / 4 - max(0, 55 - player.professionalism) / 6 + assignmentBoost,
                     20, 94);
        const int readinessScore = player_condition::readinessScore(player, *club);
        const int medicalRisk = max(player_condition::workloadRisk(player, *club),
                                    player_condition::relapseRisk(player, *club));
        string recommendation = "Seguimiento";
        if (medicalRisk >= 72) {
            recommendation = "Revisar salud antes de avanzar";
        } else if (confidence >= 82 && assignmentBoost >= 10 && fitScore >= 85) {
            recommendation = "Objetivo listo para oferta";
        } else if (player.potential >= team.getAverageSkill() + 8 && fitScore >= 85 && confidence >= 68 &&
                   readinessScore >= 58) {
            recommendation = "Objetivo prioritario";
        } else if (player.age <= 21 && player.potential - player.skill >= 10) {
            recommendation = "Proyecto a seguir";
        } else if (player.contractWeeks <= 16) {
            recommendation = "Oportunidad de mercado";
        } else if (player.consistency >= 66 && player.currentForm >= 60 && readinessScore >= 60) {
            recommendation = "Listo para competir";
        }
        const string upsideBand =
            (player.potential - player.skill >= 12) ? "Techo alto"
                                                    : (player.potential - player.skill >= 6 ? "Margen medio" : "Techo corto");
        const long long salaryExpectation = max(player.wage, wageDemandFor(player));
        string riskLabel = "riesgo controlado";
        if (medicalRisk >= 72) riskLabel = "riesgo fisico alto";
        else if (medicalRisk >= 56) riskLabel = "riesgo fisico medio";
        else if (player.happiness <= 44) riskLabel = "riesgo de vestuario";
        else if (confidence <= 45) riskLabel = "informe verde";

        ScoutingCandidate candidate;
        candidate.playerName = player.name;
        candidate.clubName = club->name;
        candidate.region = club->youthRegion;
        candidate.position = player.position;
        candidate.preferredFoot = player.preferredFoot;
        candidate.fitLabel = fitLabel;
        candidate.formLabel = playerFormLabel(player);
        candidate.reliabilityLabel = playerReliabilityLabel(player);
        candidate.personalityLabel = personalityLabel(player);
        candidate.recommendation = recommendation;
        candidate.upsideBand = upsideBand;
        candidate.networkFitLabel = scoutingCoverageLabel(team, club->youthRegion);
        candidate.availabilityLabel = availabilityLabel(player);
        candidate.agentLabel = agentProfileLabel(player);
        candidate.reportStage = scoutingReportStage(confidence, hasScoutingCoverage(team, club->youthRegion));
        if (assignmentBoost >= 10) candidate.reportStage += " | dossier regional";
        candidate.hiddenRiskLabel = scoutingHiddenRiskLabel(player, confidence);
        candidate.knowledgeLevel = clampInt(confidence + (hasScoutingCoverage(team, club->youthRegion) ? 8 : -4), 25, 99);
        candidate.secondaryPositions = player.secondaryPositions;
        candidate.traits = player.traits;
        candidate.estimatedSkillMin = estSkillLo;
        candidate.estimatedSkillMax = estSkillHi;
        candidate.estimatedPotentialMin = estPotLo;
        candidate.estimatedPotentialMax = estPotHi;
        candidate.fitScore = fitScore;
        candidate.bigMatches = player.bigMatches;
        candidate.confidence = confidence;
        candidate.readinessScore = readinessScore;
        candidate.medicalRisk = medicalRisk;
        candidate.marketValue = player.value;
        candidate.salaryExpectation = salaryExpectation;
        candidate.riskLabel = riskLabel;
        session.candidates.push_back(candidate);

        string note = player.name + " | " + club->name + " | " + club->youthRegion + " | Hab " +
                      to_string(estSkillLo) + "-" + to_string(estSkillHi) + " | Pot " +
                      to_string(estPotLo) + "-" + to_string(estPotHi) +
                      " | Pie " + player.preferredFoot +
                      " | Sec " + (player.secondaryPositions.empty() ? string("-") : joinStringValues(player.secondaryPositions, "/")) +
                      " | Forma " + playerFormLabel(player) +
                      " | Fiabilidad " + playerReliabilityLabel(player) +
                      " | Cobertura " + candidate.networkFitLabel +
                      " | " + candidate.reportStage +
                      " | Conf " + to_string(confidence) +
                      " | Conocimiento " + to_string(candidate.knowledgeLevel) +
                      " | Listo " + to_string(readinessScore) +
                      " | Riesgo medico " + to_string(medicalRisk) +
                      " | Valor " + formatMoneyValue(player.value) +
                      " | Salario esp " + formatMoneyValue(salaryExpectation) +
                      " | Disponibilidad " + candidate.availabilityLabel +
                      " | " + candidate.agentLabel +
                      " | " + recommendation +
                      " | " + upsideBand +
                      " | Riesgo visible " + riskLabel +
                      " | Riesgo oculto " + candidate.hiddenRiskLabel +
                      (assignmentBoost > 0 ? " | Dossier " + to_string(assignmentBoost) : string()) +
                      " | Rasgos " + joinStringValues(player.traits, ", ") +
                      " | Perfil " + personalityLabel(player);
        session.service.messages.push_back("- " + note);
        appendScoutInbox(career, note, "Informe nuevo de " + player.name + " en " + club->name + ".");
    }
    career.addNews("El scouting completa un informe en la region " + session.resolvedRegion + " para " + team.name + ".");
    return session;
}

ServiceResult scoutPlayersService(Career& career, const string& region, const string& focusPos) {
    return runScoutingSessionService(career, region, focusPos).service;
}

ServiceResult createScoutingAssignmentService(Career& career, const string& region, const string& focusPos, int durationWeeks) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    ScoutingAssignment assignment;
    assignment.region = resolveAssignmentRegion(team, region);
    assignment.focusPosition = normalizePosition(focusPos);
    if (assignment.focusPosition == "N/A" || assignment.focusPosition.empty()) assignment.focusPosition = detectScoutingNeed(team);
    assignment.priority = assignmentPriorityLabel(team, assignment.focusPosition);
    assignment.weeksRemaining = clampInt(durationWeeks, 2, 6);
    assignment.knowledgeLevel = clampInt(team.scoutingChief / 3 + max(0, team.performanceAnalyst - 50) / 5, 18, 58);

    for (const auto& current : career.scoutingAssignments) {
        if (current.region == assignment.region && current.focusPosition == assignment.focusPosition) {
            return failure("Ya existe una asignacion activa para esa zona y posicion.");
        }
    }

    const long long setupCost = max(1200LL, 3200LL - team.scoutingChief * 12LL - team.performanceAnalyst * 8LL);
    if (team.budget < setupCost) return failure("Presupuesto insuficiente para abrir una asignacion de scouting.");
    team.budget -= setupCost;
    career.scoutingAssignments.push_back(assignment);
    career.addInboxItem("Nueva asignacion: " + assignment.region + " | foco " + assignment.focusPosition +
                        " | prioridad " + assignment.priority + ".", "Scouting");
    career.addNews("El scouting abre seguimiento prolongado en " + assignment.region + " con foco " + assignment.focusPosition + ".");

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Asignacion creada: " + assignment.region + " | foco " + assignment.focusPosition +
                              " | prioridad " + assignment.priority +
                              " | duracion " + to_string(assignment.weeksRemaining) +
                              " sem | inversion " + formatMoneyValue(setupCost) + ".");
    return result;
}

ServiceResult shortlistPlayerService(Career& career,
                                     const string& sellerTeamName,
                                     const string& playerName) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team* seller = career.findTeamByName(sellerTeamName);
    if (!seller || seller == career.myTeam) return failure("No se encontro el club del jugador.");
    int sellerIdx = team_mgmt::playerIndexByName(*seller, playerName);
    if (sellerIdx < 0) return failure("No se encontro el jugador seleccionado.");
    const Player& player = seller->players[static_cast<size_t>(sellerIdx)];

    string entry = seller->name + "|" + player.name;
    if (find(career.scoutingShortlist.begin(), career.scoutingShortlist.end(), entry) != career.scoutingShortlist.end()) {
        return failure("Ese jugador ya esta en la shortlist.");
    }
    career.scoutingShortlist.push_back(entry);
    if (career.scoutingShortlist.size() > 25) {
        career.scoutingShortlist.erase(career.scoutingShortlist.begin(),
                                       career.scoutingShortlist.begin() +
                                           static_cast<long long>(career.scoutingShortlist.size() - 25));
    }
    career.addNews("Scouting agrega a " + player.name + " (" + seller->name + ") a la shortlist.");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back(player.name + " agregado a la shortlist.");
    return result;
}

ServiceResult followShortlistService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    if (career.scoutingShortlist.empty()) return failure("No hay jugadores en la shortlist.");

    Team& team = *career.myTeam;
    long long cost = max(4000LL, 14000LL - team.scoutingChief * 60LL);
    if (team.budget < cost) return failure("Presupuesto insuficiente para seguimiento.");
    team.budget -= cost;

    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Seguimiento de shortlist completado. Costo " + formatMoneyValue(cost) + ".");
    int error = clampInt(8 - team.scoutingChief / 18, 1, 5);
    const ClubTransferStrategy strategy = ai_transfer_manager::buildClubTransferStrategy(career, team);

    for (const auto& item : career.scoutingShortlist) {
        auto fields = splitByDelimiter(item, '|');
        if (fields.size() < 2) continue;
        Team* seller = career.findTeamByName(fields[0]);
        if (!seller) continue;
        int sellerIdx = team_mgmt::playerIndexByName(*seller, fields[1]);
        if (sellerIdx < 0) continue;
        const Player& player = seller->players[static_cast<size_t>(sellerIdx)];
        const TransferTarget target = ai_transfer_manager::evaluateTarget(career, team, *seller, player, strategy);
        long long salaryExpectation = max(player.wage, wageDemandFor(player));
        string note = "[Seguimiento] " + player.name + " | " + seller->name +
                      " | Hab " + to_string(clampInt(player.skill - error, 1, 99)) + "-" +
                      to_string(clampInt(player.skill + error, 1, 99)) +
                      " | Pot " + to_string(clampInt(player.potential - error, player.skill, 99)) + "-" +
                      to_string(clampInt(player.potential + error, player.skill, 99)) +
                      " | Contrato " + to_string(player.contractWeeks) +
                      " | Valor " + formatMoneyValue(player.value) +
                      " | Salario esp " + formatMoneyValue(salaryExpectation) +
                      " | Pie " + player.preferredFoot +
                      " | Sec " + (player.secondaryPositions.empty() ? string("-") : joinStringValues(player.secondaryPositions, "/")) +
                      " | Forma " + playerFormLabel(player) +
                      " | Fiabilidad " + playerReliabilityLabel(player) +
                      " | Conf " + to_string(target.scoutingConfidence) +
                      " | Listo " + to_string(target.readinessScore) +
                      " | Riesgo medico " + to_string(target.medicalRisk) +
                      " | Mercado " + formatMoneyValue(target.expectedFee + target.expectedAgentFee) +
                      " | Rasgos " + joinStringValues(player.traits, ", ") +
                      " | Perfil " + personalityLabel(player) +
                      " | Nota " + target.scoutingNote;
        result.messages.push_back("- " + note);
        career.scoutInbox.push_back(note);
    }

    if (career.scoutInbox.size() > 40) {
        career.scoutInbox.erase(career.scoutInbox.begin(),
                                career.scoutInbox.begin() + static_cast<long long>(career.scoutInbox.size() - 40));
    }
    career.addInboxItem("Se actualizan " + to_string(career.scoutingShortlist.size()) + " objetivos de shortlist.", "Scouting");
    career.addNews("El scouting actualiza informes de la shortlist de " + team.name + ".");
    return result;
}

std::vector<std::string> listYouthRegionsService() {
    return world_state_service::listConfiguredScoutingRegions();
}
