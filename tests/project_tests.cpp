#include "ai/ai_squad_planner.h"
#include "ai/ai_transfer_manager.h"
#include "ai/team_ai.h"
#include "career/analytics_service.h"
#include "career/app_services.h"
#include "career/career_modules.h"
#include "career/career_reports.h"
#include "career/career_runtime.h"
#include "career/career_service.h"
#include "career/career_state.h"
#include "career/inbox_service.h"
#include "career/manager_advice.h"
#include "career/transfer_briefing.h"
#include "career/dressing_room_service.h"
#include "career/medical_service.h"
#include "career/staff_service.h"
#include "career/match_analysis_store.h"
#include "career/match_center_service.h"
#include "career/weekly_focus_service.h"
#include "career/career_support.h"
#include "career/season_service.h"
#include "career/season_transition.h"
#include "career/world_state_service.h"
#include "competition/competition.h"
#include "competition/league_registry.h"
#include "development/monthly_development.h"
#include "development/training_impact_system.h"
#include "engine/models.h"
#include "engine/team_personality.h"
#include "engine/debt_system.h"
#include "engine/game_settings.h"
#ifdef _WIN32
#include "gui/gui_view_builders.h"
#endif
#include "io/save_serialization.h"
#include "io/io.h"
#include "simulation/match_context.h"
#include "simulation/match_engine_internal.h"
#include "simulation/match_engine.h"
#include "simulation/match_event_generator.h"
#include "simulation/match_phase.h"
#include "simulation/player_condition.h"
#include "simulation/simulation.h"
#include "transfers/negotiation_system.h"
#include "transfers/transfer_market.h"
#include "utils/utils.h"
#include "validators/validators.h"
#include "utils/localization.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;

namespace {

void expect(bool condition, const string& message) {
    if (!condition) throw runtime_error(message);
}

string joinLines(const vector<string>& lines) {
    ostringstream out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) out << '\n';
        out << lines[i];
    }
    return out.str();
}

string replaceAllCopy(string text, const string& needle, const string& replacement) {
    if (needle.empty()) return text;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != string::npos) {
        text.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
    return text;
}

int countLinesStartingWith(const vector<string>& lines, const string& prefix) {
    int count = 0;
    for (const string& line : lines) {
        if (line.rfind(prefix, 0) == 0) count++;
    }
    return count;
}

string testProcessSuffix() {
#ifdef _WIN32
    return to_string(static_cast<unsigned long>(GetCurrentProcessId()));
#else
    return to_string(static_cast<long long>(getpid()));
#endif
}

string processScopedTestPath(const string& path) {
    const string suffix = "_" + testProcessSuffix();
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find_last_of('.');
    if (dot != string::npos && (slash == string::npos || dot > slash)) {
        return path.substr(0, dot) + suffix + path.substr(dot);
    }
    return path + suffix;
}

struct ScopedNamedTestLock {
#ifdef _WIN32
    HANDLE handle = nullptr;

    explicit ScopedNamedTestLock(const string& name) {
        handle = CreateMutexA(nullptr, FALSE, name.c_str());
        if (handle) WaitForSingleObject(handle, INFINITE);
    }

    ~ScopedNamedTestLock() {
        if (handle) {
            ReleaseMutex(handle);
            CloseHandle(handle);
        }
    }
#else
    explicit ScopedNamedTestLock(const string&) {}
#endif
};

struct ScopedFileRestore {
    string path;
    string originalContent;
    bool existed = false;

    explicit ScopedFileRestore(string p) : path(std::move(p)) {
        ifstream input(path, ios::binary);
        if (!input.is_open()) return;
        existed = true;
        ostringstream buffer;
        buffer << input.rdbuf();
        originalContent = buffer.str();
    }

    ~ScopedFileRestore() {
        if (existed) {
            ofstream output(path, ios::binary | ios::trunc);
            if (output.is_open()) output << originalContent;
        } else {
            std::remove(path.c_str());
        }
    }
};

Player makePlayer(const string& name,
                  const string& position,
                  int skill,
                  int potential,
                  int age,
                  int stamina,
                  int fitness) {
    Player player{};
    player.name = name;
    player.position = position;
    player.preferredFoot = "Derecho";
    player.skill = skill;
    player.potential = potential;
    player.age = age;
    player.stamina = stamina;
    player.fitness = fitness;
    player.value = static_cast<long long>(skill) * 15000;
    player.wage = static_cast<long long>(skill) * 190;
    player.releaseClause = player.value * 2;
    player.setPieceSkill = skill;
    player.leadership = 52;
    player.professionalism = 68;
    player.ambition = 61;
    player.consistency = 64;
    player.bigMatches = 63;
    player.currentForm = 58 + skill / 4;
    player.tacticalDiscipline = 62;
    player.versatility = 54;
    player.happiness = 63;
    player.chemistry = 62;
    player.desiredStarts = 1;
    player.startsThisSeason = 0;
    player.wantsToLeave = false;
    player.onLoan = false;
    player.loanWeeksRemaining = 0;
    player.contractWeeks = 104;
    player.injured = false;
    player.injuryWeeks = 0;
    player.injuryHistory = 0;
    player.yellowAccumulation = 0;
    player.seasonYellowCards = 0;
    player.seasonRedCards = 0;
    player.matchesSuspended = 0;
    player.goals = 0;
    player.assists = 0;
    player.matchesPlayed = 0;
    player.lastTrainedSeason = -1;
    player.lastTrainedWeek = -1;
    applyPositionStats(player);
    ensurePlayerProfile(player, true);
    return player;
}

Team makeTeam(const string& name,
              const string& division,
              int baseSkill,
              int pressing,
              int tempo,
              const string& tactics,
              const string& instruction,
              long long budget = 450000) {
    Team team(name);
    team.division = division;
    team.formation = "4-3-3";
    team.tactics = tactics;
    team.matchInstruction = instruction;
    team.pressingIntensity = pressing;
    team.tempo = tempo;
    team.width = 3;
    team.defensiveLine = 3;
    team.markingStyle = "Zonal";
    team.rotationPolicy = "Balanceado";
    team.trainingFocus = "Balanceado";
    team.budget = budget;
    team.morale = 60;
    team.sponsorWeekly = 35000;
    team.debt = 0;
    team.stadiumLevel = 2;
    team.trainingFacilityLevel = 2;
    team.youthFacilityLevel = 2;
    team.fanBase = 24;
    team.assistantCoach = 65;
    team.fitnessCoach = 65;
    team.scoutingChief = 65;
    team.youthCoach = 64;
    team.medicalTeam = 63;

    static const vector<string> positions = {
        "ARQ", "DEF", "DEF", "DEF", "DEF",
        "MED", "MED", "MED",
        "DEL", "DEL", "DEL",
        "ARQ", "DEF", "DEF", "MED", "MED", "DEL", "DEL"
    };

    for (size_t i = 0; i < positions.size(); ++i) {
        const int skill = baseSkill - static_cast<int>(i % 4) * 2 + (positions[i] == "DEL" ? 2 : 0);
        const int age = 20 + static_cast<int>(i % 10);
        const int stamina = 66 + static_cast<int>(i % 5) * 3;
        const int fitness = 68 + static_cast<int>(i % 4) * 4;
        team.addPlayer(makePlayer(name + "_P" + to_string(i + 1), positions[i], skill, min(95, skill + 8), age, stamina, fitness));
    }

    ensureTeamIdentity(team);
    return team;
}

bool changeCurrentDirectoryForTest(const string& path) {
#ifdef _WIN32
    return _chdir(path.c_str()) == 0;
#else
    return chdir(path.c_str()) == 0;
#endif
}

struct ScopedCurrentDirectory {
    string previous;
    bool valid = false;

    explicit ScopedCurrentDirectory(const string& target) : previous(currentWorkingDirectory()) {
        valid = changeCurrentDirectoryForTest(target);
    }

    ~ScopedCurrentDirectory() {
        if (!previous.empty()) changeCurrentDirectoryForTest(previous);
    }
};

vector<string> g_runtimeMessagesA;
vector<string> g_runtimeMessagesB;

void collectRuntimeMessageA(const string& message) {
    g_runtimeMessagesA.push_back(message);
}

void collectRuntimeMessageB(const string& message) {
    g_runtimeMessagesB.push_back(message);
}

void idleRuntimeProbe() {}

#ifdef _WIN32
struct ValidationThreadProbe {
    bool ok = false;
};

DWORD WINAPI runValidationThreadProbe(LPVOID param) {
    auto* probe = static_cast<ValidationThreadProbe*>(param);
    if (!probe) return 1;
    probe->ok = buildValidationSuiteSummary().ok;
    return 0;
}
#endif

void testValidationSuiteReflectsRosterAudit() {
    const DataValidationReport report = buildRosterDataValidationReport();
    const ValidationSuiteSummary summary = buildValidationSuiteSummary();
    if (report.errorCount > 0) {
        expect(!summary.ok, "La suite debe fallar si la auditoria de datos detecta errores.");
        expect(joinLines(summary.lines).find("error(es) de datos") != string::npos,
               "El resumen de validacion debe informar errores de datos.");
    } else {
        expect(summary.ok, "La suite debe quedar sana cuando no existen errores de datos ni fallas logicas.");
    }
}

void testValidationSuiteSupportsConcurrentRuns() {
#ifdef _WIN32
    ValidationThreadProbe firstProbe;
    ValidationThreadProbe secondProbe;
    HANDLE handles[2] = {
        CreateThread(nullptr, 0, runValidationThreadProbe, &firstProbe, 0, nullptr),
        CreateThread(nullptr, 0, runValidationThreadProbe, &secondProbe, 0, nullptr)
    };
    expect(handles[0] != nullptr && handles[1] != nullptr,
           "La prueba de concurrencia debe poder crear threads de validacion.");
    WaitForMultipleObjects(2, handles, TRUE, INFINITE);
    CloseHandle(handles[0]);
    CloseHandle(handles[1]);
    expect(firstProbe.ok && secondProbe.ok,
           "La suite de validacion debe poder ejecutarse en paralelo sin pisar su save temporal.");
#else
    const ValidationSuiteSummary firstSummary = buildValidationSuiteSummary();
    const ValidationSuiteSummary secondSummary = buildValidationSuiteSummary();
    expect(firstSummary.ok && secondSummary.ok,
           "La suite de validacion debe soportar ejecuciones repetidas sin pisar su save temporal.");
#endif
}

void testLoadedPlayersHaveBoundedProfileMetrics() {
    Career career;
    career.initializeLeague(true);
    expect(!career.allTeams.empty(), "La auditoria de perfiles necesita equipos cargados.");

    for (const Team& team : career.allTeams) {
        expect(!team.players.empty(), "Cada club cargado debe conservar jugadores.");
        for (const Player& player : team.players) {
            expect(player.consistency >= 1 && player.consistency <= 100,
                   "La consistencia cargada debe quedar dentro de rango.");
            expect(player.bigMatches >= 1 && player.bigMatches <= 100,
                   "El atributo de partidos grandes debe quedar dentro de rango.");
            expect(player.currentForm >= 1 && player.currentForm <= 100,
                   "La forma actual debe quedar dentro de rango.");
            expect(player.tacticalDiscipline >= 1 && player.tacticalDiscipline <= 100,
                   "La disciplina tactica debe quedar dentro de rango.");
            expect(player.versatility >= 1 && player.versatility <= 100,
                   "La versatilidad debe quedar dentro de rango.");
            expect(player.moraleMomentum >= -25 && player.moraleMomentum <= 25,
                   "El impulso moral no puede salir del rango operativo.");
            expect(player.fatigueLoad >= 0 && player.fatigueLoad <= 100,
                   "La carga de fatiga debe quedar dentro de 0-100.");
            expect(player.unhappinessWeeks >= 0 && player.unhappinessWeeks <= 52,
                   "Las semanas de descontento deben quedar acotadas.");
        }
    }
}

void testMatchSimulationProducesStructuredPhases() {
    Team home = makeTeam("Local Unido", "primera b", 69, 4, 4, "Pressing", "Contra-presion", 600000);
    Team away = makeTeam("Visitante FC", "primera b", 66, 2, 2, "Balanced", "Bloque bajo", 420000);

    const match_engine::MatchSimulationData data = match_engine::simulate(home, away, true, false);
    const MatchResult& result = data.result;

    expect(result.timeline.phases.size() == 6, "El partido debe quedar dividido en seis fases.");
    static const vector<pair<int, int>> expectedPhases = {{1, 15}, {16, 30}, {31, 45}, {46, 60}, {61, 75}, {76, 90}};
    for (size_t i = 0; i < expectedPhases.size(); ++i) {
        const MatchPhaseReport& phase = result.timeline.phases[i];
        expect(phase.minuteStart == expectedPhases[i].first && phase.minuteEnd == expectedPhases[i].second,
               "Los limites de fase no coinciden con la estructura esperada.");
        expect(phase.homePossessionShare + phase.awayPossessionShare == 100,
               "La posesion por fase debe sumar 100.");
        expect(phase.homeProgressions >= phase.homeAttacks && phase.awayProgressions >= phase.awayAttacks,
               "Una fase no puede generar mas ataques que progresiones.");
        expect(phase.homeAttacks >= phase.homeShotsGenerated && phase.awayAttacks >= phase.awayShotsGenerated,
               "No puede haber mas remates que ataques maduros en una fase.");
    }

    expect(result.homePossession + result.awayPossession == 100, "La posesion final debe sumar 100.");
    expect(result.homeGoals == result.stats.homeGoals && result.awayGoals == result.stats.awayGoals,
           "El resultado final debe reflejar las estadisticas acumuladas.");
    expect(result.homeShots == result.stats.homeShots && result.awayShots == result.stats.awayShots,
           "Los tiros del resumen deben coincidir con MatchStats.");
    expect(result.stats.homeShotsOnTarget <= result.homeShots && result.stats.awayShotsOnTarget <= result.awayShots,
           "Los tiros al arco no pueden exceder los tiros totales.");
    expect(!result.timeline.events.empty(), "La linea de tiempo debe contener eventos.");
    expect(result.stats.homeExpectedGoals >= 0.0 && result.stats.awayExpectedGoals >= 0.0,
           "El xG no puede ser negativo.");
    expect(!result.reportLines.empty(), "El motor debe producir lineas de reporte legibles.");
    expect(result.report.phaseSummaries.size() == 6, "El reporte debe resumir las seis fases del partido.");
    expect(!result.report.explanation.likelyReason.empty(),
           "El reporte del partido debe incluir una explicacion probable.");
    expect(!result.report.playerOfTheMatch.empty(),
           "El reporte del partido debe identificar una figura del partido.");
    const string reportText = joinLines(result.reportLines);
    expect(reportText.find("Riesgo tactico:") != string::npos,
           "El resumen del partido debe exponer el riesgo tactico.");
    expect(reportText.find("Disciplina:") != string::npos,
           "El resumen del partido debe explicar el impacto disciplinario.");
}

void testHighPressRaisesPhaseFatigue() {
    Team highPress = makeTeam("Presion Alta", "primera division", 72, 5, 5, "Pressing", "Contra-presion", 800000);
    Team lowBlock = makeTeam("Bloque Bajo", "primera division", 72, 1, 2, "Defensive", "Pausar juego", 800000);
    Team opponent = makeTeam("Rival Control", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 800000);

    const MatchSetup highSetup = match_context::buildMatchSetup(highPress, opponent, false, false);
    const MatchSetup lowSetup = match_context::buildMatchSetup(lowBlock, opponent, false, false);

    const MatchPhaseEvaluation highEval =
        match_phase::evaluatePhase(highSetup, highPress, opponent, highSetup.home, highSetup.away, 0, 1, 15);
    const MatchPhaseEvaluation lowEval =
        match_phase::evaluatePhase(lowSetup, lowBlock, opponent, lowSetup.home, lowSetup.away, 0, 1, 15);

    expect(highEval.report.homeFatigueGain > lowEval.report.homeFatigueGain,
           "La presion alta debe aumentar el desgaste de fase.");
    expect(highEval.report.intensity > lowEval.report.intensity,
           "La presion alta debe elevar la intensidad del tramo.");
}

void testLowBlockSuppressesChanceQuality() {
    Team home = makeTeam("Control Interior", "primera division", 72, 3, 3, "Balanced", "Equilibrado", 700000);
    Team lowBlock = makeTeam("Muralla", "primera division", 68, 1, 2, "Defensive", "Bloque bajo", 680000);
    Team openGame = makeTeam("Abierto", "primera division", 68, 4, 4, "Offensive", "Por bandas", 680000);

    setRandomSeed(20260320);
    const MatchSetup lowBlockSetup = match_context::buildMatchSetup(home, lowBlock, false, false);
    setRandomSeed(20260320);
    const MatchSetup openSetup = match_context::buildMatchSetup(home, openGame, false, false);

    setRandomSeed(424242);
    const MatchPhaseEvaluation lowBlockEval =
        match_phase::evaluatePhase(lowBlockSetup, home, lowBlock, lowBlockSetup.home, lowBlockSetup.away, 1, 16, 30);
    setRandomSeed(424242);
    const MatchPhaseEvaluation openEval =
        match_phase::evaluatePhase(openSetup, home, openGame, openSetup.home, openSetup.away, 1, 16, 30);
    resetRandomSeed();

    expect(lowBlockEval.report.homeChanceProbability < openEval.report.homeChanceProbability,
           "Un bloque bajo debe recortar la calidad media de las llegadas rivales.");
    expect(lowBlockEval.report.homeDefensiveRisk < openEval.report.homeDefensiveRisk,
           "El local no deberia quedar mas expuesto ante un rival de bloque bajo que ante uno abierto.");
}

void testCompetitionRulesLoadFromCsv() {
    expect(reloadCompetitionConfigs(), "Las reglas de competicion deben poder cargarse desde competition_rules.csv.");
    const string divName1 = "primera division";
    const string divName2 = "tercera division b";
    const CompetitionConfig primera = getCompetitionConfig(divName1);
    const CompetitionConfig terceraB = getCompetitionConfig(divName2);

    expect(primera.baseIncome == 60000, "La Primera Division debe leer ingresos base desde el CSV externo.");
    expect(terceraB.groups.enabled && terceraB.groups.groupSize == 14,
           "La Tercera B debe conservar su configuracion zonal desde el CSV.");
    expect(competitionConfigWarnings().empty(),
           "El archivo de reglas externas no deberia generar advertencias en el escenario base.");
}

void testRuntimeValidationFlagsBrokenLoadedSquad() {
    Career career;
    career.divisions.push_back({"primera division", "data/LigaChilena/primera division", "Primera Division"});

    Team broken("Plantel Roto");
    broken.division = "primera division";
    broken.players.push_back(makePlayer("Duplicado", "DEF", 61, 65, 27, 66, 66));
    broken.players.push_back(makePlayer("Duplicado", "N/A", 60, 64, 28, 64, 64));
    broken.players.push_back(makePlayer("Sin Arquero", "MED", 62, 68, 23, 71, 71));
    career.allTeams.push_back(broken);

    const RuntimeValidationSummary summary = validateLoadedCareerData(career, 8);
    expect(!summary.ok, "La validacion en carga debe fallar con planteles estructuralmente invalidos.");
    expect(summary.errorCount >= 2, "La validacion en carga debe contar errores de duplicados y posiciones invalidas.");
}

void testStartupValidationSummaryExposesExternalAudit() {
    const StartupValidationSummary summary = buildStartupValidationSummary(3, true);
    const DataValidationReport report = buildRosterDataValidationReport();

    expect(summary.errorCount == report.errorCount,
           "La validacion de arranque debe reflejar el mismo conteo de errores que la auditoria profunda.");
    expect(summary.warningCount == report.warningCount,
           "La validacion de arranque debe reflejar el mismo conteo de advertencias que la auditoria profunda.");
    expect(!summary.lines.empty(), "La validacion de arranque debe devolver un resumen legible.");
    if (report.errorCount > 0) {
        expect(!summary.ok, "La validacion de arranque no puede marcarse sana si la base externa tiene errores.");
    }
}

void testCompetitionGroupTableScopesActiveGroup() {
    Career career;
    static const vector<string> names = {
        "Norte Azul", "Norte Rojo", "Norte Blanco", "Norte Negro", "Norte Plata", "Norte Cobre", "Norte Mar",
        "Sur Verde", "Sur Oro", "Sur Celeste", "Sur Vino", "Sur Gris", "Sur Arena", "Sur Austral"
    };
    static const vector<int> points = {24, 18, 15, 14, 12, 11, 10, 30, 16, 13, 12, 10, 9, 8};
    for (size_t i = 0; i < names.size(); ++i) {
        career.allTeams.push_back(makeTeam(names[i], "segunda division", 62 - static_cast<int>(i % 4), 3, 3, "Balanced", "Equilibrado"));
        career.allTeams.back().points = points[i];
    }

    career.setActiveDivision("segunda division");
    career.myTeam = career.getActiveTeamAt(0);

    const LeagueTable northTable = buildCompetitionGroupTable(career, true);
    expect(northTable.title == "Grupo Norte", "La tabla del grupo norte debe usar el titulo correcto.");
    expect(northTable.teams.size() == 7, "La tabla de grupo debe incluir solo a los equipos del grupo solicitado.");
    expect(northTable.teams[0]->name == "Norte Azul", "La tabla del grupo debe mantener el orden competitivo.");

    const LeagueTable relevant = buildRelevantCompetitionTable(career);
    expect(relevant.teams.size() == 7 && relevant.teams[0]->name == "Norte Azul",
           "La tabla relevante debe quedarse con el grupo del club usuario.");

    const CareerState snapshot = CareerState::fromCareer(career);
    expect(snapshot.activeTeamCount == 14, "CareerState debe capturar el numero de equipos activos.");
    expect(snapshot.usesGroupFormat() == career.usesGroupFormat(),
           "CareerState debe conservar el formato real de grupos de la carrera activa.");
}

void testTeamRepositoryResolvesStableIds() {
    Career career;
    career.allTeams.clear();

    career.allTeams.emplace_back("Repo Norte");
    career.allTeams.back().division = "primera division";
    career.allTeams.emplace_back("Repo Sur");
    career.allTeams.back().division = "primera division";
    career.refreshActiveDivisionTeamLinks("primera division");
    career.myTeam = career.findTeamByName("Repo Sur");
    career.currentSeason = 2;
    career.currentWeek = 7;

    TeamRepository repository(career);
    const TeamId firstId = repository.getTeamIdByName("Repo Norte");
    const TeamId managedId = repository.getTeamIdFor(career.myTeam);

    expect(firstId != kInvalidTeamId, "TeamRepository debe resolver IDs por nombre.");
    expect(repository.getTeamById(firstId) == &career.allTeams[0], "TeamId debe apuntar al equipo correcto.");
    expect(repository.getActiveTeamByScheduleIndex(1) == career.myTeam,
           "El acceso por indice de calendario debe validar y devolver el equipo activo.");
    expect(managedId == career.getActiveTeamIdAt(1), "Career debe exponer IDs seguros para equipos activos.");
    expect(career.hasSyncedActiveTeamIds(),
           "Career debe mantener una lista paralela de IDs activos.");
    expect(career.leagueTable.teams.size() == 2 && career.leagueTable.teams[1] == career.myTeam,
           "La tabla activa debe reconstruirse desde el acceso seguro de equipos activos.");

    CareerState snapshot = CareerState::fromCareer(career);
    expect(snapshot.currentSeason == 2 && snapshot.currentWeek == 7,
           "CareerState debe capturar temporada y semana desde Career.");
    expect(snapshot.activeDivision == "primera division", "CareerState debe capturar la division activa.");
    expect(snapshot.activeTeamCount == 2, "CareerState debe capturar el conteo de equipos activos.");

    career.allTeams.emplace_back("Repo Reserva");
    career.allTeams.back().division = "primera division";
    career.activeTeams.push_back(&career.allTeams[2]);
    expect(!career.hasSyncedActiveTeamIds(), "El test conserva IDs desactualizados para cubrir fallback legacy.");
    expect(career.getActiveTeamCount() == 3,
           "Career debe caer a activeTeams si activeTeamIds queda desincronizado.");
    expect(career.getActiveTeamAt(2) == &career.allTeams[2],
           "El acceso activo debe resolver el equipo agregado aunque falte sincronizar IDs.");
    expect(career.getActiveTeamIdAt(2) == career.getTeamIdFor(&career.allTeams[2]),
           "El acceso por ID debe caer al puntero legacy cuando la lista paralela esta desfasada.");
    career.refreshActiveDivisionTeamLinks("primera division");
    expect(career.hasSyncedActiveTeamIds() && career.leagueTable.teams.size() == 3,
           "Career debe poder relinkear la division activa y reconstruir IDs/tabla sin rehacer calendario.");
    expect(career.activeDivision == "primera division",
           "El relink activo debe fijar la division canonica desde el parametro recibido.");
    expect(find(career.leagueTable.teams.begin(), career.leagueTable.teams.end(), &career.allTeams[2]) != career.leagueTable.teams.end(),
           "El relink activo debe mantener al equipo agregado dentro de la tabla reconstruida.");
}

