#include "validators.h"

#include "competition.h"
#include "io/io.h"
#include "io/save_serialization.h"
#include "models.h"
#include "utils.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using namespace std;

struct ValidationResult {
    string name;
    bool ok;
    string detail;
};

struct RawPlayerRecord {
    string name;
    string position;
    string positionRaw;
    int age = 0;
    bool hasAge = false;
    string ageRaw;
    long long marketValue = 0;
    bool hasMarketValue = false;
    string sourceFile;
};

struct RawTeamRecord {
    string displayName;
    string folderName;
    string folderPath;
    string sourceFile;
    vector<RawPlayerRecord> players;
};

struct DuplicatePlayerOccurrence {
    string teamLabel;
    string position;
    int age = 0;
    bool hasAge = false;
};

struct CachedRosterAudit {
    bool ready = false;
    DataValidationReport report;
};

static int currentProcessId() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static string runtimeValidationSavePath() {
    static atomic<unsigned long long> counter{0};
    const auto stamp = chrono::steady_clock::now().time_since_epoch().count();
    return "saves/validation_career_save_runtime_" +
           to_string(currentProcessId()) + "_" +
           to_string(stamp) + "_" +
           to_string(counter.fetch_add(1)) + ".txt";
}

static CachedRosterAudit& rosterAuditCache() {
    static CachedRosterAudit cache;
    return cache;
}
static recursive_mutex& startupValidationMutex() {
    static recursive_mutex mutex;
    return mutex;
}

class ScopedValidationBuildFlag {
public:
    explicit ScopedValidationBuildFlag(bool& flag)
        : flag_(flag) {
        flag_ = true;
    }

    ~ScopedValidationBuildFlag() {
        flag_ = false;
    }

    ScopedValidationBuildFlag(const ScopedValidationBuildFlag&) = delete;
    ScopedValidationBuildFlag& operator=(const ScopedValidationBuildFlag&) = delete;

private:
    bool& flag_;
};

struct IgnoredFolderCache {
    bool loaded = false;
    set<string> keys;
};

struct DuplicateWhitelistCache {
    bool loaded = false;
    set<string> names;
};

static IgnoredFolderCache& ignoredFolderCache() {
    static IgnoredFolderCache cache;
    return cache;
}

static DuplicateWhitelistCache& duplicateWhitelistCache() {
    static DuplicateWhitelistCache cache;
    return cache;
}

static const set<string>& ignoredTeamFolders() {
    IgnoredFolderCache& cache = ignoredFolderCache();
    if (cache.loaded) return cache.keys;
    cache.loaded = true;

    vector<string> lines;
    if (!readTextFileLines("data/configs/ignored_team_folders.csv", lines) || lines.size() <= 1) {
        return cache.keys;
    }
    for (size_t i = 1; i < lines.size(); ++i) {
        const string line = trim(lines[i]);
        if (line.empty()) continue;
        const vector<string> cols = splitCsvLine(line);
        if (cols.size() < 2) continue;
        const string division = toLower(trim(cols[0]));
        const string folder = normalizeTeamId(cols[1]);
        if (division.empty() || folder.empty()) continue;
        cache.keys.insert(division + "|" + folder);
    }
    return cache.keys;
}

static bool isIgnoredTeamFolder(const string& divisionId, const string& folderName) {
    const string key = toLower(trim(divisionId)) + "|" + normalizeTeamId(folderName);
    return ignoredTeamFolders().find(key) != ignoredTeamFolders().end();
}

static const set<string>& duplicatePlayerWhitelist() {
    DuplicateWhitelistCache& cache = duplicateWhitelistCache();
    if (cache.loaded) return cache.names;
    cache.loaded = true;

    vector<string> lines;
    if (!readTextFileLines("data/configs/duplicate_player_whitelist.csv", lines) || lines.size() <= 1) {
        return cache.names;
    }
    for (size_t i = 1; i < lines.size(); ++i) {
        const string line = trim(lines[i]);
        if (line.empty()) continue;
        const vector<string> cols = splitCsvLine(line);
        if (cols.empty()) continue;
        const string name = toLower(trim(cols[0]));
        if (!name.empty()) cache.names.insert(name);
    }
    return cache.names;
}

static bool isWhitelistedDuplicatePlayer(const string& playerName) {
    const string key = toLower(trim(playerName));
    return !key.empty() && duplicatePlayerWhitelist().find(key) != duplicatePlayerWhitelist().end();
}

static bool likelyLegitHomonym(const vector<DuplicatePlayerOccurrence>& occurrences) {
    if (occurrences.size() < 2) return false;
    bool ageGap = false;
    bool roleGap = false;
    for (size_t i = 0; i < occurrences.size(); ++i) {
        for (size_t j = i + 1; j < occurrences.size(); ++j) {
            if (occurrences[i].hasAge && occurrences[j].hasAge && abs(occurrences[i].age - occurrences[j].age) >= 2) {
                ageGap = true;
            }
            const string leftPos = normalizePosition(occurrences[i].position);
            const string rightPos = normalizePosition(occurrences[j].position);
            if (leftPos != "N/A" && rightPos != "N/A" && leftPos != rightPos) roleGap = true;
        }
    }
    return ageGap || roleGap;
}

static bool hasSuspiciousEncoding(const string& text) {
    return text.find("Ã") != string::npos || text.find("Â") != string::npos ||
           text.find("â") != string::npos || text.find("�") != string::npos;
}

static pair<bool, int> parseRawAge(const string& raw) {
    string value = trim(raw);
    if (value.empty() || value == "N/A" || value == "-") return {false, 0};
    try {
        return {true, stoi(value)};
    } catch (...) {
        return {false, 0};
    }
}

static bool parseRawRosterCsv(const string& path, RawTeamRecord& team) {
    vector<string> lines;
    if (!readTextFileLines(path, lines) || lines.empty()) return false;
    team.sourceFile = path;
    for (size_t i = 1; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        if (line.empty()) continue;
        vector<string> cols = splitCsvLine(line);
        if (cols.size() < 6) continue;
        RawPlayerRecord player;
        player.name = trim(cols[0]);
        player.position = trim(cols[1]);
        player.positionRaw = trim(cols[2]);
        player.ageRaw = trim(cols[3]);
        pair<bool, int> age = parseRawAge(player.ageRaw);
        player.hasAge = age.first;
        player.age = age.second;
        player.marketValue = parseMarketValue(cols[5]);
        player.hasMarketValue = trim(cols[5]) != "" && trim(cols[5]) != "-" && trim(cols[5]) != "N/A";
        player.sourceFile = path;
        team.players.push_back(player);
    }
    return true;
}

