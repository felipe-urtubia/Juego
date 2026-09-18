#include "io/save_serialization.h"

#include "competition/league_registry.h"
#include "utils/utils.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

using namespace std;

namespace {

static constexpr int kCareerSaveVersion = 15;
static constexpr int kSaveSchemaVersion = 1;

vector<string> splitEscapedFields(const string& encoded, char delimiter);
int parseIntField(const string& text, int defaultValue);

struct SaveIntegritySnapshot {
    int schemaVersion = kSaveSchemaVersion;
    int teamCount = 0;
    int playerCount = 0;
    int historyCount = 0;
    int recordCount = 0;
    int pendingTransferCount = 0;
};

string canonicalizeDivisionValue(const string& raw) {
    return canonicalDivisionId(raw);
}

void rebuildCareerDivisions(Career& career) {
    career.divisions.clear();
    const auto& registered = listRegisteredDivisions();
    for (const auto& division : registered) {
        const bool hasTeams = std::any_of(career.allTeams.begin(), career.allTeams.end(), [&](const Team& team) {
            return canonicalizeDivisionValue(team.division) == division.id;
        });
        if (hasTeams) career.divisions.push_back(division);
    }
    for (const auto& team : career.allTeams) {
        const string divisionId = canonicalizeDivisionValue(team.division);
        if (divisionId.empty()) continue;
        const bool known = std::any_of(career.divisions.begin(), career.divisions.end(), [&](const DivisionInfo& division) {
            return division.id == divisionId;
        });
        if (!known) career.divisions.push_back({divisionId, "data/teams.json", divisionDisplay(divisionId)});
    }
}

SaveIntegritySnapshot buildIntegritySnapshot(const Career& career) {
    SaveIntegritySnapshot snapshot;
    snapshot.teamCount = static_cast<int>(career.allTeams.size());
    snapshot.historyCount = static_cast<int>(career.history.size());
    snapshot.recordCount = static_cast<int>(career.historicalRecords.size());
    snapshot.pendingTransferCount = static_cast<int>(career.pendingTransfers.size());
    for (const auto& team : career.allTeams) {
        snapshot.playerCount += static_cast<int>(team.players.size());
    }
    return snapshot;
}

string encodeIntegritySnapshot(const SaveIntegritySnapshot& snapshot) {
    return to_string(snapshot.schemaVersion) + "|" + to_string(snapshot.teamCount) + "|" +
           to_string(snapshot.playerCount) + "|" + to_string(snapshot.historyCount) + "|" +
           to_string(snapshot.recordCount) + "|" + to_string(snapshot.pendingTransferCount);
}

bool parseSaveSchemaLine(const string& line, int& schemaVersion) {
    const string lower = toLower(trim(line));
    if (lower.rfind("save_version", 0) == 0) {
        const size_t eq = lower.find('=');
        const string value = eq == string::npos ? trim(lower.substr(12)) : trim(lower.substr(eq + 1));
        schemaVersion = clampInt(parseIntField(value, kSaveSchemaVersion), 1, kSaveSchemaVersion);
        return true;
    }
    if (lower.rfind("save_schema ", 0) == 0) {
        schemaVersion = clampInt(parseIntField(trim(line.substr(12)), kSaveSchemaVersion), 1, kSaveSchemaVersion);
        return true;
    }
    return false;
}

bool parseIntegrityLine(const string& line, SaveIntegritySnapshot& snapshot) {
    if (line.rfind("INTEGRITY ", 0) != 0) return false;
    const vector<string> parts = splitEscapedFields(line.substr(10), '|');
    if (parts.size() < 6) return false;
    snapshot.schemaVersion = parseIntField(parts[0], kSaveSchemaVersion);
    snapshot.teamCount = parseIntField(parts[1], -1);
    snapshot.playerCount = parseIntField(parts[2], -1);
    snapshot.historyCount = parseIntField(parts[3], -1);
    snapshot.recordCount = parseIntField(parts[4], -1);
    snapshot.pendingTransferCount = parseIntField(parts[5], -1);
    return snapshot.teamCount >= 0 && snapshot.playerCount >= 0 && snapshot.historyCount >= 0 &&
           snapshot.recordCount >= 0 && snapshot.pendingTransferCount >= 0;
}

bool validateLoadedIntegrity(const Career& career, const SaveIntegritySnapshot& expected) {
    const SaveIntegritySnapshot actual = buildIntegritySnapshot(career);
    if (expected.schemaVersion > kSaveSchemaVersion) return false;
    return expected.teamCount == actual.teamCount &&
           expected.playerCount == actual.playerCount &&
           expected.historyCount == actual.historyCount &&
           expected.recordCount == actual.recordCount &&
           expected.pendingTransferCount == actual.pendingTransferCount;
}

bool validateLoadedCareerBasics(const Career& career) {
    if (career.currentSeason <= 0 || career.currentWeek <= 0) return false;
    if (career.allTeams.empty()) return false;
    if (!career.myTeam) return false;
    for (const auto& team : career.allTeams) {
        if (team.name.empty() || team.division.empty() || team.players.empty()) return false;
        if (team.budget < 0 || team.debt < 0) return false;  // Validate budget/debt non-negative
        for (const auto& player : team.players) {
            if (player.name.empty()) return false;
            if (player.wage < 0 || player.value < 0) return false;  // Validate player finances
        }
    }
    return true;
}

string escapeSaveField(const string& value) {
    string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\\':
            case '|':
            case ';':
            case '^':
            case '~':
            case '=':
                out.push_back('\\');
                out.push_back(c);
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

vector<string> splitEscapedFields(const string& encoded, char delimiter) {
    vector<string> fields;
    string current;
    bool escaping = false;
    for (char c : encoded) {
        if (escaping) {
            if (c == 'n') current.push_back('\n');
            else if (c == 'r') current.push_back('\r');
            else current.push_back(c);
            escaping = false;
            continue;
        }
        if (c == '\\') {
            escaping = true;
            continue;
        }
        if (c == delimiter) {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    if (escaping) current.push_back('\\');
    fields.push_back(current);
    return fields;
}

string joinEscapedFields(const vector<string>& fields, char delimiter) {
    string out;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i) out.push_back(delimiter);
        out += escapeSaveField(fields[i]);
    }
    return out;
}

int parseIntField(const string& text, int defaultValue = 0) {
    try {
        return stoi(text);
    } catch (...) {
        return defaultValue;
    }
}

long long parseLongField(const string& text, long long defaultValue = 0) {
    try {
        return stoll(text);
    } catch (...) {
        return defaultValue;
    }
}

bool parseBoolField(const string& text, bool defaultValue = false) {
    const string normalized = toLower(trim(text));
    if (normalized == "1" || normalized == "true") return true;
    if (normalized == "0" || normalized == "false") return false;
    return defaultValue;
}

string encodeHeadToHead(const vector<HeadToHeadRecord>& records) {
    vector<string> encoded;
    encoded.reserve(records.size());
    for (const auto& record : records) {
        encoded.push_back(joinEscapedFields({record.opponent, to_string(record.points)}, '='));
    }
    return joinEscapedFields(encoded, ';');
}

void decodeHeadToHead(const string& encoded, Team& team) {
    team.headToHead.clear();
    if (encoded.empty()) return;
    for (const string& token : splitEscapedFields(encoded, ';')) {
        if (token.empty()) continue;
        const vector<string> fields = splitEscapedFields(token, '=');
        if (fields.size() < 2 || fields[0].empty()) continue;
        team.headToHead.push_back({fields[0], parseIntField(fields[1])});
    }
}

string encodeStringList(const vector<string>& items) {
    if (items.empty()) return "";
    return joinEscapedFields(items, ';');
}

vector<string> decodeStringList(const string& encoded) {
    if (encoded.empty()) return {};
    return splitEscapedFields(encoded, ';');
}

string encodeMatchCenterSnapshot(const MatchCenterSnapshot& snapshot) {
    return encodeStringList({
        snapshot.competitionLabel,
        snapshot.opponentName,
        snapshot.venueLabel,
        to_string(snapshot.myGoals),
        to_string(snapshot.oppGoals),
        to_string(snapshot.myShots),
        to_string(snapshot.oppShots),
        to_string(snapshot.myShotsOnTarget),
        to_string(snapshot.oppShotsOnTarget),
        to_string(snapshot.myPossession),
        to_string(snapshot.oppPossession),
        to_string(snapshot.myCorners),
        to_string(snapshot.oppCorners),
        to_string(snapshot.mySubstitutions),
        to_string(snapshot.oppSubstitutions),
        to_string(snapshot.myExpectedGoalsTenths),
        to_string(snapshot.oppExpectedGoalsTenths),
        snapshot.weather,
        snapshot.dominanceSummary,
        snapshot.tacticalSummary,
        snapshot.fatigueSummary,
        snapshot.postMatchImpact
    });
}

MatchCenterSnapshot decodeMatchCenterSnapshot(const string& encoded) {
    MatchCenterSnapshot snapshot;
    vector<string> fields = decodeStringList(encoded);
    size_t idx = 0;
    if (idx < fields.size()) snapshot.competitionLabel = fields[idx++]; else return snapshot;
    if (idx < fields.size()) snapshot.opponentName = fields[idx++];
    if (idx < fields.size()) snapshot.venueLabel = fields[idx++];
    if (idx < fields.size()) snapshot.myGoals = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.oppGoals = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.myShots = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.oppShots = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.myShotsOnTarget = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.oppShotsOnTarget = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.myPossession = parseIntField(fields[idx++], 50);
    if (idx < fields.size()) snapshot.oppPossession = parseIntField(fields[idx++], 50);
    if (idx < fields.size()) snapshot.myCorners = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.oppCorners = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.mySubstitutions = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.oppSubstitutions = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.myExpectedGoalsTenths = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.oppExpectedGoalsTenths = parseIntField(fields[idx++]);
    if (idx < fields.size()) snapshot.weather = fields[idx++];
    if (idx < fields.size()) snapshot.dominanceSummary = fields[idx++];
    if (idx < fields.size()) snapshot.tacticalSummary = fields[idx++];
    if (idx < fields.size()) snapshot.fatigueSummary = fields[idx++];
    if (idx < fields.size()) snapshot.postMatchImpact = fields[idx++];
    return snapshot;
}

string encodeHistory(const vector<SeasonHistoryEntry>& entries) {
    vector<string> encodedEntries;
    encodedEntries.reserve(entries.size());
    for (const auto& entry : entries) {
        encodedEntries.push_back(joinEscapedFields({
            to_string(entry.season),
            entry.division,
            entry.club,
            to_string(entry.finish),
            entry.champion,
            entry.promoted,
            entry.relegated,
            entry.note
        }, '^'));
    }
    return joinEscapedFields(encodedEntries, '~');
}

vector<SeasonHistoryEntry> decodeHistory(const string& encoded) {
    vector<SeasonHistoryEntry> entries;
    if (encoded.empty()) return entries;
    for (const string& token : splitEscapedFields(encoded, '~')) {
        auto parts = splitEscapedFields(token, '^');
        if (parts.size() < 8) continue;
        SeasonHistoryEntry entry;
        entry.season = parseIntField(parts[0], 1);
        entry.division = canonicalizeDivisionValue(parts[1]);
        entry.club = parts[2];
        entry.finish = parseIntField(parts[3]);
        entry.champion = parts[4];
        entry.promoted = parts[5];
        entry.relegated = parts[6];
        entry.note = parts[7];
        entries.push_back(entry);
    }
    return entries;
}

string encodePendingTransfers(const vector<PendingTransfer>& entries) {
    vector<string> encodedEntries;
    encodedEntries.reserve(entries.size());
    for (const auto& entry : entries) {
        encodedEntries.push_back(joinEscapedFields({
            entry.playerName,
            entry.fromTeam,
            entry.toTeam,
            to_string(entry.effectiveSeason),
            to_string(entry.loanWeeks),
            to_string(entry.fee),
            to_string(entry.wage),
            to_string(entry.contractWeeks),
            to_string(entry.preContract ? 1 : 0),
            to_string(entry.loan ? 1 : 0),
            entry.promisedRole
        }, '^'));
    }
    return joinEscapedFields(encodedEntries, '~');
}

vector<PendingTransfer> decodePendingTransfers(const string& encoded) {
    vector<PendingTransfer> entries;
    if (encoded.empty()) return entries;
    for (const string& token : splitEscapedFields(encoded, '~')) {
        auto parts = splitEscapedFields(token, '^');
        if (parts.size() < 8) continue;
        PendingTransfer entry;
        entry.playerName = parts[0];
        entry.fromTeam = parts[1];
        entry.toTeam = parts[2];
        entry.effectiveSeason = parseIntField(parts[3], 1);
        entry.loanWeeks = parseIntField(parts[4]);
        entry.fee = parseLongField(parts[5]);
        if (parts.size() >= 10) {
            entry.wage = parseLongField(parts[6]);
            entry.contractWeeks = parseIntField(parts[7], 104);
            entry.preContract = parseBoolField(parts[8]);
            entry.loan = parseBoolField(parts[9]);
            entry.promisedRole = (parts.size() > 10) ? parts[10] : "Sin promesa";
        } else {
            entry.wage = 0;
            entry.contractWeeks = 104;
            entry.preContract = parseBoolField(parts[6]);
            entry.loan = parseBoolField(parts[7]);
            entry.promisedRole = "Sin promesa";
        }
        entries.push_back(entry);
    }
    return entries;
}

string encodePromises(const vector<SquadPromise>& promises) {
    vector<string> encodedEntries;
    encodedEntries.reserve(promises.size());
    for (const auto& promise : promises) {
        encodedEntries.push_back(joinEscapedFields({
            promise.subjectName,
            promise.category,
            promise.target,
            to_string(promise.issuedWeek),
            to_string(promise.deadlineWeek),
            to_string(promise.progress),
            to_string(promise.fulfilled ? 1 : 0),
            to_string(promise.failed ? 1 : 0)
        }, '^'));
    }
    return joinEscapedFields(encodedEntries, '~');
}

vector<SquadPromise> decodePromises(const string& encoded) {
    vector<SquadPromise> promises;
    if (encoded.empty()) return promises;
    for (const string& token : splitEscapedFields(encoded, '~')) {
        auto parts = splitEscapedFields(token, '^');
        if (parts.size() < 6) continue;
        SquadPromise promise;
        promise.subjectName = parts[0];
        promise.category = parts[1];
        promise.target = parts[2];
        promise.issuedWeek = parseIntField(parts[3]);
        promise.deadlineWeek = parseIntField(parts[4]);
        promise.progress = parseIntField(parts[5]);
        promise.fulfilled = parts.size() > 6 ? parseBoolField(parts[6]) : false;
        promise.failed = parts.size() > 7 ? parseBoolField(parts[7]) : false;
        promises.push_back(promise);
    }
    return promises;
}

string encodeHistoricalRecords(const vector<HistoricalRecord>& records) {
    vector<string> encodedEntries;
    encodedEntries.reserve(records.size());
    for (const auto& record : records) {
        encodedEntries.push_back(joinEscapedFields({
            record.category,
            record.holderName,
            record.teamName,
            to_string(record.season),
            to_string(record.value),
            record.note
        }, '^'));
    }
    return joinEscapedFields(encodedEntries, '~');
}

vector<HistoricalRecord> decodeHistoricalRecords(const string& encoded) {
    vector<HistoricalRecord> records;
    if (encoded.empty()) return records;
    for (const string& token : splitEscapedFields(encoded, '~')) {
        auto parts = splitEscapedFields(token, '^');
        if (parts.size() < 6) continue;
        HistoricalRecord record;
        record.category = parts[0];
        record.holderName = parts[1];
        record.teamName = parts[2];
        record.season = parseIntField(parts[3], 1);
        record.value = parseIntField(parts[4]);
        record.note = parts[5];
        records.push_back(record);
    }
    return records;
}

string encodeScoutingAssignments(const vector<ScoutingAssignment>& assignments) {
    vector<string> encodedEntries;
    encodedEntries.reserve(assignments.size());
    for (const auto& assignment : assignments) {
        encodedEntries.push_back(joinEscapedFields({
            assignment.region,
            assignment.focusPosition,
            assignment.priority,
            to_string(assignment.weeksRemaining),
            to_string(assignment.knowledgeLevel)
        }, '^'));
    }
    return joinEscapedFields(encodedEntries, '~');
}

vector<ScoutingAssignment> decodeScoutingAssignments(const string& encoded) {
    vector<ScoutingAssignment> assignments;
    if (encoded.empty()) return assignments;
    for (const string& token : splitEscapedFields(encoded, '~')) {
        auto parts = splitEscapedFields(token, '^');
        if (parts.size() < 5) continue;
        ScoutingAssignment assignment;
        assignment.region = parts[0];
        assignment.focusPosition = parts[1];
        assignment.priority = parts[2];
        assignment.weeksRemaining = parseIntField(parts[3]);
        assignment.knowledgeLevel = parseIntField(parts[4]);
        assignments.push_back(assignment);
    }
    return assignments;
}

string encodeHumanManagers(const vector<HumanManagerProfile>& managers) {
    vector<string> encodedEntries;
    encodedEntries.reserve(managers.size());
    for (const auto& manager : managers) {
        encodedEntries.push_back(joinEscapedFields({
            manager.managerName,
            manager.teamName,
            to_string(manager.reputation)
        }, '^'));
    }
    return joinEscapedFields(encodedEntries, '~');
}

vector<HumanManagerProfile> decodeHumanManagers(const string& encoded) {
    vector<HumanManagerProfile> managers;
    if (encoded.empty()) return managers;
    for (const string& token : splitEscapedFields(encoded, '~')) {
        const auto parts = splitEscapedFields(token, '^');
        if (parts.size() < 2) continue;
        HumanManagerProfile manager;
        manager.managerName = parts[0].empty() ? "Manager" : parts[0];
        manager.teamName = parts[1];
        manager.reputation = clampInt(parts.size() > 2 ? parseIntField(parts[2], 50) : 50, 1, 100);
        managers.push_back(std::move(manager));
    }
    return managers;
}

string encodePlayerFields(const Player& player) {
    return joinEscapedFields({
        player.name,
        player.position,
        to_string(player.attack),
        to_string(player.defense),
        to_string(player.stamina),
        to_string(player.fitness),
        to_string(player.skill),
        to_string(player.potential),
        to_string(player.age),
        to_string(player.value),
        to_string(player.onLoan ? 1 : 0),
        player.parentClub,
        to_string(player.loanWeeksRemaining),
        to_string(player.injured ? 1 : 0),
        player.injuryType,
        to_string(player.injuryWeeks),
        to_string(player.goals),
        to_string(player.assists),
        to_string(player.matchesPlayed),
        to_string(player.lastTrainedSeason),
        to_string(player.lastTrainedWeek),
        to_string(player.wage),
        to_string(player.releaseClause),
        to_string(player.contractWeeks),
        to_string(player.injuryHistory),
        to_string(player.yellowAccumulation),
        to_string(player.seasonYellowCards),
        to_string(player.seasonRedCards),
        to_string(player.matchesSuspended),
        player.role,
        player.roleDuty,
        player.individualInstruction,
        to_string(player.setPieceSkill),
        to_string(player.leadership),
        to_string(player.professionalism),
        to_string(player.ambition),
        to_string(player.happiness),
        to_string(player.chemistry),
        to_string(player.desiredStarts),
        to_string(player.startsThisSeason),
        to_string(player.wantsToLeave ? 1 : 0),
        player.developmentPlan,
        player.promisedRole,
        encodeStringList(player.traits),
        player.preferredFoot,
        encodeStringList(player.secondaryPositions),
        to_string(player.consistency),
        to_string(player.bigMatches),
        to_string(player.currentForm),
        to_string(player.tacticalDiscipline),
        player.personality,
        to_string(player.discipline),
        to_string(player.injuryProneness),
        to_string(player.adaptation),
        to_string(player.versatility),
        to_string(player.moraleMomentum),
        to_string(player.fatigueLoad),
        to_string(player.unhappinessWeeks),
        player.promisedPosition,
        player.socialGroup
    }, '|');
}

Player decodePlayerFields(const vector<string>& fields) {
    Player player;
    size_t idx = 0;
    const bool hasLoanState = fields.size() >= 30;
    const bool hasFitness = fields.size() >= 16;
    const size_t remaining = (idx <= fields.size()) ? (fields.size() - idx) : 0;
    const bool hasPotential = remaining >= 11;
    if (idx < fields.size()) player.name = fields[idx++];
    if (idx < fields.size()) player.position = fields[idx++];
    if (idx < fields.size()) player.attack = parseIntField(fields[idx++], 40); else player.attack = 40;
    if (idx < fields.size()) player.defense = parseIntField(fields[idx++], 40); else player.defense = 40;
    if (idx < fields.size()) player.stamina = parseIntField(fields[idx++], 60); else player.stamina = 60;
    if (hasFitness && idx < fields.size()) player.fitness = parseIntField(fields[idx++], player.stamina); else player.fitness = player.stamina;
    if (idx < fields.size()) player.skill = parseIntField(fields[idx++], (player.attack + player.defense) / 2);
    else player.skill = (player.attack + player.defense) / 2;
    if (hasPotential && idx < fields.size()) player.potential = parseIntField(fields[idx++], player.skill);
    else player.potential = clampInt(player.skill + randInt(0, 8), player.skill, 95);
    if (idx < fields.size()) player.age = parseIntField(fields[idx++], 24); else player.age = 24;
    if (idx < fields.size()) player.value = parseLongField(fields[idx++], static_cast<long long>(player.skill) * 10000);
    else player.value = static_cast<long long>(player.skill) * 10000;
    if (hasLoanState && idx < fields.size()) player.onLoan = parseBoolField(fields[idx++]);
    else player.onLoan = false;
    if (hasLoanState && idx < fields.size()) player.parentClub = fields[idx++]; else player.parentClub.clear();
    if (hasLoanState && idx < fields.size()) player.loanWeeksRemaining = parseIntField(fields[idx++]); else player.loanWeeksRemaining = 0;
    player.injured = (idx < fields.size()) ? parseBoolField(fields[idx++]) : false;
    if (hasPotential && idx < fields.size()) player.injuryType = fields[idx++]; else player.injuryType = player.injured ? "Leve" : "";
    if (idx < fields.size()) player.injuryWeeks = parseIntField(fields[idx++]); else player.injuryWeeks = 0;
    if (idx < fields.size()) player.goals = parseIntField(fields[idx++]); else player.goals = 0;
    if (idx < fields.size()) player.assists = parseIntField(fields[idx++]); else player.assists = 0;
    if (idx < fields.size()) player.matchesPlayed = parseIntField(fields[idx++]); else player.matchesPlayed = 0;
    if (idx + 1 < fields.size()) {
        player.lastTrainedSeason = parseIntField(fields[idx++], -1);
        player.lastTrainedWeek = parseIntField(fields[idx++], -1);
    } else {
        player.lastTrainedSeason = -1;
        player.lastTrainedWeek = -1;
    }
    if (idx < fields.size()) player.wage = parseLongField(fields[idx++], static_cast<long long>(player.skill) * 150 + randInt(0, 800));
    else player.wage = static_cast<long long>(player.skill) * 150 + randInt(0, 800);
    if (idx < fields.size()) player.releaseClause = parseLongField(fields[idx++], max(50000LL, player.value * 2));
    else player.releaseClause = max(50000LL, player.value * 2);
    if (idx < fields.size()) player.contractWeeks = parseIntField(fields[idx++], randInt(52, 156)); else player.contractWeeks = randInt(52, 156);
    if (idx < fields.size()) player.injuryHistory = parseIntField(fields[idx++]); else player.injuryHistory = 0;
    if (idx < fields.size()) player.yellowAccumulation = parseIntField(fields[idx++]); else player.yellowAccumulation = 0;
    if (idx < fields.size()) player.seasonYellowCards = parseIntField(fields[idx++]); else player.seasonYellowCards = 0;
    if (idx < fields.size()) player.seasonRedCards = parseIntField(fields[idx++]); else player.seasonRedCards = 0;
    if (idx < fields.size()) player.matchesSuspended = parseIntField(fields[idx++]); else player.matchesSuspended = 0;
    const bool hasIndividualInstruction = fields.size() >= 56;
    const bool hasAdvancedPlayerAttributes = fields.size() >= 60;
    if (idx < fields.size()) player.role = fields[idx++]; else player.role = defaultRoleForPosition(player.position);
    if (idx < fields.size()) player.roleDuty = fields[idx++]; else player.roleDuty = defaultDutyForRole(player.role, player.position);
    if (hasIndividualInstruction && idx < fields.size()) player.individualInstruction = fields[idx++]; else player.individualInstruction = defaultInstructionForPosition(player.position);
    if (idx < fields.size()) player.setPieceSkill = parseIntField(fields[idx++], clampInt(player.skill + randInt(-6, 6), 25, 99));
    else player.setPieceSkill = clampInt(player.skill + randInt(-6, 6), 25, 99);
    if (idx < fields.size()) player.leadership = parseIntField(fields[idx++], clampInt(35 + randInt(0, 45), 1, 99));
    else player.leadership = clampInt(35 + randInt(0, 45), 1, 99);
    if (idx < fields.size()) player.professionalism = parseIntField(fields[idx++], clampInt(40 + randInt(0, 45), 1, 99));
    else player.professionalism = clampInt(40 + randInt(0, 45), 1, 99);
    if (idx < fields.size()) player.ambition = parseIntField(fields[idx++], clampInt(35 + randInt(0, 50), 1, 99));
    else player.ambition = clampInt(35 + randInt(0, 50), 1, 99);
    if (idx < fields.size()) player.happiness = parseIntField(fields[idx++], clampInt(55 + randInt(-10, 20), 1, 99));
    else player.happiness = clampInt(55 + randInt(-10, 20), 1, 99);
    if (idx < fields.size()) player.chemistry = parseIntField(fields[idx++], clampInt(45 + randInt(0, 35), 1, 99));
    else player.chemistry = clampInt(45 + randInt(0, 35), 1, 99);
    if (idx < fields.size()) player.desiredStarts = parseIntField(fields[idx++], 1); else player.desiredStarts = 1;
    if (idx < fields.size()) player.startsThisSeason = parseIntField(fields[idx++]); else player.startsThisSeason = 0;
    if (idx < fields.size()) player.wantsToLeave = parseBoolField(fields[idx++]); else player.wantsToLeave = false;
    if (idx < fields.size()) player.developmentPlan = fields[idx++]; else player.developmentPlan = defaultDevelopmentPlanForPosition(player.position);
    if (idx < fields.size()) player.promisedRole = fields[idx++]; else player.promisedRole = "Sin promesa";
    if (idx < fields.size()) player.traits = decodeStringList(fields[idx++]);
    if (idx < fields.size()) player.preferredFoot = fields[idx++];
    if (idx < fields.size()) player.secondaryPositions = decodeStringList(fields[idx++]);
    if (idx < fields.size()) player.consistency = parseIntField(fields[idx++]); else player.consistency = 0;
    if (idx < fields.size()) player.bigMatches = parseIntField(fields[idx++]); else player.bigMatches = 0;
    if (idx < fields.size()) player.currentForm = parseIntField(fields[idx++]); else player.currentForm = 0;
    if (idx < fields.size()) player.tacticalDiscipline = parseIntField(fields[idx++]); else player.tacticalDiscipline = 0;
    if (hasAdvancedPlayerAttributes && idx < fields.size()) player.personality = fields[idx++]; else player.personality.clear();
    if (hasAdvancedPlayerAttributes && idx < fields.size()) player.discipline = parseIntField(fields[idx++]); else player.discipline = 0;
    if (hasAdvancedPlayerAttributes && idx < fields.size()) player.injuryProneness = parseIntField(fields[idx++]); else player.injuryProneness = 0;
    if (hasAdvancedPlayerAttributes && idx < fields.size()) player.adaptation = parseIntField(fields[idx++]); else player.adaptation = 0;
    if (idx < fields.size()) player.versatility = parseIntField(fields[idx++]); else player.versatility = 0;
    if (idx < fields.size()) player.moraleMomentum = parseIntField(fields[idx++]); else player.moraleMomentum = 0;
    if (idx < fields.size()) player.fatigueLoad = parseIntField(fields[idx++]); else player.fatigueLoad = 0;
    if (idx < fields.size()) player.unhappinessWeeks = parseIntField(fields[idx++]); else player.unhappinessWeeks = 0;
    if (idx < fields.size()) player.promisedPosition = fields[idx++]; else player.promisedPosition = player.position;
    if (idx < fields.size()) player.socialGroup = fields[idx++]; else player.socialGroup.clear();
    ensurePlayerProfile(player, player.traits.empty());
    return player;
}

}  // namespace

namespace save_serialization {

int currentCareerSaveVersion() {
    return kCareerSaveVersion;
}

bool serializeCareer(ostream& file, const Career& career) {
    const int& currentSeason = career.currentSeason;
    const int& currentWeek = career.currentWeek;
    const string& activeDivision = career.activeDivision;
    Team* const& myTeam = career.myTeam;
    const string& managerName = career.managerName;
    const int& managerReputation = career.managerReputation;
    vector<HumanManagerProfile> humanManagers = career.humanManagers;
    if (humanManagers.empty() && myTeam) {
        humanManagers.push_back({managerName.empty() ? "Manager" : managerName, myTeam->name,
                                 clampInt(managerReputation <= 0 ? 50 : managerReputation, 1, 100)});
    }
    const int activeHumanManagerIndex = humanManagers.empty()
                                            ? 0
                                            : clampInt(career.activeHumanManagerIndex, 0,
                                                       static_cast<int>(humanManagers.size()) - 1);
    const string& boardMonthlyObjective = career.boardMonthlyObjective;
    const int& boardMonthlyTarget = career.boardMonthlyTarget;
    const int& boardMonthlyProgress = career.boardMonthlyProgress;
    const int& boardMonthlyDeadlineWeek = career.boardMonthlyDeadlineWeek;
    const vector<string>& achievements = career.achievements;
    const int& boardConfidence = career.boardConfidence;
    const int& boardExpectedFinish = career.boardExpectedFinish;
    const long long& boardBudgetTarget = career.boardBudgetTarget;
    const int& boardYouthTarget = career.boardYouthTarget;
    const int& boardWarningWeeks = career.boardWarningWeeks;
    const vector<string>& newsFeed = career.newsFeed;
    const vector<string>& managerInbox = career.managerInbox;
    const vector<string>& scoutInbox = career.scoutInbox;
    const vector<string>& scoutingShortlist = career.scoutingShortlist;
    const vector<ScoutingAssignment>& scoutingAssignments = career.scoutingAssignments;
    const vector<SeasonHistoryEntry>& history = career.history;
    const vector<SquadPromise>& activePromises = career.activePromises;
    const vector<HistoricalRecord>& historicalRecords = career.historicalRecords;
    const vector<PendingTransfer>& pendingTransfers = career.pendingTransfers;
    const bool& cupActive = career.cupActive;
    const int& cupRound = career.cupRound;
    const vector<string>& cupRemainingTeams = career.cupRemainingTeams;
    const string& cupChampion = career.cupChampion;
    const string& lastMatchAnalysis = career.lastMatchAnalysis;
    const vector<string>& lastMatchReportLines = career.lastMatchReportLines;
    const vector<string>& lastMatchEvents = career.lastMatchEvents;
    const string& lastMatchPlayerOfTheMatch = career.lastMatchPlayerOfTheMatch;
    const MatchCenterSnapshot& lastMatchCenter = career.lastMatchCenter;
    const deque<Team>& allTeams = career.allTeams;
    const SaveIntegritySnapshot integrity = buildIntegritySnapshot(career);

    file << "VERSION " << kCareerSaveVersion << "\n";
    file << "save_version = " << kSaveSchemaVersion << "\n";
    file << "INTEGRITY " << encodeIntegritySnapshot(integrity) << "\n";
    file << "SEASON " << currentSeason << " WEEK " << currentWeek << "\n";
    file << "DIVISION " << escapeSaveField(activeDivision) << "\n";
    file << "MYTEAM " << escapeSaveField(myTeam ? myTeam->name : "") << "\n";
    file << "MANAGER " << joinEscapedFields({managerName, to_string(managerReputation)}, '|') << "\n";
    file << "HUMANS " << encodeHumanManagers(humanManagers) << "\n";
    file << "ACTIVE_MANAGER " << activeHumanManagerIndex << "\n";
    file << "DYNOBJ "
         << joinEscapedFields({boardMonthlyObjective,
                               to_string(boardMonthlyTarget),
                               to_string(boardMonthlyProgress),
                               to_string(boardMonthlyDeadlineWeek)}, '|')
         << "\n";
    file << "ACHIEVEMENTS " << achievements.size() << "\n";
    for (const auto& achievement : achievements) file << "ACH " << escapeSaveField(achievement) << "\n";
    file << "BOARD "
         << joinEscapedFields({to_string(boardConfidence),
                               to_string(boardExpectedFinish),
                               to_string(boardBudgetTarget),
                               to_string(boardYouthTarget),
                               to_string(boardWarningWeeks)}, '|')
         << "\n";
    file << "NEWS " << encodeStringList(newsFeed) << "\n";
    file << "INBOX " << encodeStringList(managerInbox) << "\n";
    file << "SCOUT " << encodeStringList(scoutInbox) << "\n";
    file << "SHORTLIST " << encodeStringList(scoutingShortlist) << "\n";
    file << "ASSIGNMENTS " << encodeScoutingAssignments(scoutingAssignments) << "\n";
    file << "HISTORY " << encodeHistory(history) << "\n";
    file << "PROMISES " << encodePromises(activePromises) << "\n";
    file << "RECORDS " << encodeHistoricalRecords(historicalRecords) << "\n";
    file << "PENDING " << encodePendingTransfers(pendingTransfers) << "\n";
    file << "CUP "
         << joinEscapedFields({to_string(cupActive ? 1 : 0),
                               to_string(cupRound),
                               encodeStringList(cupRemainingTeams),
                               cupChampion}, '|')
         << "\n";
    file << "LASTMATCH " << escapeSaveField(lastMatchAnalysis) << "\n";
    file << "LASTMATCH_REPORT " << encodeStringList(lastMatchReportLines) << "\n";
    file << "LASTMATCH_EVENTS " << encodeStringList(lastMatchEvents) << "\n";
    file << "LASTMATCH_POTM " << escapeSaveField(lastMatchPlayerOfTheMatch) << "\n";
    file << "LASTMATCH_CENTER " << encodeMatchCenterSnapshot(lastMatchCenter) << "\n";
    file << "LASTMATCH_PHASES " << encodeStringList(lastMatchCenter.phaseSummaries) << "\n";
    file << "LASTMATCH_RATINGS " << encodeStringList(lastMatchCenter.playerRatingLines) << "\n";
    
    // Serialize new gameplay systems
    file << "GAMEPLAY_SYSTEMS " 
         << joinEscapedFields({
             to_string(career.managerStress.stressLevel),
             to_string(career.managerStress.energy),
             to_string(career.managerStress.mentalFortitude),
             to_string(career.infrastructure.levels.trainingGround),
             to_string(career.infrastructure.levels.youthAcademy),
             to_string(career.infrastructure.levels.medical),
             to_string(career.infrastructure.levels.stadium),
             to_string(career.infrastructure.levels.facilities),
             to_string(career.debtStatus.totalDebt),
             to_string(career.debtStatus.debtSeverity),
             to_string(career.debtStatus.weeksUntilSeizure)
         }, '|') << "\n";
    
    // Serialize dressing room dynamics
    file << "DRESSING_ROOM " << career.dressingRoomDynamics.cliques.size() << "\n";
    for (const auto& clique : career.dressingRoomDynamics.cliques) {
        file << "CLIQUE " << joinEscapedFields({
            clique.name,
            to_string(clique.cohesion),
            to_string(clique.moralBoost),
            encodeStringList(clique.memberNames)
        }, '|') << "\n";
    }
    
    // Serialize rival AI map
    file << "RIVAL_AI " << career.rivalAIMap.size() << "\n";
    for (const auto& rivalPair : career.rivalAIMap) {
        const std::string& teamName = rivalPair.first;
        const RivalAI& rivalAI = rivalPair.second;
        file << "RIVAL " << joinEscapedFields({
            teamName,
            rivalAI.personality.teamName,
            rivalAI.personality.playstyle,
            to_string(rivalAI.personality.adaptability),
            to_string(rivalAI.personality.tactical_awareness),
            to_string(rivalAI.personality.unpredictability),
            to_string(rivalAI.memoryBank.size())
        }, '|') << "\n";
        for (const auto& memory : rivalAI.memoryBank) {
            file << "MEMORY " << joinEscapedFields({
                memory.opponentName,
                to_string(memory.matchesPlayed),
                to_string(memory.wins),
                to_string(memory.draws),
                to_string(memory.losses),
                encodeStringList(memory.favoredFormations),
                encodeStringList(memory.favoredTactics),
                escapeSaveField(memory.commonPlayPattern),
                encodeStringList(memory.identifiedWeaknesses),
                encodeStringList(memory.identifiedStrengths),
                to_string(memory.lastMatchOutcome),
                to_string(memory.consecutiveVsThisTeam)
            }, '|') << "\n";
        }
    }
    
    // Serialize rivalry dynamics
    file << "RIVALRY_DYNAMICS " << career.rivalryDynamics.rivalries.size() << "\n";
    for (const auto& record : career.rivalryDynamics.rivalries) {
        file << "RIVALRY " << joinEscapedFields({
            record.team1,
            record.team2,
            to_string(record.encounters),
            to_string(record.team1Wins),
            to_string(record.team1Draws),
            to_string(record.team1Losses),
            to_string(record.lastMeetingWeek),
            to_string(record.intensity),
            to_string(record.isLocalRivalry ? 1 : 0),
            to_string(record.isHistoricRivalry ? 1 : 0)
        }, '|') << "\n";
    }
    
    file << "TEAMS " << allTeams.size() << "\n";

    for (const auto& team : allTeams) {
        const int storedPrestige = team.clubPrestige > 0 ? team.clubPrestige : teamPrestigeScore(team);
        const string storedClubStyle = team.clubStyle.empty() ? "Equilibrio competitivo" : team.clubStyle;
        const string storedYouthIdentity = team.youthIdentity.empty() ? "Talento local" : team.youthIdentity;
        file << "TEAM "
             << joinEscapedFields({
                    team.name,
                    team.division,
                    team.tactics,
                    team.formation,
                    to_string(team.budget),
                    to_string(team.morale),
                    to_string(team.points),
                    to_string(team.goalsFor),
                    to_string(team.goalsAgainst),
                    to_string(team.wins),
                    to_string(team.draws),
                    to_string(team.losses),
                    team.trainingFocus,
                    to_string(team.awayGoals),
                    to_string(team.redCards),
                    to_string(team.yellowCards),
                    to_string(team.tiebreakerSeed),
                    encodeHeadToHead(team.headToHead),
                    to_string(team.pressingIntensity),
                    to_string(team.defensiveLine),
                    to_string(team.tempo),
                    to_string(team.width),
                    team.markingStyle,
                    encodeStringList(team.preferredXI),
                    encodeStringList(team.preferredBench),
                    team.captain,
                    team.penaltyTaker,
                    team.freeKickTaker,
                    team.cornerTaker,
                    team.rotationPolicy,
                    to_string(team.assistantCoach),
                    to_string(team.fitnessCoach),
                    to_string(team.scoutingChief),
                    to_string(team.youthCoach),
                    to_string(team.medicalTeam),
                    to_string(team.goalkeepingCoach),
                    to_string(team.performanceAnalyst),
                    team.youthRegion,
                    to_string(team.debt),
                    to_string(team.sponsorWeekly),
                    to_string(team.stadiumLevel),
                    to_string(team.youthFacilityLevel),
                    to_string(team.trainingFacilityLevel),
                    to_string(team.fanBase),
                    team.matchInstruction,
                    to_string(storedPrestige),
                    storedClubStyle,
                    storedYouthIdentity,
                    team.primaryRival,
                    team.headCoachName,
                    to_string(team.headCoachReputation),
                    team.headCoachStyle,
                    to_string(team.jobSecurity),
                    team.transferPolicy,
                    encodeStringList(team.scoutingRegions),
                    team.assistantCoachName,
                    team.fitnessCoachName,
                    team.scoutingChiefName,
                    team.youthCoachName,
                    team.medicalChiefName,
                    team.goalkeepingCoachName,
                    team.performanceAnalystName,
                    to_string(team.headCoachTenureWeeks)
                }, '|')
             << "\n";
        file << "PLAYERS " << team.players.size() << "\n";
        for (const auto& player : team.players) {
            file << "PLAYER " << encodePlayerFields(player) << "\n";
        }
        file << "ENDTEAM\n";
    }
    return file.good();
}

bool deserializeCareer(istream& file, Career& career) {
    auto& currentSeason = career.currentSeason;
    auto& currentWeek = career.currentWeek;
    string activeDivision;
    auto& myTeam = career.myTeam;
    auto& managerName = career.managerName;
    auto& managerReputation = career.managerReputation;
    auto& humanManagers = career.humanManagers;
    auto& activeHumanManagerIndex = career.activeHumanManagerIndex;
    auto& boardMonthlyObjective = career.boardMonthlyObjective;
    auto& boardMonthlyTarget = career.boardMonthlyTarget;
    auto& boardMonthlyProgress = career.boardMonthlyProgress;
    auto& boardMonthlyDeadlineWeek = career.boardMonthlyDeadlineWeek;
    auto& achievements = career.achievements;
    auto& boardConfidence = career.boardConfidence;
    auto& boardExpectedFinish = career.boardExpectedFinish;
    auto& boardBudgetTarget = career.boardBudgetTarget;
    auto& boardYouthTarget = career.boardYouthTarget;
    auto& boardWarningWeeks = career.boardWarningWeeks;
    auto& newsFeed = career.newsFeed;
    auto& managerInbox = career.managerInbox;
    auto& scoutInbox = career.scoutInbox;
    auto& scoutingShortlist = career.scoutingShortlist;
    auto& scoutingAssignments = career.scoutingAssignments;
    auto& history = career.history;
    auto& activePromises = career.activePromises;
    auto& historicalRecords = career.historicalRecords;
    auto& pendingTransfers = career.pendingTransfers;
    auto& cupActive = career.cupActive;
    auto& cupRound = career.cupRound;
    auto& cupRemainingTeams = career.cupRemainingTeams;
    auto& cupChampion = career.cupChampion;
    auto& lastMatchAnalysis = career.lastMatchAnalysis;
    auto& lastMatchReportLines = career.lastMatchReportLines;
    auto& lastMatchEvents = career.lastMatchEvents;
    auto& lastMatchPlayerOfTheMatch = career.lastMatchPlayerOfTheMatch;
    auto& lastMatchCenter = career.lastMatchCenter;
    auto& allTeams = career.allTeams;
    auto& divisions = career.divisions;
    auto& initialized = career.initialized;

    try {
    string line;
    if (!getline(file, line)) return false;
    line = trim(line);

    int saveVersion = 1;
    int saveSchemaVersion = 0;
    SaveIntegritySnapshot expectedIntegrity;
    bool hasIntegrity = false;
    if (line.rfind("VERSION ", 0) == 0) {
        saveVersion = parseIntField(trim(line.substr(8)), 1);
        if (saveVersion > kCareerSaveVersion) return false;
        if (!getline(file, line)) return false;
        line = trim(line);
    }
    while (true) {
        if (parseSaveSchemaLine(line, saveSchemaVersion)) {
            if (saveSchemaVersion > kSaveSchemaVersion) return false;
            if (!getline(file, line)) return false;
            line = trim(line);
            continue;
        }
        SaveIntegritySnapshot parsedIntegrity;
        if (parseIntegrityLine(line, parsedIntegrity)) {
            expectedIntegrity = parsedIntegrity;
            hasIntegrity = true;
            if (!getline(file, line)) return false;
            line = trim(line);
            continue;
        }
        break;
    }

    if (line.rfind("SEASON ", 0) == 0) {
        string token;
        stringstream ss(line);
        ss >> token >> currentSeason >> token >> currentWeek;

        string divisionLine;
        if (!getline(file, divisionLine)) return false;
        if (divisionLine.rfind("DIVISION ", 0) == 0) {
            activeDivision = canonicalizeDivisionValue(splitEscapedFields(divisionLine.substr(9), '|').front());
        }

        string myTeamLine;
        if (!getline(file, myTeamLine)) return false;
        string myTeamName;
        if (myTeamLine.rfind("MYTEAM ", 0) == 0) myTeamName = splitEscapedFields(myTeamLine.substr(7), '|').front();

        string teamsLine;
        if (!getline(file, teamsLine)) return false;
        managerName = "Manager";
        managerReputation = 50;
        humanManagers.clear();
        activeHumanManagerIndex = 0;
        if (teamsLine.rfind("MANAGER ", 0) == 0) {
            auto managerFields = splitEscapedFields(teamsLine.substr(8), '|');
            if (!managerFields.empty()) managerName = managerFields[0];
            if (managerFields.size() > 1) managerReputation = parseIntField(managerFields[1], 50);
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("HUMANS ", 0) == 0) {
            humanManagers = decodeHumanManagers(teamsLine.substr(7));
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("ACTIVE_MANAGER ", 0) == 0) {
            activeHumanManagerIndex = parseIntField(trim(teamsLine.substr(15)), 0);
            if (!getline(file, teamsLine)) return false;
        }
        boardMonthlyObjective.clear();
        boardMonthlyTarget = 0;
        boardMonthlyProgress = 0;
        boardMonthlyDeadlineWeek = 0;
        if (teamsLine.rfind("DYNOBJ ", 0) == 0) {
            auto objectiveFields = splitEscapedFields(teamsLine.substr(7), '|');
            if (!objectiveFields.empty()) boardMonthlyObjective = objectiveFields[0];
            if (objectiveFields.size() > 1) boardMonthlyTarget = parseIntField(objectiveFields[1]);
            if (objectiveFields.size() > 2) boardMonthlyProgress = parseIntField(objectiveFields[2]);
            if (objectiveFields.size() > 3) boardMonthlyDeadlineWeek = parseIntField(objectiveFields[3]);
            if (!getline(file, teamsLine)) return false;
        }
        achievements.clear();
        if (teamsLine.rfind("ACHIEVEMENTS ", 0) == 0) {
            int achCount = parseIntField(trim(teamsLine.substr(13)));
            for (int i = 0; i < achCount; ++i) {
                if (!getline(file, line) || line.rfind("ACH ", 0) != 0) return false;
                achievements.push_back(splitEscapedFields(line.substr(4), '|').front());
            }
            if (!getline(file, teamsLine)) return false;
        }
        boardConfidence = 60;
        boardExpectedFinish = 0;
        boardBudgetTarget = 0;
        boardYouthTarget = 1;
        boardWarningWeeks = 0;
        if (teamsLine.rfind("BOARD ", 0) == 0) {
            auto boardFields = splitEscapedFields(teamsLine.substr(6), '|');
            if (!boardFields.empty()) boardConfidence = clampInt(parseIntField(boardFields[0], 60), 0, 100);
            if (boardFields.size() > 1) boardExpectedFinish = parseIntField(boardFields[1]);
            if (boardFields.size() > 2) boardBudgetTarget = parseLongField(boardFields[2]);
            if (boardFields.size() > 3) boardYouthTarget = parseIntField(boardFields[3], 1);
            if (boardFields.size() > 4) boardWarningWeeks = parseIntField(boardFields[4]);
            if (!getline(file, teamsLine)) return false;
        }
        newsFeed.clear();
        if (teamsLine.rfind("NEWS ", 0) == 0) {
            newsFeed = decodeStringList(teamsLine.substr(5));
            if (!getline(file, teamsLine)) return false;
        }
        managerInbox.clear();
        if (teamsLine.rfind("INBOX ", 0) == 0) {
            managerInbox = decodeStringList(teamsLine.substr(6));
            if (!getline(file, teamsLine)) return false;
        }
        scoutInbox.clear();
        if (teamsLine.rfind("SCOUT ", 0) == 0) {
            scoutInbox = decodeStringList(teamsLine.substr(6));
            if (!getline(file, teamsLine)) return false;
        }
        scoutingShortlist.clear();
        if (teamsLine.rfind("SHORTLIST ", 0) == 0) {
            scoutingShortlist = decodeStringList(teamsLine.substr(10));
            if (!getline(file, teamsLine)) return false;
        }
        scoutingAssignments.clear();
        if (teamsLine.rfind("ASSIGNMENTS ", 0) == 0) {
            scoutingAssignments = decodeScoutingAssignments(teamsLine.substr(12));
            if (!getline(file, teamsLine)) return false;
        }
        history.clear();
        if (teamsLine.rfind("HISTORY ", 0) == 0) {
            history = decodeHistory(teamsLine.substr(8));
            if (!getline(file, teamsLine)) return false;
        }
        activePromises.clear();
        if (teamsLine.rfind("PROMISES ", 0) == 0) {
            activePromises = decodePromises(teamsLine.substr(9));
            if (!getline(file, teamsLine)) return false;
        }
        historicalRecords.clear();
        if (teamsLine.rfind("RECORDS ", 0) == 0) {
            historicalRecords = decodeHistoricalRecords(teamsLine.substr(8));
            if (!getline(file, teamsLine)) return false;
        }
        pendingTransfers.clear();
        if (teamsLine.rfind("PENDING ", 0) == 0) {
            pendingTransfers = decodePendingTransfers(teamsLine.substr(8));
            if (!getline(file, teamsLine)) return false;
        }
        cupActive = false;
        cupRound = 0;
        cupRemainingTeams.clear();
        cupChampion.clear();
        if (teamsLine.rfind("CUP ", 0) == 0) {
            auto cupFields = splitEscapedFields(teamsLine.substr(4), '|');
            if (!cupFields.empty()) cupActive = parseBoolField(cupFields[0]);
            if (cupFields.size() > 1) cupRound = parseIntField(cupFields[1]);
            if (cupFields.size() > 2) cupRemainingTeams = decodeStringList(cupFields[2]);
            if (cupFields.size() > 3) cupChampion = cupFields[3];
            if (!getline(file, teamsLine)) return false;
        }
        lastMatchAnalysis.clear();
        lastMatchReportLines.clear();
        lastMatchEvents.clear();
        lastMatchPlayerOfTheMatch.clear();
        lastMatchCenter = MatchCenterSnapshot{};
        if (teamsLine.rfind("LASTMATCH ", 0) == 0) {
            lastMatchAnalysis = splitEscapedFields(teamsLine.substr(10), '|').front();
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("LASTMATCH_REPORT ", 0) == 0) {
            lastMatchReportLines = decodeStringList(teamsLine.substr(17));
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("LASTMATCH_EVENTS ", 0) == 0) {
            lastMatchEvents = decodeStringList(teamsLine.substr(17));
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("LASTMATCH_POTM ", 0) == 0) {
            lastMatchPlayerOfTheMatch = splitEscapedFields(teamsLine.substr(15), '|').front();
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("LASTMATCH_CENTER ", 0) == 0) {
            lastMatchCenter = decodeMatchCenterSnapshot(teamsLine.substr(17));
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("LASTMATCH_PHASES ", 0) == 0) {
            lastMatchCenter.phaseSummaries = decodeStringList(teamsLine.substr(16));
            if (!getline(file, teamsLine)) return false;
        }
        if (teamsLine.rfind("LASTMATCH_RATINGS ", 0) == 0) {
            lastMatchCenter.playerRatingLines = decodeStringList(teamsLine.substr(18));
            if (!getline(file, teamsLine)) return false;
        }
        
        // Deserialize new gameplay systems
        career.managerStress = initializeManagerStress();
        career.infrastructure = InfrastructureSystem{};
        career.debtStatus = DebtStatus{};
        if (teamsLine.rfind("GAMEPLAY_SYSTEMS ", 0) == 0) {
            auto systemFields = splitEscapedFields(teamsLine.substr(16), '|');
            if (systemFields.size() >= 3) {
                career.managerStress.stressLevel = parseIntField(systemFields[0], 0);
                career.managerStress.energy = parseIntField(systemFields[1], 100);
                career.managerStress.mentalFortitude = parseIntField(systemFields[2], 50);
            }
            if (systemFields.size() >= 8) {
                career.infrastructure.levels.trainingGround = parseIntField(systemFields[3], 1);
                career.infrastructure.levels.youthAcademy = parseIntField(systemFields[4], 1);
                career.infrastructure.levels.medical = parseIntField(systemFields[5], 1);
                career.infrastructure.levels.stadium = parseIntField(systemFields[6], 1);
                career.infrastructure.levels.facilities = parseIntField(systemFields[7], 1);
            }
            if (systemFields.size() >= 11) {
                career.debtStatus.totalDebt = parseLongField(systemFields[8], 0);
                career.debtStatus.debtSeverity = parseIntField(systemFields[9], 0);
                career.debtStatus.weeksUntilSeizure = parseIntField(systemFields[10], 0);
            }
            if (!getline(file, teamsLine)) return false;
        }
        
        // Deserialize dressing room dynamics
        career.dressingRoomDynamics = DressingRoomDynamics{};
        if (teamsLine.rfind("DRESSING_ROOM ", 0) == 0) {
            int cliqueCount = parseIntField(trim(teamsLine.substr(14)));
            for (int i = 0; i < cliqueCount; ++i) {
                if (!getline(file, line) || line.rfind("CLIQUE ", 0) != 0) return false;
                auto cliqueFields = splitEscapedFields(line.substr(7), '|');
                if (cliqueFields.size() >= 4) {
                    SocialGroup clique;
                    clique.name = cliqueFields[0];
                    clique.cohesion = parseIntField(cliqueFields[1], 50);
                    clique.moralBoost = parseIntField(cliqueFields[2], 0);
                    clique.memberNames = decodeStringList(cliqueFields[3]);
                    career.dressingRoomDynamics.cliques.push_back(clique);
                }
            }
            if (!getline(file, teamsLine)) return false;
        }
        
        // Deserialize rival AI map
        career.rivalAIMap.clear();
        if (teamsLine.rfind("RIVAL_AI ", 0) == 0) {
            int rivalCount = parseIntField(trim(teamsLine.substr(9)));
            for (int i = 0; i < rivalCount; ++i) {
                if (!getline(file, line) || line.rfind("RIVAL ", 0) != 0) return false;
                auto rivalFields = splitEscapedFields(line.substr(6), '|');
                if (rivalFields.size() >= 7) {
                    std::string teamName = rivalFields[0];
                    RivalAI rivalAI;
                    rivalAI.personality.teamName = rivalFields[1];
                    rivalAI.personality.playstyle = rivalFields[2];
                    rivalAI.personality.adaptability = parseIntField(rivalFields[3], 50);
                    rivalAI.personality.tactical_awareness = parseIntField(rivalFields[4], 70);
                    rivalAI.personality.unpredictability = parseIntField(rivalFields[5], 50);
                    int memoryCount = parseIntField(rivalFields[6], 0);
                    
                    for (int j = 0; j < memoryCount; ++j) {
                        if (!getline(file, line) || line.rfind("MEMORY ", 0) != 0) return false;
                        auto memoryFields = splitEscapedFields(line.substr(7), '|');
                        if (memoryFields.size() >= 5) {
                            RivalMemory memory;
                            memory.opponentName = memoryFields[0];
                            memory.matchesPlayed = parseIntField(memoryFields[1], 0);
                            memory.wins = parseIntField(memoryFields[2], 0);
                            memory.draws = parseIntField(memoryFields[3], 0);
                            memory.losses = parseIntField(memoryFields[4], 0);

                            if (memoryFields.size() == 6) {
                                memory.lastMatchOutcome = parseIntField(memoryFields[5], 0);
                            } else if (memoryFields.size() == 7) {
                                memory.lastMatchOutcome = parseIntField(memoryFields[5], 0);
                                memory.consecutiveVsThisTeam = parseIntField(memoryFields[6], 0);
                            } else if (memoryFields.size() >= 11) {
                                memory.favoredFormations = decodeStringList(memoryFields[5]);
                                memory.favoredTactics = decodeStringList(memoryFields[6]);
                                memory.commonPlayPattern = memoryFields[7];
                                memory.identifiedWeaknesses = decodeStringList(memoryFields[8]);
                                memory.identifiedStrengths = decodeStringList(memoryFields[9]);
                                memory.lastMatchOutcome = parseIntField(memoryFields[10], 0);
                                memory.consecutiveVsThisTeam = parseIntField(memoryFields[11], 0);
                            }
                            rivalAI.memoryBank.push_back(memory);
                        }
                    }
                    career.rivalAIMap[teamName] = rivalAI;
                }
            }
            if (!getline(file, teamsLine)) return false;
        }
        
        // Deserialize rivalry dynamics
        career.rivalryDynamics = RivalryDynamics{};
        if (teamsLine.rfind("RIVALRY_DYNAMICS ", 0) == 0) {
            int rivalryCount = parseIntField(trim(teamsLine.substr(16)));
            for (int i = 0; i < rivalryCount; ++i) {
                if (!getline(file, line) || line.rfind("RIVALRY ", 0) != 0) return false;
                auto rivalryFields = splitEscapedFields(line.substr(8), '|');
                if (rivalryFields.size() >= 10) {
                    RivalryRecord record;
                    record.team1 = rivalryFields[0];
                    record.team2 = rivalryFields[1];
                    record.encounters = parseIntField(rivalryFields[2], 0);
                    record.team1Wins = parseIntField(rivalryFields[3], 0);
                    record.team1Draws = parseIntField(rivalryFields[4], 0);
                    record.team1Losses = parseIntField(rivalryFields[5], 0);
                    record.lastMeetingWeek = parseIntField(rivalryFields[6], 0);
                    record.intensity = parseIntField(rivalryFields[7], 0);
                    record.isLocalRivalry = parseBoolField(rivalryFields[8]);
                    record.isHistoricRivalry = parseBoolField(rivalryFields[9]);
                    career.rivalryDynamics.rivalries.push_back(record);
                }
            }
            if (!getline(file, teamsLine)) return false;
        }
        
        if (teamsLine.rfind("TEAMS ", 0) != 0) return false;
        int teamCount = parseIntField(trim(teamsLine.substr(6)));

        allTeams.clear();
        for (int i = 0; i < teamCount; ++i) {
            if (!getline(file, line) || line.rfind("TEAM ", 0) != 0) return false;
            auto fields = splitEscapedFields(line.substr(5), '|');
            if (fields.size() < 11) return false;

            Team team(fields[0]);
            team.division = canonicalizeDivisionValue(fields[1]);
            team.tactics = fields[2];
            team.formation = fields[3];
            team.budget = parseLongField(fields[4]);
            size_t base = 5;
            if (fields.size() >= 12) {
                team.morale = clampInt(parseIntField(fields[5], 50), 0, 100);
                base = 6;
            } else {
                team.morale = 50;
            }
            team.points = parseIntField(fields[base + 0]);
            team.goalsFor = parseIntField(fields[base + 1]);
            team.goalsAgainst = parseIntField(fields[base + 2]);
            team.wins = parseIntField(fields[base + 3]);
            team.draws = parseIntField(fields[base + 4]);
            team.losses = parseIntField(fields[base + 5]);
            size_t tfIndex = base + 6;
            if (fields.size() > tfIndex) team.trainingFocus = fields[tfIndex];
            else team.trainingFocus = "Balanceado";
            size_t extra = tfIndex + 1;
            if (fields.size() > extra) team.awayGoals = parseIntField(fields[extra]); else team.awayGoals = 0;
            if (fields.size() > extra + 1) team.redCards = parseIntField(fields[extra + 1]); else team.redCards = 0;
            if (fields.size() > extra + 2) team.yellowCards = parseIntField(fields[extra + 2]); else team.yellowCards = 0;
            if (fields.size() > extra + 3) team.tiebreakerSeed = parseIntField(fields[extra + 3], randInt(0, 1000000)); else team.tiebreakerSeed = randInt(0, 1000000);
            if (fields.size() > extra + 4) decodeHeadToHead(fields[extra + 4], team); else team.headToHead.clear();
            if (fields.size() > extra + 5) team.pressingIntensity = clampInt(parseIntField(fields[extra + 5], 3), 1, 5); else team.pressingIntensity = 3;
            if (fields.size() > extra + 6) team.defensiveLine = clampInt(parseIntField(fields[extra + 6], 3), 1, 5); else team.defensiveLine = 3;
            if (fields.size() > extra + 7) team.tempo = clampInt(parseIntField(fields[extra + 7], 3), 1, 5); else team.tempo = 3;
            if (fields.size() > extra + 8) team.width = clampInt(parseIntField(fields[extra + 8], 3), 1, 5); else team.width = 3;
            if (fields.size() > extra + 9) team.markingStyle = fields[extra + 9]; else team.markingStyle = "Zonal";
            if (fields.size() > extra + 10) team.preferredXI = decodeStringList(fields[extra + 10]); else team.preferredXI.clear();
            if (fields.size() > extra + 11) team.preferredBench = decodeStringList(fields[extra + 11]); else team.preferredBench.clear();
            if (fields.size() > extra + 12) team.captain = fields[extra + 12]; else team.captain.clear();
            if (fields.size() > extra + 13) team.penaltyTaker = fields[extra + 13]; else team.penaltyTaker.clear();
            if (fields.size() > extra + 14) team.freeKickTaker = fields[extra + 14]; else team.freeKickTaker.clear();
            if (fields.size() > extra + 15) team.cornerTaker = fields[extra + 15]; else team.cornerTaker.clear();
            if (fields.size() > extra + 16) team.rotationPolicy = fields[extra + 16]; else team.rotationPolicy = "Balanceado";
            if (fields.size() > extra + 17) team.assistantCoach = clampInt(parseIntField(fields[extra + 17], 55), 1, 99); else team.assistantCoach = 55;
            if (fields.size() > extra + 18) team.fitnessCoach = clampInt(parseIntField(fields[extra + 18], 55), 1, 99); else team.fitnessCoach = 55;
            if (fields.size() > extra + 19) team.scoutingChief = clampInt(parseIntField(fields[extra + 19], 55), 1, 99); else team.scoutingChief = 55;
            if (fields.size() > extra + 20) team.youthCoach = clampInt(parseIntField(fields[extra + 20], 55), 1, 99); else team.youthCoach = 55;
            if (fields.size() > extra + 21) team.medicalTeam = clampInt(parseIntField(fields[extra + 21], 55), 1, 99); else team.medicalTeam = 55;
            const bool hasExpandedWorldFields = fields.size() > extra + 35;
            if (hasExpandedWorldFields && fields.size() > extra + 22) team.goalkeepingCoach = clampInt(parseIntField(fields[extra + 22], 55), 1, 99); else team.goalkeepingCoach = 55;
            if (hasExpandedWorldFields && fields.size() > extra + 23) team.performanceAnalyst = clampInt(parseIntField(fields[extra + 23], 55), 1, 99); else team.performanceAnalyst = 55;
            if (fields.size() > extra + (hasExpandedWorldFields ? 24 : 22)) team.youthRegion = fields[extra + (hasExpandedWorldFields ? 24 : 22)]; else team.youthRegion = "Metropolitana";
            if (fields.size() > extra + (hasExpandedWorldFields ? 25 : 23)) team.debt = parseLongField(fields[extra + (hasExpandedWorldFields ? 25 : 23)]); else team.debt = 0;
            if (fields.size() > extra + (hasExpandedWorldFields ? 26 : 24)) team.sponsorWeekly = parseLongField(fields[extra + (hasExpandedWorldFields ? 26 : 24)], 25000); else team.sponsorWeekly = 25000;
            if (fields.size() > extra + (hasExpandedWorldFields ? 27 : 25)) team.stadiumLevel = parseIntField(fields[extra + (hasExpandedWorldFields ? 27 : 25)], 1); else team.stadiumLevel = 1;
            if (fields.size() > extra + (hasExpandedWorldFields ? 28 : 26)) team.youthFacilityLevel = parseIntField(fields[extra + (hasExpandedWorldFields ? 28 : 26)], 1); else team.youthFacilityLevel = 1;
            if (fields.size() > extra + (hasExpandedWorldFields ? 29 : 27)) team.trainingFacilityLevel = parseIntField(fields[extra + (hasExpandedWorldFields ? 29 : 27)], 1); else team.trainingFacilityLevel = 1;
            if (fields.size() > extra + (hasExpandedWorldFields ? 30 : 28)) team.fanBase = parseIntField(fields[extra + (hasExpandedWorldFields ? 30 : 28)], 12); else team.fanBase = 12;
            if (fields.size() > extra + (hasExpandedWorldFields ? 31 : 29)) team.matchInstruction = fields[extra + (hasExpandedWorldFields ? 31 : 29)]; else team.matchInstruction = "Equilibrado";
            if (fields.size() > extra + (hasExpandedWorldFields ? 32 : 30)) team.clubPrestige = clampInt(parseIntField(fields[extra + (hasExpandedWorldFields ? 32 : 30)]), 1, 99); else team.clubPrestige = 0;
            if (fields.size() > extra + (hasExpandedWorldFields ? 33 : 31)) team.clubStyle = fields[extra + (hasExpandedWorldFields ? 33 : 31)]; else team.clubStyle.clear();
            if (fields.size() > extra + (hasExpandedWorldFields ? 34 : 32)) team.youthIdentity = fields[extra + (hasExpandedWorldFields ? 34 : 32)]; else team.youthIdentity.clear();
            if (fields.size() > extra + (hasExpandedWorldFields ? 35 : 33)) team.primaryRival = fields[extra + (hasExpandedWorldFields ? 35 : 33)]; else team.primaryRival.clear();
            if (hasExpandedWorldFields && fields.size() > extra + 36) team.headCoachName = fields[extra + 36]; else team.headCoachName.clear();
            if (hasExpandedWorldFields && fields.size() > extra + 37) team.headCoachReputation = parseIntField(fields[extra + 37], 50); else team.headCoachReputation = 50;
            if (hasExpandedWorldFields && fields.size() > extra + 38) team.headCoachStyle = fields[extra + 38]; else team.headCoachStyle.clear();
            if (hasExpandedWorldFields && fields.size() > extra + 39) team.jobSecurity = parseIntField(fields[extra + 39], 58); else team.jobSecurity = 58;
            if (hasExpandedWorldFields && fields.size() > extra + 40) team.transferPolicy = fields[extra + 40]; else team.transferPolicy.clear();
            if (hasExpandedWorldFields && fields.size() > extra + 41) team.scoutingRegions = decodeStringList(fields[extra + 41]); else team.scoutingRegions.clear();
            const bool hasNamedStaffFields = fields.size() > extra + 42;
            if (hasNamedStaffFields && fields.size() > extra + 42) team.assistantCoachName = fields[extra + 42]; else team.assistantCoachName.clear();
            if (hasNamedStaffFields && fields.size() > extra + 43) team.fitnessCoachName = fields[extra + 43]; else team.fitnessCoachName.clear();
            if (hasNamedStaffFields && fields.size() > extra + 44) team.scoutingChiefName = fields[extra + 44]; else team.scoutingChiefName.clear();
            if (hasNamedStaffFields && fields.size() > extra + 45) team.youthCoachName = fields[extra + 45]; else team.youthCoachName.clear();
            if (hasNamedStaffFields && fields.size() > extra + 46) team.medicalChiefName = fields[extra + 46]; else team.medicalChiefName.clear();
            if (hasNamedStaffFields && fields.size() > extra + 47) team.goalkeepingCoachName = fields[extra + 47]; else team.goalkeepingCoachName.clear();
            if (hasNamedStaffFields && fields.size() > extra + 48) team.performanceAnalystName = fields[extra + 48]; else team.performanceAnalystName.clear();
            if (hasNamedStaffFields && fields.size() > extra + 49) team.headCoachTenureWeeks = parseIntField(fields[extra + 49], 0); else team.headCoachTenureWeeks = 0;
            if (fields.size() <= extra + 19) {
                team.preferredBench.clear();
                if (fields.size() > extra + 11) team.captain = fields[extra + 11];
                if (fields.size() > extra + 12) team.penaltyTaker = fields[extra + 12];
                if (fields.size() > extra + 13) team.rotationPolicy = fields[extra + 13];
                if (fields.size() > extra + 14) team.debt = parseLongField(fields[extra + 14]);
                if (fields.size() > extra + 15) team.sponsorWeekly = parseLongField(fields[extra + 15], 25000);
                if (fields.size() > extra + 16) team.stadiumLevel = parseIntField(fields[extra + 16], 1);
                if (fields.size() > extra + 17) team.youthFacilityLevel = parseIntField(fields[extra + 17], 1);
                if (fields.size() > extra + 18) team.trainingFacilityLevel = parseIntField(fields[extra + 18], 1);
                if (fields.size() > extra + 19) team.fanBase = parseIntField(fields[extra + 19], 12);
                team.freeKickTaker = team.penaltyTaker;
                team.cornerTaker = team.penaltyTaker;
                team.assistantCoach = team.fitnessCoach = team.scoutingChief = team.youthCoach = team.medicalTeam = 55;
                team.youthRegion = "Metropolitana";
                team.matchInstruction = "Equilibrado";
            }
            const int loadedPrestige = team.clubPrestige;
            const string loadedClubStyle = team.clubStyle;
            const string loadedYouthIdentity = team.youthIdentity;
            const string loadedPrimaryRival = team.primaryRival;
            ensureTeamIdentity(team);
            if (loadedPrestige > 0) team.clubPrestige = loadedPrestige;
            if (!loadedClubStyle.empty()) team.clubStyle = loadedClubStyle;
            if (!loadedYouthIdentity.empty()) team.youthIdentity = loadedYouthIdentity;
            if (!loadedPrimaryRival.empty()) team.primaryRival = loadedPrimaryRival;

            if (!getline(file, line) || line.rfind("PLAYERS ", 0) != 0) return false;
            int playersCount = parseIntField(trim(line.substr(8)));
            for (int j = 0; j < playersCount; ++j) {
                if (!getline(file, line) || line.rfind("PLAYER ", 0) != 0) return false;
                auto playerFields = splitEscapedFields(line.substr(7), '|');
                if (playerFields.size() < 13) continue;
                team.addPlayer(decodePlayerFields(playerFields));
            }
            if (!getline(file, line) || trim(line) != "ENDTEAM") return false;
            allTeams.push_back(std::move(team));
        }

        for (auto& team : allTeams) {
            team.division = canonicalizeDivisionValue(team.division);
        }
        rebuildCareerDivisions(career);

        initialized = true;
        const string parsedDivision = activeDivision.empty() && !allTeams.empty()
                                          ? canonicalizeDivisionValue(allTeams.front().division)
                                          : canonicalizeDivisionValue(activeDivision);
        if (!parsedDivision.empty()) {
            career.setActiveDivision(parsedDivision);
        }

        if (humanManagers.empty() && !myTeamName.empty()) {
            humanManagers.push_back({managerName.empty() ? "Manager" : managerName,
                                     myTeamName,
                                     clampInt(managerReputation <= 0 ? 50 : managerReputation, 1, 100)});
            activeHumanManagerIndex = 0;
        }

        career.applyActiveHumanManager();
        if (!myTeam) {
            for (auto& team : allTeams) {
                if (team.name == myTeamName) {
                    myTeam = &team;
                    break;
                }
            }
        }
        const string resolvedDivision = myTeam ? canonicalizeDivisionValue(myTeam->division) : activeDivision;
        if (!resolvedDivision.empty()) career.setActiveDivision(resolvedDivision);
        career.syncActiveHumanManager();
        if (boardExpectedFinish <= 0) career.initializeBoardObjectives();
        if (boardMonthlyObjective.empty()) career.initializeDynamicObjective();
        if (!cupActive && career.getActiveTeamCount() > 0) career.initializeSeasonCup();
        if (hasIntegrity && !validateLoadedIntegrity(career, expectedIntegrity)) return false;
        if (!validateLoadedCareerBasics(career)) return false;
        return true;
    }

    file.clear();
    file.seekg(0);
    int season = 1;
    int week = 1;
    if (!(file >> season >> week)) return false;
    currentSeason = season;
    currentWeek = week;
    managerName = "Manager";
    managerReputation = 50;
    humanManagers.clear();
    activeHumanManagerIndex = 0;
    newsFeed.clear();
    managerInbox.clear();
    scoutInbox.clear();
    scoutingShortlist.clear();
    history.clear();
    pendingTransfers.clear();
    boardMonthlyObjective.clear();
    boardMonthlyTarget = 0;
    boardMonthlyProgress = 0;
    boardMonthlyDeadlineWeek = 0;
    cupActive = false;
    cupRound = 0;
    cupRemainingTeams.clear();
    cupChampion.clear();
    lastMatchAnalysis.clear();
    lastMatchReportLines.clear();
    lastMatchEvents.clear();
    lastMatchPlayerOfTheMatch.clear();
    lastMatchCenter = MatchCenterSnapshot{};
    string teamName;
    getline(file, teamName);
    getline(file, teamName);

    career.initializeLeague(true);
    myTeam = nullptr;
    for (auto& team : allTeams) {
        if (team.name == teamName) {
            myTeam = &team;
            break;
        }
    }

    if (myTeam) {
        myTeam->players.clear();
        string line2;
        getline(file, line2);
        while (getline(file, line2)) {
            if (line2.empty()) continue;
            stringstream ss(line2);
            string token;
            Player player;
            getline(ss, player.name, ',');
            getline(ss, player.position, ',');
            try {
                getline(ss, token, ','); player.attack = stoi(token);
                getline(ss, token, ','); player.defense = stoi(token);
                getline(ss, token, ','); player.stamina = stoi(token);
                player.fitness = player.stamina;
                getline(ss, token, ','); player.skill = stoi(token);
                player.potential = clampInt(player.skill + randInt(0, 6), player.skill, 95);
                getline(ss, token, ','); player.age = stoi(token);
                getline(ss, token, ','); player.value = stoll(token);
                player.onLoan = false;
                player.parentClub.clear();
                player.loanWeeksRemaining = 0;
                string injuredStr;
                getline(ss, injuredStr, ',');
                injuredStr = toLower(trim(injuredStr));
                player.injured = (injuredStr == "1" || injuredStr == "true");
                player.injuryType = player.injured ? "Leve" : "";
                getline(ss, token, ','); player.injuryWeeks = stoi(token);
                getline(ss, token, ','); player.goals = stoi(token);
                getline(ss, token, ','); player.assists = stoi(token);
                getline(ss, token, ','); player.matchesPlayed = stoi(token);
                player.lastTrainedSeason = -1;
                player.lastTrainedWeek = -1;
                player.wage = static_cast<long long>(player.skill) * 150 + randInt(0, 800);
                player.releaseClause = max(50000LL, player.value * 2);
                player.setPieceSkill = clampInt(player.skill + randInt(-6, 6), 25, 99);
                player.leadership = clampInt(35 + randInt(0, 45), 1, 99);
                player.professionalism = clampInt(40 + randInt(0, 45), 1, 99);
                player.ambition = clampInt(35 + randInt(0, 50), 1, 99);
                player.happiness = clampInt(55 + randInt(-10, 20), 1, 99);
                player.chemistry = clampInt(45 + randInt(0, 35), 1, 99);
                player.desiredStarts = 1;
                player.startsThisSeason = 0;
                player.wantsToLeave = false;
                player.contractWeeks = randInt(52, 156);
                player.injuryHistory = 0;
                player.yellowAccumulation = 0;
                player.seasonYellowCards = 0;
                player.seasonRedCards = 0;
                player.matchesSuspended = 0;
                ensurePlayerProfile(player, true);
                myTeam->addPlayer(player);
            } catch (...) {
            }
        }
    }

    for (auto& team : allTeams) {
        team.division = canonicalizeDivisionValue(team.division);
    }
    rebuildCareerDivisions(career);
    const string legacyDivision = myTeam ? canonicalizeDivisionValue(myTeam->division)
                                         : (divisions.empty() ? string() : canonicalizeDivisionValue(divisions.front().id));
    if (!legacyDivision.empty()) career.setActiveDivision(legacyDivision);
    career.syncActiveHumanManager();
    career.initializeBoardObjectives();
    career.initializeSeasonCup();
    career.initializeDynamicObjective();
    return true;
    } catch (...) {
        return false;
    }
}

}  // namespace save_serialization