void testCareerServiceWrapperProducesGameplayOutputs() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 5;
    career.allTeams.push_back(makeTeam("Servicio FC", "primera division", 66, 3, 3, "Balanced", "Equilibrado"));
    career.allTeams.push_back(makeTeam("Servicio Rival", "primera division", 64, 2, 3, "Defensive", "Pausar juego"));
    career.refreshActiveDivisionTeamLinks("primera division");
    career.myTeam = career.getActiveTeamAt(0);
    career.boardConfidence = 42;
    career.myTeam->assistantCoach = 42;
    career.myTeam->fitnessCoach = 39;
    career.myTeam->performanceAnalyst = 38;
    career.myTeam->players[0].injured = true;
    career.myTeam->players[0].injuryWeeks = 2;
    for (size_t i = 1; i <= 3; ++i) {
        career.myTeam->players[i].fitness = 48;
    }
    career.myTeam->players[4].contractWeeks = 4;

    CareerService service(career);
    service.dispatchStaffBriefing();
    service.addSquadAlerts();
    service.generateWeeklyNarrative();

    const string inbox = joinLines(career.managerInbox);
    expect(inbox.find("Staff") != string::npos || inbox.find("Plantel") != string::npos,
           "CareerService debe escribir alertas reales en el inbox compilado.");
    expect(inbox.find("Contratos") != string::npos,
           "CareerService debe traducir renovaciones urgentes a alerta de directiva.");
    expect(!career.newsFeed.empty(), "CareerService debe generar narrativa semanal real.");
}

void testTransferEvaluationPenalizesUnaffordableDeals() {
    Career career;
    career.currentSeason = 4;
    career.managerReputation = 84;

    Team buyer = makeTeam("Club Comprador", "primera division", 66, 3, 3, "Balanced", "Equilibrado", 90000);
    buyer.sponsorWeekly = 60000;
    buyer.debt = 0;
    buyer.fanBase = 42;
    buyer.trainingFacilityLevel = 4;
    buyer.youthFacilityLevel = 3;
    ensureTeamIdentity(buyer);

    Team seller = makeTeam("Club Vendedor", "primera division", 74, 3, 3, "Balanced", "Equilibrado", 900000);
    ensureTeamIdentity(seller);

    ClubTransferStrategy strategy = ai_transfer_manager::buildClubTransferStrategy(career, buyer);

    Player expensiveVeteran = makePlayer("Veterano Caro", "DEL", 81, 82, 27, 71, 72);
    expensiveVeteran.value = 900000;
    expensiveVeteran.releaseClause = 1500000;
    expensiveVeteran.wage = 52000;
    expensiveVeteran.contractWeeks = 88;
    expensiveVeteran.currentForm = 76;

    TransferTarget expensiveTarget =
        ai_transfer_manager::evaluateTarget(career, buyer, seller, expensiveVeteran, strategy);
    expect(!expensiveTarget.availableForLoan, "Un jugador de 27 anos no deberia marcarse como prestable.");
    expect(expensiveTarget.affordabilityScore < 0,
           "Una operacion muy cara debe penalizarse por presupuesto.");

    Player youngProspect = makePlayer("Proyecto Cesion", "MED", 72, 86, 20, 74, 76);
    youngProspect.value = 650000;
    youngProspect.releaseClause = 1200000;
    youngProspect.wage = 9000;
    youngProspect.contractWeeks = 104;
    youngProspect.currentForm = 61;

    TransferTarget loanTarget =
        ai_transfer_manager::evaluateTarget(career, buyer, seller, youngProspect, strategy);
    expect(loanTarget.availableForLoan, "Un juvenil con contrato largo debe quedar disponible para cesion.");
    expect(loanTarget.affordabilityScore > expensiveTarget.affordabilityScore,
           "La IA debe valorar mejor una cesion asumible que una compra imposible.");
}

void testTransferNegotiationBuildsStructuredDeal() {
    Career career;
    career.currentSeason = 5;
    career.managerReputation = 78;

    Team buyer = makeTeam("Comprador Tactico", "primera division", 72, 4, 4, "Pressing", "Contra-presion", 2200000);
    buyer.sponsorWeekly = 140000;
    buyer.fanBase = 72;
    buyer.trainingFacilityLevel = 5;
    buyer.youthFacilityLevel = 4;
    ensureTeamIdentity(buyer);

    Team seller = makeTeam("Vendedor Historico", "primera b", 69, 3, 3, "Balanced", "Equilibrado", 480000);
    seller.fanBase = 18;
    seller.trainingFacilityLevel = 2;
    seller.youthFacilityLevel = 2;
    ensureTeamIdentity(seller);

    Player target = makePlayer("Mediapunta Premium", "MED", 77, 82, 24, 75, 77);
    target.value = 520000;
    target.releaseClause = 900000;
    target.wage = 24000;
    target.contractWeeks = 84;
    target.currentForm = 74;
    target.consistency = 70;
    target.ambition = 68;
    target.professionalism = 73;

    const NegotiationState state = runTransferNegotiation(
        career, buyer, seller, target, NegotiationProfile::Balanced, NegotiationPromise::Starter);

    expect(state.clubAccepted, "La negociacion estructurada debe cerrar el acuerdo con el club en este escenario.");
    expect(state.playerAccepted, "La negociacion estructurada debe cerrar el acuerdo con el jugador en este escenario.");
    expect(state.round >= 5, "La negociacion debe registrar varias rondas de ida y vuelta.");
    expect(state.agreedFee >= state.sellerExpectation * 90 / 100,
           "El fee acordado debe aproximarse a la expectativa del vendedor.");
    expect(state.agreedWage >= target.wage, "El salario acordado no puede empeorar el contrato actual del jugador.");
    expect(!state.roundSummaries.empty(), "La negociacion debe devolver trazabilidad de sus rondas.");
}

void testTransferShortlistRewardsScoutedNeed() {
    Career career;
    career.currentSeason = 3;
    career.managerReputation = 75;

    career.allTeams.push_back(makeTeam("Comprador Scout", "primera division", 64, 3, 3, "Balanced", "Equilibrado", 1800000));
    career.allTeams.push_back(makeTeam("Vendedor A", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 900000));
    career.allTeams.push_back(makeTeam("Vendedor B", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 900000));
    Team& buyer = career.allTeams[0];
    Team& sellerA = career.allTeams[1];
    Team& sellerB = career.allTeams[2];
    buyer.players.erase(buyer.players.begin() + 5, buyer.players.begin() + 8);

    Player targetA = makePlayer("Objetivo Scouteado", "MED", 76, 84, 22, 77, 78);
    targetA.value = 420000;
    targetA.releaseClause = 780000;
    targetA.wage = 14000;
    sellerA.addPlayer(targetA);
    career.scoutingShortlist.push_back(sellerA.name + "|" + targetA.name);

    Player targetB = makePlayer("Alternativa", "MED", 75, 80, 25, 74, 75);
    targetB.value = 410000;
    targetB.releaseClause = 760000;
    targetB.wage = 14500;
    sellerB.addPlayer(targetB);

    const vector<TransferTarget> shortlist = transfer_market::buildTransferShortlist(career, buyer, 5);
    expect(!shortlist.empty(), "El mercado debe construir una shortlist para el club comprador.");
    expect(shortlist.front().onShortlist, "Un objetivo ya scouteado debe subir en la prioridad de fichaje.");
}

void testSquadPlannerFlagsUnusedSeniorSaleCandidates() {
    Team team = makeTeam("Plantel Largo", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 650000);

    Player& expendableA = team.players[12];
    expendableA.age = 31;
    expendableA.startsThisSeason = 0;
    expendableA.matchesPlayed = 0;
    expendableA.potential = expendableA.skill + 1;
    expendableA.fitness = 53;
    expendableA.wantsToLeave = true;

    Player& expendableB = team.players[13];
    expendableB.age = 29;
    expendableB.startsThisSeason = 0;
    expendableB.matchesPlayed = 1;
    expendableB.potential = expendableB.skill + 2;
    expendableB.happiness = 42;

    const SquadNeedReport report = ai_squad_planner::analyzeSquad(team);
    expect(report.unusedSeniorPlayers >= 2,
           "La IA de plantilla debe contar veteranos casi sin uso para planificar salidas.");
    expect(report.salePressure >= 1,
           "La IA de plantilla debe generar presion de venta cuando el plantel acumula piezas marginales.");
    expect(find(report.saleCandidates.begin(), report.saleCandidates.end(), expendableA.name) != report.saleCandidates.end(),
           "Un jugador veterano, poco usado y con deseo de salida debe entrar en candidatos de venta.");
}

void testSeasonTransitionAdvancesCareerWithoutUiDependencies() {
    Career career;
    career.currentSeason = 6;
    career.currentWeek = 34;
    career.managerName = "Test Manager";
    career.managerReputation = 70;

    career.allTeams.push_back(makeTeam("Campeon Capital", "primera division", 76, 3, 3, "Balanced", "Equilibrado", 900000));
    career.allTeams.push_back(makeTeam("Puerto Unido", "primera division", 73, 3, 3, "Balanced", "Equilibrado", 820000));
    career.allTeams.push_back(makeTeam("Valle FC", "primera division", 67, 2, 2, "Defensive", "Bloque bajo", 510000));
    career.allTeams.push_back(makeTeam("Mineros del Sur", "primera division", 66, 2, 2, "Defensive", "Pausar juego", 480000));

    career.allTeams.push_back(makeTeam("Ascenso Norte", "primera b", 71, 3, 3, "Balanced", "Equilibrado", 420000));
    career.allTeams.push_back(makeTeam("Ascenso Sur", "primera b", 70, 3, 3, "Balanced", "Equilibrado", 410000));
    career.allTeams.push_back(makeTeam("Primera B Central", "primera b", 64, 2, 2, "Balanced", "Equilibrado", 340000));
    career.allTeams.push_back(makeTeam("Primera B Costa", "primera b", 63, 2, 2, "Balanced", "Equilibrado", 330000));

    career.setActiveDivision("primera division");
    expect(career.getActiveTeamCount() == 4, "La division activa debe cargar el bloque de equipos principal.");

    career.myTeam = career.findTeamByName("Campeon Capital");
    expect(career.myTeam != nullptr, "El equipo del usuario debe existir en el fixture.");

    const vector<int> points = {68, 61, 29, 23};
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        Team* activeTeam = career.getActiveTeamAt(i);
        expect(activeTeam != nullptr, "La prueba necesita equipos activos validos.");
        if (!activeTeam) continue;
        activeTeam->points = points[static_cast<size_t>(i)];
        activeTeam->goalsFor = 30 - i * 3;
        activeTeam->goalsAgainst = 12 + i * 4;
        activeTeam->wins = points[static_cast<size_t>(i)] / 3;
    }
    career.leagueTable.sortTable();

    SeasonTransitionSummary summary = endSeason(career);

    expect(summary.champion == "Campeon Capital", "La transicion debe reconocer al campeon de la tabla.");
    expect(!summary.lines.empty(), "La transicion de temporada debe devolver un resumen legible.");
    expect(career.currentSeason == 7, "El cierre de temporada debe avanzar el ano de carrera.");
    expect(career.currentWeek == 1, "El cierre de temporada debe reiniciar la semana actual.");
    expect(!career.history.empty(), "El historial de carrera debe registrar la temporada cerrada.");
    expect(career.history.back().champion == "Campeon Capital",
           "El historial debe guardar el campeon del ano que termino.");
    expect(career.activeDivision == career.myTeam->division,
           "Tras la transicion la carrera debe seguir la division real del club usuario.");
    expect(!career.schedule.empty(), "La nueva temporada debe reconstruir su calendario.");
}

void testSeasonTransitionPromotesByStandingsNotSquadValue() {
    Career career;
    career.currentSeason = 4;
    career.currentWeek = 34;
    career.managerName = "Standing Manager";

    career.allTeams.push_back(makeTeam("Primera Lider", "primera division", 74, 3, 3, "Balanced", "Equilibrado", 950000));
    career.allTeams.push_back(makeTeam("Primera Medio", "primera division", 71, 3, 3, "Balanced", "Equilibrado", 820000));
    career.allTeams.push_back(makeTeam("Primera Tenso", "primera division", 68, 2, 2, "Defensive", "Bloque bajo", 540000));
    career.allTeams.push_back(makeTeam("Primera Descenso", "primera division", 67, 2, 2, "Defensive", "Pausar juego", 500000));

    career.allTeams.push_back(makeTeam("Primera B Lider", "primera b", 71, 3, 3, "Balanced", "Equilibrado", 430000));
    career.allTeams.push_back(makeTeam("Primera B Liguilla", "primera b", 69, 3, 3, "Balanced", "Equilibrado", 410000));
    career.allTeams.push_back(makeTeam("Primera B Medio", "primera b", 66, 2, 2, "Balanced", "Equilibrado", 360000));
    career.allTeams.push_back(makeTeam("Primera B Fondo", "primera b", 64, 2, 2, "Defensive", "Bloque bajo", 320000));

    career.allTeams.push_back(makeTeam("Segunda Deportivo", "segunda division", 63, 3, 3, "Balanced", "Equilibrado", 260000));
    career.allTeams.push_back(makeTeam("Segunda Rico", "segunda division", 72, 3, 3, "Balanced", "Equilibrado", 980000));
    career.allTeams.push_back(makeTeam("Segunda Centro", "segunda division", 66, 2, 2, "Balanced", "Equilibrado", 420000));
    career.allTeams.push_back(makeTeam("Segunda Fondo", "segunda division", 61, 2, 2, "Defensive", "Pausar juego", 240000));

    auto setStanding = [](Team* team, int points, int wins, int draws, int goalsFor, int goalsAgainst) {
        expect(team != nullptr, "La prueba de ascensos por tabla necesita equipos existentes.");
        team->points = points;
        team->wins = wins;
        team->draws = draws;
        team->losses = max(0, 30 - wins - draws);
        team->goalsFor = goalsFor;
        team->goalsAgainst = goalsAgainst;
    };

    setStanding(career.findTeamByName("Primera Lider"), 68, 21, 5, 52, 24);
    setStanding(career.findTeamByName("Primera Medio"), 56, 17, 5, 45, 28);
    setStanding(career.findTeamByName("Primera Tenso"), 31, 8, 7, 27, 43);
    setStanding(career.findTeamByName("Primera Descenso"), 22, 5, 7, 21, 49);

    setStanding(career.findTeamByName("Segunda Deportivo"), 61, 18, 7, 46, 24);
    setStanding(career.findTeamByName("Segunda Rico"), 34, 9, 7, 33, 37);
    setStanding(career.findTeamByName("Segunda Centro"), 49, 14, 7, 41, 31);
    setStanding(career.findTeamByName("Segunda Fondo"), 20, 5, 5, 22, 48);

    career.setActiveDivision("primera b");
    career.myTeam = career.findTeamByName("Primera B Lider");
    expect(career.myTeam != nullptr, "La prueba de ascensos por tabla necesita club usuario.");

    setStanding(career.findTeamByName("Primera B Lider"), 67, 20, 7, 51, 22);
    setStanding(career.findTeamByName("Primera B Liguilla"), 59, 17, 8, 44, 29);
    setStanding(career.findTeamByName("Primera B Medio"), 38, 10, 8, 31, 38);
    setStanding(career.findTeamByName("Primera B Fondo"), 21, 5, 6, 20, 47);
    career.leagueTable.sortTable();

    SeasonTransitionSummary summary = endSeason(career);

    expect(!summary.lines.empty(), "La transicion debe seguir devolviendo resumen legible.");
    expect(career.findTeamByName("Segunda Deportivo")->division == "primera b",
           "El ascenso desde Segunda debe seguir la tabla y no el valor del plantel.");
    expect(career.findTeamByName("Segunda Rico")->division == "segunda division",
           "Un club mas caro no debe ascender si su rendimiento fue inferior.");
}

void testSeasonServiceReturnsStructuredWeekResult() {
    Career career;
    career.currentSeason = 3;
    career.currentWeek = 1;

    career.allTeams.push_back(makeTeam("Servicio Local", "primera division", 72, 4, 4, "Pressing", "Contra-presion", 650000));
    career.allTeams.push_back(makeTeam("Servicio Visita", "primera division", 69, 2, 2, "Defensive", "Bloque bajo", 520000));
    career.allTeams.push_back(makeTeam("Servicio Centro", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 500000));
    career.allTeams.push_back(makeTeam("Servicio Sur", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 470000));

    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Servicio Local");
    expect(career.myTeam != nullptr, "El fixture del SeasonService debe tener club de usuario.");
    expect(!career.schedule.empty(), "El fixture del SeasonService debe crear calendario.");

    SeasonService service;
    SeasonStepResult result = service.simulateWeek(career);

    expect(result.ok, "SeasonService debe completar la simulacion semanal.");
    expect(result.week.ok, "WeekSimulationResult debe marcarse como exitoso.");
    expect(result.week.weekBefore == 1 && result.week.weekAfter == 2,
           "SeasonService debe avanzar exactamente una semana regular.");
    expect(result.week.seasonBefore == 3 && result.week.seasonAfter == 3,
           "Una fecha normal no debe cerrar la temporada.");
    expect(!result.week.messages.empty(), "SeasonService debe devolver mensajes estructurados.");
    expect(!result.week.lastMatchAnalysis.empty(), "SeasonService debe propagar el ultimo analisis de partido.");
}

void testOpponentReportExplainsNextFixture() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 1;

    career.allTeams.push_back(makeTeam("Reporte Azul", "primera division", 71, 4, 3, "Pressing", "Contra-presion", 600000));
    career.allTeams.push_back(makeTeam("Reporte Rival", "primera division", 69, 2, 4, "Counter", "Juego directo", 520000));
    career.allTeams.push_back(makeTeam("Reporte Centro", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 500000));
    career.allTeams.push_back(makeTeam("Reporte Sur", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 470000));

    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Reporte Azul");
    expect(career.myTeam != nullptr, "El reporte rival necesita club usuario.");

    const string report = buildOpponentReport(career);
    expect(report.find("Sin informe rival disponible") == string::npos,
           "El informe rival no debe quedar vacio cuando existe una fecha activa.");
    expect(report.find("lineas") != string::npos, "El informe rival debe incluir lectura por lineas.");
    expect(report.find("alerta") != string::npos, "El informe rival debe incluir una vulnerabilidad o alerta.");
}

void testMatchAnalysisStoreProducesStructuredCareerData() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 4;
    career.allTeams.push_back(makeTeam("Analisis Local", "primera division", 72, 4, 4, "Pressing", "Contra-presion", 700000));
    career.allTeams.push_back(makeTeam("Analisis Rival", "primera division", 68, 2, 3, "Counter", "Juego directo", 620000));
    career.myTeam = &career.allTeams[0];

    const MatchResult result = match_engine::simulate(career.allTeams[0], career.allTeams[1], true, false).result;
    career_match_analysis::storeMatchAnalysis(career, career.allTeams[0], career.allTeams[1], result, false);

    expect(!career.lastMatchAnalysis.empty(), "El servicio de analisis debe guardar un resumen del ultimo partido.");
    expect(!career.lastMatchReportLines.empty(), "El servicio de analisis debe guardar lineas estructuradas del reporte.");
    expect(!career.lastMatchEvents.empty(), "El servicio de analisis debe guardar eventos del partido.");
    expect(!career.lastMatchPlayerOfTheMatch.empty(), "El servicio de analisis debe conservar la figura del partido.");

    const string detail = career_match_analysis::buildLastMatchInsightText(career, 3, 4);
    expect(detail.find("MatchReport") != string::npos, "La vista detallada debe incluir el bloque MatchReport.");
    expect(detail.find("MatchTimeline") != string::npos, "La vista detallada debe incluir el bloque MatchTimeline.");
}

void testMatchCenterServiceBuildsStructuredView() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 7;
    career.allTeams.push_back(makeTeam("Centro Local", "primera division", 73, 4, 4, "Pressing", "Contra-presion", 760000));
    career.allTeams.push_back(makeTeam("Centro Rival", "primera division", 69, 2, 3, "Counter", "Juego directo", 630000));
    career.myTeam = &career.allTeams[0];

    const MatchResult result = match_engine::simulate(career.allTeams[0], career.allTeams[1], true, false).result;
    career_match_analysis::storeMatchAnalysis(career, career.allTeams[0], career.allTeams[1], result, false);

    const MatchCenterView center = match_center_service::buildLastMatchCenter(career, 3, 4);
    expect(center.available, "El match center debe marcarse disponible tras almacenar un partido.");
    expect(!center.scoreboard.empty(), "El match center debe construir un marcador legible.");
    expect(center.metrics.size() >= 4, "El match center debe exponer metricas comparables.");
    expect(!center.phaseLines.empty(), "El match center debe conservar lectura por fases.");
    expect(!center.eventLines.empty(), "El match center debe conservar una timeline resumida.");
    expect(!career.lastMatchCenter.opponentName.empty(), "El snapshot persistente del ultimo partido debe llenarse.");
}

void testDressingRoomServiceFlagsPromiseAndFatigueRisk() {
    Career career;
    career.currentSeason = 4;
    career.currentWeek = 8;
    career.allTeams.push_back(makeTeam("Vestuario Unido", "primera division", 70, 4, 4, "Pressing", "Contra-presion", 720000));
    career.myTeam = &career.allTeams[0];

    Player& starter = career.myTeam->players[1];
    starter.promisedRole = "Titular";
    starter.startsThisSeason = 1;
    starter.happiness = 44;
    starter.fitness = 53;
    starter.contractWeeks = 10;

    Player& rotation = career.myTeam->players[2];
    rotation.promisedRole = "Rotacion";
    rotation.startsThisSeason = 0;
    rotation.happiness = 42;
    rotation.fitness = 56;

    const DressingRoomSnapshot snapshotBefore =
        dressing_room_service::buildSnapshot(*career.myTeam, career.currentWeek);
    expect(snapshotBefore.promiseRiskCount >= 2, "El snapshot debe detectar promesas en riesgo.");
    expect(snapshotBefore.fatigueRiskCount >= 2, "El snapshot debe detectar jugadores fundidos.");

    const DressingRoomSnapshot snapshotAfter = dressing_room_service::applyWeeklyUpdate(career, 0);
    expect(snapshotAfter.lowMoraleCount >= 2, "La actualizacion semanal debe reflejar mal clima.");
    expect(!snapshotAfter.summary.empty(), "La actualizacion semanal debe devolver un resumen de vestuario.");
    expect(career.newsFeed.size() > 0, "Una semana conflictiva debe dejar noticia de vestuario.");
}