static bool parseRawRosterTxt(const string& path, RawTeamRecord& team) {
    vector<string> lines;
    if (!readTextFileLines(path, lines)) return false;
    team.sourceFile = path;
    for (string line : lines) {
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '-') line = trim(line.substr(1));
        vector<string> parts = splitByDelimiter(line, '|');
        if (parts.size() < 2) continue;

        RawPlayerRecord player;
        player.name = trim(parts[0]);
        player.position = trim(parts[1]);
        size_t open = player.position.find('(');
        size_t close = player.position.rfind(')');
        if (open != string::npos && close != string::npos && close > open) {
            player.positionRaw = trim(player.position.substr(open + 1, close - open - 1));
            player.position = trim(player.position.substr(0, open));
        }
        for (const string& part : parts) {
            if (part.find("Edad:") != string::npos) {
                player.ageRaw = trim(part.substr(part.find("Edad:") + 5));
            }
            if (part.find("Valor:") != string::npos) {
                string value = trim(part.substr(part.find("Valor:") + 6));
                player.marketValue = parseMarketValue(value);
                player.hasMarketValue = !value.empty() && value != "-" && value != "N/A";
            }
        }
        pair<bool, int> age = parseRawAge(player.ageRaw);
        player.hasAge = age.first;
        player.age = age.second;
        player.sourceFile = path;
        team.players.push_back(player);
    }
    return true;
}

static bool parseRawRosterJson(const string& path, RawTeamRecord& team) {
    vector<string> lines;
    if (!readTextFileLines(path, lines)) return false;
    team.sourceFile = path;
    RawPlayerRecord current;
    bool inObject = false;
    auto flush = [&]() {
        if (!current.name.empty()) {
            pair<bool, int> age = parseRawAge(current.ageRaw);
            current.hasAge = age.first;
            current.age = age.second;
            current.sourceFile = path;
            team.players.push_back(current);
        }
        current = RawPlayerRecord{};
        inObject = false;
    };
    for (string line : lines) {
        line = trim(line);
        if (line == "{") {
            inObject = true;
            current = RawPlayerRecord{};
            continue;
        }
        if (!inObject) continue;
        if (line == "}," || line == "}") {
            flush();
            continue;
        }
        auto readJsonValue = [&](const string& key) -> string {
            string marker = "\"" + key + "\"";
            size_t pos = line.find(marker);
            if (pos == string::npos) return "";
            pos = line.find(':', pos);
            if (pos == string::npos) return "";
            string value = trim(line.substr(pos + 1));
            if (!value.empty() && value.back() == ',') value.pop_back();
            value = trim(value);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            return value;
        };
        string value;
        value = readJsonValue("name");
        if (!value.empty()) current.name = value;
        value = readJsonValue("position");
        if (!value.empty()) current.position = value;
        value = readJsonValue("position_raw");
        if (!value.empty()) current.positionRaw = value;
        value = readJsonValue("age");
        if (!value.empty()) current.ageRaw = value;
        value = readJsonValue("market_value");
        if (!value.empty()) {
            current.marketValue = parseMarketValue(value);
            current.hasMarketValue = value != "-" && value != "N/A";
        }
    }
    if (inObject) flush();
    return !team.players.empty();
}

static bool loadRawTeamRoster(const string& folderPath, const string& displayName, const string& folderName, RawTeamRecord& team) {
    team.displayName = displayName;
    team.folderName = folderName;
    team.folderPath = folderPath;
    string csv = joinPath(folderPath, "players.csv");
    if (pathExists(csv) && parseRawRosterCsv(csv, team)) return true;
    string txt = joinPath(folderPath, "players.txt");
    if (pathExists(txt) && parseRawRosterTxt(txt, team)) return true;
    string json = joinPath(folderPath, "players.json");
    if (pathExists(json) && parseRawRosterJson(json, team)) return true;
    return false;
}

static string combinedPositionText(const RawPlayerRecord& player) {
    return toLower(trim(player.position + " " + player.positionRaw));
}

static string resolvedRawPosition(const RawPlayerRecord& player) {
    const string directPosition = normalizePosition(player.position);
    const string rawPosition = normalizePosition(player.positionRaw);
    if (directPosition != "N/A" && rawPosition != "N/A") {
        return directPosition == rawPosition ? directPosition : rawPosition;
    }
    if (rawPosition != "N/A") return rawPosition;
    if (directPosition != "N/A") return directPosition;
    return "N/A";
}

static string rawNormalizedPosition(const RawPlayerRecord& player) {
    return resolvedRawPosition(player);
}

static bool isGoalkeeperRole(const RawPlayerRecord& player) {
    return rawNormalizedPosition(player) == "ARQ";
}

static bool isCenterBackRole(const RawPlayerRecord& player) {
    string text = combinedPositionText(player);
    return text.find("centre-back") != string::npos || text.find("center-back") != string::npos ||
           text.find("defensa central") != string::npos || text.find(" zaguero ") != string::npos ||
           text == "cb" || text.find(" cb") != string::npos;
}

static bool isFullBackRole(const RawPlayerRecord& player) {
    string text = combinedPositionText(player);
    return text.find("left-back") != string::npos || text.find("right-back") != string::npos ||
           text.find("full-back") != string::npos || text.find("fullback") != string::npos ||
           text.find("wing-back") != string::npos || text.find("lateral") != string::npos ||
           text.find("carrilero") != string::npos || text.find(" lb") != string::npos ||
           text.find(" rb") != string::npos || text.find("lwb") != string::npos ||
           text.find("rwb") != string::npos;
}

static bool isMidfieldRole(const RawPlayerRecord& player) {
    return rawNormalizedPosition(player) == "MED";
}

static bool isForwardRole(const RawPlayerRecord& player) {
    return rawNormalizedPosition(player) == "DEL";
}

