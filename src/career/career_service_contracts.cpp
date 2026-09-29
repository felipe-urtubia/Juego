#include "career/career_service.h"
#include "career/career_runtime.h"
#include "career/team_management.h"
#include "transfers/negotiation_system.h"
#include "utils.h"

#include <algorithm>
#include <cstddef>
#include <string>

using namespace std;
namespace {

bool shouldAutoRenewContract(const Team& team, const Player& player, long long demandedWage) {
    if (team.budget < demandedWage * 6) return false;
    if (player.wantsToLeave && player.happiness < 45) return false;
    return player.skill >= team.getAverageSkill() - 5 || team.players.size() <= 18;
}

}  // namespace

void CareerService::processContractUpdates() {
    Career& career = career_;
    for (auto& teamRef : career.allTeams) {
        Team* team = &teamRef;
        ensureTeamIdentity(*team);
        for (size_t i = 0; i < team->players.size();) {
            Player& player = team->players[i];
            if (player.contractWeeks > 0) player.contractWeeks--;
            if (player.contractWeeks > 0) {
                ++i;
                continue;
            }

            if (team == career.myTeam) {
                long long demandedWage = max(player.wage, wageDemandFor(player));
                if (player.wantsToLeave) demandedWage = demandedWage * 120 / 100;
                if (promiseAtRisk(player, career.currentWeek)) demandedWage = demandedWage * 108 / 100;
                if (player.skill >= team->getAverageSkill()) demandedWage = demandedWage * 110 / 100;
                int demandedWeeks = randInt(78, 182);
                long long demandedClause = max(player.value * 2,
                                               demandedWage * (player.skill >= team->getAverageSkill() ? 48 : 40));
                emitUiMessage("");
                emitUiMessage("Contrato expirado: " + player.name);
                emitUiMessage("Demanda renovar por " + to_string(demandedWeeks) +
                              " semanas | Salario $" + to_string(demandedWage) +
                              " | Clausula $" + to_string(demandedClause));
                if (player.wantsToLeave) {
                    emitUiMessage(player.name + " esta inquieto por su rol en el club y exige mejores condiciones.");
                }
                if (promiseAtRisk(player, career.currentWeek)) {
                    emitUiMessage("Advertencia: " + player.name + " siente que su promesa de rol fue incumplida.");
                }

                bool renew = shouldAutoRenewContract(*team, player, demandedWage);
                if (contractRenewalDecisionCallback()) {
                    renew = contractRenewalDecisionCallback()(career, *team, player, demandedWage, demandedWeeks,
                                                              demandedClause);
                }

                if (renew) {
                    if (team->budget < demandedWage * 6) {
                        emitUiMessage("No hay margen salarial suficiente. " + player.name + " deja el club.");
                        career.addNews(player.name + " deja el club tras no acordar renovacion.");
                        team_mgmt::detachPlayerFromSelections(*team, player.name);
                        team_mgmt::applyDepartureShock(*team, player);
                        team->players.erase(team->players.begin() + static_cast<long long>(i));
                    } else {
                        player.contractWeeks = demandedWeeks;
                        player.wage = demandedWage;
                        player.releaseClause = demandedClause;
                        player.wantsToLeave = false;
                        player.happiness = clampInt(player.happiness + 6, 1, 99);
                        emitUiMessage("Renovado. Nuevo salario $" + to_string(player.wage));
                        career.addNews(player.name + " renueva contrato en " + team->name + ".");
                        ++i;
                    }
                } else {
                    emitUiMessage(player.name + " deja el club.");
                    career.addNews(player.name + " deja el club al finalizar su contrato.");
                    team_mgmt::detachPlayerFromSelections(*team, player.name);
                    team_mgmt::applyDepartureShock(*team, player);
                    team->players.erase(team->players.begin() + static_cast<long long>(i));
                }
            } else {
                if (team->budget > player.wage * 8 &&
                    randInt(1, 100) <= (promiseAtRisk(player, career.currentWeek) ? 45 : 70)) {
                    player.contractWeeks = randInt(52, 156);
                    player.wage = static_cast<long long>(player.wage * (1.05 + randInt(0, 15) / 100.0));
                    player.releaseClause = max(player.value * 2, player.wage * 45);
                    ++i;
                } else {
                    team_mgmt::detachPlayerFromSelections(*team, player.name);
                    team->players.erase(team->players.begin() + static_cast<long long>(i));
                }
            }
        }
    }
}
