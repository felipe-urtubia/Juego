#include "models.h"

#include "utils/utils.h"

#include <algorithm>
#include <sstream>
#include <vector>

using namespace std;

namespace {

void parseFormation(const string& formation, int& defCount, int& midCount, int& fwdCount) {
    defCount = 4;
    midCount = 4;
    fwdCount = 2;
    string s = trim(formation);
    if (s.empty()) return;
    stringstream ss(s);
    string token;
    vector<int> nums;
    while (getline(ss, token, '-')) {
        token = trim(token);
        if (token.empty()) continue;
        try {
            nums.push_back(stoi(token));
        } catch (...) {
        }
    }
    if (nums.size() == 3) {
        defCount = nums[0];
        midCount = nums[1];
        fwdCount = nums[2];
    }
    int total = 1 + defCount + midCount + fwdCount;
    if (total != 11) {
        defCount = 4;
        midCount = 4;
        fwdCount = 2;
    }
}

bool isAvailableForSelection(const Player& player) {
    return !player.injured && player.matchesSuspended <= 0;
}

int selectionScore(const Team& team, int idx) {
    const Player& player = team.players[idx];
    int score = player.skill * 10 + player.fitness * 4 + player.currentForm * 3 + player.consistency * 2 +
                player.tacticalDiscipline - player.matchesPlayed;
    if (team.rotationPolicy == "Titulares") {
        score = player.skill * 12 + player.fitness * 2 + player.currentForm * 4 + player.consistency * 2 -
                player.matchesPlayed / 2;
    } else if (team.rotationPolicy == "Rotacion") {
        score = player.skill * 8 + player.fitness * 6 + player.currentForm * 2 + player.versatility -
                player.matchesPlayed * 2;
        if (player.age <= 21) score += max(0, player.potential - player.skill) * 3;
    }
    if (player.age <= 20 && player.potential >= player.skill + 8 && team.youthCoach >= 65) {
        score += 10 + max(0, player.potential - player.skill) * 2;
    }
    if (player.age <= 21 && player.fitness >= 72 && team.rotationPolicy != "Titulares") {
        score += 6;
    }
    if (player.wantsToLeave) score -= 12;
    if (!team.captain.empty() && player.name == team.captain) score += 10;
    if (!team.penaltyTaker.empty() && player.name == team.penaltyTaker) score += 6;
    return score;
}

}  // namespace

vector<int> Team::getStartingXIIndices() const {
    vector<int> xi;
    if (players.empty()) return xi;
    int defCount, midCount, fwdCount;
    parseFormation(formation, defCount, midCount, fwdCount);

    vector<bool> used(players.size(), false);
    auto tryPreferred = [&]() {
        for (const auto& preferred : preferredXI) {
            for (size_t i = 0; i < players.size(); ++i) {
                if (used[i]) continue;
                if (players[i].name != preferred) continue;
                if (!isAvailableForSelection(players[i])) continue;
                used[i] = true;
                xi.push_back(static_cast<int>(i));
                break;
            }
            if (xi.size() >= 11) return;
        }
    };
    auto pick = [&](const string& pos, int count) {
        if (xi.size() >= 11 || count <= 0) return;
        vector<int> candidates;
        for (size_t i = 0; i < players.size(); ++i) {
            if (used[i]) continue;
            if (!isAvailableForSelection(players[i])) continue;
            if (positionFitScore(players[i], pos) >= 40) candidates.push_back(static_cast<int>(i));
        }
        sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            int aScore = selectionScore(*this, a) + positionFitScore(players[a], pos) * 6;
            int bScore = selectionScore(*this, b) + positionFitScore(players[b], pos) * 6;
            if (aScore != bScore) return aScore > bScore;
            return players[a].skill > players[b].skill;
        });
        for (int i = 0; i < count && i < static_cast<int>(candidates.size()); ++i) {
            int idx = candidates[i];
            used[idx] = true;
            xi.push_back(idx);
            if (xi.size() >= 11) break;
        }
    };

    tryPreferred();
    pick("ARQ", 1);
    pick("DEF", defCount);
    pick("MED", midCount);
    pick("DEL", fwdCount);

    if (xi.size() < 11) {
        vector<int> candidates;
        for (size_t i = 0; i < players.size(); ++i) {
            if (!used[i] && isAvailableForSelection(players[i])) {
                candidates.push_back(static_cast<int>(i));
            }
        }
        sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            int aScore = selectionScore(*this, a);
            int bScore = selectionScore(*this, b);
            if (aScore != bScore) return aScore > bScore;
            return players[a].skill > players[b].skill;
        });
        for (int idx : candidates) {
            used[idx] = true;
            xi.push_back(idx);
            if (xi.size() >= 11) break;
        }
    }
    if (xi.size() < 11) {
        vector<int> candidates;
        for (size_t i = 0; i < players.size(); ++i) {
            if (!used[i] && players[i].matchesSuspended <= 0) candidates.push_back(static_cast<int>(i));
        }
        sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            int aScore = selectionScore(*this, a);
            int bScore = selectionScore(*this, b);
            if (aScore != bScore) return aScore > bScore;
            return players[a].skill > players[b].skill;
        });
        for (int idx : candidates) {
            used[idx] = true;
            xi.push_back(idx);
            if (xi.size() >= 11) break;
        }
    }
    return xi;
}