static void addDataIssue(DataValidationReport& report,
                         const string& severity,
                         const string& category,
                         const string& division,
                         const string& team,
                         const string& player,
                         const string& detail,
                         const string& suggestion) {
    DataValidationIssue issue{severity, category, division, team, player, detail, suggestion};
    report.issues.push_back(issue);
    if (severity == "ERROR") report.errorCount++;
    else report.warningCount++;
}

static string formatIssueLine(const DataValidationIssue& issue) {
    string line = "[" + issue.severity + "] " + issue.category + " | " + issue.division;
    if (!issue.team.empty()) line += " | " + issue.team;
    if (!issue.player.empty()) line += " | " + issue.player;
    line += " | " + issue.detail;
    if (!issue.suggestion.empty()) line += " | Sugerencia: " + issue.suggestion;
    return line;
}

static string pairKey(const string& a, const string& b) {
    return (a < b) ? (a + "||" + b) : (b + "||" + a);
}

static ValidationResult validateDivisionCounts(Career& career) {
    for (const auto& div : career.divisions) {
        const CompetitionConfig& config = getCompetitionConfig(div.id);
        int count = static_cast<int>(career.getDivisionTeams(div.id).size());
        if (config.expectedTeamCount > 0 && count != config.expectedTeamCount) {
            return {"Conteo divisiones", false, div.id + ": esperado " + to_string(config.expectedTeamCount) + ", actual " + to_string(count)};
        }
    }
    return {"Conteo divisiones", true, "Conteos esperados validados."};
}

static ValidationResult validateUniqueTeams(Career& career) {
    for (const auto& div : career.divisions) {
        set<string> seen;
        for (auto* team : career.getDivisionTeams(div.id)) {
            string key = toLower(trim(team->name));
            if (!seen.insert(key).second) {
                return {"Equipos unicos", false, "Equipo duplicado en " + div.id + ": " + team->name};
            }
        }
    }
    return {"Equipos unicos", true, "No hay nombres duplicados por division."};
}

static ValidationResult validateRosterCaps(Career& career) {
    for (auto& team : career.allTeams) {
        int maxSquad = getCompetitionConfig(team.division).maxSquadSize;
        if (static_cast<int>(team.players.size()) < 11) {
            return {"Planteles", false, team.name + " tiene menos de 11 jugadores."};
        }
        if (maxSquad > 0 && static_cast<int>(team.players.size()) > maxSquad) {
            return {"Planteles", false, team.name + " excede maximo de plantel."};
        }
    }
    return {"Planteles", true, "Planteles dentro de rangos validos."};
}

static ValidationResult validateRosterStructure(Career& career) {
    int warningCount = 0;
    vector<string> examples;
    for (auto& team : career.allTeams) {
        int goalkeepers = 0;
        int defenders = 0;
        int midfielders = 0;
        int forwards = 0;
        set<string> playerNames;
        for (const auto& player : team.players) {
            string key = toLower(trim(player.name));
            if (!playerNames.insert(key).second) {
                return {"Estructura plantel", false, team.name + " tiene jugadores duplicados: " + player.name};
            }
            string pos = normalizePosition(player.position);
            if (pos == "ARQ") goalkeepers++;
            else if (pos == "DEF") defenders++;
            else if (pos == "MED") midfielders++;
            else if (pos == "DEL") forwards++;
        }
        if (goalkeepers < 1 || defenders < 2 || (midfielders < 2 && forwards < 2)) {
            warningCount++;
            if (examples.size() < 3) {
                string note = team.name + " (ARQ " + to_string(goalkeepers) +
                              ", DEF " + to_string(defenders) +
                              ", MED " + to_string(midfielders) +
                              ", DEL " + to_string(forwards) + ")";
                examples.push_back(note);
            }
        }
    }
    string detail = "Cobertura minima de posiciones revisada.";
    if (warningCount > 0) {
        detail += " Advertencias en " + to_string(warningCount) + " planteles: " + joinStringValues(examples, " | ");
    }
    return {"Estructura plantel", true, detail};
}

static ValidationResult validateSchedules(Career& career) {
    for (const auto& div : career.divisions) {
        career.setActiveDivision(div.id);
        if (career.getActiveTeamCount() < 2) continue;
        if (career.schedule.empty()) {
            return {"Calendarios", false, "Calendario vacio en " + div.id};
        }

        map<string, int> pairCounts;
        map<string, int> homeAwayBalance;
        for (const auto& round : career.schedule) {
            set<int> used;
            for (const auto& match : round) {
                Team* home = career.getActiveTeamAt(match.first);
                Team* away = career.getActiveTeamAt(match.second);
                if (!home || !away) {
                    return {"Calendarios", false, "Indice fuera de rango en " + div.id};
                }
                if (!used.insert(match.first).second || !used.insert(match.second).second) {
                    return {"Calendarios", false, "Equipo repetido en una misma fecha de " + div.id};
                }
                if (career.usesGroupFormat()) {
                    bool homeNorth = find(career.groupNorthIdx.begin(), career.groupNorthIdx.end(), match.first) != career.groupNorthIdx.end();
                    bool awayNorth = find(career.groupNorthIdx.begin(), career.groupNorthIdx.end(), match.second) != career.groupNorthIdx.end();
                    if (homeNorth != awayNorth) {
                        return {"Calendarios", false, "Cruce entre grupos detectado en " + div.id};
                    }
                }
                string key = pairKey(home->name, away->name);
                pairCounts[key]++;
                homeAwayBalance[home->name + "->" + away->name]++;
            }
        }

        if (!career.usesGroupFormat()) {
            for (const auto& entry : pairCounts) {
                if (entry.second != 2) {
                    return {"Calendarios", false, "Pareja sin ida/vuelta en " + div.id + ": " + entry.first};
                }
            }
        }
    }
    return {"Calendarios", true, "Fixtures validados."};
}

