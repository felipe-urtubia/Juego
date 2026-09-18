#include "career/app_services.h"

#include "career/career_reports.h"
#include "career/team_management.h"
#include "competition/competition.h"
#include "engine/debt_system.h"
#include "engine/models.h"
#include "transfers/negotiation_system.h"
#include "transfers/transfer_market.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

ServiceResult failure(const string& message) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(message);
    return result;
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

}  // namespace

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