void testDressingRoomSnapshotTracksSocialTension() {
    Career career;
    career.currentSeason = 5;
    career.currentWeek = 9;
    career.allTeams.push_back(makeTeam("Grupo Quebrado", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 710000));
    career.myTeam = &career.allTeams[0];

    for (int idx : {1, 2, 3}) {
        Player& player = career.myTeam->players[static_cast<size_t>(idx)];
        player.happiness = 39;
        player.startsThisSeason = 0;
        player.unhappinessWeeks = 4;
        player.wantsToLeave = true;
    }
    career.myTeam->players[0].leadership = 80;
    career.myTeam->players[0].happiness = 67;

    const DressingRoomSnapshot snapshot = dressing_room_service::buildSnapshot(*career.myTeam, career.currentWeek);
    expect(snapshot.socialTension >= 4, "El snapshot debe medir tension social cuando se acumulan focos de conflicto.");
    expect(snapshot.coreGroupSize >= 2, "El vestuario debe identificar un grupo dominante.");
    expect(find(snapshot.groups.begin(), snapshot.groups.end(), string("Frustrados: 3")) != snapshot.groups.end(),
           "Los jugadores conflictivos deben agruparse como foco social visible.");
}

void testScoutingConfidenceReflectsStaffQuality() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 3;
    career.allTeams.push_back(makeTeam("Club Scout", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 1500000));
    career.allTeams.push_back(makeTeam("Club Fuente", "primera division", 72, 3, 3, "Balanced", "Equilibrado", 900000));
    career.allTeams.push_back(makeTeam("Club Apoyo", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 850000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Club Scout");
    expect(career.myTeam != nullptr, "La prueba de scouting necesita club usuario.");

    career.myTeam->scoutingChief = 45;
    career.myTeam->budget = 1500000;
    const ScoutingSessionResult low = runScoutingSessionService(career, "Todas", "MED");
    expect(low.service.ok && !low.candidates.empty(), "El scouting debe devolver candidatos en el escenario base.");
    const int lowConfidence = low.candidates.front().confidence;
    expect(low.candidates.front().salaryExpectation > 0,
           "El informe debe exponer una expectativa salarial utilizable.");
    expect(!low.candidates.front().riskLabel.empty(), "El informe debe etiquetar el riesgo del objetivo.");

    career.myTeam->scoutingChief = 88;
    career.myTeam->budget = 1500000;
    const ScoutingSessionResult high = runScoutingSessionService(career, "Todas", "MED");
    expect(high.service.ok && !high.candidates.empty(), "El scouting de alto nivel debe devolver candidatos.");
    expect(high.candidates.front().confidence > lowConfidence,
           "Un mejor jefe de scouting debe elevar la confianza del informe.");
}

void testSeasonTransitionResolvesCarryoverPromises() {
    Career career;
    career.currentSeason = 6;
    career.currentWeek = 34;
    career.managerName = "Manager Promesas";
    career.managerReputation = 66;

    career.allTeams.push_back(makeTeam("Promesa Capital", "primera division", 75, 3, 3, "Balanced", "Equilibrado", 900000));
    career.allTeams.push_back(makeTeam("Promesa Norte", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 760000));
    career.allTeams.push_back(makeTeam("Promesa Sur", "primera division", 67, 2, 2, "Defensive", "Bloque bajo", 580000));
    career.allTeams.push_back(makeTeam("Promesa Costa", "primera division", 66, 2, 2, "Defensive", "Pausar juego", 560000));
    career.allTeams.push_back(makeTeam("Ascenso A", "primera b", 69, 3, 3, "Balanced", "Equilibrado", 420000));
    career.allTeams.push_back(makeTeam("Ascenso B", "primera b", 68, 3, 3, "Balanced", "Equilibrado", 400000));
    career.allTeams.push_back(makeTeam("Ascenso C", "primera b", 64, 2, 2, "Balanced", "Equilibrado", 330000));
    career.allTeams.push_back(makeTeam("Ascenso D", "primera b", 63, 2, 2, "Balanced", "Equilibrado", 320000));

    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Promesa Capital");
    expect(career.myTeam != nullptr, "La prueba de transicion necesita club usuario.");

    career.myTeam->players[1].name = "Promesa Tardia";
    career.myTeam->players[1].promisedRole = "Titular";
    career.myTeam->players[1].startsThisSeason = 1;
    career.activePromises.push_back({"Promesa Tardia", "Minutos", "Titular", 30, 40, 1, false, false});

    const vector<int> points = {70, 61, 29, 24};
    for (int i = 0; i < career.getActiveTeamCount(); ++i) {
        Team* activeTeam = career.getActiveTeamAt(i);
        expect(activeTeam != nullptr, "La prueba necesita equipos activos validos.");
        if (!activeTeam) continue;
        activeTeam->points = points[static_cast<size_t>(i)];
        activeTeam->goalsFor = 31 - i * 3;
        activeTeam->goalsAgainst = 12 + i * 4;
        activeTeam->wins = points[static_cast<size_t>(i)] / 3;
    }
    career.leagueTable.sortTable();

    SeasonTransitionSummary summary = endSeason(career);
    const string lines = joinLines(summary.lines);
    expect(lines.find("Promesas cerradas al final del curso") != string::npos,
           "La transicion debe cerrar promesas pendientes antes de resetear la temporada.");
    expect(lines.find("Promesa rota:") != string::npos || lines.find("Promesa cerrada:") != string::npos,
           "La transicion debe explicar como se resolvio la promesa arrastrada.");
}

void testSaveLoadRoundTripPreservesCareerState() {
    const string savePath = processScopedTestPath("saves/test_roundtrip_save.txt");

    Career original;
    original.currentSeason = 8;
    original.currentWeek = 5;
    original.managerName = "Arquitecto|Senior";
    original.managerReputation = 77;
    original.boardConfidence = 66;
    original.boardExpectedFinish = 3;
    original.boardBudgetTarget = 250000;
    original.boardYouthTarget = 2;
    original.boardWarningWeeks = 1;
    original.boardMonthlyObjective = "Sumar; al menos ^6 puntos | en 4 semanas";
    original.boardMonthlyTarget = 6;
    original.boardMonthlyProgress = 4;
    original.boardMonthlyDeadlineWeek = 8;
    original.lastMatchAnalysis = "Control total | del mediocampo; con ajuste^final";
    original.lastMatchReportLines = {"Claves| se domino el mediocampo", "Fatiga; el rival llego roto", "Disciplina^ sin expulsiones"};
    original.lastMatchEvents = {"12' Gol | de prueba", "64' Ajuste; tactico", "88' Atajada ^ clave"};
    original.lastMatchPlayerOfTheMatch = "Jugador|Persistente";
    original.lastMatchCenter.competitionLabel = "Liga";
    original.lastMatchCenter.opponentName = "Club Destino";
    original.lastMatchCenter.venueLabel = "Local";
    original.lastMatchCenter.myGoals = 2;
    original.lastMatchCenter.oppGoals = 1;
    original.lastMatchCenter.myShots = 11;
    original.lastMatchCenter.oppShots = 8;
    original.lastMatchCenter.myShotsOnTarget = 5;
    original.lastMatchCenter.oppShotsOnTarget = 3;
    original.lastMatchCenter.myPossession = 56;
    original.lastMatchCenter.oppPossession = 44;
    original.lastMatchCenter.myCorners = 6;
    original.lastMatchCenter.oppCorners = 2;
    original.lastMatchCenter.mySubstitutions = 4;
    original.lastMatchCenter.oppSubstitutions = 5;
    original.lastMatchCenter.myExpectedGoalsTenths = 17;
    original.lastMatchCenter.oppExpectedGoalsTenths = 9;
    original.lastMatchCenter.weather = "Despejado; viento";
    original.lastMatchCenter.dominanceSummary = "El local controlo | la zona media.";
    original.lastMatchCenter.tacticalSummary = "La presion alta sostuvo recuperaciones ^altas.";
    original.lastMatchCenter.fatigueSummary = "El rival llego; fundido al cierre.";
    original.lastMatchCenter.postMatchImpact = "Moral +3 | / -2";
    original.lastMatchCenter.phaseSummaries = {"1-15: domina | Club Persistencia", "16-30: domina ^ Club Persistencia"};
    original.newsFeed.push_back("T8-F5: Noticia| de prueba;");
    original.managerInbox.push_back("[Resumen] T8-F5: Bandeja| de manager");
    original.scoutingAssignments.push_back({"Sur", "DEF", "Urgente", 2, 61});
    original.scoutInbox.push_back("Informe^ de ojeo");
    original.scoutingShortlist.push_back("Promesa| del norte; central");
    original.history.push_back({7, "primera division", "Club Persistencia", 2, "Club Persistencia", "Ascenso| Norte", "Descenso; Sur", "Nota ^ historica"});
    original.activePromises.push_back({"Proyecto Norte", "Minutos", "Proyecto", 5, 10, 2, false, false});
    original.historicalRecords.push_back({"Primera Division - Mas puntos", "Club Persistencia", "Club Persistencia", 7, 71, "Puntos en una temporada"});
    original.pendingTransfers.push_back({"Jugador| Pendiente", "Club Persistencia", "Club Destino", 9, 0, 120000, 9000, 104, false, false, "Titular^"});

    original.managerStress = initializeManagerStress();
    original.managerStress.stressLevel = 82;
    original.managerStress.energy = 64;
    original.managerStress.mentalFortitude = 73;
    original.infrastructure.levels.trainingGround = 4;
    original.infrastructure.levels.youthAcademy = 3;
    original.infrastructure.levels.medical = 4;
    original.infrastructure.levels.stadium = 3;
    original.infrastructure.levels.facilities = 2;
    original.debtStatus.totalDebt = 420000;
    original.debtStatus.debtSeverity = 38;
    original.debtStatus.weeksUntilSeizure = 5;
    SocialGroup clique;
    clique.id = "lideres";
    clique.name = "Lideres";
    clique.memberNames = {"Jugador Persistente", "Jugador Recluta"};
    clique.cohesion = 68;
    clique.moralBoost = 6;
    clique.personality = "leadership";
    original.dressingRoomDynamics.cliques.push_back(clique);
    RivalAI rival;
    rival.personality.teamName = "Club Destino";
    rival.personality.playstyle = "aggressive";
    rival.personality.adaptability = 80;
    rival.personality.tactical_awareness = 72;
    rival.personality.unpredictability = 28;
    RivalMemory rivalMemory;
    rivalMemory.opponentName = "Club Persistencia";
    rivalMemory.matchesPlayed = 2;
    rivalMemory.wins = 1;
    rivalMemory.draws = 0;
    rivalMemory.losses = 1;
    rivalMemory.favoredFormations = {"4-3-3"};
    rivalMemory.favoredTactics = {"aggressive"};
    rivalMemory.commonPlayPattern = "pressing_high";
    rivalMemory.identifiedWeaknesses = {"weak_defense"};
    rivalMemory.identifiedStrengths = {"fast_attack"};
    rivalMemory.lastMatchOutcome = -1;
    rivalMemory.consecutiveVsThisTeam = 1;
    rival.memoryBank.push_back(rivalMemory);
    original.rivalAIMap["Club Destino"] = rival;

    original.allTeams.push_back(makeTeam("Club Persistencia", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    original.allTeams.push_back(makeTeam("Club Destino", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 650000));
    original.setActiveDivision("primera division");
    original.myTeam = original.findTeamByName("Club Persistencia");
    expect(original.myTeam != nullptr, "El fixture de persistencia debe tener club de usuario.");
    original.myTeam->players[0].moraleMomentum = 7;
    original.myTeam->players[0].fatigueLoad = 31;
    original.myTeam->players[0].unhappinessWeeks = 2;
    original.myTeam->players[0].promisedPosition = "DEF";
    original.myTeam->players[0].socialGroup = "Lideres";
    original.myTeam->players[0].roleDuty = "Ataque";
    original.myTeam->players[0].individualInstruction = "Marcar fuerte";
    original.myTeam->players[0].personality = "Adaptable";
    original.myTeam->players[0].discipline = 81;
    original.myTeam->players[0].injuryProneness = 22;
    original.myTeam->players[0].adaptation = 77;
    original.myTeam->headCoachName = "DT Persistente";
    original.myTeam->headCoachStyle = "Presion";
    original.myTeam->headCoachTenureWeeks = 28;
    original.myTeam->jobSecurity = 47;
    original.myTeam->transferPolicy = "Cantera y valor futuro";
    original.myTeam->scoutingRegions = {"Metropolitana", "Sur", "Internacional"};
    original.myTeam->assistantCoachName = "Ayudante Persistente";
    original.myTeam->fitnessCoachName = "PF Persistente";
    original.myTeam->scoutingChiefName = "Scout Persistente";
    original.myTeam->youthCoachName = "Juveniles Persistente";
    original.myTeam->medicalChiefName = "Medico Persistente";
    original.myTeam->goalkeepingCoachName = "Arqueros Persistente";
    original.myTeam->performanceAnalystName = "Analista Persistente";
    expect(original.myTeam->headCoachTenureWeeks == 28 &&
               original.myTeam->assistantCoachName == "Ayudante Persistente",
           "El fixture debe sembrar tenure y staff nominal antes del guardado.");
    original.saveFile = savePath;

    expect(original.saveCareer(), "El guardado roundtrip debe completarse.");

    Career loaded;
    loaded.saveFile = savePath;
    expect(loaded.loadCareer(), "La carga roundtrip debe completarse.");
    expect(loaded.managerName == original.managerName, "La carga debe preservar el nombre del manager.");
    expect(loaded.managerReputation == original.managerReputation, "La carga debe preservar la reputacion del manager.");
    expect(loaded.boardConfidence == original.boardConfidence, "La carga debe preservar confianza de directiva.");
    expect(loaded.lastMatchAnalysis == original.lastMatchAnalysis, "La carga debe preservar el ultimo analisis de partido.");
    expect(loaded.lastMatchReportLines.size() == original.lastMatchReportLines.size(),
           "La carga debe preservar el reporte estructurado del ultimo partido.");
    expect(loaded.lastMatchEvents.size() == original.lastMatchEvents.size(),
           "La carga debe preservar la timeline del ultimo partido.");
    expect(loaded.lastMatchPlayerOfTheMatch == original.lastMatchPlayerOfTheMatch,
           "La carga debe preservar la figura del ultimo partido.");
    expect(loaded.lastMatchCenter.opponentName == original.lastMatchCenter.opponentName,
           "La carga debe preservar el snapshot del match center.");
    expect(loaded.lastMatchCenter.phaseSummaries.size() == original.lastMatchCenter.phaseSummaries.size(),
           "La carga debe preservar las fases resumidas del match center.");
    expect(loaded.history.size() == 1 && loaded.history.front().champion == "Club Persistencia",
           "La carga debe preservar historial de temporada.");
    expect(!loaded.managerInbox.empty() && loaded.managerInbox.front().find("Bandeja") != string::npos,
           "La carga debe preservar la bandeja del manager.");
    expect(!loaded.activePromises.empty() && loaded.activePromises.front().category == "Minutos",
           "La carga debe preservar promesas activas.");
    expect(!loaded.historicalRecords.empty() && loaded.historicalRecords.front().value == 71,
           "La carga debe preservar records historicos.");
    expect(loaded.myTeam != nullptr && loaded.myTeam->name == "Club Persistencia",
           "La carga debe restaurar el club controlado.");
    expect(loaded.myTeam->players[0].moraleMomentum == 7 &&
               loaded.myTeam->players[0].fatigueLoad == 31 &&
               loaded.myTeam->players[0].promisedPosition == "DEF" &&
               loaded.myTeam->players[0].roleDuty == "Ataque" &&
               loaded.myTeam->players[0].individualInstruction == "Marcar fuerte",
           "La carga debe preservar momento, carga, duty, instruccion y promesa de posicion del jugador.");
    expect(loaded.myTeam->players[0].personality == "Adaptable" &&
               loaded.myTeam->players[0].discipline == 81 &&
               loaded.myTeam->players[0].injuryProneness == 22 &&
               loaded.myTeam->players[0].adaptation == 77,
           "La carga debe preservar atributos avanzados del jugador.");
    expect(loaded.myTeam->headCoachName == "DT Persistente" &&
               loaded.myTeam->transferPolicy == "Cantera y valor futuro" &&
               loaded.myTeam->scoutingRegions.size() == 3 &&
               loaded.myTeam->headCoachTenureWeeks == 28,
           "La carga debe preservar metadata de entrenador, politica, red de scouting y antiguedad del club.");
    expect(loaded.myTeam->assistantCoachName == "Ayudante Persistente" &&
               loaded.myTeam->performanceAnalystName == "Analista Persistente",
           "La carga debe preservar el staff con nombre propio.");
    expect(!loaded.pendingTransfers.empty() && loaded.pendingTransfers.front().toTeam == "Club Destino",
           "La carga debe preservar fichajes pendientes.");
    expect(!loaded.scoutingAssignments.empty() && loaded.scoutingAssignments.front().region == "Sur",
           "La carga debe preservar asignaciones activas de scouting.");
    expect(loaded.managerStress.stressLevel == original.managerStress.stressLevel &&
               loaded.managerStress.energy == original.managerStress.energy &&
               loaded.managerStress.mentalFortitude == original.managerStress.mentalFortitude,
           "La carga debe preservar el estado de estres del manager.");
    expect(loaded.infrastructure.levels.trainingGround == original.infrastructure.levels.trainingGround &&
               loaded.infrastructure.levels.youthAcademy == original.infrastructure.levels.youthAcademy &&
               loaded.infrastructure.levels.medical == original.infrastructure.levels.medical &&
               loaded.infrastructure.levels.stadium == original.infrastructure.levels.stadium &&
               loaded.infrastructure.levels.facilities == original.infrastructure.levels.facilities,
           "La carga debe preservar el estado de infraestructura.");
    expect(loaded.debtStatus.totalDebt == original.debtStatus.totalDebt &&
               loaded.debtStatus.debtSeverity == original.debtStatus.debtSeverity &&
               loaded.debtStatus.weeksUntilSeizure == original.debtStatus.weeksUntilSeizure,
           "La carga debe preservar el estado de la deuda.");
    expect(loaded.dressingRoomDynamics.cliques.size() == original.dressingRoomDynamics.cliques.size() &&
               !loaded.dressingRoomDynamics.cliques.empty() &&
               loaded.dressingRoomDynamics.cliques.front().name == original.dressingRoomDynamics.cliques.front().name &&
               loaded.dressingRoomDynamics.cliques.front().cohesion == original.dressingRoomDynamics.cliques.front().cohesion,
           "La carga debe preservar el dinamismo del vestuario.");
    expect(loaded.rivalAIMap.size() == original.rivalAIMap.size() &&
               loaded.rivalAIMap.begin()->second.personality.teamName == original.rivalAIMap.begin()->second.personality.teamName &&
               loaded.rivalAIMap.begin()->second.memoryBank.size() == original.rivalAIMap.begin()->second.memoryBank.size() &&
               loaded.rivalAIMap.begin()->second.memoryBank.front().commonPlayPattern == original.rivalAIMap.begin()->second.memoryBank.front().commonPlayPattern,
           "La carga debe preservar la memoria y configuracion de la IA rival.");

    std::remove(savePath.c_str());
}

void testSaveSchemaAndIntegrityRejectCorruptPayload() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 4;
    career.allTeams.push_back(makeTeam("Schema FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Schema Rival", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Schema FC");
    expect(career.myTeam != nullptr, "La prueba de integridad necesita club usuario.");

    ostringstream encoded;
    expect(save_serialization::serializeCareer(encoded, career),
           "La carrera debe serializarse para validar versionado e integridad.");
    const string saveText = encoded.str();
    expect(saveText.find("save_version = 1") != string::npos,
           "El save debe declarar save_version = 1 para compatibilidad futura.");
    expect(saveText.find("INTEGRITY ") != string::npos,
           "El save debe incluir una linea de integridad.");

    string corrupt = saveText;
    const size_t marker = corrupt.find("INTEGRITY 1|2|");
    expect(marker != string::npos, "El fixture debe contener un snapshot de integridad esperado.");
    corrupt.replace(marker, string("INTEGRITY 1|2|").size(), "INTEGRITY 1|3|");

    istringstream input(corrupt);
    Career loaded;
    expect(!save_serialization::deserializeCareer(input, loaded),
           "La carga debe rechazar un save cuya integridad declarada no coincide con el contenido.");
}

void testExternalJsonLoaderAddsModLeagueAndAdvancedPlayers() {
    ScopedNamedTestLock modsLock("FootballManagerTests.ExternalJsonMods");
    expect(ensureDirectory("mods"), "La prueba de mods necesita crear la carpeta mods.");
    const string leaguesPath = "mods/leagues.json";
    const string teamsPath = "mods/teams.json";
    const string playersPath = "mods/players.json";
    ScopedFileRestore restoreLeagues(leaguesPath);
    ScopedFileRestore restoreTeams(teamsPath);
    ScopedFileRestore restorePlayers(playersPath);

    {
        ofstream leagues(leaguesPath);
        expect(leagues.is_open(), "No se pudo crear mods/leagues.json.");
        leagues << "[{\"id\":\"liga mod test\",\"display\":\"Liga Mod Test\",\"folder\":\"mods\"}]\n";
    }
    {
        ofstream teams(teamsPath);
        expect(teams.is_open(), "No se pudo crear mods/teams.json.");
        teams << "[{\"name\":\"Mod Probador\",\"division\":\"liga mod test\",\"budget\":310000,\"tactics\":\"Pressing\"}]\n";
    }
    {
        ofstream players(playersPath);
        expect(players.is_open(), "No se pudo crear mods/players.json.");
        players << "[{\"team\":\"Mod Probador\",\"name\":\"Canterano JSON\",\"position\":\"MED\",\"age\":18,"
                   "\"market_value\":150000,\"potential\":82,\"personality\":\"Adaptable\","
                   "\"discipline\":74,\"injury_proneness\":16,\"adaptation\":86,\"current_form\":63}]\n";
    }

    deque<Team> teams;
    ExternalJsonLoadResult result = loadExternalJsonData(teams);
    Team* modTeam = nullptr;
    for (auto& team : teams) {
        if (team.name == "Mod Probador") modTeam = &team;
    }
    expect(modTeam != nullptr, "El loader JSON debe agregar equipos desde mods/teams.json.");
    expect(any_of(result.divisions.begin(), result.divisions.end(), [](const DivisionInfo& division) {
               return division.id == "liga mod test";
           }),
           "El loader JSON debe exponer ligas nuevas desde mods/leagues.json.");
    const Player* prospect = nullptr;
    for (const auto& player : modTeam->players) {
        if (player.name == "Canterano JSON") prospect = &player;
    }
    expect(prospect != nullptr, "El loader JSON debe agregar jugadores desde mods/players.json.");
    expect(prospect->potential >= 82 && prospect->personality == "Adaptable" &&
               prospect->discipline == 74 && prospect->injuryProneness == 16 && prospect->adaptation == 86,
           "El loader JSON debe respetar atributos avanzados y tolerar campos faltantes.");
}

void testLoadTeamFromDirectoryFallsBackAndResolvesRawPositions() {
    const string folderPath = processScopedTestPath("saves/test_loader_team");
    const string playersPath = joinPath(folderPath, "players.txt");

    expect(ensureDirectory(folderPath), "No se pudo crear la carpeta temporal del loader.");
    ofstream file(playersPath);
    expect(file.is_open(), "No se pudo crear el players.txt temporal.");
    file
        << "- Uno | DEF (Uno Goalkeeper) | Edad: 25 | Valor: 120k\n"
        << "- Dos | N/A (Dos Goalkeeper) | Edad: 22 | Valor: 90k\n"
        << "- Tres | DEL (Tres Central Midfield) | Edad: 24 | Valor: 180k\n"
        << "- Cuatro | DEF (Cuatro Central Midfield) | Edad: 21 | Valor: 150k\n"
        << "- Cinco | MED (Cinco Attacking Midfield) | Edad: 20 | Valor: 140k\n"
        << "- Seis | DEL (Seis Centre-Forward) | Edad: 26 | Valor: 200k\n";
    file.close();

    Team loaded("Temporal FC");
    loaded.division = "primera b";
    expect(loadTeamFromFile(folderPath, loaded), "El loader debe caer a players.txt cuando no existe CSV.");
    expect(static_cast<int>(loaded.players.size()) >= 18, "El loader debe completar un plantel jugable.");

    Team* teamPtr = &loaded;
    int goalkeepers = 0;
    int midfielders = 0;
    for (const auto& player : teamPtr->players) {
        if (normalizePosition(player.position) == "ARQ") goalkeepers++;
        if (normalizePosition(player.position) == "MED") midfielders++;
    }
    expect(goalkeepers >= 2, "El loader debe garantizar cobertura minima de arqueros.");
    expect(midfielders >= 3, "El loader debe garantizar cobertura minima de mediocampo.");

    auto findPlayer = [&](const string& name) -> const Player* {
        for (const auto& player : teamPtr->players) {
            if (player.name == name) return &player;
        }
        return nullptr;
    };

    const Player* one = findPlayer("Uno");
    const Player* three = findPlayer("Tres");
    expect(one && one->position == "ARQ", "El loader debe resolver Goalkeeper desde la posicion cruda.");
    expect(three && three->position == "MED", "El loader debe priorizar la posicion cruda cuando la principal es inconsistente.");

    std::remove(playersPath.c_str());
}

void testSimulateMatchAppliesPostProcessState() {
    Team home = makeTeam("Aplicacion Local", "primera division", 71, 4, 4, "Pressing", "Contra-presion", 700000);
    Team away = makeTeam("Aplicacion Visita", "primera division", 68, 2, 3, "Balanced", "Equilibrado", 620000);

    const MatchResult result = simulateMatch(home, away, true, false);

    expect(home.goalsFor == result.homeGoals && home.goalsAgainst == result.awayGoals,
           "La aplicacion postpartido debe actualizar goles del local.");
    expect(away.goalsFor == result.awayGoals && away.goalsAgainst == result.homeGoals,
           "La aplicacion postpartido debe actualizar goles del visitante.");
    expect(away.awayGoals == result.awayGoals,
           "La aplicacion postpartido debe contabilizar goles de visita.");
    expect(home.points + away.points >= 2 && home.points + away.points <= 3,
           "La aplicacion postpartido debe repartir puntos validos.");

    int homePlayed = 0;
    for (const Player& player : home.players) {
        if (player.matchesPlayed > 0) homePlayed++;
    }
    int awayPlayed = 0;
    for (const Player& player : away.players) {
        if (player.matchesPlayed > 0) awayPlayed++;
    }
    expect(homePlayed >= 11 && awayPlayed >= 11,
           "La aplicacion postpartido debe registrar minutos para los participantes.");
}

void testWorldStateSeedsPromisesAndRecords() {
    Career career;
    career.currentSeason = 3;
    career.currentWeek = 2;
    career.boardExpectedFinish = 2;

    career.allTeams.push_back(makeTeam("Pulso Club", "primera division", 72, 4, 4, "Pressing", "Contra-presion", 700000));
    career.allTeams.push_back(makeTeam("Pulso Rival", "primera division", 68, 2, 3, "Balanced", "Equilibrado", 620000));
    career.allTeams.push_back(makeTeam("Pulso Norte", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 560000));
    career.allTeams.push_back(makeTeam("Pulso Sur", "primera division", 65, 2, 2, "Defensive", "Pausar juego", 550000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Pulso Club");
    expect(career.myTeam != nullptr, "La prueba de mundo necesita club usuario.");

    world_state_service::seedSeasonPromises(career);
    expect(career.activePromises.size() >= 2,
           "El estado del mundo debe sembrar promesas activas de competicion y gestion de plantilla.");

    career.leagueTable.sortTable();
    career.myTeam->points = 73;
    career.myTeam->goalsFor = 44;
    career.myTeam->goalsAgainst = 16;
    career.myTeam->players[8].goals = 17;
    career.leagueTable.sortTable();

    const int updates = world_state_service::updateSeasonRecords(career, career.leagueTable);
    expect(updates >= 1, "El estado del mundo debe registrar al menos un record historico cuando se supera una marca.");
    expect(!career.historicalRecords.empty(), "Los records historicos deben persistirse en la carrera.");
}

void testWeeklyWorldStateResolvesMinutesPromise() {
    Career career;
    career.currentSeason = 4;
    career.currentWeek = 8;

    career.allTeams.push_back(makeTeam("Promesa Activa", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Rival Uno", "primera division", 67, 2, 2, "Defensive", "Bloque bajo", 620000));
    career.allTeams.push_back(makeTeam("Rival Dos", "primera division", 66, 3, 3, "Balanced", "Equilibrado", 610000));
    career.allTeams.push_back(makeTeam("Rival Tres", "primera division", 65, 2, 2, "Defensive", "Pausar juego", 600000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Promesa Activa");
    expect(career.myTeam != nullptr, "La prueba de promesas necesita club usuario.");

    Player& youngster = career.myTeam->players[0];
    youngster.promisedRole = "Proyecto";
    youngster.startsThisSeason = 2;
    career.activePromises.push_back({youngster.name, "Minutos", "Proyecto", 4, 8, 2, false, false});

    const WorldPulseSummary summary = world_state_service::processWeeklyWorldState(career);
    expect(summary.resolvedPromises >= 1, "Una promesa de minutos cumplida debe resolverse en el pulso semanal.");
    expect(career.activePromises.empty(), "Las promesas resueltas deben salir del backlog activo.");
    expect(youngster.happiness >= 64, "Cumplir una promesa debe mejorar el estado del jugador.");
}


void testWeeklyTrainingScheduleAdaptsToCongestion() {
    Team team = makeTeam("Microciclo FC", "primera division", 70, 4, 4, "Pressing", "Juego directo", 700000);
    team.trainingFocus = "Ataque";

    const auto regular = development::buildWeeklyTrainingSchedule(team, false);
    const auto congested = development::buildWeeklyTrainingSchedule(team, true);
    expect(regular.size() == 6 && congested.size() == 6,
           "El microciclo semanal debe construir seis sesiones visibles.");

    int regularLoad = 0;
    for (const auto& session : regular) regularLoad += session.load;
    int congestedLoad = 0;
    for (const auto& session : congested) congestedLoad += session.load;
    expect(congestedLoad < regularLoad,
           "La semana congestionada debe bajar la carga total del entrenamiento.");
    expect(congested.front().focus == "Recuperacion",
           "La semana congestionada debe abrir con recuperacion.");
}

void testTeamMeetingImprovesMorale() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 6;
    career.allTeams.push_back(makeTeam("Union Manager", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Rival Uno", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 620000));
    career.allTeams.push_back(makeTeam("Rival Dos", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 610000));
    career.allTeams.push_back(makeTeam("Rival Tres", "primera division", 65, 2, 2, "Defensive", "Pausar juego", 600000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Union Manager");
    expect(career.myTeam != nullptr, "La prueba de reunion necesita club usuario.");

    career.myTeam->morale = 44;
    career.myTeam->players[0].happiness = 32;
    career.myTeam->players[0].wantsToLeave = true;
    const int previousMorale = career.myTeam->morale;
    const int previousHappiness = career.myTeam->players[0].happiness;

    const ServiceResult result = holdTeamMeetingService(career);
    expect(result.ok, "La reunion de plantel debe completar correctamente.");
    expect(career.myTeam->morale > previousMorale,
           "La reunion debe mejorar la moral colectiva.");
    expect(career.myTeam->players[0].happiness > previousHappiness,
           "La reunion debe mejorar el estado del jugador mas afectado.");
}

void testPlayerInstructionServiceCyclesInstruction() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 2;
    career.allTeams.push_back(makeTeam("Instruccion FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Rival A", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 620000));
    career.allTeams.push_back(makeTeam("Rival B", "primera division", 66, 2, 2, "Defensive", "Pausar juego", 610000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Instruccion FC");
    expect(career.myTeam != nullptr, "La prueba de instrucciones necesita club usuario.");

    Player& player = career.myTeam->players[8];
    const string before = player.individualInstruction;
    const ServiceResult result = cyclePlayerInstructionService(career, player.name);
    expect(result.ok, "La instruccion individual debe poder ciclarse desde servicio.");
    expect(player.individualInstruction != before, "La instruccion individual debe cambiar al menos un paso.");
}

void testInboxDecisionPrioritizesRecovery() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 4;
    career.allTeams.push_back(makeTeam("Inbox Medico", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Rival A", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 620000));
    career.allTeams.push_back(makeTeam("Rival B", "primera division", 66, 2, 2, "Defensive", "Pausar juego", 610000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Inbox Medico");
    expect(career.myTeam != nullptr, "La prueba de inbox medico necesita club usuario.");

    Player& player = career.myTeam->players[0];
    player.fitness = 48;
    player.fatigueLoad = 78;
    career.addInboxItem("Lesion y fatiga acumulada en el lateral titular.", "Medical");

    const auto actionable = inbox_service::buildActionableInbox(career, 5);
    expect(!actionable.empty() && actionable.front().command == "Revisar medico",
           "El inbox accionable debe traducir una alerta medica a comando concreto.");

    const ServiceResult result = resolveInboxDecisionService(career);
    expect(result.ok, "El inbox debe poder disparar una decision medica automatizada.");
    expect(career.myTeam->trainingFocus == "Recuperacion", "La decision medica debe orientar la semana a recuperacion.");
    expect(player.individualInstruction == "Descanso medico", "El jugador en riesgo debe recibir proteccion individual.");
}

void testInboxDecisionConsumesWeeklyDigest() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 4;
    career.allTeams.push_back(makeTeam("Inbox Semanal", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Rival Semanal", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Inbox Semanal");
    expect(career.myTeam != nullptr, "La prueba de cierre semanal necesita club usuario.");

    career.lastMatchCenter.opponentName = "Rival Semanal";
    career.lastMatchCenter.myGoals = 0;
    career.lastMatchCenter.oppGoals = 1;
    career.lastMatchCenter.myShots = 5;
    career.lastMatchCenter.oppShots = 9;
    career.lastMatchCenter.myShotsOnTarget = 1;
    career.lastMatchCenter.oppShotsOnTarget = 3;
    career.lastMatchCenter.myExpectedGoalsTenths = 7;
    career.lastMatchCenter.oppExpectedGoalsTenths = 11;
    career.addInboxItem("Urgente | Cockpit semanal: ataque plano | Decision: Entrenar fuerte", "Centro semanal");

    const ServiceResult result = resolveInboxDecisionService(career);
    expect(result.ok, "El inbox debe poder resolver un cierre semanal pendiente.");
    expect(!result.messages.empty() &&
               result.messages.front().find("cierre semanal") != string::npos,
           "El cierre semanal debe resolverse como recomendacion semanal, no como alerta generica.");
    expect(career.myTeam->trainingFocus == "Ataque",
           "La decision del cierre semanal debe reutilizar el contexto del ultimo partido.");
    expect(joinLines(career.managerInbox).find("[Centro semanal]") == string::npos,
           "El cierre semanal resuelto desde inbox debe desaparecer como pendiente.");
}

void testStaffReviewImprovesWeakestArea() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 3;
    career.allTeams.push_back(makeTeam("Staff Review", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 1200000));
    career.allTeams.push_back(makeTeam("Rival A", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 620000));
    career.allTeams.push_back(makeTeam("Rival B", "primera division", 66, 2, 2, "Defensive", "Pausar juego", 610000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Staff Review");
    expect(career.myTeam != nullptr, "La prueba de staff necesita club usuario.");

    career.myTeam->medicalTeam = 42;
    career.myTeam->medicalChiefName.clear();
    const ServiceResult result = reviewStaffStructureService(career);
    expect(result.ok, "La revision de staff debe poder reforzar el area mas debil.");
    expect(career.myTeam->medicalTeam > 42, "La revision de staff debe subir el nivel del area mas debil.");
}

void testScoutingAssignmentShowsInReport() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 2;
    career.allTeams.push_back(makeTeam("Asignacion Scout", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Rival A", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 620000));
    career.allTeams.push_back(makeTeam("Rival B", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 610000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Asignacion Scout");
    expect(career.myTeam != nullptr, "La prueba de asignacion necesita club usuario.");

    const ServiceResult result = createScoutingAssignmentService(career, "Sur", "DEF", 3);
    expect(result.ok, "La asignacion de scouting debe crearse correctamente.");
    expect(career.scoutingAssignments.size() == 1, "La carrera debe conservar la asignacion activa.");
    const CareerReport report = buildScoutingReport(career);
    const string dump = formatCareerReport(report);
    expect(dump.find("Sur") != string::npos && dump.find("DEF") != string::npos,
           "El informe de scouting debe mostrar las asignaciones activas.");
}

void testNegotiationTracksAgentCosts() {
    Career career;
    Team team = makeTeam("Club Grande", "primera division", 74, 4, 4, "Pressing", "Contra-presion", 1500000);
    Player player = team.players.front();
    player.contractWeeks = 40;
    player.value = max(player.value, 200000LL);
    player.releaseClause = max(player.releaseClause, 450000LL);

    const NegotiationState state = runRenewalNegotiation(career,
                                                         team,
                                                         player,
                                                         NegotiationProfile::Balanced,
                                                         NegotiationPromise::Starter,
                                                         12);
    expect(state.playerAccepted,
           "La renovacion de prueba debe cerrarse para validar el paquete contractual.");
    expect(state.agreedAgentFee > 0 && state.agreedLoyaltyBonus > 0,
           "La negociacion debe capturar fee de agente y bonus de fidelidad.");
    expect(state.agreedAppearanceBonus > 0,
           "La negociacion debe incluir bonus por partido.");
}

void testRoleDutyShapesMatchSnapshot() {
    Player player = makePlayer("Duty Test", "DEF", 68, 74, 24, 72, 72);
    player.role = "Carrilero";

    int attackAttack = player.attack;
    int defenseAttack = player.defense;
    player.roleDuty = "Ataque";
    match_internal::applyRoleModifier(player, attackAttack, defenseAttack);

    int attackDefend = player.attack;
    int defenseDefend = player.defense;
    player.roleDuty = "Defensa";
    match_internal::applyRoleModifier(player, attackDefend, defenseDefend);

    expect(attackAttack > attackDefend,
           "El duty ofensivo debe subir mas el peso ofensivo del rol respecto al duty defensivo.");
    expect(defenseDefend > defenseAttack,
           "El duty defensivo debe subir mas el peso defensivo del rol respecto al duty ofensivo.");
}

void testTeamIdentitySeedsWorldMetadata() {
    Team team = makeTeam("Metadata FC", "primera division", 69, 3, 3, "Pressing", "Contra-presion", 700000);
    team.headCoachName.clear();
    team.transferPolicy.clear();
    team.scoutingRegions.clear();
    team.goalkeepingCoach = 0;
    team.performanceAnalyst = 0;

    ensureTeamIdentity(team);

    expect(!team.headCoachName.empty(), "La identidad del club debe sembrar un entrenador principal.");
    expect(!team.transferPolicy.empty(), "La identidad del club debe sembrar una politica de mercado.");
    expect(!team.scoutingRegions.empty(), "La identidad del club debe sembrar una red minima de scouting.");
    expect(team.goalkeepingCoach > 0 && team.performanceAnalyst > 0,
           "La identidad del club debe completar el staff ampliado.");
}

void testManagerInboxTracksNewsAndScouting() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 3;
    career.allTeams.push_back(makeTeam("Inbox United", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 800000));
    career.allTeams.push_back(makeTeam("Region Sur FC", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Inbox United");
    expect(career.myTeam != nullptr, "La prueba de inbox necesita club usuario.");

    career.myTeam->scoutingRegions = {"Metropolitana", "Sur"};
    career.findTeamByName("Region Sur FC")->youthRegion = "Sur";
    career.addNews("[Mundo] Prueba de noticia prioritaria.");
    const size_t before = career.managerInbox.size();
    const ScoutingSessionResult session = runScoutingSessionService(career, "Sur", "DEF");
    expect(session.service.ok, "La sesion de scouting de prueba debe completarse.");
    expect(career.managerInbox.size() > before,
           "El inbox del manager debe crecer con noticias y scouting relevante.");
}

void testConfiguredWorldDataLoads() {
    const int managerChangeChance = world_state_service::worldRuleValue("manager_change_chance", 0);
    const vector<string> regions = world_state_service::listConfiguredScoutingRegions();
    expect(managerChangeChance > 0, "Las reglas de mundo configuradas deben cargarse desde CSV o fallback.");
    expect(find(regions.begin(), regions.end(), "Internacional") != regions.end(),
           "La lista configurable de regiones debe exponer la cobertura internacional.");
}

void testLeagueRegistryLoadsConfiguredData() {
    const vector<DivisionInfo>& divisions = listRegisteredDivisions();
    expect(!divisions.empty(), "El registro de ligas debe exponer divisiones configuradas.");
    expect(divisions.front().folder.find("LigaChilena") != string::npos,
           "El registro de ligas debe entregar carpetas de datos reales.");
    expect(registeredCompetitionRulesPath().find("competition_rules.csv") != string::npos,
           "El registro de ligas debe exponer la ruta de reglas de competicion.");
}

void testAnalyticsAndInboxServicesProduceUsefulBlocks() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 5;
    career.allTeams.push_back(makeTeam("Analitica FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 800000));
    career.allTeams.push_back(makeTeam("Rival Analitico", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Analitica FC");
    expect(career.myTeam != nullptr, "La prueba de analitica necesita club usuario.");
    career.lastMatchCenter.opponentName = "Rival Analitico";
    career.lastMatchCenter.myGoals = 2;
    career.lastMatchCenter.oppGoals = 1;
    career.lastMatchCenter.myExpectedGoalsTenths = 16;
    career.lastMatchCenter.oppExpectedGoalsTenths = 8;
    career.lastMatchCenter.tacticalSummary = "Se gano la espalda del rival.";
    career.lastMatchCenter.fatigueSummary = "El lateral derecho termino cargado.";
    career.addInboxItem("Hay revision de contratos en tres puestos.", "Directiva");
    career.scoutInbox.push_back("Scout: aparece una opcion joven en el sur.");

    const auto analytics = analytics_service::buildTeamAnalyticsLines(career, *career.myTeam);
    const auto inbox = inbox_service::buildInboxSummaryLines(career, 4);
    expect(!analytics.empty() && analytics.front().find("Indice ofensivo") != string::npos,
           "El data hub debe producir lineas utiles para reportes y GUI.");
    expect(!inbox.empty() && inbox.front().find("|") != string::npos,
           "El inbox combinado debe resumir canal y contenido.");
}

void testGameSettingsCycleAndDifficultyImpact() {
    GameSettings settings;
    settings.volume = 70;
    expect(game_settings::nextVolume(settings.volume) == 75,
           "El volumen debe ciclar al siguiente escalon previsto.");
    game_settings::cycleDifficulty(settings);
    expect(settings.difficulty == GameDifficulty::Challenging,
           "La dificultad debe avanzar al siguiente nivel.");
    game_settings::cycleSimulationSpeed(settings);
    expect(settings.simulationSpeed == SimulationSpeed::Rapid,
           "La velocidad de simulacion debe avanzar al siguiente ritmo previsto.");
    game_settings::cycleSimulationMode(settings);
    expect(settings.simulationMode == SimulationMode::Fast,
           "El modo de simulacion debe alternar entre detallado y rapido.");
    expect(game_settings::settingsSummary(settings).find("Velocidad") != string::npos,
           "El resumen de configuracion debe incluir la velocidad de simulacion.");

    Career career;
    career.boardConfidence = 52;
    career.managerReputation = 50;
    career.allTeams.push_back(makeTeam("Proyecto Ligero", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 600000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Proyecto Ligero");
    expect(career.myTeam != nullptr, "La prueba de settings necesita club usuario.");

    const long long baseBudget = career.myTeam->budget;
    settings.difficulty = GameDifficulty::Accessible;
    game_settings::applyNewCareerDifficulty(career, settings);

    expect(career.myTeam->budget > baseBudget,
           "La dificultad accesible debe dar algo mas de aire economico al arranque.");
    expect(career.boardConfidence > 52,
           "La dificultad accesible debe mejorar el respaldo inicial de la directiva.");
}

void testGameSettingsPersistenceAndFrontendScope() {
    const string settingsDir = processScopedTestPath("saves/test_settings_runtime");
    const string settingsPath = settingsDir + "/game_settings.cfg";
    std::remove(settingsPath.c_str());

    GameSettings original;
    original.volume = 25;
    original.difficulty = GameDifficulty::Challenging;
    original.simulationSpeed = SimulationSpeed::Relaxed;
    original.simulationMode = SimulationMode::Fast;
    original.language = UiLanguage::English;
    original.textSpeed = TextSpeed::Rapid;
    original.visualProfile = VisualProfile::HighContrast;
    original.menuMusicMode = MenuMusicMode::FrontendPages;
    original.menuAudioFade = false;
    original.menuThemeId = "el-crack";

    expect(game_settings::saveToDisk(original, settingsPath),
           "La configuracion del frontend debe persistirse en disco.");
    expect(pathExists(settingsPath),
           "Guardar configuracion debe crear tambien carpetas intermedias si hacen falta.");

    GameSettings loaded;
    loaded.volume = 100;
    loaded.difficulty = GameDifficulty::Accessible;
    loaded.simulationSpeed = SimulationSpeed::Rapid;
    loaded.simulationMode = SimulationMode::Detailed;
    loaded.language = UiLanguage::Spanish;
    loaded.textSpeed = TextSpeed::Relaxed;
    loaded.visualProfile = VisualProfile::Broadcast;
    loaded.menuMusicMode = MenuMusicMode::Off;
    loaded.menuAudioFade = true;
    loaded.menuThemeId = "otro";

    expect(game_settings::loadFromDisk(loaded, settingsPath),
           "La configuracion persistida debe volver a cargarse.");
    expect(loaded.volume == 25, "El volumen debe sobrevivir entre sesiones.");
    expect(loaded.difficulty == GameDifficulty::Challenging, "La dificultad persistida debe restaurarse.");
    expect(loaded.simulationSpeed == SimulationSpeed::Relaxed, "La velocidad persistida debe restaurarse.");
    expect(loaded.simulationMode == SimulationMode::Fast, "El modo de simulacion persistido debe restaurarse.");
    expect(loaded.language == UiLanguage::English, "El idioma persistido debe restaurarse.");
    expect(loaded.textSpeed == TextSpeed::Rapid, "La velocidad de texto persistida debe restaurarse.");
    expect(loaded.visualProfile == VisualProfile::HighContrast, "El perfil visual persistido debe restaurarse.");
    expect(loaded.menuMusicMode == MenuMusicMode::FrontendPages, "El alcance de musica persistido debe restaurarse.");
    expect(!loaded.menuAudioFade, "La preferencia de fade de audio debe sobrevivir entre sesiones.");
    expect(game_settings::shouldPlayMenuMusic(loaded, false, true),
           "El modo de musica extendido debe habilitar audio en otras pantallas del frontend.");
    expect(!game_settings::shouldPlayMenuMusic(loaded, false, false),
           "El audio del menu no debe salir del frontend.");
    expect(game_settings::uiPulseDelayMs(loaded) > game_settings::pageTransitionDelayMs(loaded),
           "El pulso general debe ser mas lento que la transicion de pagina.");

    std::remove(settingsPath.c_str());
}

void testLegacyDivisionIdentifiersCanonicalizeOnLoad() {
    Career original;
    original.currentSeason = 3;
    original.currentWeek = 11;
    original.managerName = "Moises";
    original.boardConfidence = 61;
    original.allTeams.push_back(makeTeam("Audax Italiano", "primera division", 71, 3, 3, "Balanced", "Equilibrado", 700000));
    original.allTeams.push_back(makeTeam("Rival Clasico", "primera division", 68, 2, 2, "Defensive", "Bloque bajo", 620000));
    original.setActiveDivision("primera division");
    original.myTeam = original.findTeamByName("Audax Italiano");
    expect(original.myTeam != nullptr, "La prueba de migracion de divisiones necesita club controlado.");

    ostringstream encoded;
    expect(save_serialization::serializeCareer(encoded, original),
           "La carrera base debe serializarse antes de simular un save legacy.");

    string legacySave = replaceAllCopy(encoded.str(), "primera division", "Primera Division");
    istringstream input(legacySave);
    Career loaded;
    loaded.saveFile = "saves/legacy_division_test.txt";
    expect(save_serialization::deserializeCareer(input, loaded),
           "El cargador debe aceptar identificadores legacy de division.");
    expect(loaded.activeDivision == "primera division",
           "La division activa debe migrarse al ID canonico.");
    expect(!loaded.divisions.empty() && loaded.divisions.front().id == "primera division",
           "La lista de divisiones cargadas debe reconstruirse con IDs canonicos.");
    expect(std::all_of(loaded.allTeams.begin(), loaded.allTeams.end(), [](const Team& team) {
               return team.division == "primera division";
           }),
           "Los equipos del save legacy deben quedar normalizados al ID canonico.");
    expect(loaded.myTeam != nullptr && loaded.myTeam->division == "primera division",
           "El club usuario debe mantener la division canonica tras la migracion.");
}

void testManagerHubDigestCombinesStaffAndAgenda() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 7;
    career.boardConfidence = 33;
    career.boardMonthlyObjective = "Entrar a puestos altos";
    career.boardMonthlyTarget = 3;
    career.boardMonthlyProgress = 1;
    career.allTeams.push_back(makeTeam("Hub FC", "primera division", 69, 4, 4, "Pressing", "Contra-presion", 180000));
    career.allTeams.push_back(makeTeam("Radar Sur", "primera division", 67, 2, 2, "Defensive", "Bloque bajo", 640000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Hub FC");
    expect(career.myTeam != nullptr, "La prueba del hub necesita club usuario.");

    career.myTeam->morale = 44;
    career.myTeam->debt = 420000;
    career.myTeam->players[0].contractWeeks = 8;
    career.myTeam->players[0].fitness = 53;
    career.myTeam->players[0].fatigueLoad = 76;
    career.myTeam->players[1].happiness = 39;
    career.scoutingShortlist.push_back("Radar Sur|" + career.findTeamByName("Radar Sur")->players[0].name);
    career.addInboxItem("La directiva pide reaccion inmediata.", "Directiva");

    const auto priority = inbox_service::buildPriorityInboxLines(career, 6);
    const auto actionable = inbox_service::buildActionableInbox(career, 6);
    const string digest = inbox_service::buildManagerHubDigest(career, 6);

    expect(!priority.empty(), "El hub del manager debe producir lineas priorizadas.");
    expect(!actionable.empty() && !actionable.front().destination.empty() && !actionable.front().command.empty(),
           "El hub debe producir entradas accionables con destino y comando.");
    expect(joinLines(priority).find("->") != string::npos,
           "Las lineas priorizadas deben mostrar hacia donde actuar.");
    expect(joinLines(priority).find("Staff |") != string::npos,
           "El hub debe mezclar alertas del staff.");
    expect(joinLines(priority).find("Agenda |") != string::npos,
           "El hub debe mezclar acciones del manager.");
    expect(digest.find("Centro del manager") != string::npos && digest.find("Staff |") != string::npos,
           "El digest del hub debe quedar legible para GUI e inbox.");
}

void testManagerAdviceHighlightsUrgentActions() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 6;
    career.boardConfidence = 34;
    career.boardMonthlyObjective = "Sumar al menos 6 puntos en 4 semanas";
    career.boardMonthlyTarget = 6;
    career.boardMonthlyProgress = 2;
    career.allTeams.push_back(makeTeam("Plan FC", "primera division", 68, 4, 4, "Pressing", "Contra-presion", 120000));
    career.allTeams.push_back(makeTeam("Rival Sur", "primera division", 67, 2, 2, "Defensive", "Bloque bajo", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Plan FC");
    expect(career.myTeam != nullptr, "La prueba de recomendaciones necesita club usuario.");

    career.myTeam->debt = 500000;
    career.myTeam->morale = 42;
    career.myTeam->players[0].contractWeeks = 8;
    career.myTeam->players[1].contractWeeks = 10;
    career.myTeam->players[0].fitness = 54;
    career.myTeam->players[0].fatigueLoad = 74;
    career.myTeam->players[1].happiness = 36;
    career.activePromises.push_back({career.myTeam->players[0].name, "Minutos", "Titular", 4, 8, 1, false, false});

    const auto advice = manager_advice::buildManagerActionLines(career, 6);
    const string dump = joinLines(advice);
    expect(dump.find("Renovar") != string::npos,
           "Las recomendaciones deben detectar contratos cortos.");
    expect(dump.find("vestuario") != string::npos || dump.find("Vestuario") != string::npos,
           "Las recomendaciones deben detectar gestion emocional urgente.");
    expect(dump.find("Directiva") != string::npos,
           "Las recomendaciones deben detectar presion de directiva.");
}

void testSuggestedBoardObjectiveReasonMatchesSituation() {
    Career career;
    career.currentSeason = 1;
    career.currentWeek = 2;
    career.allTeams.push_back(makeTeam("Prueba FC", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 50000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Prueba FC");
    expect(career.myTeam != nullptr, "La prueba del objetivo sugerido necesita club usuario.");

    career.myTeam->sponsorWeekly = 20000;
    career.myTeam->budget = 45000;
    career.myTeam->debt = 0;
    const string objective = manager_advice::buildSuggestedBoardObjective(career);
    const string reason = manager_advice::buildSuggestedBoardObjectiveReason(career);

    expect(objective.find("presupuesto") != string::npos,
           "El objetivo sugerido debe priorizar presupuesto cuando el club es de caja ajustada.");
    expect(reason.find("finanzas") != string::npos || reason.find("deuda") != string::npos,
           "La razon debe mencionar finanzas o deuda en escenarios de presupuesto ajustado.");

    career.myTeam->budget = 300000;
    career.myTeam->debt = 0;
    career.myTeam->players[0].injured = true;
    career.myTeam->players[1].injured = true;
    career.myTeam->players[2].injured = true;
    const string injuryObjective = manager_advice::buildSuggestedBoardObjective(career);
    const string injuryReason = manager_advice::buildSuggestedBoardObjectiveReason(career);
    expect(injuryObjective.find("Rotar") != string::npos || injuryObjective.find("cuidar") != string::npos,
           "El objetivo sugerido debe recomendar rotacion cuando hay varias lesiones.");
    expect(injuryReason.find("lesionados") != string::npos,
           "La razon debe mencionar lesionados cuando hay un riesgo de bajas.");
}

void testCareerStorylinesAddChileanFixtureContext() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 6;
    career.allTeams.push_back(makeTeam("Historia Local", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 720000));
    career.allTeams.push_back(makeTeam("Historia Rival", "primera division", 68, 4, 4, "Pressing", "Presion alta", 680000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Historia Local");
    Team* rival = career.findTeamByName("Historia Rival");
    expect(career.myTeam != nullptr && rival != nullptr,
           "La narrativa semanal necesita club controlado y rival.");

    career.myTeam->primaryRival = rival->name;
    career.myTeam->youthRegion = "Metropolitana";
    rival->youthRegion = "Sur";
    career.myTeam->points = 12;
    rival->points = 14;
    career.schedule = {{}, {}, {}, {}, {}, {{1, 0}}};

    const auto storylines = manager_advice::buildCareerStorylines(career, 8);
    const string dump = joinLines(storylines);
    expect(dump.find("Semana de clasico") != string::npos,
           "La narrativa debe elevar partidos de rivalidad como semana de clasico.");
    expect(dump.find("Semana de viaje") != string::npos,
           "La narrativa debe detectar visitas fuera de la zona del club.");
    expect(dump.find("Partido bisagra") != string::npos,
           "La narrativa debe marcar duelos cercanos por puntos en la tabla.");
}

void testTransferBriefingBuildsActionableMarketView() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 4;
    career.managerReputation = 72;
    career.scoutingShortlist.push_back("Mercado Norte|Objetivo Creativo");

    career.allTeams.push_back(makeTeam("Mercado Local", "primera division", 66, 3, 3, "Balanced", "Equilibrado", 950000));
    career.allTeams.push_back(makeTeam("Mercado Norte", "primera division", 72, 3, 3, "Balanced", "Equilibrado", 820000));
    career.allTeams.push_back(makeTeam("Mercado Sur", "primera division", 70, 2, 2, "Defensive", "Bloque bajo", 760000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Mercado Local");
    expect(career.myTeam != nullptr, "La prueba de briefing de mercado necesita club usuario.");

    Team* seller = career.findTeamByName("Mercado Norte");
    expect(seller != nullptr, "La prueba de briefing de mercado necesita club vendedor.");
    seller->players.clear();

    Player target = makePlayer("Objetivo Creativo", "MED", 76, 84, 22, 74, 76);
    target.value = 420000;
    target.releaseClause = 760000;
    target.wage = 18000;
    target.contractWeeks = 16;
    target.currentForm = 72;
    seller->addPlayer(target);

    Player loanTarget = makePlayer("Proyecto Cedido", "MED", 71, 83, 20, 75, 77);
    loanTarget.value = 360000;
    loanTarget.releaseClause = 700000;
    loanTarget.wage = 11000;
    loanTarget.contractWeeks = 92;
    seller->addPlayer(loanTarget);

    const auto options = transfer_briefing::buildTransferOptions(career, "MED", true, 5);
    expect(!options.empty(), "El briefing de mercado debe devolver objetivos.");
    expect(!options.front().competitionLabel.empty() &&
               !options.front().actionLabel.empty() &&
               !options.front().packageLabel.empty(),
           "Cada objetivo debe bajar el mercado a competencia, accion y paquete.");

    const auto pulse = transfer_briefing::buildMarketPulseLines(career, 4);
    expect(!pulse.empty(), "El mercado debe devolver una lectura resumida para la semana.");

    const auto opportunities = transfer_briefing::buildTransferOpportunityLines(career, "MED", 2);
    expect(!opportunities.empty() && opportunities.front().find("Entrada") != string::npos,
           "Las oportunidades resumidas deben incluir el paquete economico recomendado.");
}

void testScoutingBriefingMasksUncertainAttributes() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 4;
    career.allTeams.push_back(makeTeam("Radar Bajo", "primera division", 66, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Cantera Oculta", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Radar Bajo");
    expect(career.myTeam != nullptr, "La prueba de scouting incierto necesita club usuario.");
    career.myTeam->scoutingChief = 18;

    Team* seller = career.findTeamByName("Cantera Oculta");
    expect(seller != nullptr, "La prueba de scouting incierto necesita club vendedor.");
    seller->players.clear();
    Player target = makePlayer("Volante Tapado", "MED", 77, 88, 24, 74, 76);
    target.contractWeeks = 60;
    target.value = 520000;
    target.wage = 16000;
    seller->addPlayer(target);

    auto lowOptions = transfer_briefing::buildTransferOptions(career, "MED", true, 3);
    auto lowIt = find_if(lowOptions.begin(), lowOptions.end(), [](const transfer_briefing::TransferOptionBrief& option) {
        return option.playerName == "Volante Tapado";
    });
    expect(lowIt != lowOptions.end(), "El briefing debe listar al objetivo con scouting bajo.");
    expect(lowIt->skillLabel.find("-") != string::npos && lowIt->potentialLabel.find("-") != string::npos,
           "Con baja confianza el scouting debe mostrar rangos, no valores exactos.");

    career.myTeam->scoutingChief = 92;
    career.scoutingShortlist.push_back("Cantera Oculta|Volante Tapado");
    career.scoutingAssignments.push_back({"Cantera Oculta", "MED", "Urgente", 4, 95});
    auto highOptions = transfer_briefing::buildTransferOptions(career, "MED", true, 3);
    auto highIt = find_if(highOptions.begin(), highOptions.end(), [](const transfer_briefing::TransferOptionBrief& option) {
        return option.playerName == "Volante Tapado";
    });
    expect(highIt != highOptions.end(), "El briefing debe mantener al objetivo tras subir la cobertura.");
    expect(highIt->skillLabel == "77" && highIt->potentialLabel == "88",
           "Con alta confianza el scouting debe desbloquear valores exactos.");
}

void testTransferWindowBlocksImmediateDeals() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 8;
    career.managerReputation = 72;

    career.allTeams.push_back(makeTeam("Ventana Local", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 950000));
    career.allTeams.push_back(makeTeam("Ventana Norte", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 850000));
    career.setActiveDivision("primera division");
    career.schedule.assign(20, vector<pair<int, int>>{{0, 1}});
    career.myTeam = career.findTeamByName("Ventana Local");
    expect(career.myTeam != nullptr, "La prueba de ventana necesita club usuario.");

    expect(!transfer_market::isTransferWindowOpen(career),
           "La semana 8 debe quedar fuera de la ventana inicial y media.");
    const string label = transfer_market::transferWindowLabel(career);
    expect(label.find("Mercado cerrado") != string::npos,
           "La etiqueta de mercado debe declarar cierre de ventana.");

    ServiceResult buyResult =
        buyTransferTargetService(career, "Ventana Norte", "Ventana Norte_P1", NegotiationProfile::Balanced);
    expect(!buyResult.ok, "El servicio debe bloquear fichajes inmediatos fuera de ventana.");
    expect(joinLines(buyResult.messages).find("Mercado cerrado") != string::npos,
           "El bloqueo debe explicar que el mercado esta cerrado.");

    career.currentWeek = 10;
    expect(transfer_market::isTransferWindowOpen(career),
           "La semana media debe abrir una segunda ventana de mercado.");
}

void testTransferServicesMoveBoughtAndLoanedPlayers() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 1;
    career.managerReputation = 95;

    career.allTeams.push_back(makeTeam("Mercado Local", "primera division", 72, 3, 3, "Balanced", "Equilibrado", 8000000));
    career.allTeams.push_back(makeTeam("Mercado Vendedor", "primera b", 68, 3, 3, "Balanced", "Equilibrado", 900000));
    career.allTeams.push_back(makeTeam("Mercado Destino", "primera b", 64, 2, 3, "Balanced", "Equilibrado", 800000));
    career.allTeams[0].sponsorWeekly = 140000;
    career.allTeams[0].fanBase = 80;
    career.allTeams[0].trainingFacilityLevel = 5;
    career.allTeams[0].youthFacilityLevel = 5;
    career.allTeams[0].assistantCoach = 84;
    career.allTeams[0].performanceAnalyst = 84;
    career.allTeams[1].fanBase = 12;
    career.allTeams[1].trainingFacilityLevel = 1;
    career.allTeams[1].youthFacilityLevel = 1;
    career.allTeams[1].sponsorWeekly = 35000;
    career.allTeams[1].players[0].wantsToLeave = true;
    career.allTeams[1].players[0].ambition = 35;
    career.allTeams[1].players[0].professionalism = 80;
    career.allTeams[1].players[0].adaptation = 80;
    ensureTeamIdentity(career.allTeams[0]);
    ensureTeamIdentity(career.allTeams[1]);
    career.allTeams[1].addPlayer(makePlayer("Mercado Vendedor_Extra1", "MED", 66, 74, 22, 74, 76));
    career.allTeams[1].addPlayer(makePlayer("Mercado Vendedor_Extra2", "DEL", 65, 73, 23, 72, 75));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Mercado Local");
    expect(career.myTeam != nullptr, "La prueba de mercado necesita club usuario.");

    auto hasPlayer = [](const Team* team, const string& playerName) {
        return team && any_of(team->players.begin(), team->players.end(), [&](const Player& player) {
            return player.name == playerName;
        });
    };

    Team* seller = career.findTeamByName("Mercado Vendedor");
    Team* receiver = career.findTeamByName("Mercado Destino");
    expect(seller != nullptr && receiver != nullptr, "La prueba de mercado necesita clubes destino.");

    ServiceResult buyResult =
        buyTransferTargetService(career,
                                 "Mercado Vendedor",
                                 "Mercado Vendedor_P1",
                                 NegotiationProfile::Safe,
                                 NegotiationPromise::Starter);
    expect(buyResult.ok,
           string("El servicio debe permitir comprar jugadores en ventana abierta. ") + joinLines(buyResult.messages));
    expect(hasPlayer(career.myTeam, "Mercado Vendedor_P1"), "El jugador comprado debe entrar al plantel usuario.");
    expect(!hasPlayer(seller, "Mercado Vendedor_P1"), "El jugador comprado debe salir del club vendedor.");

    ServiceResult loanInResult = loanInPlayerService(career, "Mercado Vendedor", "Mercado Vendedor_P2", 26);
    expect(loanInResult.ok, "El servicio debe permitir pedir un jugador cedido en ventana abierta.");
    expect(hasPlayer(career.myTeam, "Mercado Vendedor_P2"), "El jugador cedido debe entrar al plantel usuario.");
    auto loanedIn = find_if(career.myTeam->players.begin(), career.myTeam->players.end(), [](const Player& player) {
        return player.name == "Mercado Vendedor_P2";
    });
    expect(loanedIn != career.myTeam->players.end() && loanedIn->onLoan && loanedIn->parentClub == "Mercado Vendedor",
           "El prestamo entrante debe conservar club propietario y estado de cesion.");

    ServiceResult loanOutResult = loanOutPlayerService(career, "Mercado Local_P3", "Mercado Destino", 20);
    expect(loanOutResult.ok, "El servicio debe permitir ceder jugadores propios en ventana abierta.");
    expect(!hasPlayer(career.myTeam, "Mercado Local_P3"), "El jugador cedido debe salir temporalmente del plantel usuario.");
    expect(hasPlayer(receiver, "Mercado Local_P3"), "El jugador cedido debe llegar al club destino.");
}

void testMarketPulseReflectsClosedWindow() {
    Career career;
    career.currentSeason = 2;
    career.currentWeek = 8;
    career.allTeams.push_back(makeTeam("Pulso Cerrado", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 650000));
    career.allTeams.push_back(makeTeam("Pulso Rival", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 650000));
    career.setActiveDivision("primera division");
    career.schedule.assign(20, vector<pair<int, int>>{{0, 1}});
    career.myTeam = career.findTeamByName("Pulso Cerrado");
    expect(career.myTeam != nullptr, "La prueba de pulso de mercado necesita club usuario.");

    const auto pulse = transfer_briefing::buildMarketPulseLines(career, 5);
    const string text = joinLines(pulse);
    expect(text.find("Mercado cerrado") != string::npos,
           "El pulso de mercado debe avisar cuando la ventana esta cerrada.");
    expect(text.find("shortlist") != string::npos || text.find("precontratos") != string::npos,
           "El pulso cerrado debe orientar hacia preparacion y no cierre inmediato.");
}

void testLateDeficitRaisesUrgencyInMatchPhase() {
    Team home = makeTeam("Urgencia Local", "primera division", 71, 3, 3, "Balanced", "Equilibrado", 780000);
    Team away = makeTeam("Urgencia Visitante", "primera division", 71, 3, 3, "Balanced", "Equilibrado", 780000);

    setRandomSeed(20260325);
    const MatchSetup setup = match_context::buildMatchSetup(home, away, false, false);
    setRandomSeed(4242);
    const MatchPhaseEvaluation even =
        match_phase::evaluatePhase(setup, home, away, setup.home, setup.away, 5, 76, 90, 0, 0, 11, 11);
    setRandomSeed(4242);
    const MatchPhaseEvaluation chasing =
        match_phase::evaluatePhase(setup, home, away, setup.home, setup.away, 5, 76, 90, 0, 1, 11, 11);
    resetRandomSeed();

    expect(chasing.report.homeUrgency > even.report.homeUrgency,
           "Ir perdiendo al final debe aumentar la urgencia del local.");
    expect(chasing.homeAttack >= even.homeAttack || chasing.homeAttacks >= even.homeAttacks,
           "El contexto de remontada debe empujar el volumen ofensivo.");
    expect(chasing.report.homeDefensiveRisk >= even.report.homeDefensiveRisk,
           "Empujar el resultado debe venir con mas riesgo defensivo.");
}

void testMatchInstructionsShapeChanceProfiles() {
    Team wide = makeTeam("Bandas FC", "primera division", 71, 3, 3, "Balanced", "Por bandas", 780000);
    wide.width = 5;
    Team patient = wide;
    patient.name = "Pausa FC";
    patient.matchInstruction = "Pausar juego";
    patient.width = 3;
    Team opponent = makeTeam("Rival Medio", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 760000);

    setRandomSeed(20260326);
    const MatchSetup wideSetup = match_context::buildMatchSetup(wide, opponent, false, false);
    setRandomSeed(20260326);
    const MatchSetup patientSetup = match_context::buildMatchSetup(patient, opponent, false, false);

    setRandomSeed(7777);
    const MatchPhaseEvaluation wideEval =
        match_phase::evaluatePhase(wideSetup, wide, opponent, wideSetup.home, wideSetup.away, 2, 31, 45, 0, 0, 11, 11);
    setRandomSeed(7777);
    const MatchPhaseEvaluation patientEval =
        match_phase::evaluatePhase(patientSetup, patient, opponent, patientSetup.home, patientSetup.away, 2, 31, 45, 0, 0, 11, 11);
    resetRandomSeed();

    expect(wideEval.report.homeChanceProbability > patientEval.report.homeChanceProbability,
           "Atacar por bandas debe generar mas probabilidad de ocasiones que pausar el juego.");
    expect(wideEval.homeAttacks >= patientEval.homeAttacks,
           "El plan por bandas debe sostener al menos el mismo volumen ofensivo.");
}

void testMatchInjuryTriggersRealReplacement() {
    Team team = makeTeam("Lesionados FC", "primera division", 70, 4, 4, "Pressing", "Contra-presion", 820000);
    Team opponent = makeTeam("Rival Control", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 810000);
    (void)opponent;

    vector<int> activeXI = team.getStartingXIIndices();
    expect(!activeXI.empty(), "La prueba de lesion necesita once inicial.");
    const int riskyIndex = activeXI.front();
    team.players[static_cast<size_t>(riskyIndex)].fitness = 34;
    team.players[static_cast<size_t>(riskyIndex)].fatigueLoad = 88;
    team.players[static_cast<size_t>(riskyIndex)].injuryHistory = 5;
    ensurePlayerProfile(team.players[static_cast<size_t>(riskyIndex)], true);

    vector<int> participants = activeXI;
    MatchTimeline timeline;
    vector<string> injuredPlayers;

    setRandomSeed(20260325);
    match_event_generator::maybeInjure(team, activeXI, participants, 1.0, 30, 45, timeline, injuredPlayers);
    resetRandomSeed();

    expect(!injuredPlayers.empty(), "El motor debe registrar la lesion forzada.");
    expect(find(activeXI.begin(), activeXI.end(), riskyIndex) == activeXI.end(),
           "El jugador lesionado no debe seguir activo en el XI.");
    expect(participants.size() >= 12,
           "Una lesion con banca disponible debe registrar un reemplazo.");
    expect(any_of(timeline.events.begin(), timeline.events.end(), [](const MatchEvent& event) {
               return event.type == MatchEventType::Substitution;
           }),
           "La cronologia debe reflejar el cambio obligado por lesion.");
}

void testTransferEvaluationPenalizesMedicalRisk() {
    Career career;
    Team buyer = makeTeam("Comprador Saludable", "primera division", 72, 3, 3, "Balanced", "Equilibrado", 900000);
    Team seller = makeTeam("Vendedor Test", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 700000);
    const ClubTransferStrategy strategy = ai_transfer_manager::buildClubTransferStrategy(career, buyer);

    Player healthy = makePlayer("Creador Sano", "MED", 74, 84, 23, 76, 78);
    healthy.contractWeeks = 40;
    healthy.value = 420000;
    healthy.releaseClause = 820000;
    healthy.wage = 17000;

    Player risky = healthy;
    risky.name = "Creador Fragil";
    risky.fitness = 44;
    risky.fatigueLoad = 82;
    risky.injuryHistory = 5;
    ensurePlayerProfile(risky, true);

    const TransferTarget healthyTarget = ai_transfer_manager::evaluateTarget(career, buyer, seller, healthy, strategy);
    const TransferTarget riskyTarget = ai_transfer_manager::evaluateTarget(career, buyer, seller, risky, strategy);

    expect(riskyTarget.medicalRisk > healthyTarget.medicalRisk,
           "El target fragil debe reflejar mas riesgo medico.");
    expect(riskyTarget.totalScore < healthyTarget.totalScore,
           "El mercado debe penalizar perfiles con mayor riesgo fisico.");
}

void testTeamPersonalityProfileShapesCpuTactics() {
    Team intense = makeTeam("Presion Identitaria", "primera division", 71, 3, 3, "Balanced", "Equilibrado", 760000);
    intense.headCoachStyle = "Intensidad";
    intense.clubStyle = "Presion vertical";
    intense.transferPolicy = "Mercado de oportunidades";
    intense.youthIdentity = "Desarrollo mixto";
    intense.morale = 74;
    for (auto& player : intense.players) {
        player.fitness = 78;
        player.injured = false;
    }

    Team ordered = makeTeam("Bloque Identitario", "primera division", 71, 3, 3, "Balanced", "Equilibrado", 760000);
    ordered.headCoachStyle = "Orden";
    ordered.clubStyle = "Bloque ordenado";
    ordered.transferPolicy = "Vender antes de comprar";
    ordered.youthIdentity = "Talento local";
    ordered.morale = 66;
    for (auto& player : ordered.players) {
        player.fitness = 78;
        player.injured = false;
    }

    Team opponent = makeTeam("Rival Referencia", "primera division", 69, 4, 4, "Pressing", "Contra-presion", 740000);
    for (auto& player : opponent.players) {
        player.fitness = 76;
        player.injured = false;
    }

    team_ai::adjustCpuTactics(intense, opponent);
    team_ai::adjustCpuTactics(ordered, opponent);

    expect(intense.tactics == "Pressing" || intense.pressingIntensity >= 4,
           "Un proyecto de intensidad debe traducirse en una IA mas agresiva.");
    expect(ordered.tactics == "Defensive" || ordered.defensiveLine <= 2,
           "Un proyecto de orden debe tender a un bloque mas conservador.");
}

void testTransferEvaluationReflectsClubPersonality() {
    Career career;
    career.currentSeason = 3;

    Team academy = makeTeam("Academia Azul", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 1600000);
    academy.headCoachStyle = "Control";
    academy.clubStyle = "Control de posesion";
    academy.youthIdentity = "Cantera estructurada";
    academy.transferPolicy = "Cantera y valor futuro";
    academy.youthFacilityLevel = 4;
    academy.youthCoach = 82;
    ensureTeamIdentity(academy);

    Team contender = makeTeam("Contendiente Rojo", "primera division", 73, 4, 4, "Pressing", "Contra-presion", 2000000);
    contender.headCoachStyle = "Ofensivo";
    contender.clubStyle = "Ataque por bandas";
    contender.youthIdentity = "Plantel de mercado";
    contender.transferPolicy = "Competir por titulares hechos";
    contender.clubPrestige = 78;
    ensureTeamIdentity(contender);

    Team seller = makeTeam("Mercado Gris", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 900000);
    ensureTeamIdentity(seller);

    Player prospect = makePlayer("Proyecto Fino", "MED", 68, 87, 19, 76, 78);
    prospect.value = 380000;
    prospect.releaseClause = 760000;
    prospect.wage = 12000;
    prospect.contractWeeks = 96;
    prospect.currentForm = 58;

    Player ready = makePlayer("Titular Hecho", "DEL", 82, 83, 27, 77, 80);
    ready.value = 420000;
    ready.releaseClause = 840000;
    ready.wage = 18000;
    ready.contractWeeks = 88;
    ready.currentForm = 79;

    const ClubTransferStrategy academyStrategy = ai_transfer_manager::buildClubTransferStrategy(career, academy);
    const ClubTransferStrategy contenderStrategy = ai_transfer_manager::buildClubTransferStrategy(career, contender);

    const TransferTarget academyProspect =
        ai_transfer_manager::evaluateTarget(career, academy, seller, prospect, academyStrategy);
    const TransferTarget academyReady =
        ai_transfer_manager::evaluateTarget(career, academy, seller, ready, academyStrategy);
    const TransferTarget contenderProspect =
        ai_transfer_manager::evaluateTarget(career, contender, seller, prospect, contenderStrategy);
    const TransferTarget contenderReady =
        ai_transfer_manager::evaluateTarget(career, contender, seller, ready, contenderStrategy);

    expect(academyProspect.totalScore > academyReady.totalScore,
           "Un club formador debe priorizar el talento joven sobre el impacto corto.");
    expect(contenderReady.totalScore > contenderProspect.totalScore,
           "Un club hecho para competir ya debe valorar mas al titular listo.");
}

void testOpponentReportIncludesCompetitivePersonality() {
    Career career;
    career.allTeams.push_back(makeTeam("Mi Club", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 820000));
    career.allTeams.push_back(makeTeam("Rival con Sello", "primera division", 72, 4, 3, "Balanced", "Equilibrado", 840000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Mi Club");
    Team* rival = career.findTeamByName("Rival con Sello");
    expect(career.myTeam != nullptr && rival != nullptr, "La prueba de informe rival necesita dos clubes.");

    rival->headCoachStyle = "Intensidad";
    rival->transferPolicy = "Rearme competitivo";
    rival->clubStyle = "Ataque por bandas";
    rival->morale = 67;
    career.schedule = {{{0, 1}}};
    career.currentWeek = 1;

    const string report = buildOpponentReport(career);
    expect(report.find("DT Intensidad") != string::npos,
           "El informe rival debe exponer la personalidad del entrenador.");
    expect(report.find("mercado Competir por titulares hechos") != string::npos,
           "El informe rival debe traducir la politica de mercado a una lectura usable.");
}

void testNextOpponentPlanBuildsActionableLines() {
    Career career;
    career.allTeams.push_back(makeTeam("Plan Local", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 820000));
    career.allTeams.push_back(makeTeam("Plan Rival", "primera division", 72, 5, 4, "Pressing", "Contra-presion", 840000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Plan Local");
    Team* rival = career.findTeamByName("Plan Rival");
    expect(career.myTeam != nullptr && rival != nullptr, "La prueba de plan rival necesita dos clubes.");

    rival->defensiveLine = 5;
    rival->width = 4;
    rival->morale = 74;
    career.schedule = {{{0, 1}}};
    career.currentWeek = 1;

    const vector<string> plan = buildNextOpponentPlanLines(career, 5);
    const string text = joinLines(plan);
    expect(plan.size() >= 4, "El plan rival debe entregar varias lineas accionables.");
    expect(text.find("Plan Rival") != string::npos, "El plan rival debe nombrar al oponente.");
    expect(text.find("Amenaza") != string::npos, "El plan rival debe exponer la amenaza principal.");
    expect(text.find("Vulnerabilidad") != string::npos, "El plan rival debe sugerir donde atacar.");
    expect(text.find("Plan sugerido") != string::npos, "El plan rival debe recomendar una instruccion o microciclo.");
}

void testMatchPreparationPlanServiceAppliesOpponentInstruction() {
    Career career;
    career.allTeams.push_back(makeTeam("Plan Aplicado", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 920000));
    career.allTeams.push_back(makeTeam("Rival Alto", "primera division", 72, 4, 4, "Pressing", "Contra-presion", 880000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Plan Aplicado");
    Team* rival = career.findTeamByName("Rival Alto");
    expect(career.myTeam != nullptr && rival != nullptr, "La prueba de plan aplicado necesita dos clubes.");

    rival->defensiveLine = 5;
    rival->width = 4;
    rival->pressingIntensity = 3;
    career.currentWeek = 1;
    career.schedule = {{{0, 1}}};

    const ServiceResult result = applyMatchPreparationPlanService(career);
    const string messages = joinLines(result.messages);
    expect(result.ok, "El plan de partido contextual debe aplicarse.");
    expect(career.myTeam->trainingFocus == "Preparacion partido",
           "El plan aplicado debe orientar el entrenamiento a preparacion de partido.");
    expect(career.myTeam->matchInstruction == "Juego directo",
           "Ante linea defensiva alta, el plan aplicado debe sugerir juego directo.");
    expect(messages.find("Plan de partido aplicado") != string::npos &&
               messages.find("Checklist del asistente") != string::npos,
           "El servicio debe explicar el plan aplicado y anexar checklist del asistente.");
}

void testMonthlyDevelopmentCycleImprovesStableProspects() {
    Team team = makeTeam("Cantera Pro", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 860000);
    team.youthCoach = 82;
    team.trainingFacilityLevel = 4;
    team.youthFacilityLevel = 4;
    team.performanceAnalyst = 78;
    team.assistantCoach = 76;

    vector<int> originalSkills;
    originalSkills.reserve(team.players.size());
    for (size_t i = 0; i < team.players.size(); ++i) {
        Player& player = team.players[i];
        if (i < 4) {
            player.age = 18 + static_cast<int>(i % 2);
            player.skill = 60 + static_cast<int>(i);
            player.potential = player.skill + 16;
            player.professionalism = 82;
            player.happiness = 72;
            player.matchesPlayed = 6;
            player.startsThisSeason = 4;
            player.currentForm = 68;
            player.fatigueLoad = 18;
            player.fitness = 78;
            player.developmentPlan = (i % 2 == 0) ? "Creatividad" : "Finalizacion";
            applyPositionStats(player);
            ensurePlayerProfile(player, true);
        }
        originalSkills.push_back(player.skill);
    }

    setRandomSeed(20260325);
    const development::MonthlyDevelopmentSummary summary = development::runMonthlyDevelopmentCycle(team, 20);
    resetRandomSeed();

    const bool anyImproved = any_of(team.players.begin(), team.players.end(), [&](const Player& player) {
        const ptrdiff_t index = &player - team.players.data();
        return index >= 0 && index < static_cast<ptrdiff_t>(originalSkills.size()) &&
               player.skill > originalSkills[static_cast<size_t>(index)];
    });

    expect(summary.improvedPlayers > 0 && anyImproved,
           "La progresion mensual debe empujar al menos a un prospecto estable.");
}

void testMatchCenterAddsRecommendedAdjustments() {
    Career career;
    career.lastMatchAnalysis = "Partido denso y largo.";
    career.lastMatchPlayerOfTheMatch = "Figura Test";
    career.lastMatchCenter.competitionLabel = "Liga";
    career.lastMatchCenter.opponentName = "Rival Tactico";
    career.lastMatchCenter.venueLabel = "Visita";
    career.lastMatchCenter.myGoals = 0;
    career.lastMatchCenter.oppGoals = 2;
    career.lastMatchCenter.myShots = 6;
    career.lastMatchCenter.oppShots = 15;
    career.lastMatchCenter.myShotsOnTarget = 1;
    career.lastMatchCenter.oppShotsOnTarget = 7;
    career.lastMatchCenter.myPossession = 39;
    career.lastMatchCenter.oppPossession = 61;
    career.lastMatchCenter.myExpectedGoalsTenths = 7;
    career.lastMatchCenter.oppExpectedGoalsTenths = 18;
    career.lastMatchCenter.weather = "Lluvia";
    career.lastMatchCenter.tacticalSummary = "El rival encontro la espalda.";
    career.lastMatchCenter.fatigueSummary = "El equipo llego roto al cierre.";
    career.lastMatchCenter.postMatchImpact = "Moral -4";
    career.lastMatchCenter.phaseSummaries = {"1-15: el rival rompe por fuera"};

    const MatchCenterView view = match_center_service::buildLastMatchCenter(career, 3, 3);
    expect(view.available, "El match center debe quedar disponible con snapshot cargado.");
    expect(!view.recommendationLines.empty(),
           "El match center debe devolver ajustes sugeridos cuando el partido deja problemas claros.");
    const string dump = joinLines(view.recommendationLines);
    expect(dump.find("Proteger mejor el area") != string::npos || dump.find("volumen ofensivo") != string::npos,
           "Los ajustes sugeridos deben explicar que corregir tras el partido.");
}

void testClubPrestigeUsesCanonicalTerceraDivisionIds() {
    Team terceraA = makeTeam("Prestigio A", "tercera division a", 60, 3, 3, "Balanced", "Equilibrado", 260000);
    Team terceraB = terceraA;
    terceraB.name = "Prestigio B";
    terceraB.division = "tercera division b";

    expect(teamPrestigeScore(terceraA) > teamPrestigeScore(terceraB),
           "Las divisiones canonicas de Tercera deben conservar escalones distintos de prestigio.");
}

#ifdef _WIN32
void testManagementViewFiltersChangeVisibleContent() {
    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Finances;
    state.career.allTeams.push_back(makeTeam("Filtro Club", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 850000));
    state.career.allTeams.push_back(makeTeam("Filtro Rival", "primera division", 67, 2, 2, "Defensive", "Bloque bajo", 620000));
    state.career.setActiveDivision("primera division");
    state.career.myTeam = state.career.findTeamByName("Filtro Club");
    expect(state.career.myTeam != nullptr, "La prueba de filtros GUI necesita club usuario.");

    Team& team = *state.career.myTeam;
    team.players[0].contractWeeks = 8;
    team.players[0].wantsToLeave = true;
    team.headCoachTenureWeeks = 18;
    state.career.boardMonthlyObjective = "Entrar al top 3";
    state.career.boardMonthlyTarget = 3;
    state.career.boardMonthlyProgress = 1;
    state.career.boardWarningWeeks = 2;
    state.career.history.push_back({3, "primera division", "Filtro Club", 5, "Campeon Historico", "Ascenso", "Descenso", "Nota de temporada"});

    state.currentFilter = "Salarios";
    const gui_win32::GuiPageModel salaryFinance = gui_win32::buildFinancesModel(state);
    expect(salaryFinance.primary.title == "SalaryTable",
           "El filtro Salarios debe llevar la tabla salarial al panel principal.");

    state.currentFilter = "Infraestructura";
    const gui_win32::GuiPageModel infraFinance = gui_win32::buildFinancesModel(state);
    expect(infraFinance.primary.title == "Infrastructure",
           "El filtro Infraestructura debe priorizar instalaciones y staff.");

    state.currentPage = gui_win32::GuiPage::Board;
    state.currentFilter = "Historial";
    const gui_win32::GuiPageModel boardHistory = gui_win32::buildBoardModel(state);
    expect(boardHistory.primary.title == "CoachHistory",
           "El filtro Historial debe mover el historial al panel principal.");

    state.currentPage = gui_win32::GuiPage::Tactics;
    state.currentFilter = "Presion alta";
    const gui_win32::GuiPageModel pressModel = gui_win32::buildTacticsModel(state);
    state.currentFilter = "Bloque bajo";
    const gui_win32::GuiPageModel blockModel = gui_win32::buildTacticsModel(state);
    expect(pressModel.infoLine != blockModel.infoLine,
           "Los filtros tacticos deben cambiar el enfoque visible de la pagina.");
    string tacticalRows;
    for (const auto& row : pressModel.primary.rows) {
        for (const auto& cell : row) tacticalRows += cell + "\n";
    }
    string tacticalXi;
    for (const auto& row : pressModel.secondary.rows) {
        for (const auto& cell : row) tacticalXi += cell + "\n";
    }
    expect(pressModel.summary.content.find("Familiaridad tactica") != string::npos,
           "La vista de tacticas debe exponer familiaridad tactica en el resumen.");
    expect(tacticalRows.find("Riesgo plan") != string::npos &&
               tacticalRows.find("Balance roles") != string::npos,
           "El tablero tactico debe mostrar riesgo del plan y balance de roles.");
    expect(pressModel.secondary.columns.size() >= 7 &&
               (tacticalXi.find("Encaje") != string::npos ||
                tacticalXi.find("Ajuste") != string::npos ||
                tacticalXi.find("Riesgo de rol") != string::npos),
           "El once inicial debe mostrar rol y encaje tactico por jugador.");
    expect(pressModel.detail.content.find("Lectura de familiaridad") != string::npos,
           "El detalle tactico debe explicar familiaridad, riesgo y recomendacion.");
    expect(pressModel.detail.content.find("Mapa tactico") != string::npos &&
               pressModel.detail.content.find("DEL:") != string::npos &&
               pressModel.detail.content.find("ARQ:") != string::npos,
           "El detalle tactico debe incluir una cancha textual con lineas del XI.");
}

void testCalendarModelShowsMatchPreparationPreview() {
    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Calendar;
    state.currentFilter = "Proximos 5";
    state.career.allTeams.push_back(makeTeam("Calendario Club", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 850000));
    state.career.allTeams.push_back(makeTeam("Calendario Rival", "primera division", 73, 5, 4, "Pressing", "Contra-presion", 900000));
    state.career.setActiveDivision("primera division");
    state.career.myTeam = state.career.findTeamByName("Calendario Club");
    expect(state.career.myTeam != nullptr, "La prueba de calendario necesita club usuario.");
    state.career.currentWeek = 1;
    state.career.schedule = {{{1, 0}}, {{0, 1}}, {{0, 1}}};
    state.career.myTeam->players[0].fitness = 55;
    state.career.myTeam->players[1].fatigueLoad = 65;
    state.career.myTeam->players[2].fatigueLoad = 68;

    const gui_win32::GuiPageModel model = gui_win32::buildCalendarModel(state);
    string rows;
    for (const auto& row : model.secondary.rows) {
        for (const auto& cell : row) rows += cell + "\n";
    }

    expect(model.primary.title == "FixtureListView",
           "Calendario debe conservar la lista de partidos como panel principal.");
    expect(model.secondary.title == "MatchPrepPanel",
           "Calendario debe priorizar la preparacion del proximo partido.");
    expect(model.summary.content.find("Previa:") != string::npos &&
               model.summary.content.find("Foco sugerido") != string::npos,
           "El resumen de calendario debe mostrar previa y foco sugerido.");
    expect(rows.find("Riesgo de fecha") != string::npos &&
               rows.find("Foco semanal") != string::npos &&
               rows.find("Calendario Rival") != string::npos,
           "El panel de preparacion debe mostrar rival, riesgo y foco semanal.");
    expect(model.detail.content.find("Previa del partido") != string::npos &&
               model.detail.content.find("Informe rival") != string::npos &&
               model.detail.content.find("Checklist previo") != string::npos,
           "El detalle del calendario debe incluir informe rival y checklist previo.");
}

void testBoardModelShowsInstitutionalPressurePanel() {
    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Board;
    state.currentFilter = "Objetivos";
    state.career.allTeams.push_back(makeTeam("Directiva Club", "primera division", 67, 3, 3, "Balanced", "Equilibrado", 90000));
    state.career.allTeams.push_back(makeTeam("Directiva Rival", "primera division", 72, 4, 4, "Pressing", "Contra-presion", 800000));
    state.career.setActiveDivision("primera division");
    state.career.myTeam = state.career.findTeamByName("Directiva Club");
    expect(state.career.myTeam != nullptr, "La prueba de directiva necesita club usuario.");
    state.career.currentWeek = 7;
    state.career.boardConfidence = 28;
    state.career.boardWarningWeeks = 4;
    state.career.boardMonthlyObjective = "Entrar al top 3";
    state.career.boardMonthlyTarget = 3;
    state.career.boardMonthlyProgress = 6;
    state.career.boardMonthlyDeadlineWeek = 8;
    state.career.myTeam->jobSecurity = 31;
    state.career.myTeam->debt = 400000;
    for (Player& player : state.career.myTeam->players) player.wage = 30000;

    const gui_win32::GuiPageModel model = gui_win32::buildBoardModel(state);
    string rows;
    for (const auto& row : model.primary.rows) {
        for (const auto& cell : row) rows += cell + "\n";
    }
    string profile;
    for (const auto& row : model.secondary.rows) {
        for (const auto& cell : row) profile += cell + "\n";
    }

    expect(model.summary.content.find("Mandato:") != string::npos &&
               model.summary.content.find("Presion institucional") != string::npos,
           "Directiva debe resumir mandato y presion institucional.");
    expect(rows.find("Presion directiva") != string::npos &&
               rows.find("Mandato") != string::npos &&
               rows.find("Estado objetivo") != string::npos,
           "La tabla de objetivos debe incluir presion, mandato y estado del objetivo.");
    expect(profile.find("Riesgo cargo") != string::npos &&
               profile.find("Riesgo caja") != string::npos,
           "El perfil institucional debe mostrar riesgo del cargo y riesgo financiero.");
    expect(model.detail.content.find("Panel de presion institucional") != string::npos &&
               model.detail.content.find("Recomendacion") != string::npos,
           "El detalle de directiva debe explicar la recomendacion institucional.");
}

void testFinancesModelShowsFairPlayPressure() {
    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Finances;
    state.currentFilter = "Resumen";
    state.career.allTeams.push_back(makeTeam("Fair Play Club", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 850000));
    state.career.allTeams.push_back(makeTeam("Fair Play Rival", "primera division", 65, 2, 2, "Defensive", "Bloque bajo", 620000));
    state.career.setActiveDivision("primera division");
    state.career.myTeam = state.career.findTeamByName("Fair Play Club");
    expect(state.career.myTeam != nullptr, "La prueba de fair play necesita club usuario.");

    for (Player& player : state.career.myTeam->players) {
        player.wage = 4000000;
    }

    const gui_win32::GuiPageModel model = gui_win32::buildFinancesModel(state);
    string rows;
    for (const auto& row : model.primary.rows) {
        for (const auto& cell : row) rows += cell + "\n";
    }

    expect(rows.find("Fair play salarial") != string::npos,
           "Finanzas debe mostrar el maximo salarial recomendado por fair play.");
    expect(rows.find("Riesgos fair play") != string::npos,
           "Finanzas debe mostrar cuantas alertas de fair play existen.");
    expect(model.detail.content.find("Alertas fair play") != string::npos,
           "El detalle financiero debe explicar las alertas de fair play.");
}

void testPlayerProfileShowsFmStyleStaffReport() {
    Team team = makeTeam("Ficha Club", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 850000);
    Player& player = team.players.front();
    player.name = "Volante Informe";
    player.position = "MED";
    player.secondaryPositions = {"DEF", "DEL"};
    player.skill = 74;
    player.potential = 86;
    player.attack = 76;
    player.defense = 64;
    player.setPieceSkill = 73;
    player.stamina = 71;
    player.fitness = 58;
    player.fatigueLoad = 66;
    player.leadership = 72;
    player.professionalism = 81;
    player.ambition = 75;
    player.consistency = 48;
    player.bigMatches = 70;
    player.tacticalDiscipline = 53;
    player.discipline = 52;
    player.adaptation = 78;
    player.injuryProneness = 72;
    player.contractWeeks = 10;
    player.value = 510000;
    player.wage = 18000;
    player.releaseClause = 900000;
    player.currentForm = 73;
    player.happiness = 44;
    player.developmentPlan = "Creatividad";
    player.role = "Organizador";
    player.roleDuty = "Apoyo";
    player.individualInstruction = "Pase vertical";
    player.traits = {"Competidor", "Pase riesgoso"};

    const string profile = gui_win32::buildPlayerProfile(team, &player);
    expect(profile.find("Atributos tecnicos") != string::npos,
           "La ficha debe separar atributos tecnicos.");
    expect(profile.find("Radar FM") != string::npos &&
               profile.find("Decision manager") != string::npos &&
               profile.find("Plan 30 dias") != string::npos,
           "La ficha debe sumar radar, decision del manager y plan de corto plazo.");
    expect(profile.find("Atributos mentales") != string::npos,
           "La ficha debe separar atributos mentales.");
    expect(profile.find("Atributos fisicos") != string::npos,
           "La ficha debe separar atributos fisicos.");
    expect(profile.find("Contrato y mercado") != string::npos &&
               profile.find("Valor") != string::npos,
           "La ficha debe mostrar lectura de contrato y mercado.");
    expect(profile.find("Informe del staff") != string::npos &&
               profile.find("Fortalezas") != string::npos &&
               profile.find("Debilidades") != string::npos &&
               profile.find("Recomendacion") != string::npos,
           "La ficha debe incluir informe del staff con fortalezas, debilidades y recomendacion.");
    expect(profile.find("Contrato corto") != string::npos || profile.find("contrato") != string::npos,
           "La recomendacion debe detectar riesgo contractual.");

    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Squad;
    state.currentFilter = "Todos";
    state.career.allTeams.push_back(team);
    state.career.myTeam = &state.career.allTeams.front();
    const gui_win32::ListPanelModel table = gui_win32::buildPlayerTableModel(state, false);
    string tableDump;
    for (const auto& column : table.columns) tableDump += std::string(column.first.begin(), column.first.end()) + "\n";
    for (const auto& row : table.rows) {
        for (const auto& cell : row) tableDump += cell + "\n";
    }
    expect(tableDump.find("Decision") != string::npos &&
               (tableDump.find("Renovar") != string::npos || tableDump.find("Rotar") != string::npos),
           "La tabla de plantilla debe mostrar una decision manager accionable por jugador.");
}

void testTransferFeedStaysMarketFocusedAcrossFilters() {
    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Transfers;
    state.career.allTeams.push_back(makeTeam("Mercado Club", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 900000));
    state.career.allTeams.push_back(makeTeam("Mercado Rival", "primera division", 72, 3, 3, "Balanced", "Equilibrado", 720000));
    state.career.setActiveDivision("primera division");
    state.career.myTeam = state.career.findTeamByName("Mercado Club");
    expect(state.career.myTeam != nullptr, "La prueba del feed de mercado necesita club usuario.");

    state.career.newsFeed.push_back("T4-F8: Resultado de copa sin novedades de mercado.");
    state.career.newsFeed.push_back("T4-F8: Rumor de fichaje por un delantero internacional.");

    state.currentFilter = "Todos";
    const gui_win32::GuiPageModel allModel = gui_win32::buildTransfersModel(state);
    state.currentFilter = "DEF";
    const gui_win32::GuiPageModel defModel = gui_win32::buildTransfersModel(state);

    const string allFeed = joinLines(allModel.feed.lines);
    const string defFeed = joinLines(defModel.feed.lines);
    expect(allFeed.find("fichaje") != string::npos,
           "La vista de mercado debe seguir mostrando noticias de fichajes con el filtro general.");
    expect(allFeed.find("Resultado de copa") == string::npos && defFeed.find("Resultado de copa") == string::npos,
           "El feed de Transfers no debe abrirse a noticias generales por cambiar el filtro de jugadores.");
}

void testTransferTargetsUseSelectablePrimaryList() {
    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Transfers;
    state.currentFilter = "Todos";
    state.career.currentSeason = 2;
    state.career.currentWeek = 1;
    state.career.managerReputation = 80;
    state.career.allTeams.push_back(makeTeam("Seleccion Local", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 1800000));
    state.career.allTeams.push_back(makeTeam("Seleccion Mercado", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 720000));
    state.career.allTeams.back().addPlayer(makePlayer("Seleccion Mercado_Extra", "MED", 66, 74, 22, 74, 76));
    state.career.setActiveDivision("primera division");
    state.career.myTeam = state.career.findTeamByName("Seleccion Local");
    expect(state.career.myTeam != nullptr, "La prueba de seleccion de mercado necesita club usuario.");
    state.career.myTeam->scoutingChief = 20;

    const gui_win32::GuiPageModel model = gui_win32::buildTransfersModel(state);
    expect(model.primary.title == "TransferMarketView",
           "Los objetivos comprables/cedibles deben renderizarse en el panel primario conectado a tableList.");
    expect(model.secondary.title == "TransferSearchPanel",
           "El panel secundario de Fichajes debe quedar para lectura/filtros, no para acciones de compra.");
    expect(!model.primary.rows.empty() && model.primary.rows.front().size() > 10,
           "La tabla de mercado debe exponer Jugador y Club para que Comprar/Pedir cesion identifiquen el objetivo.");
    expect(state.selectedTransferPlayer == model.primary.rows.front()[0] &&
               state.selectedTransferClub == model.primary.rows.front()[10],
           "La seleccion cacheada debe apuntar al mismo jugador/club que la fila seleccionable del mercado.");
    expect(model.primary.rows.front()[3].find("-") != string::npos &&
               model.primary.rows.front()[5].find("-") != string::npos,
           "Con bajo scouting, media y coste deben mostrarse como rangos y no como datos exactos.");
    expect(model.detail.content.find("Riesgo informe") != string::npos &&
               model.detail.content.find("Siguiente paso") != string::npos,
           "El detalle del objetivo debe explicar riesgo de scouting y siguiente accion.");
    expect(model.detail.content.find("Mesa de negociacion") != string::npos &&
               model.detail.content.find("Perfil Segura") != string::npos &&
               model.detail.content.find("Perfil Agresiva") != string::npos,
           "El detalle de mercado debe mostrar una mesa de negociacion con perfiles de oferta.");
}

void testDashboardActionCueHighlightsHardRisks() {
    gui_win32::AppState state;
    state.currentPage = gui_win32::GuiPage::Dashboard;
    state.currentFilter = "Todo";
    state.career.allTeams.push_back(makeTeam("Riesgo Club", "primera division", 66, 3, 3, "Balanced", "Equilibrado", 90000));
    state.career.allTeams.push_back(makeTeam("Riesgo Rival", "primera division", 70, 3, 3, "Pressing", "Presion alta", 700000));
    state.career.setActiveDivision("primera division");
    state.career.myTeam = state.career.findTeamByName("Riesgo Club");
    expect(state.career.myTeam != nullptr, "La prueba de proximas acciones necesita club usuario.");

    Team& team = *state.career.myTeam;
    state.career.boardConfidence = 38;
    state.career.boardWarningWeeks = 2;
    state.career.schedule = {{{0, 1}}};
    team.budget = 20000;
    team.players[0].injured = true;
    team.players[1].injured = true;
    team.players[2].matchesSuspended = 1;
    team.players[3].fitness = 55;
    team.players[4].fitness = 58;
    team.players[5].contractWeeks = 5;
    team.players[5].wage = 15000;
    state.career.addInboxItem(
        "Cockpit semanal: Directiva al frente | Directiva | Confianza 38/100 | Decision: Entrenar fuerte",
        "Centro semanal");

    const gui_win32::GuiPageModel model = gui_win32::buildDashboardModel(state);
    expect(model.footer.title == "ActionCuePanel",
           "El dashboard debe usar el panel de proximas acciones.");
    expect(!model.footer.rows.empty() &&
               model.footer.rows.front().size() >= 4 &&
               model.footer.rows.front()[2] == "Aplicar decision" &&
               model.footer.rows.front()[3].find("Cierre post-semana") != string::npos,
           "El cierre post-semana debe aparecer como primera accion ejecutable del dashboard.");

    string dump;
    for (const auto& row : model.footer.rows) {
        for (const auto& cell : row) dump += cell + "\n";
    }
    expect(dump.find("jugadores disponibles") != string::npos,
           "Proximas acciones debe avisar cuando la convocatoria queda corta.");
    expect(dump.find("Bajas acumuladas") != string::npos,
           "Proximas acciones debe agrupar lesiones y suspensiones.");
    expect(dump.find("Directiva bajo presion") != string::npos,
           "Proximas acciones debe detectar confianza baja o advertencias.");
    expect(dump.find("contrato") != string::npos,
           "Proximas acciones debe incluir contratos cortos.");

    expect(model.summary.content.find("Riesgos inmediatos") != string::npos,
           "El resumen de Inicio debe explicar los riesgos fuertes de la semana.");
    expect(model.summary.content.find("Cierre post-semana") != string::npos &&
               model.summary.content.find("Decision: Entrenar fuerte") != string::npos,
           "El dashboard debe mostrar el ultimo cierre semanal accionable en el resumen.");
    expect(model.detail.content.find("Cierre post-semana") != string::npos,
           "El panel de detalle debe repetir el cierre semanal junto al ultimo partido.");
}
#endif

void testIgnoredArchivedFoldersConfigSuppressesKnownWarnings() {
    const DataValidationReport report = buildRosterDataValidationReport();
    const string issueDump = joinLines(report.lines);
    expect(issueDump.find("AC Barnechea") == string::npos,
           "Las carpetas historicas ignoradas no deben seguir apareciendo como advertencias activas.");
}

void testTeamsTxtFolderAliasesDoNotTriggerOrphanWarnings() {
    const DataValidationReport report = buildRosterDataValidationReport();
    const string issueDump = joinLines(report.lines);
    expect(issueDump.find("Integridad liga | tercera division a | Futuro |") == string::npos,
           "Las entradas Display|Folder de teams.txt no deben marcar carpetas activas como huerfanas.");
    expect(issueDump.find("Integridad liga | tercera division b | Audax Paipote |") == string::npos,
           "Las entradas con alias de carpeta en teams.txt deben reconocerse en la auditoria.");
    expect(issueDump.find("Integridad liga | tercera division b | Rep?blica Ind. de Hualqui |") == string::npos,
           "Los nombres de carpeta configurados explicitamente en teams.txt deben evitar falsos positivos de integridad.");
}

void testSaveCareerCreatesBackup() {
    const string savePath = processScopedTestPath("saves/test_backup_save.txt");
    Career career;
    career.currentSeason = 3;
    career.currentWeek = 4;
    career.allTeams.push_back(makeTeam("Backup FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Backup Rival", "primera division", 66, 2, 2, "Balanced", "Equilibrado", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Backup FC");
    expect(career.myTeam != nullptr, "La prueba de backup necesita club usuario.");
    career.saveFile = savePath;

    expect(career.saveCareer(), "El primer guardado debe completarse.");
    career.currentWeek = 5;
    expect(career.saveCareer(), "El segundo guardado debe completarse.");
    expect(pathExists(savePath + ".bak"),
           "El segundo guardado debe dejar un backup del save previo.");

    std::remove(savePath.c_str());
    std::remove((savePath + ".bak").c_str());
}

void testSaveCareerCreatesNestedDirectory() {
    const string saveDir = processScopedTestPath("saves/runtime_nested");
    const string savePath = saveDir + "/test_nested_save.txt";
    std::remove(savePath.c_str());
    std::remove((savePath + ".bak").c_str());

    Career career;
    career.currentSeason = 4;
    career.currentWeek = 9;
    career.allTeams.push_back(makeTeam("Nested FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Nested Rival", "primera division", 66, 2, 2, "Balanced", "Equilibrado", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Nested FC");
    expect(career.myTeam != nullptr, "La prueba de directorio anidado necesita club usuario.");
    career.saveFile = savePath;

    expect(career.saveCareer(), "Guardar carrera debe crear tambien la carpeta del save si no existe.");
    expect(pathExists(savePath), "El save anidado debe quedar creado en disco.");

    Career loaded;
    loaded.saveFile = savePath;
    expect(loaded.loadCareer(), "La carga debe funcionar tambien desde el nuevo directorio anidado.");
    expect(loaded.myTeam != nullptr && loaded.myTeam->name == "Nested FC",
           "El save anidado debe restaurar correctamente el club controlado.");

    std::remove(savePath.c_str());
    std::remove((savePath + ".bak").c_str());
}

void testSaveCareerOverwriteDoesNotKeepTrailingBlocks() {
    const string savePath = processScopedTestPath("saves/test_overwrite_structure_save.txt");
    const string resolvedSavePath = resolveProjectPath(savePath);
    std::remove(resolvedSavePath.c_str());
    std::remove((resolvedSavePath + ".bak").c_str());

    Career large;
    large.currentSeason = 2;
    large.currentWeek = 6;
    large.allTeams.push_back(makeTeam("Grande FC", "primera division", 72, 3, 3, "Balanced", "Equilibrado", 900000));
    large.allTeams.push_back(makeTeam("Grande Rival", "primera division", 69, 3, 3, "Balanced", "Equilibrado", 820000));
    large.allTeams.push_back(makeTeam("Grande Tercero", "primera division", 67, 2, 2, "Balanced", "Equilibrado", 760000));
    large.setActiveDivision("primera division");
    large.myTeam = large.findTeamByName("Grande FC");
    expect(large.myTeam != nullptr, "La prueba de overwrite necesita club usuario en el save grande.");
    large.saveFile = savePath;
    expect(large.saveCareer(), "El save grande debe escribirse correctamente.");

    Career compact;
    compact.currentSeason = 3;
    compact.currentWeek = 2;
    compact.allTeams.push_back(makeTeam("Compacto FC", "primera division", 68, 3, 3, "Balanced", "Equilibrado", 700000));
    compact.allTeams.push_back(makeTeam("Compacto Rival", "primera division", 66, 2, 2, "Balanced", "Equilibrado", 620000));
    compact.setActiveDivision("primera division");
    compact.myTeam = compact.findTeamByName("Compacto FC");
    expect(compact.myTeam != nullptr, "La prueba de overwrite necesita club usuario en el save compacto.");
    compact.saveFile = savePath;
    expect(compact.saveCareer(), "El save compacto debe sobrescribir el archivo previo.");

    vector<string> lines;
    expect(readTextFileLines(savePath, lines), "La prueba de overwrite necesita leer el save resultante.");

    int declaredTeams = -1;
    int declaredPlayers = 0;
    for (const string& line : lines) {
        if (line.rfind("TEAMS ", 0) == 0) declaredTeams = stoi(trim(line.substr(6)));
        if (line.rfind("PLAYERS ", 0) == 0) declaredPlayers += stoi(trim(line.substr(8)));
    }

    expect(declaredTeams == 2, "El save compacto debe declarar exactamente dos equipos.");
    expect(countLinesStartingWith(lines, "TEAM ") == declaredTeams,
           "El archivo final no debe conservar bloques TEAM sobrantes.");
    expect(countLinesStartingWith(lines, "ENDTEAM") == declaredTeams,
           "El archivo final no debe conservar cierres ENDTEAM sobrantes.");
    expect(countLinesStartingWith(lines, "PLAYER ") == declaredPlayers,
           "El archivo final no debe conservar lineas PLAYER sobrantes tras sobrescribir.");

    std::remove(resolvedSavePath.c_str());
    std::remove((resolvedSavePath + ".bak").c_str());
}

void testProjectPathsResolveFromNestedWorkingDirectory() {
    const string probeRoot = processScopedTestPath("saves/runtime_cwd_probe");
    const string probeDir = resolveProjectPath(joinPath(probeRoot, "nested"));
    expect(ensureDirectory(probeDir), "La prueba de cwd necesita crear un directorio de trabajo anidado.");

    const string probeSettingsPath = joinPath(probeRoot, "settings_probe.cfg");
    const string resolvedProbeSettingsPath = resolveProjectPath(probeSettingsPath);
    std::remove(resolvedProbeSettingsPath.c_str());

    {
        ScopedCurrentDirectory cwd(probeDir);
        expect(cwd.valid, "La prueba de rutas necesita cambiar el directorio actual.");

        vector<string> lines;
        expect(readTextFileLines("data/configs/league_registry.csv", lines) && lines.size() > 1,
               "La lectura de configs debe seguir funcionando fuera de la raiz del repo.");

        GameSettings saved;
        saved.volume = 25;
        saved.menuThemeId = "path-probe";
        expect(game_settings::saveToDisk(saved, probeSettingsPath),
               "Los settings deben poder guardarse con cwd anidado.");

        GameSettings loaded;
        loaded.volume = 70;
        loaded.menuThemeId = "default";
        expect(game_settings::loadFromDisk(loaded, probeSettingsPath),
               "Los settings deben poder cargarse con cwd anidado.");
        expect(loaded.volume == 25 && loaded.menuThemeId == "path-probe",
               "La persistencia de settings debe resolverse contra la raiz del proyecto.");

        Career career;
        career.initializeLeague(true);
        expect(!career.divisions.empty(),
               "La carga de divisiones no debe depender del directorio de trabajo.");
    }

    std::remove(resolvedProbeSettingsPath.c_str());
}

void testCareerRuntimeScopeRestoresContext() {
    g_runtimeMessagesA.clear();
    g_runtimeMessagesB.clear();

    const CareerRuntimeContext previous = currentCareerRuntimeContext();
    setUiMessageCallback(collectRuntimeMessageA);
    setIdleCallback(idleRuntimeProbe);
    setWeekSimulationPresentation(WeekSimulationPresentation::Compact);

    CareerRuntimeContext scoped = currentCareerRuntimeContext();
    scoped.uiMessage = collectRuntimeMessageB;
    scoped.idle = nullptr;
    scoped.presentation = WeekSimulationPresentation::Detailed;

    {
        ScopedCareerRuntimeContext runtimeScope(scoped);
        expect(uiMessageCallback() == collectRuntimeMessageB,
               "El contexto de runtime activo debe exponer el callback de UI scoped.");
        expect(idleCallback() == nullptr,
               "El scope debe poder anular callbacks temporales sin tocar el default externo.");
        expect(weekSimulationPresentation() == WeekSimulationPresentation::Detailed,
               "La presentacion scoped debe sobreescribir la del runtime base.");
        emitUiMessage("scope");
    }

    expect(uiMessageCallback() == collectRuntimeMessageA,
           "Al salir del scope debe restaurarse el callback de UI previo.");
    expect(idleCallback() == idleRuntimeProbe,
           "Al salir del scope debe restaurarse el idle callback previo.");
    expect(weekSimulationPresentation() == WeekSimulationPresentation::Compact,
           "La presentacion previa debe restaurarse tras destruir el scope.");

    emitUiMessage("default");
    expect(g_runtimeMessagesB.size() == 1 && g_runtimeMessagesB[0] == "scope",
           "El mensaje scoped debe ir solo al colector temporal.");
    expect(g_runtimeMessagesA.size() == 1 && g_runtimeMessagesA[0] == "default",
           "El callback base debe volver a recibir mensajes tras cerrar el scope.");

    setUiMessageCallback(previous.uiMessage);
    setIdleCallback(previous.idle);
    setIncomingOfferDecisionCallback(previous.incomingOfferDecision);
    setContractRenewalDecisionCallback(previous.contractRenewalDecision);
    setManagerJobSelectionCallback(previous.managerJobSelection);
    setWeekSimulationPresentation(previous.presentation);
}

void testSeasonServiceForwardsRuntimeMessages() {
    g_runtimeMessagesA.clear();

    Career career;
    career.allTeams.push_back(makeTeam("Forward Club", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Forward Rival", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Forward Club");
    career.managerName = "Manager Forward";
    career.currentSeason = 1;
    career.currentWeek = 1;
    expect(career.myTeam != nullptr, "La prueba de forwarding necesita club usuario.");
    expect(!career.schedule.empty(), "La prueba de forwarding necesita calendario.");

    CareerRuntimeContext runtime = currentCareerRuntimeContext();
    runtime.uiMessage = collectRuntimeMessageA;
    runtime.presentation = WeekSimulationPresentation::Compact;

    ServiceResult result;
    {
        ScopedCareerRuntimeContext runtimeScope(runtime);
        result = simulateCareerWeekService(career, idleRuntimeProbe);
    }

    const string resultMessages = joinLines(result.messages);
    const string forwardedMessages = joinLines(g_runtimeMessagesA);
    expect(result.ok, "La simulacion semanal de prueba debe completarse.");
    expect(resultMessages.find("Simulando semana") != string::npos,
           "SeasonService debe conservar los mensajes emitidos por la simulacion.");
    expect(forwardedMessages.find("Simulando semana") != string::npos,
           "SeasonService debe reenviar los mensajes al callback runtime activo.");
}

void testPostWeekSimulationAddsActionableDigest() {
    Career career;
    career.allTeams.push_back(makeTeam("Digest FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 700000));
    career.allTeams.push_back(makeTeam("Digest Rival", "primera division", 68, 4, 3, "Pressing", "Presion alta", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Digest FC");
    career.managerName = "Manager Digest";
    career.currentSeason = 1;
    career.currentWeek = 1;
    career.boardConfidence = 36;
    career.boardMonthlyObjective = "Entrar al top 3";
    career.boardMonthlyTarget = 3;
    career.boardMonthlyProgress = 5;
    expect(career.myTeam != nullptr, "El digest post-semana necesita club usuario.");
    expect(!career.schedule.empty(), "El digest post-semana necesita calendario.");

    CareerRuntimeContext runtime = currentCareerRuntimeContext();
    runtime.presentation = WeekSimulationPresentation::Compact;

    ServiceResult result;
    {
        ScopedCareerRuntimeContext runtimeScope(runtime);
        result = simulateCareerWeekService(career, idleRuntimeProbe);
    }

    const string messages = joinLines(result.messages);
    const string inbox = joinLines(career.managerInbox);
    expect(result.ok, "La simulacion semanal con digest debe completarse.");
    expect(messages.find("Post-semana: Cockpit semanal:") != string::npos,
           "La simulacion debe cerrar con un resumen post-semana accionable.");
    expect(messages.find("Decision sugerida:") != string::npos,
           "El digest debe incluir una decision semanal sugerida.");
    expect(messages.find("Siguiente accion:") != string::npos,
           "El digest debe indicar el siguiente paso dentro del juego.");
    expect(inbox.find("[Centro semanal]") != string::npos,
           "El digest debe quedar tambien en el centro del manager.");
    expect(consumeLatestWeeklyDigestService(career),
           "El digest post-semana debe poder consumirse al aplicar la decision sugerida.");
    expect(joinLines(career.managerInbox).find("[Centro semanal]") == string::npos,
           "El digest post-semana consumido no debe seguir apareciendo como pendiente.");
}

void testHumanManagerProfilesPersistAcrossSaveSerialization() {
    Career career;
    career.allTeams.push_back(makeTeam("Control Uno", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 800000));
    career.allTeams.push_back(makeTeam("Control Dos", "primera division", 68, 2, 3, "Balanced", "Equilibrado", 720000));
    career.allTeams.push_back(makeTeam("CPU Club", "primera division", 66, 2, 2, "Defensive", "Bloque bajo", 640000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Control Uno");
    expect(career.myTeam != nullptr, "La prueba multijugador necesita un club inicial.");

    career.managerName = "Manager Uno";
    career.managerReputation = 67;
    career.clearHumanManagers();
    expect(career.addHumanManager("Manager Uno", "Control Uno", 67, true),
           "Debe poder registrarse el primer manager humano.");
    expect(career.addHumanManager("Manager Dos", "Control Dos", 59, false),
           "Debe poder registrarse un segundo manager humano sin desplazar al primero.");
    expect(career.humanManagerCount() == 2,
           "El estado de carrera debe conservar los dos perfiles humanos registrados.");
    expect(career.switchActiveHumanManager(1),
           "Debe poder activarse el segundo manager humano.");
    expect(career.myTeam != nullptr && career.myTeam->name == "Control Dos" &&
               career.managerName == "Manager Dos",
           "Cambiar de manager activo debe sincronizar el club controlado y la vista legacy.");

    stringstream saved;
    expect(save_serialization::serializeCareer(saved, career),
           "La serializacion debe aceptar carreras con multiples managers humanos.");

    Career loaded;
    stringstream input(saved.str());
    expect(save_serialization::deserializeCareer(input, loaded),
           "La deserializacion debe restaurar carreras con multiples managers humanos.");
    expect(loaded.humanManagerCount() == 2,
           "Los perfiles humanos deben persistir a traves del save.");
    expect(loaded.switchActiveHumanManager(0),
           "Tras cargar, debe poder volver a activarse el primer manager.");
    expect(loaded.myTeam != nullptr && loaded.myTeam->name == "Control Uno" &&
               loaded.managerName == "Manager Uno",
           "La carga debe restaurar el primer manager y su club.");
    expect(loaded.switchActiveHumanManager(1),
           "Tras cargar, debe poder activarse el segundo manager.");
    expect(loaded.myTeam != nullptr && loaded.myTeam->name == "Control Dos" &&
               loaded.managerName == "Manager Dos",
           "La carga debe restaurar correctamente el segundo manager humano.");
    expect(loaded.findTeamByName("Control Uno") != nullptr &&
               loaded.isHumanControlledTeam(*loaded.findTeamByName("Control Uno")),
           "La carrera cargada debe reconocer clubes humanos aunque no sean el slot activo.");
}

void testWeeklyDashboardReportHighlightsHumanManagersAndAgenda() {
    Career career;
    career.allTeams.push_back(makeTeam("Dashboard FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 820000));
    career.allTeams.push_back(makeTeam("Dashboard B", "primera division", 67, 2, 2, "Defensive", "Bloque bajo", 690000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Dashboard FC");
    expect(career.myTeam != nullptr, "La prueba de dashboard necesita un club controlado.");

    career.managerName = "Manager Principal";
    career.managerReputation = 64;
    career.clearHumanManagers();
    expect(career.addHumanManager("Manager Principal", "Dashboard FC", 64, true),
           "El dashboard necesita un manager principal.");
    expect(career.addHumanManager("Manager Secundario", "Dashboard B", 58, false),
           "El dashboard debe reflejar tambien managers humanos secundarios.");

    career.currentSeason = 2;
    career.currentWeek = 6;
    career.boardConfidence = 48;
    career.boardMonthlyObjective = "Entrar al top 3";
    career.boardMonthlyTarget = 3;
    career.boardMonthlyProgress = 2;
    career.managerInbox.push_back("[Directiva] Necesitas una reaccion competitiva.");
    career.scoutInbox.push_back("Seguimiento activo sobre un volante mixto.");
    career.myTeam->players[0].injured = true;

    const CareerReport report = buildWeeklyDashboardReport(career);
    const vector<string> lines = formatCareerReportLines(report);
    const string dump = joinLines(lines);

    expect(report.title == "Dashboard Semanal",
           "El nuevo dashboard semanal debe exponerse como reporte estructurado.");
    expect(dump.find("Managers humanos: 2 | activo Manager Principal") != string::npos,
           "El dashboard debe informar cuando la carrera tiene mas de un manager humano.");
    expect(dump.find("Cockpit: Cockpit semanal:") != string::npos,
           "El dashboard debe exponer un headline de cockpit semanal reutilizable por UI.");
    expect(dump.find("Cockpit semanal:") != string::npos,
           "El dashboard debe incluir prioridades semanales agrupadas.");
    expect(dump.find("KPIs accionables:") != string::npos,
           "El dashboard debe resumir KPIs accionables para la semana.");
    expect(dump.find("Acciones sugeridas:") != string::npos,
           "El dashboard debe incluir recomendaciones accionables.");
    expect(dump.find("Centro del manager:") != string::npos,
           "El dashboard debe integrar el centro del manager en la misma vista.");
    expect(dump.find("Ayuda contextual:") != string::npos,
           "El dashboard debe incluir guias contextuales para la siguiente decision.");
}

void testWeeklyFocusSnapshotPrioritizesUrgentClubState() {
    Career career;
    career.allTeams.push_back(makeTeam("Focus FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 90000));
    career.allTeams.push_back(makeTeam("Rival Focus", "primera division", 71, 3, 3, "Pressing", "Presion alta", 620000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Focus FC");
    expect(career.myTeam != nullptr, "La prueba de cockpit semanal necesita un club controlado.");

    career.currentSeason = 1;
    career.currentWeek = 2;
    career.boardConfidence = 34;
    career.boardMonthlyObjective = "Entrar al top 3";
    career.boardMonthlyTarget = 3;
    career.boardMonthlyProgress = 5;
    career.boardYouthTarget = 2;
    career.schedule = {{{0, 1}}};

    career.myTeam->debt = 550000;
    career.myTeam->sponsorWeekly = 25000;
    career.myTeam->budget = 70000;
    career.myTeam->morale = 44;
    career.myTeam->players[0].injured = true;
    career.myTeam->players[1].contractWeeks = 6;
    career.myTeam->players[2].fatigueLoad = 72;
    career.myTeam->players[2].fitness = 58;
    career.myTeam->players[3].fatigueLoad = 68;
    career.myTeam->players[3].fitness = 59;
    career.myTeam->players[4].happiness = 35;
    career.myTeam->players[5].age = 19;
    career.myTeam->players[5].potential = career.myTeam->players[5].skill + 10;

    const auto snapshot = weekly_focus_service::buildWeeklyFocusSnapshot(career, 4, 5, 4);
    const string priorities = joinLines(snapshot.priorityLines);
    const string tips = joinLines(snapshot.tutorialLines);

    expect(snapshot.headline.find("Cockpit semanal:") != string::npos,
           "El servicio de cockpit debe producir un headline reusable por CLI y GUI.");
    expect(priorities.find("Directiva |") != string::npos,
           "Cuando la confianza cae, la directiva debe aparecer como foco prioritario.");
    expect(priorities.find("Fisico |") != string::npos || priorities.find("Vestuario |") != string::npos,
           "El cockpit debe detectar urgencias fisicas o de clima interno.");
    expect(priorities.find("Caja |") != string::npos,
           "La caja comprometida debe emerger como prioridad semanal.");
    expect(tips.find("scouting activo") != string::npos || tips.find("mercado") != string::npos,
           "La ayuda contextual debe ensenar el siguiente paso, no solo describir el estado.");
}

void testWeeklyDecisionServiceAppliesActionableConsequences() {
    Career career;
    career.allTeams.push_back(makeTeam("Decision FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 900000));
    career.allTeams.push_back(makeTeam("Decision Rival", "primera division", 68, 4, 3, "Pressing", "Contra-presion", 700000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Decision FC");
    expect(career.myTeam != nullptr, "La decision semanal necesita un club controlado.");

    career.currentWeek = 3;
    career.schedule = {{{0, 1}}};
    career.managerStress = initializeManagerStress();
    career.managerStress.stressLevel = 84;
    career.managerStress.energy = 24;
    career.infrastructure.levels.trainingGround = 3;
    career.infrastructure.levels.youthAcademy = 3;
    career.infrastructure.levels.medical = 4;
    career.infrastructure.levels.stadium = 3;
    career.infrastructure.levels.facilities = 3;
    career.myTeam->players[0].fatigueLoad = 75;
    career.myTeam->players[0].fitness = 52;
    career.myTeam->players[1].age = 19;
    career.myTeam->players[1].potential = career.myTeam->players[1].skill + 10;

    const vector<string> options = buildWeeklyDecisionOptions(career);
    expect(!options.empty() && options.front().find("Auto") != string::npos,
           "El centro semanal debe exponer una recomendacion automatica.");

    const int stressBefore = career.managerStress.stressLevel;
    ServiceResult rest = applyWeeklyDecisionService(career, WeeklyDecision::ManagerRest);
    expect(rest.ok, "El descanso del manager debe aplicarse como decision semanal.");
    expect(career.managerStress.stressLevel < stressBefore,
           "El descanso del manager debe reducir el estres.");
    expect(career.myTeam->trainingFocus == "Recuperacion",
           "El descanso del manager debe dejar un microciclo conservador.");

    const int fatigueBefore = career.myTeam->players[0].fatigueLoad;
    ServiceResult recovery = applyWeeklyDecisionService(career, WeeklyDecision::Recovery);
    expect(recovery.ok, "La recuperacion debe aplicarse como decision semanal.");
    expect(career.myTeam->players[0].fatigueLoad < fatigueBefore,
           "La decision de recuperacion debe bajar carga de fatiga.");

    ServiceResult youth = applyWeeklyDecisionService(career, WeeklyDecision::YouthPathway);
    expect(youth.ok, "El impulso juvenil debe aplicarse como decision semanal.");
    expect(!career.myTeam->players[1].developmentPlan.empty(),
           "El impulso juvenil debe asignar un plan individual al proyecto joven.");
}

void testDebtAndInfrastructureConsequencesAreEnforced() {
    Career career;
    career.allTeams.push_back(makeTeam("Debt FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 900000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Debt FC");
    expect(career.myTeam != nullptr, "La prueba de deuda necesita un club controlado.");

    career.myTeam->debt = 9000000;
    career.myTeam->budget = 700000;
    career.myTeam->sponsorWeekly = 15000;
    career.debtStatus = calculateDebtStatus(career.myTeam->budget, career.myTeam->debt, career.myTeam->sponsorWeekly);
    applyFinancialSanctions(career.debtStatus);

    ServiceResult blockedUpgrade = upgradeClubService(career, ClubUpgrade::Stadium);
    expect(!blockedUpgrade.ok,
           "La deuda grave debe bloquear inversiones de infraestructura.");

    ServiceResult control = applyWeeklyDecisionService(career, WeeklyDecision::FinancialControl);
    expect(control.ok, "El control financiero debe poder aplicarse aun con deuda alta.");
    expect(career.myTeam->transferPolicy == "Vender antes de comprar",
           "El control financiero debe cambiar la politica de mercado.");
}

void testMatchCenterRecommendationsFeedWeeklyDecisionLoop() {
    Career career;
    career.lastMatchAnalysis = "Liga: derrota de prueba.";
    career.lastMatchCenter.opponentName = "Rival Claro";
    career.lastMatchCenter.competitionLabel = "Liga";
    career.lastMatchCenter.venueLabel = "Visita";
    career.lastMatchCenter.myGoals = 0;
    career.lastMatchCenter.oppGoals = 2;
    career.lastMatchCenter.myShots = 5;
    career.lastMatchCenter.oppShots = 14;
    career.lastMatchCenter.myShotsOnTarget = 1;
    career.lastMatchCenter.oppShotsOnTarget = 6;
    career.lastMatchCenter.myExpectedGoalsTenths = 6;
    career.lastMatchCenter.oppExpectedGoalsTenths = 18;
    career.lastMatchCenter.myPossession = 42;
    career.lastMatchCenter.oppPossession = 58;

    const MatchCenterView view = match_center_service::buildLastMatchCenter(career, 2, 2);
    const string recommendations = joinLines(view.recommendationLines);
    expect(recommendations.find("Decision semanal sugerida") != string::npos,
           "El match center debe cerrar el feedback post-partido con una decision semanal sugerida.");
}

void testAutoWeeklyDecisionUsesMatchCenterContext() {
    Career career;
    career.allTeams.push_back(makeTeam("Auto Match FC", "primera division", 70, 3, 3, "Balanced", "Equilibrado", 900000));
    career.allTeams.push_back(makeTeam("Auto Match Rival", "primera division", 68, 4, 3, "Pressing", "Presion alta", 700000));
    career.setActiveDivision("primera division");
    career.myTeam = career.findTeamByName("Auto Match FC");
    expect(career.myTeam != nullptr, "La decision automatica necesita un club controlado.");
    career.managerStress = initializeManagerStress();
    career.managerStress.stressLevel = 20;
    career.managerStress.energy = 82;
    career.schedule = {{{0, 1}}};

    career.lastMatchCenter.opponentName = "Auto Match Rival";
    career.lastMatchCenter.myGoals = 0;
    career.lastMatchCenter.oppGoals = 1;
    career.lastMatchCenter.myShots = 5;
    career.lastMatchCenter.oppShots = 9;
    career.lastMatchCenter.myShotsOnTarget = 1;
    career.lastMatchCenter.oppShotsOnTarget = 3;
    career.lastMatchCenter.myExpectedGoalsTenths = 7;
    career.lastMatchCenter.oppExpectedGoalsTenths = 11;

    const vector<string> attackOptions = buildWeeklyDecisionOptions(career);
    expect(!attackOptions.empty() && attackOptions.front().find("Entrenar fuerte") != string::npos,
           "Si el ultimo partido tuvo ataque plano, el staff automatico debe pedir entrenamiento fuerte.");
    ServiceResult attackPlan = applyWeeklyDecisionService(career, WeeklyDecision::Auto);
    expect(attackPlan.ok, "La decision automatica basada en match center debe aplicarse.");
    expect(career.myTeam->trainingFocus == "Ataque",
           "Un partido con poca produccion debe orientar el microciclo hacia Ataque.");

    career.lastMatchCenter.myGoals = 1;
    career.lastMatchCenter.oppGoals = 1;
    career.lastMatchCenter.myShots = 11;
    career.lastMatchCenter.oppShots = 18;
    career.lastMatchCenter.myShotsOnTarget = 4;
    career.lastMatchCenter.oppShotsOnTarget = 8;
    career.lastMatchCenter.myExpectedGoalsTenths = 15;
    career.lastMatchCenter.oppExpectedGoalsTenths = 19;
    career.managerStress.energy = 82;

    ServiceResult defensivePlan = applyWeeklyDecisionService(career, WeeklyDecision::Auto);
    expect(defensivePlan.ok, "La decision automatica defensiva debe aplicarse.");
    expect(career.myTeam->trainingFocus == "Defensa",
           "Si el rival genero demasiado peligro, el auto-staff debe orientar el trabajo hacia Defensa.");
}

void testLocalizationSupportsMultipleLanguages() {
    Localization& loc = Localization::getInstance();
    loc.setLanguage(Localization::Language::Spanish);
    expect(loc.getText("menu_continue") == "Continuar", "Debe retornar texto en español.");
    loc.setLanguage(Localization::Language::English);
    expect(loc.getText("menu_continue") == "Continue", "Debe retornar texto en inglés.");
    loc.setLanguage(Localization::Language::Portuguese);
    expect(loc.getText("menu_continue") == "Continuar", "Debe retornar texto en portugués.");
    loc.setLanguage(Localization::Language::French);
    expect(loc.getText("menu_continue") == "Continuer", "Debe retornar texto en francés.");
    expect(loc.getText("unknown_key") == "unknown_key", "Debe retornar la clave si no existe traducción.");
}

}  // namespace

int main() {
    const vector<pair<string, void (*)()>> tests = {
        {"validation_suite", testValidationSuiteReflectsRosterAudit},
        {"validation_suite_concurrency", testValidationSuiteSupportsConcurrentRuns},
        {"loaded_player_profiles", testLoadedPlayersHaveBoundedProfileMetrics},
        {"match_engine_structure", testMatchSimulationProducesStructuredPhases},
        {"tactical_fatigue", testHighPressRaisesPhaseFatigue},
        {"low_block_chance_quality", testLowBlockSuppressesChanceQuality},
        {"competition_rules_csv", testCompetitionRulesLoadFromCsv},
        {"runtime_load_validation", testRuntimeValidationFlagsBrokenLoadedSquad},
        {"startup_data_validation", testStartupValidationSummaryExposesExternalAudit},
        {"competition_group_table", testCompetitionGroupTableScopesActiveGroup},
        {"team_id_repository", testTeamRepositoryResolvesStableIds},
        {"career_service_wrapper", testCareerServiceWrapperProducesGameplayOutputs},
        {"game_settings_cycle", testGameSettingsCycleAndDifficultyImpact},
        {"game_settings_persistence", testGameSettingsPersistenceAndFrontendScope},
        {"transfer_affordability", testTransferEvaluationPenalizesUnaffordableDeals},
        {"transfer_shortlist", testTransferShortlistRewardsScoutedNeed},
        {"squad_sale_candidates", testSquadPlannerFlagsUnusedSeniorSaleCandidates},
        {"transfer_negotiation", testTransferNegotiationBuildsStructuredDeal},
        {"season_transition", testSeasonTransitionAdvancesCareerWithoutUiDependencies},
        {"season_transition_standings", testSeasonTransitionPromotesByStandingsNotSquadValue},
        {"season_service", testSeasonServiceReturnsStructuredWeekResult},
        {"opponent_report", testOpponentReportExplainsNextFixture},
        {"match_analysis_store", testMatchAnalysisStoreProducesStructuredCareerData},
        {"match_center_service", testMatchCenterServiceBuildsStructuredView},
        {"dressing_room_service", testDressingRoomServiceFlagsPromiseAndFatigueRisk},
        {"dressing_room_tension", testDressingRoomSnapshotTracksSocialTension},
        {"scouting_confidence", testScoutingConfidenceReflectsStaffQuality},
        {"world_state_seed", testWorldStateSeedsPromisesAndRecords},
        {"world_state_promises", testWeeklyWorldStateResolvesMinutesPromise},
        {"season_promise_carryover", testSeasonTransitionResolvesCarryoverPromises},
        {"training_microcycle", testWeeklyTrainingScheduleAdaptsToCongestion},
        {"team_meeting", testTeamMeetingImprovesMorale},
        {"player_instruction_service", testPlayerInstructionServiceCyclesInstruction},
        {"inbox_medical_decision", testInboxDecisionPrioritizesRecovery},
        {"inbox_weekly_digest_decision", testInboxDecisionConsumesWeeklyDigest},
        {"staff_review", testStaffReviewImprovesWeakestArea},
        {"scouting_assignments", testScoutingAssignmentShowsInReport},
        {"role_duty_snapshot", testRoleDutyShapesMatchSnapshot},
        {"club_world_metadata", testTeamIdentitySeedsWorldMetadata},
        {"club_prestige_divisions", testClubPrestigeUsesCanonicalTerceraDivisionIds},
        {"manager_inbox", testManagerInboxTracksNewsAndScouting},
        {"configured_world_data", testConfiguredWorldDataLoads},
        {"localization_support", testLocalizationSupportsMultipleLanguages},
        {"league_registry_data", testLeagueRegistryLoadsConfiguredData},
        {"analytics_inbox_blocks", testAnalyticsAndInboxServicesProduceUsefulBlocks},
        {"manager_hub_digest", testManagerHubDigestCombinesStaffAndAgenda},
        {"manager_advice", testManagerAdviceHighlightsUrgentActions},
        {"suggested_board_objective", testSuggestedBoardObjectiveReasonMatchesSituation},
        {"career_storylines_fixture_context", testCareerStorylinesAddChileanFixtureContext},
        {"transfer_briefing", testTransferBriefingBuildsActionableMarketView},
        {"scouting_briefing_uncertainty", testScoutingBriefingMasksUncertainAttributes},
        {"transfer_window_rules", testTransferWindowBlocksImmediateDeals},
        {"transfer_buy_and_loans", testTransferServicesMoveBoughtAndLoanedPlayers},
        {"market_pulse_window", testMarketPulseReflectsClosedWindow},
        {"career_runtime_scope", testCareerRuntimeScopeRestoresContext},
        {"season_service_runtime_forwarding", testSeasonServiceForwardsRuntimeMessages},
        {"post_week_action_digest", testPostWeekSimulationAddsActionableDigest},
        {"human_manager_persistence", testHumanManagerProfilesPersistAcrossSaveSerialization},
        {"weekly_dashboard_report", testWeeklyDashboardReportHighlightsHumanManagersAndAgenda},
        {"weekly_focus_snapshot", testWeeklyFocusSnapshotPrioritizesUrgentClubState},
        {"weekly_decision_service", testWeeklyDecisionServiceAppliesActionableConsequences},
        {"debt_infrastructure_consequences", testDebtAndInfrastructureConsequencesAreEnforced},
        {"match_center_weekly_decision_loop", testMatchCenterRecommendationsFeedWeeklyDecisionLoop},
        {"auto_weekly_decision_match_context", testAutoWeeklyDecisionUsesMatchCenterContext},
        {"personality_cpu_tactics", testTeamPersonalityProfileShapesCpuTactics},
        {"personality_transfer_eval", testTransferEvaluationReflectsClubPersonality},
        {"opponent_personality_report", testOpponentReportIncludesCompetitivePersonality},
        {"next_opponent_plan", testNextOpponentPlanBuildsActionableLines},
        {"match_preparation_plan_service", testMatchPreparationPlanServiceAppliesOpponentInstruction},
        {"late_match_urgency", testLateDeficitRaisesUrgencyInMatchPhase},
        {"instruction_chance_profiles", testMatchInstructionsShapeChanceProfiles},
        {"injury_replacement", testMatchInjuryTriggersRealReplacement},
        {"transfer_medical_risk", testTransferEvaluationPenalizesMedicalRisk},
        {"monthly_development", testMonthlyDevelopmentCycleImprovesStableProspects},
        {"match_center_adjustments", testMatchCenterAddsRecommendedAdjustments},
        {"ignored_archived_folders", testIgnoredArchivedFoldersConfigSuppressesKnownWarnings},
        {"teams_txt_folder_aliases", testTeamsTxtFolderAliasesDoNotTriggerOrphanWarnings},
        {"negotiation_agent_costs", testNegotiationTracksAgentCosts},
        {"save_backup", testSaveCareerCreatesBackup},
        {"save_overwrite_structure", testSaveCareerOverwriteDoesNotKeepTrailingBlocks},
        {"save_nested_directory", testSaveCareerCreatesNestedDirectory},
        {"project_root_paths", testProjectPathsResolveFromNestedWorkingDirectory},
        {"simulate_match_state", testSimulateMatchAppliesPostProcessState},
        {"save_load_roundtrip", testSaveLoadRoundTripPreservesCareerState},
        {"save_schema_integrity", testSaveSchemaAndIntegrityRejectCorruptPayload},
        {"external_json_mod_loader", testExternalJsonLoaderAddsModLeagueAndAdvancedPlayers},
        {"legacy_division_ids", testLegacyDivisionIdentifiersCanonicalizeOnLoad},
        {"loader_fallback", testLoadTeamFromDirectoryFallsBackAndResolvesRawPositions},
#ifdef _WIN32
        {"management_view_filters", testManagementViewFiltersChangeVisibleContent},
        {"calendar_match_preview", testCalendarModelShowsMatchPreparationPreview},
        {"board_pressure_panel", testBoardModelShowsInstitutionalPressurePanel},
        {"finance_fair_play", testFinancesModelShowsFairPlayPressure},
        {"player_profile_staff_report", testPlayerProfileShowsFmStyleStaffReport},
        {"transfer_feed_focus", testTransferFeedStaysMarketFocusedAcrossFilters},
        {"transfer_selection_source", testTransferTargetsUseSelectablePrimaryList},
        {"dashboard_action_cue", testDashboardActionCueHighlightsHardRisks},
#endif
    };

    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            cout << "[PASS] " << test.first << '\n';
        } catch (const exception& ex) {
            ++failures;
            cerr << "[FAIL] " << test.first << ": " << ex.what() << '\n';
        }
    }

    if (failures == 0) {
        cout << "All tests passed." << endl;
        return 0;
    }

    cerr << failures << " test(s) failed." << endl;
    return 1;
}