static ValidationResult validateSaveLoad() {
    Career career;
    career.initializeLeague(true);
    const auto makeFailure = [](const string& detail) {
        return ValidationResult{"Guardado/carga", false, detail};
    };
    if (career.divisions.empty()) {
        return makeFailure("No hay divisiones para validar.");
    }
    career.setActiveDivision(career.divisions.front().id);
    if (career.getActiveTeamCount() == 0) {
        return makeFailure("No hay equipos en la division inicial.");
    }
    career.myTeam = career.getActiveTeamAt(0);
    career.saveFile = runtimeValidationSavePath();
    const string resolvedValidationSave = resolveProjectPath(career.saveFile);
    auto cleanupValidationSave = [&]() {
        std::remove(resolvedValidationSave.c_str());
        std::remove((resolvedValidationSave + ".tmp").c_str());
        std::remove((resolvedValidationSave + ".bak").c_str());
    };
    cleanupValidationSave();
    auto fail = [&](const string& detail) {
        cleanupValidationSave();
        return makeFailure(detail);
    };
    career.currentSeason = 2;
    career.currentWeek = 3;
    career.resetSeason();
    career.myTeam->assistantCoach = 73;
    career.myTeam->fitnessCoach = 74;
    career.myTeam->scoutingChief = 75;
    career.myTeam->youthCoach = 76;
    career.myTeam->medicalTeam = 77;
    career.myTeam->youthRegion = "Sur";
    career.myTeam->matchInstruction = "Balon parado";
    career.myTeam->clubStyle = "Presion vertical";
    career.myTeam->youthIdentity = "Cantera estructurada";
    career.myTeam->primaryRival = "Rival de prueba";
    career.myTeam->clubPrestige = 81;
    career.boardMonthlyObjective = "Objetivo de prueba";
    career.boardMonthlyTarget = 5;
    career.boardMonthlyProgress = 2;
    career.boardMonthlyDeadlineWeek = 6;
    career.lastMatchAnalysis = "Analisis de prueba";
    if (!career.myTeam->players.empty()) {
        career.myTeam->preferredXI = {career.myTeam->players.front().name};
        career.myTeam->players.front().developmentPlan = "Liderazgo";
        career.myTeam->players.front().promisedRole = "Titular";
        career.myTeam->players.front().traits = {"Lider", "Competidor"};
        career.myTeam->players.front().preferredFoot = "Ambos";
        career.myTeam->players.front().secondaryPositions = {"MED", "DEL"};
        career.myTeam->players.front().consistency = 82;
        career.myTeam->players.front().bigMatches = 79;
        career.myTeam->players.front().currentForm = 74;
        career.myTeam->players.front().tacticalDiscipline = 77;
        career.myTeam->players.front().versatility = 71;
        if (career.myTeam->players.size() > 1) {
            career.myTeam->preferredBench = {career.myTeam->players[1].name};
        }
    }
    career.addNews("Prueba de guardado.");
    career.scoutingShortlist.push_back("Club Prueba|Jugador Prueba");
    if (!career.saveCareer()) {
        return fail("No se pudo escribir el archivo de validacion.");
    }

    ifstream rawSave(resolvedValidationSave);
    string firstLine;
    const string expectedVersionLine = "VERSION " + to_string(save_serialization::currentCareerSaveVersion());
    const bool hasExpectedVersionLine = rawSave.is_open() &&
                                        static_cast<bool>(getline(rawSave, firstLine)) &&
                                        trim(firstLine) == expectedVersionLine;
    rawSave.close();
    if (!hasExpectedVersionLine) {
        return fail("El archivo guardado no incluye version de save.");
    }

    Career loaded;
    loaded.saveFile = career.saveFile;
    loaded.initializeLeague(true);
    if (!loaded.loadCareer()) {
        return fail("No pudo recargar el archivo de validacion.");
    }
    if (!loaded.myTeam || !career.myTeam || loaded.myTeam->name != career.myTeam->name) {
        return fail("El equipo cargado no coincide.");
    }
    if (loaded.currentSeason != career.currentSeason || loaded.currentWeek != career.currentWeek) {
        return fail("Temporada o semana no coinciden tras carga.");
    }
    if (loaded.myTeam->assistantCoach != career.myTeam->assistantCoach ||
        loaded.myTeam->medicalTeam != career.myTeam->medicalTeam ||
        loaded.myTeam->youthRegion != career.myTeam->youthRegion ||
        loaded.myTeam->matchInstruction != career.myTeam->matchInstruction ||
        loaded.myTeam->clubStyle != career.myTeam->clubStyle ||
        loaded.myTeam->youthIdentity != career.myTeam->youthIdentity ||
        loaded.myTeam->primaryRival != career.myTeam->primaryRival) {
        return fail("No se preservaron correctamente los datos de staff.");
    }
    if (!loaded.myTeam->players.empty() && !career.myTeam->players.empty()) {
        const Player& loadedPlayer = loaded.myTeam->players.front();
        const Player& savedPlayer = career.myTeam->players.front();
        if (loadedPlayer.developmentPlan != savedPlayer.developmentPlan ||
            loadedPlayer.promisedRole != savedPlayer.promisedRole ||
            loadedPlayer.traits != savedPlayer.traits ||
            loadedPlayer.preferredFoot != savedPlayer.preferredFoot ||
            loadedPlayer.secondaryPositions != savedPlayer.secondaryPositions ||
            loadedPlayer.consistency != savedPlayer.consistency ||
            loadedPlayer.bigMatches != savedPlayer.bigMatches ||
            loadedPlayer.currentForm != savedPlayer.currentForm ||
            loadedPlayer.tacticalDiscipline != savedPlayer.tacticalDiscipline ||
            loadedPlayer.versatility != savedPlayer.versatility) {
            return fail("No se preservaron correctamente los datos avanzados del jugador.");
        }
    }
    if (loaded.boardMonthlyObjective != career.boardMonthlyObjective ||
        loaded.lastMatchAnalysis != career.lastMatchAnalysis) {
        return fail("No se preservaron correctamente datos avanzados de carrera.");
    }
    if (loaded.scoutingShortlist != career.scoutingShortlist) {
        return fail("No se preservo correctamente la shortlist de scouting.");
    }
    cleanupValidationSave();
    return {"Guardado/carga", true, "Guardado y carga basicos verificados."};
}

