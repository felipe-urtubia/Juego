#include "career/career_service.h"
#include "career/career_runtime.h"
#include "career/game_events_system.h"
#include "career/team_management.h"
#include "transfers/transfer_market.h"
#include "utils.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace std;
void CareerService::processIncomingOffers() {
    Career& career = career_;
    if (!career.myTeam || career.myTeam->players.size() <= 18) return;
    bool squadUnrest = false;
    for (const auto& player : career.myTeam->players) {
        if (player.wantsToLeave) {
            squadUnrest = true;
            break;
        }
    }
    if (randInt(1, 100) > (squadUnrest ? 50 : 32)) return;

    vector<int> candidates;
    for (size_t i = 0; i < career.myTeam->players.size(); ++i) {
        const Player& player = career.myTeam->players[i];
        if (!player.injured && player.contractWeeks > 8) {
            candidates.push_back(static_cast<int>(i));
            if (player.wantsToLeave) candidates.push_back(static_cast<int>(i));
        }
    }
    if (candidates.empty()) return;

    int index = candidates[static_cast<size_t>(randInt(0, static_cast<int>(candidates.size()) - 1))];
    Player& player = career.myTeam->players[static_cast<size_t>(index)];
    Team* bidder = nullptr;
    int bidderNeed = -100000;
    for (auto& club : career.allTeams) {
        if (&club == career.myTeam) continue;
        if (club.players.size() >= 26) continue;
        if (club.budget < player.value * 9 / 10) continue;
        int need = positionFitScore(player, transfer_market::weakestSquadPosition(club)) +
                   teamPrestigeScore(club) - teamPrestigeScore(*career.myTeam) / 2;
        if (areRivalClubs(club, *career.myTeam)) need += 8;
        if (need > bidderNeed) {
            bidderNeed = need;
            bidder = &club;
        }
    }
    if (!bidder) return;

    ensureTeamIdentity(*career.myTeam);
    ensureTeamIdentity(*bidder);
    long long maxOffer = max(player.value, player.value * (105 + randInt(0, 40)) / 100);
    if (areRivalClubs(*bidder, *career.myTeam)) maxOffer = maxOffer * 112 / 100;
    maxOffer = max(maxOffer, static_cast<long long>(player.value * (100 + teamPrestigeScore(*bidder) / 10) / 100));
    long long offer = max(player.value * 9 / 10, maxOffer * 85 / 100);

    emitUiMessage("");
    emitUiMessage("Oferta recibida por " + player.name + " desde " + bidder->name + ": $" + to_string(offer) +
                  " (tope estimado del mercado: $" + to_string(maxOffer) + ")" +
                  (areRivalClubs(*bidder, *career.myTeam) ? " [rival directo]" : ""));

    int choice = (offer >= maxOffer || (player.wantsToLeave && offer >= player.value)) ? 1 : 3;
    long long counter = 0;
    if (incomingOfferDecisionCallback()) {
        IncomingOfferDecision decision = incomingOfferDecisionCallback()(career, player, offer, maxOffer);
        if (decision.action >= 1 && decision.action <= 3) {
            choice = decision.action;
            counter = decision.counterOffer;
        }
    }

    if (choice == 1) {
        career.myTeam->budget += offer;
        bidder->budget = max(0LL, bidder->budget - offer);
        Player moved = player;
        moved.wantsToLeave = false;
        moved.onLoan = false;
        moved.parentClub.clear();
        moved.loanWeeksRemaining = 0;
        bidder->addPlayer(moved);
        emitUiMessage("Transferencia aceptada. " + player.name + " vendido a " + bidder->name + ".");
        career.addNews(player.name + " es vendido a " + bidder->name + " por $" + to_string(offer) + ".");
        career_events::EventNotificationSystem::recordEvent(
            career_events::EventType::TransferCompleted,
            "Transferencia completada",
            player.name + " fue vendido a " + bidder->name + " por $" + to_string(offer) + "."
        );
        team_mgmt::detachPlayerFromSelections(*career.myTeam, player.name);
        team_mgmt::applyDepartureShock(*career.myTeam, player);
        career.myTeam->players.erase(career.myTeam->players.begin() + index);
        return;
    }

    if (choice == 2) {
        if (counter <= maxOffer && bidder->budget >= counter) {
            career.myTeam->budget += counter;
            bidder->budget = max(0LL, bidder->budget - counter);
            Player moved = player;
            moved.wantsToLeave = false;
            moved.onLoan = false;
            moved.parentClub.clear();
            moved.loanWeeksRemaining = 0;
            bidder->addPlayer(moved);
            emitUiMessage("Contraoferta aceptada. " + player.name + " vendido a " + bidder->name + " por $" +
                          to_string(counter));
            career.addNews(player.name + " es vendido a " + bidder->name + " tras contraoferta por $" +
                           to_string(counter) + ".");
            career_events::EventNotificationSystem::recordEvent(
                career_events::EventType::TransferCompleted,
                "Transferencia completada",
                player.name + " fue vendido a " + bidder->name + " tras contraoferta por $" +
                    to_string(counter) + "."
            );
            team_mgmt::detachPlayerFromSelections(*career.myTeam, player.name);
            team_mgmt::applyDepartureShock(*career.myTeam, player);
            career.myTeam->players.erase(career.myTeam->players.begin() + index);
        } else {
            emitUiMessage("La contraoferta fue rechazada.");
        }
        return;
    }

    emitUiMessage("Oferta rechazada.");
}