vector<int> Team::getBenchIndices(int count) const {
    vector<int> xi = getStartingXIIndices();
    vector<int> bench;
    if (count <= 0) return bench;
    vector<bool> used(players.size(), false);
    for (int idx : xi) {
        if (idx >= 0 && idx < static_cast<int>(players.size())) used[idx] = true;
    }

    auto tryPreferred = [&]() {
        for (const auto& preferred : preferredBench) {
            for (size_t i = 0; i < players.size(); ++i) {
                if (used[i]) continue;
                if (players[i].name != preferred) continue;
                if (!isAvailableForSelection(players[i])) continue;
                used[i] = true;
                bench.push_back(static_cast<int>(i));
                break;
            }
            if (static_cast<int>(bench.size()) >= count) return;
        }
    };

    tryPreferred();
    if (static_cast<int>(bench.size()) < count) {
        vector<int> candidates;
        for (size_t i = 0; i < players.size(); ++i) {
            if (used[i]) continue;
            if (!isAvailableForSelection(players[i])) continue;
            candidates.push_back(static_cast<int>(i));
        }
        sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            int aScore = selectionScore(*this, a);
            int bScore = selectionScore(*this, b);
            if (aScore != bScore) return aScore > bScore;
            return players[a].skill > players[b].skill;
        });
        for (int idx : candidates) {
            used[idx] = true;
            bench.push_back(idx);
            if (static_cast<int>(bench.size()) >= count) break;
        }
    }
    return bench;
}

int Team::getTotalAttack() const {
    auto xi = getStartingXIIndices();
    int total = 0;
    for (int idx : xi) total += players[idx].attack;
    return total;
}

int Team::getTotalDefense() const {
    auto xi = getStartingXIIndices();
    int total = 0;
    for (int idx : xi) total += players[idx].defense;
    return total;
}

int Team::getAverageSkill() const {
    auto xi = getStartingXIIndices();
    if (xi.empty()) return 0;
    int total = 0;
    for (int idx : xi) total += players[idx].skill;
    return total / static_cast<int>(xi.size());
}

int Team::getAverageStamina() const {
    const auto xi = getStartingXIIndices();

    if (xi.empty()) {
        return 0;
    }

    int total = 0;

    for (int idx : xi) {
        total += players[static_cast<size_t>(idx)].stamina;
    }

    return total / static_cast<int>(xi.size());
}

long long Team::getSquadValue() const {
    long long total = 0;
    for (const auto& p : players) total += p.value;
    return total;
}