static ValidationResult validateTableSorting() {
    Team a("A");
    Team b("B");
    Team c("C");
    a.points = 10; a.goalsFor = 8; a.goalsAgainst = 4; a.wins = 3;
    b.points = 10; b.goalsFor = 7; b.goalsAgainst = 4; b.wins = 2;
    c.points = 9; c.goalsFor = 9; c.goalsAgainst = 5; c.wins = 2;
    a.division = "primera b";
    b.division = "primera b";
    c.division = "primera b";
    LeagueTable table;
    table.ruleId = "primera b";
    table.addTeam(&b);
    table.addTeam(&c);
    table.addTeam(&a);
    table.sortTable();
    if (table.teams.size() != 3 || table.teams[0] != &a || table.teams[1] != &b || table.teams[2] != &c) {
        return {"Desempates", false, "El orden de tabla esperado no coincide."};
    }
    return {"Desempates", true, "Orden de tabla base validado."};
}

RuntimeValidationSummary validateLoadedCareerData(const Career& career, size_t maxLines) {
    RuntimeValidationSummary summary{};
    summary.ok = true;
    summary.errorCount = 0;
    summary.warningCount = 0;
    summary.lines.push_back("Validacion automatica de carga");

    auto pushLine = [&](bool error, const string& line) {
        if (error) summary.errorCount++;
        else summary.warningCount++;
        if (summary.lines.size() <= maxLines) {
            summary.lines.push_back(string(error ? "[ERROR] " : "[WARN] ") + line);
        }
    };

    for (const string& warning : competitionConfigWarnings()) {
        pushLine(false, warning);
    }

    for (const DivisionInfo& division : career.divisions) {
        int teamCount = 0;
        for (const Team& team : career.allTeams) {
            if (team.division == division.id) teamCount++;
        }
        const CompetitionConfig& config = getCompetitionConfig(division.id);
        if (config.expectedTeamCount > 0 && teamCount != config.expectedTeamCount) {
            pushLine(true,
                     division.id + ": cantidad de equipos cargados " + to_string(teamCount) +
                         " no coincide con la regla " + to_string(config.expectedTeamCount) + ".");
        }
    }

    set<string> globalTeamKeys;
    for (const Team& team : career.allTeams) {
        const string teamKey = toLower(trim(team.division + "|" + team.name));
        if (!globalTeamKeys.insert(teamKey).second) {
            pushLine(true, team.division + ": equipo duplicado en memoria -> " + team.name + ".");
        }

        const int maxSquad = getCompetitionConfig(team.division).maxSquadSize;
        if (static_cast<int>(team.players.size()) < 11) {
            pushLine(true, team.name + ": plantilla insuficiente para simular.");
        }
        if (maxSquad > 0 && static_cast<int>(team.players.size()) > maxSquad) {
            pushLine(true, team.name + ": excede el maximo de plantel de la division.");
        }

        int goalkeepers = 0;
        int defenders = 0;
        int midfielders = 0;
        int forwards = 0;
        set<string> names;
        for (const Player& player : team.players) {
            const string playerKey = toLower(trim(player.name));
            if (!names.insert(playerKey).second) {
                pushLine(true, team.name + ": jugador duplicado -> " + player.name + ".");
            }
            const string pos = normalizePosition(player.position);
            if (pos == "ARQ") goalkeepers++;
            else if (pos == "DEF") defenders++;
            else if (pos == "MED") midfielders++;
            else if (pos == "DEL") forwards++;
            else pushLine(true, team.name + ": jugador sin posicion valida -> " + player.name + ".");
        }

        if (goalkeepers < 1) pushLine(true, team.name + ": no tiene arquero disponible.");
        if (defenders < 2) pushLine(false, team.name + ": defensa demasiado corta (" + to_string(defenders) + ").");
        if (midfielders < 2) pushLine(false, team.name + ": mediocampo demasiado corto (" + to_string(midfielders) + ").");
        if (forwards < 1) pushLine(false, team.name + ": falta profundidad ofensiva.");

        for (const string& preferred : team.preferredXI) {
            bool exists = false;
            for (const Player& player : team.players) {
                if (player.name == preferred) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                pushLine(false, team.name + ": preferredXI referencia un jugador ausente -> " + preferred + ".");
            }
        }
    }

    if (summary.errorCount == 0 && summary.warningCount == 0) {
        summary.lines.push_back("[OK] La carga no detecto inconsistencias estructurales.");
    } else if (summary.lines.size() > maxLines + 1) {
        const int omitted = static_cast<int>(summary.lines.size() - (maxLines + 1));
        summary.lines.resize(maxLines + 1);
        summary.lines.push_back("... " + to_string(omitted) + " incidencia(s) adicionales omitidas.");
    }
    summary.ok = (summary.errorCount == 0);
    return summary;
}

DataValidationReport buildRosterDataValidationReport() {
    DataValidationReport report{};
    report.ok = true;
    report.divisionsScanned = 0;
    report.teamsScanned = 0;
    report.playersScanned = 0;
    report.errorCount = 0;
    report.warningCount = 0;

    map<string, vector<DuplicatePlayerOccurrence> > playerTeams;

    for (const DivisionInfo& div : kDivisions) {
        if (!isDirectory(div.folder)) continue;
        report.divisionsScanned++;

        vector<pair<string, string> > listedTeams;
        if (!loadTeamsList(div.folder, listedTeams)) {
            vector<string> dirs = listDirectories(div.folder);
            sort(dirs.begin(), dirs.end(), [](const string& a, const string& b) {
                return toLower(pathFilename(a)) < toLower(pathFilename(b));
            });
            for (const string& dir : dirs) {
                listedTeams.push_back({pathFilename(dir), pathFilename(dir)});
            }
        }

        const CompetitionConfig& config = getCompetitionConfig(div.id);
        if (config.expectedTeamCount > 0 && static_cast<int>(listedTeams.size()) != config.expectedTeamCount) {
            addDataIssue(report,
                         "ERROR",
                         "Integridad liga",
                         div.id,
                         "",
                         "",
                         "La division declara " + to_string(listedTeams.size()) + " equipos en teams.txt y se esperan " +
                             to_string(config.expectedTeamCount) + ".",
                         "Corregir teams.txt o completar/eliminar equipos hasta alcanzar el formato oficial.");
        }

        set<string> listedIds;
        for (const auto& entry : listedTeams) {
            const string listedFolderName = entry.second.empty() ? entry.first : entry.second;
            listedIds.insert(normalizeTeamId(listedFolderName));
        }
        vector<string> actualDirs = listDirectories(div.folder);
        for (const string& dir : actualDirs) {
            string folderName = pathFilename(dir);
            string folderId = normalizeTeamId(folderName);
            if (isIgnoredTeamFolder(div.id, folderName)) continue;
            if (!folderId.empty() && listedIds.find(folderId) == listedIds.end()) {
                addDataIssue(report,
                             "WARNING",
                             "Integridad liga",
                             div.id,
                             folderName,
                             "",
                             "Existe una carpeta de equipo no referenciada en teams.txt.",
                             "Agregarla a teams.txt o retirarla si ya no forma parte de la base.");
            }
        }

        for (const auto& entry : listedTeams) {
            const string displayName = entry.first;
            const string folderName = entry.second.empty() ? entry.first : entry.second;
            const string folderPath = joinPath(div.folder, folderName);
            report.teamsScanned++;

            if (!isDirectory(folderPath)) {
                addDataIssue(report,
                             "ERROR",
                             "Integridad liga",
                             div.id,
                             displayName,
                             "",
                             "La carpeta del equipo no existe.",
                             "Crear la carpeta del club o corregir el nombre en teams.txt.");
                continue;
            }

            RawTeamRecord team;
            if (!loadRawTeamRoster(folderPath, displayName, folderName, team)) {
                addDataIssue(report,
                             "ERROR",
                             "Plantilla",
                             div.id,
                             displayName,
                             "",
                             "No existe un archivo de plantilla valido (players.csv / players.txt / players.json) o la plantilla quedo vacia.",
                             "Agregar un roster valido antes de intentar simular la division.");
                continue;
            }

            const int squadSize = static_cast<int>(team.players.size());
            report.playersScanned += squadSize;
            if (squadSize < 18) {
                addDataIssue(report,
                             squadSize < 15 ? "ERROR" : "WARNING",
                             "Tamano plantilla",
                             div.id,
                             displayName,
                             "",
                             "Plantilla corta con " + to_string(squadSize) + " jugadores.",
                             "Subir el roster al menos a 18 y preferir un rango de 22 a 25.");
            } else if (squadSize > 30) {
                addDataIssue(report,
                             "WARNING",
                             "Tamano plantilla",
                             div.id,
                             displayName,
                             "",
                             "Plantilla sobredimensionada con " + to_string(squadSize) + " jugadores.",
                             "Reducir duplicados o excedentes por posicion.");
            }

            int goalkeepers = 0;
            int centerBacks = 0;
            int fullBacks = 0;
            int midfielders = 0;
            int forwards = 0;
            map<string, int> normalizedPositionCount;
            set<string> localPlayers;

            for (const RawPlayerRecord& player : team.players) {
                string playerName = trim(player.name);
                string playerKey = toLower(playerName);
                string normalizedPosition = rawNormalizedPosition(player);
                string sourcePosition = trim(player.position);
                string rawPosition = trim(player.positionRaw);

                if (playerName.empty() || toLower(playerName) == "n/a") {
                    addDataIssue(report,
                                 "ERROR",
                                 "Datos invalidos",
                                 div.id,
                                 displayName,
                                 "",
                                 "Se encontro un jugador sin nombre valido en " + pathFilename(team.sourceFile) + ".",
                                 "Completar el nombre o eliminar la fila corrupta.");
                    continue;
                }
                if (!localPlayers.insert(playerKey).second) {
                    addDataIssue(report,
                                 "ERROR",
                                 "Jugador duplicado",
                                 div.id,
                                 displayName,
                                 playerName,
                                 "El jugador aparece repetido dentro del mismo equipo.",
                                 "Eliminar la fila duplicada o corregir el nombre.");
                }
                playerTeams[playerKey].push_back({displayName + " [" + div.id + "]", normalizedPosition, player.age, player.hasAge});

                if (hasSuspiciousEncoding(playerName) || hasSuspiciousEncoding(sourcePosition) || hasSuspiciousEncoding(rawPosition)) {
                    addDataIssue(report,
                                 "WARNING",
                                 "Codificacion",
                                 div.id,
                                 displayName,
                                 playerName,
                                 "Se detectan caracteres mal codificados en nombre o posicion.",
                                 "Reguardar el archivo en UTF-8 real y corregir textos con mojibake.");
                }

                if (!player.hasAge) {
                    addDataIssue(report,
                                 "ERROR",
                                 "Datos invalidos",
                                 div.id,
                                 displayName,
                                 playerName,
                                 "Edad faltante o ilegible.",
                                 "Asignar una edad entre 15 y 45.");
                } else if (player.age < 15 || player.age > 45) {
                    addDataIssue(report,
                                 "ERROR",
                                 "Datos invalidos",
                                 div.id,
                                 displayName,
                                 playerName,
                                 "Edad fuera de rango: " + to_string(player.age) + ".",
                                 "Ajustar la edad al rango 15-45.");
                }

                string rawOnlyPosition = normalizePosition(player.positionRaw);
                string directPosition = normalizePosition(player.position);
                if (normalizedPosition == "N/A") {
                    addDataIssue(report,
                                 "ERROR",
                                 "Posicion",
                                 div.id,
                                 displayName,
                                 playerName,
                                 "No tiene posicion valida.",
                                 "Completar la posicion principal con ARQ/DEF/MED/DEL o una posicion reconocible.");
                } else if ((sourcePosition.empty() || directPosition == "N/A") && rawOnlyPosition != "N/A") {
                    addDataIssue(report,
                                 "WARNING",
                                 "Posicion",
                                 div.id,
                                 displayName,
                                 playerName,
                                 "La posicion principal no es valida y solo se puede inferir desde position_raw.",
                                 "Completar el campo position para no depender del parser.");
                } else if (directPosition != "N/A" && rawOnlyPosition != "N/A" && directPosition != rawOnlyPosition) {
                    addDataIssue(report,
                                 "WARNING",
                                 "Posicion",
                                 div.id,
                                 displayName,
                                 playerName,
                                 "position y position_raw no coinciden (" + directPosition + " vs " + rawOnlyPosition + ").",
                                 "Corregir la fila para que ambos campos representen el mismo rol.");
                }

                if (isGoalkeeperRole(player)) goalkeepers++;
                if (isCenterBackRole(player)) centerBacks++;
                if (isFullBackRole(player)) fullBacks++;
                if (isMidfieldRole(player)) midfielders++;
                if (isForwardRole(player)) forwards++;
                normalizedPositionCount[normalizedPosition]++;
            }

            if (goalkeepers < 2) {
                addDataIssue(report, "ERROR", "Posiciones obligatorias", div.id, displayName, "",
                             "Solo tiene " + to_string(goalkeepers) + " portero(s).",
                             "Agregar al menos 2 arqueros.");
            }
            if (centerBacks < 3) {
                addDataIssue(report, "ERROR", "Posiciones obligatorias", div.id, displayName, "",
                             "Solo tiene " + to_string(centerBacks) + " central(es).",
                             "Agregar o reclasificar defensas centrales hasta llegar a 3.");
            }
            if (fullBacks < 2) {
                addDataIssue(report, "ERROR", "Posiciones obligatorias", div.id, displayName, "",
                             "Solo tiene " + to_string(fullBacks) + " lateral(es)/carrilero(s).",
                             "Agregar laterales naturales o corregir position_raw.");
            }
            if (midfielders < 3) {
                addDataIssue(report, "ERROR", "Posiciones obligatorias", div.id, displayName, "",
                             "Solo tiene " + to_string(midfielders) + " mediocampista(s).",
                             "Agregar mediocampistas para sostener la simulacion.");
            }
            if (forwards < 2) {
                addDataIssue(report, "ERROR", "Posiciones obligatorias", div.id, displayName, "",
                             "Solo tiene " + to_string(forwards) + " delantero(s).",
                             "Agregar al menos dos atacantes.");
            }

            for (const auto& entryPos : normalizedPositionCount) {
                if (entryPos.first == "N/A") continue;
                if (entryPos.second >= max(8, squadSize * 6 / 10)) {
                    addDataIssue(report,
                                 "WARNING",
                                 "Balance plantilla",
                                 div.id,
                                 displayName,
                                 "",
                                 "Demasiados jugadores en " + entryPos.first + ": " + to_string(entryPos.second) + "/" +
                                     to_string(squadSize) + ".",
                                 "Redistribuir posiciones o completar lineas vacias.");
                }
            }
        }
    }

    for (const auto& entry : playerTeams) {
        set<string> uniqueTeams;
        for (const auto& occurrence : entry.second) uniqueTeams.insert(occurrence.teamLabel);
        if (uniqueTeams.size() <= 1) continue;
        if (isWhitelistedDuplicatePlayer(entry.first) || likelyLegitHomonym(entry.second)) continue;
        vector<string> teams(uniqueTeams.begin(), uniqueTeams.end());
        addDataIssue(report,
                     "WARNING",
                     "Jugador duplicado",
                     "",
                     "",
                     entry.first,
                     "El mismo nombre aparece en varios equipos: " + joinStringValues(teams, ", "),
                     "Verificar si es el mismo jugador repetido o un caso legitimo de homonimia.");
    }

    vector<string> playerDirs = listDirectories("data/players");
    if (!playerDirs.empty()) {
        addDataIssue(report,
                     "WARNING",
                     "Jugadores sin equipo",
                     "",
                     "",
                     "",
                     "Hay contenido adicional en data/players que no se vincula automaticamente a clubes.",
                     "Revisar si esos jugadores deben asignarse a un equipo o eliminarse.");
    }

    Career loadedCareer;
    loadedCareer.initializeLeague(true);
    for (const Team& team : loadedCareer.allTeams) {
        if (team.players.empty()) {
            addDataIssue(report,
                         "ERROR",
                         "Integridad liga",
                         team.division,
                         team.name,
                         "",
                         "El equipo queda vacio incluso despues de cargar la base.",
                         "Corregir o crear una plantilla base para el club.");
            continue;
        }
        for (const Player& player : team.players) {
            auto checkRange = [&](int value, const string& label, int lo, int hi) {
                if (value < lo || value > hi) {
                    addDataIssue(report,
                                 "ERROR",
                                 "Atributos derivados",
                                 team.division,
                                 team.name,
                                 player.name,
                                 label + " fuera de rango: " + to_string(value) + ".",
                                 "Revisar el parser o el dato crudo que origina este jugador.");
                }
            };
            checkRange(player.age, "Edad", 15, 45);
            checkRange(player.attack, "Ataque", 1, 100);
            checkRange(player.defense, "Defensa", 1, 100);
            checkRange(player.stamina, "Stamina", 1, 100);
            checkRange(player.fitness, "Fitness", 1, 100);
            checkRange(player.skill, "Media", 1, 100);
            checkRange(player.potential, "Potencial", 1, 100);
            checkRange(player.consistency, "Consistencia", 1, 100);
            checkRange(player.bigMatches, "Partidos grandes", 1, 100);
            checkRange(player.currentForm, "Forma actual", 1, 100);
            checkRange(player.tacticalDiscipline, "Disciplina tactica", 1, 100);
            checkRange(player.discipline, "Disciplina", 1, 100);
            checkRange(player.injuryProneness, "Propension a lesiones", 1, 100);
            checkRange(player.adaptation, "Adaptacion", 1, 100);
            checkRange(player.versatility, "Versatilidad", 1, 100);
            checkRange(player.moraleMomentum, "Impulso moral", -25, 25);
            checkRange(player.fatigueLoad, "Carga de fatiga", 0, 100);
            checkRange(player.unhappinessWeeks, "Semanas de descontento", 0, 52);
            if (player.potential < player.skill) {
                addDataIssue(report,
                             "ERROR",
                             "Atributos derivados",
                             team.division,
                             team.name,
                             player.name,
                             "Potencial menor que media (" + to_string(player.potential) + " < " + to_string(player.skill) + ").",
                             "Ajustar el calculo o la fuente de datos.");
            }
            if (normalizePosition(player.position) == "N/A") {
                addDataIssue(report,
                             "ERROR",
                             "Atributos derivados",
                             team.division,
                             team.name,
                             player.name,
                             "El jugador cargado quedo sin posicion valida.",
                             "Corregir el archivo fuente o el parser de posiciones.");
            }
        }
    }

    report.ok = (report.errorCount == 0);
    report.lines.push_back("");
    report.lines.push_back("=== Auditoria de Plantillas ===");
    report.lines.push_back("Divisiones: " + to_string(report.divisionsScanned) +
                           " | Equipos revisados: " + to_string(report.teamsScanned) +
                           " | Jugadores crudos: " + to_string(report.playersScanned));
    report.lines.push_back("Errores: " + to_string(report.errorCount) +
                           " | Advertencias: " + to_string(report.warningCount));
    const size_t maxLines = min<size_t>(60, report.issues.size());
    for (size_t i = 0; i < maxLines; ++i) {
        report.lines.push_back(formatIssueLine(report.issues[i]));
    }
    if (report.issues.size() > maxLines) {
        report.lines.push_back("... " + to_string(report.issues.size() - maxLines) + " incidencia(s) adicionales omitidas.");
    }
    return report;
}

static bool writeRosterDataValidationReportInternal(const DataValidationReport& report, const string& path) {
    const string resolvedPath = resolveProjectPath(path);
    string parent = resolvedPath;
    size_t slash = parent.find_last_of("/\\");
    if (slash != string::npos) {
        parent = parent.substr(0, slash);
        if (!parent.empty()) ensureDirectory(parent);
    }

    ofstream file(resolvedPath.c_str());
    if (!file.is_open()) return false;

    file << "=== Auditoria de Plantillas ===\n";
    file << "Divisiones: " << report.divisionsScanned
         << " | Equipos revisados: " << report.teamsScanned
         << " | Jugadores crudos: " << report.playersScanned << "\n";
    file << "Errores: " << report.errorCount
         << " | Advertencias: " << report.warningCount << "\n\n";

    for (const DataValidationIssue& issue : report.issues) {
        file << formatIssueLine(issue) << "\n";
    }
    return file.good();
}

bool writeRosterDataValidationReport(const string& path) {
    return writeRosterDataValidationReportInternal(buildRosterDataValidationReport(), path);
}

StartupValidationSummary buildStartupValidationSummary(
    size_t maxLines,
    bool forceRefresh
) {
    lock_guard<recursive_mutex> lock(startupValidationMutex());

    static thread_local bool buildingSummary = false;

    if (buildingSummary) {
        StartupValidationSummary skipped{};
        skipped.ok = true;
        skipped.errorCount = 0;
        skipped.warningCount = 0;
        return skipped;
    }

    ScopedValidationBuildFlag buildGuard(buildingSummary);

    CachedRosterAudit& cache = rosterAuditCache();

    if (!cache.ready || forceRefresh) {
        DataValidationReport refreshedReport =
            buildRosterDataValidationReport();

        writeRosterDataValidationReportInternal(
            refreshedReport,
            "saves/roster_validation_report.txt"
        );

        cache.report = std::move(refreshedReport);
        cache.ready = true;
    }

    StartupValidationSummary summary{};
    summary.ok = cache.report.errorCount == 0;
    summary.errorCount = cache.report.errorCount;
    summary.warningCount = cache.report.warningCount;

    summary.lines.push_back(
        "Auditoria automatica de datos externos"
    );

    summary.lines.push_back(
        "Errores: " + to_string(summary.errorCount) +
        " | Advertencias: " + to_string(summary.warningCount)
    );

    if (cache.report.issues.empty()) {
        summary.lines.push_back(
            "No se detectaron incidencias al revisar la base externa."
        );

        return summary;
    }

    const size_t limit =
        min(maxLines, cache.report.issues.size());

    for (size_t i = 0; i < limit; ++i) {
        summary.lines.push_back(
            formatIssueLine(cache.report.issues[i])
        );
    }

    if (cache.report.issues.size() > limit) {
        summary.lines.push_back(
            "Reporte completo disponible en "
            "saves/roster_validation_report.txt"
        );
    }

    return summary;
}

ValidationSuiteSummary buildValidationSuiteSummary() {
    vector<ValidationResult> results;
    Career career;
    career.initializeLeague(true);

    results.push_back(validateDivisionCounts(career));
    results.push_back(validateUniqueTeams(career));
    results.push_back(validateRosterCaps(career));
    results.push_back(validateRosterStructure(career));
    results.push_back(validateSchedules(career));
    results.push_back(validateSaveLoad());
    results.push_back(validateTableSorting());
    DataValidationReport rosterReport = buildRosterDataValidationReport();
    writeRosterDataValidationReportInternal(
    rosterReport,
    "saves/roster_validation_report.txt"
);

    int failures = 0;
    ValidationSuiteSummary summary;
    summary.logicFailureCount = 0;
    summary.dataErrorCount = 0;
    summary.dataWarningCount = 0;
    summary.lines.push_back("");
    summary.lines.push_back("=== Suite de Validacion ===");
    for (const auto& result : results) {
        if (!result.ok) failures++;
        summary.lines.push_back(string(result.ok ? "[OK] " : "[FAIL] ") + result.name + ": " + result.detail);
    }
    summary.logicFailureCount = failures;
    summary.dataErrorCount = rosterReport.errorCount;
    summary.dataWarningCount = rosterReport.warningCount;
    summary.lines.insert(summary.lines.end(), rosterReport.lines.begin(), rosterReport.lines.end());
    summary.ok = (failures == 0 && rosterReport.errorCount == 0);
    summary.lines.push_back("");
    if (summary.ok) {
        summary.lines.push_back("Resultado: sin fallas");
    } else {
        summary.lines.push_back("Resultado: " + to_string(failures) + " falla(s) logica(s), " +
                                to_string(rosterReport.errorCount) + " error(es) de datos.");
    }
    return summary;
}

int runValidationSuite(bool verbose) {
    ValidationSuiteSummary summary = buildValidationSuiteSummary();
    if (verbose) {
        for (const auto& line : summary.lines) {
            cout << line << endl;
        }
    }
    return summary.ok ? 0 : 1;
}
