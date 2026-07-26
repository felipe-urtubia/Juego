#include "engine/game_controller.h"

#include "career/career_runtime.h"
#include "engine/front_menu.h"
#include "gui.h"
#include "io/io.h"
#include "ui.h"
#include "utils.h"
#include "validators.h"

#include <clocale>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace {

size_t warningSignature(const vector<string>& warnings) {
    size_t seed = warnings.size();
    hash<string> hasher;
    for (const auto& warning : warnings) {
        seed ^= hasher(warning) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

bool startsWith(const string& value, const string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool loadTeamFromInputPath(const string& filename, Team& team) {
    if (isDirectory(filename)) {
        const string csv = joinPath(filename, "players.csv");
        if (pathExists(csv) && loadTeamFromCsv(csv, team)) return true;

        const string txt = joinPath(filename, "players.txt");
        if (pathExists(txt) && loadTeamFromPlayersTxt(txt, team)) return true;
        if (pathExists(txt) && loadTeamFromLegacyTxt(txt, team)) return true;

        const string json = joinPath(filename, "players.json");
        if (pathExists(json) && loadTeamFromJson(json, team)) return true;
        return false;
    }

    const string ext = toLower(pathExtension(filename));
    if (ext == ".csv") return loadTeamFromCsv(filename, team);
    if (ext == ".json") return loadTeamFromJson(filename, team);
    if (ext == ".txt") {
        if (loadTeamFromPlayersTxt(filename, team)) return true;
        return loadTeamFromLegacyTxt(filename, team);
    }
    return false;
}

void configureConsoleEncoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    setlocale(LC_ALL, ".UTF-8");
}

}  // namespace

void GameController::loadPersistedSettings() {
    game_settings::loadFromDisk(settings_);
}

void GameController::savePersistedSettings() const {
    game_settings::saveToDisk(settings_);
}

void GameController::showLoadWarnings() const {
    const auto& warnings = engine_.career().loadWarnings;
    if (warnings.empty()) return;

    const size_t signature = warningSignature(warnings);
    if (signature == lastLoadWarningSignature_) return;
    lastLoadWarningSignature_ = signature;

    string summary;
    string reportLine;
    string okLine;
    vector<string> highlighted;
    size_t warningLines = 0;
    for (const auto& warning : warnings) {
        if (summary.empty() && startsWith(warning, "Errores:")) {
            summary = warning;
            continue;
        }
        if (startsWith(warning, "[WARNING]")) {
            warningLines++;
            if (highlighted.size() < 3) highlighted.push_back(warning);
            continue;
        }
        if (reportLine.empty() && warning.find("Reporte completo disponible") != string::npos) {
            reportLine = warning;
            continue;
        }
        if (okLine.empty() && startsWith(warning, "[OK]")) {
            okLine = warning;
        }
    }

    if (!summary.empty()) {
        cout << "[AVISO] " << summary << endl;
    }
    if (!highlighted.empty()) {
        cout << "[AVISO] Incidencias destacadas:" << endl;
        for (const auto& line : highlighted) {
            cout << "[AVISO] - " << line << endl;
        }
        if (warningLines > highlighted.size()) {
            cout << "[AVISO] ... y " << (warningLines - highlighted.size())
                 << " advertencia(s) adicional(es)." << endl;
        }
    }
    if (!reportLine.empty()) {
        cout << "[AVISO] " << reportLine << endl;
    }
    if (!okLine.empty()) {
        cout << "[AVISO] " << okLine << endl;
    }
}

void GameController::pauseForContinue() const {
    cout << "\nPresiona Enter para continuar...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int GameController::run(int argc, char* argv[]) {
    loadPersistedSettings();
    if (argc > 1 && string(argv[1]) == "--validate") {
        const int result = runValidationSuite(true);
        savePersistedSettings();
        return result;
    }
#ifdef FM_CONSOLE_DEFAULT
    const int result = runConsoleApp();
    savePersistedSettings();
    return result;
#else
    if (argc > 1 && string(argv[1]) == "--cli") {
        const int result = runConsoleApp();
        savePersistedSettings();
        return result;
    }

#ifdef _WIN32
    const int result = runGuiApp(settings_);
    savePersistedSettings();
    return result;
#else
    const int result = runConsoleApp();
    savePersistedSettings();
    return result;
#endif
#endif
}

int GameController::runConsoleApp() {
    configureConsoleEncoding();
    engine_.initialize();
    MenuController menu(settings_, engine_.career());

    while (true) {
        const FrontMenuAction action = menu.runMainMenu();
        if (action == FrontMenuAction::Exit) {
            cout << "Saliendo del juego." << endl;
            break;
        }
        if (action == FrontMenuAction::ContinueCareer) {
            if (loadCareerForResume(false)) {
                runCareerLoop();
            } else {
                cout << "No hay una carrera lista para continuar." << endl;
                pauseForContinue();
            }
            continue;
        }
        if (action == FrontMenuAction::LoadCareer) {
            if (loadCareerForResume(true)) {
                runCareerLoop();
            } else {
                cout << "No se pudo cargar una carrera guardada." << endl;
                pauseForContinue();
            }
            continue;
        }
        if (action == FrontMenuAction::Settings) {
            menu.runSettingsMenu();
            savePersistedSettings();
            continue;
        }
        if (action == FrontMenuAction::Credits) {
            menu.runCreditsMenu();
            continue;
        }
        runConsoleGameHub();
    }

    return 0;
}

void GameController::runConsoleGameHub() {
    showLoadWarnings();
    while (true) {
        displayMainMenu();
        int mainChoice = readInt("Elige una opcion: ", 1, 5);
        if (mainChoice == 5) {
            cout << "Volviendo al menu principal." << endl;
            break;
        }

        if (mainChoice == 1) {
            runCareerMode();
        } else if (mainChoice == 2) {
            runQuickGame();
        } else if (mainChoice == 3) {
            playCupMode(engine_.career());
        } else if (mainChoice == 4) {
            int result = runValidationSuite(true);
            cout << (result == 0 ? "Validacion completada sin fallas." : "Validacion con fallas detectadas.") << endl;
        }
    }
}

bool GameController::loadCareerForResume(bool forceReloadFromDisk) {
    Career& career = engine_.career();
    if (career.myTeam && !forceReloadFromDisk) {
        return true;
    }

    ServiceResult loadResult = engine_.loadCareer();
    for (const auto& message : loadResult.messages) cout << message << endl;
    showLoadWarnings();
    return loadResult.ok && career.myTeam;
}

bool GameController::createNewCareerFromConsole() {
    Career& career = engine_.career();
    engine_.initialize(true);
    showLoadWarnings();
    if (career.divisions.empty()) {
        cout << "No se encontraron divisiones disponibles." << endl;
        return false;
    }

    cout << "\nSelecciona la division:" << endl;
    for (size_t i = 0; i < career.divisions.size(); ++i) {
        cout << i + 1 << ". " << career.divisions[static_cast<size_t>(i)].display << endl;
    }
    int divisionChoice = readInt("Elige una division: ", 1, static_cast<int>(career.divisions.size()));
    const string divisionId = career.divisions[static_cast<size_t>(divisionChoice - 1)].id;
    auto teams = career.getDivisionTeams(divisionId);
    if (teams.empty()) {
        cout << "No hay equipos para esta division." << endl;
        return false;
    }

    cout << "\nEquipos de " << divisionDisplay(divisionId) << ":" << endl;
    for (size_t i = 0; i < teams.size(); ++i) {
        cout << i + 1 << ". " << teams[static_cast<size_t>(i)]->name << endl;
    }
    int teamChoice = readInt("Elige un numero de equipo: ", 1, static_cast<int>(teams.size()));
    const string managerName = readLine("Nombre del manager (Enter para 'Manager'): ");
    ServiceResult startResult =
        engine_.startCareer(divisionId, teams[static_cast<size_t>(teamChoice - 1)]->name, managerName);
    if (startResult.ok) {
        game_settings::applyNewCareerDifficulty(career, settings_);
        startResult.messages.push_back("Configuracion aplicada: " + game_settings::settingsSummary(settings_) + ".");
    }
    for (const auto& message : startResult.messages) cout << message << endl;
    return startResult.ok && career.myTeam;
}

void GameController::runCareerMode() {
    Career& career = engine_.career();
    cout << "\n=== Modo Carrera ===" << endl;
    cout << "Configuracion activa: " << game_settings::settingsSummary(settings_) << endl;
    cout << "\nOpciones de Carrera:" << endl;
    cout << "1. Continuar Carrera" << endl;
    cout << "2. Nueva Carrera" << endl;
    cout << "3. Volver al Menu Principal" << endl;
    int careerStartChoice = readInt("Elige una opcion: ", 1, 3);

    bool careerStarted = false;
    if (careerStartChoice == 1) {
        careerStarted = loadCareerForResume(true);
    } else if (careerStartChoice == 2) {
        careerStarted = createNewCareerFromConsole();
    } else {
        return;
    }

    if (!careerStarted || !career.myTeam) return;
    runCareerLoop();
}

void GameController::runCareerLoop() {
    Career& career = engine_.career();
    int careerChoice = 0;
    do {
        cout << "\nTemporada " << career.currentSeason << ", Semana " << career.currentWeek << endl;
        displayCareerMenu();
        careerChoice = readInt("Elige una opcion: ", 1, 20);
        switch (careerChoice) {
            case 1: viewTeam(*career.myTeam); break;
            case 2: trainPlayer(*career.myTeam, career.currentSeason, career.currentWeek); break;
            case 3: changeTactics(*career.myTeam); break;
            case 4: {
                cout << "\nComo deseas disputar el partido de esta semana?" << endl;
                cout << "1. Simular semana normalmente" << endl;
                cout << "2. Ver mi partido en Match Center" << endl;
                cout << "3. Cancelar" << endl;

                const int simulationMode = readInt("Elige una opcion: ", 1, 3);
                if (simulationMode == 3) {
                    break;
                }

                const WeekSimulationPresentation previousPresentation =
                    weekSimulationPresentation();

                if (simulationMode == 2) {
                    setWeekSimulationPresentation(
                        WeekSimulationPresentation::MatchCenter);
                } else {
                    setWeekSimulationPresentation(
                        game_settings::isDetailedSimulation(settings_)
                            ? WeekSimulationPresentation::Detailed
                            : WeekSimulationPresentation::Compact);
                }

                // Create idle callback to prevent UI freezing during week simulation
                // Processes Windows messages periodically to keep UI responsive
                auto idleFunc = []() {
                    #ifdef _WIN32
                    // Process every Nth call to avoid excessive function call overhead
                    static int counter = 0;
                    if (++counter > 10) {
                        counter = 0;
                        MSG msg;
                        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                            ::TranslateMessage(&msg);
                            ::DispatchMessage(&msg);
                        }
                    }
                    #endif
                };

                ServiceResult simResult = engine_.simulateCareerWeek(idleFunc);
                setWeekSimulationPresentation(previousPresentation);
                for (const auto& message : simResult.messages) cout << message << endl;
                break;
            }
            case 5: displayCompetitionCenter(career); break;
            case 6: displayLeagueTables(career); break;
            case 7: transferMarket(career); break;
            case 8: scoutPlayers(career); break;
            case 9: displayStatistics(*career.myTeam); break;
            case 10: displayBoardStatus(career); break;
            case 11: displayNewsFeed(career); break;
            case 12: displaySeasonHistory(career); break;
            case 13: manageLineup(career); break;
            case 14: displayClubOperations(career); break;
            case 15: displayAchievementsMenu(career); break;
            case 16: {
                ServiceResult saveResult = engine_.saveCareer();
                for (const auto& message : saveResult.messages) cout << message << endl;
                break;
            }
            case 17: retirePlayer(*career.myTeam); break;
            case 18: setTrainingPlan(*career.myTeam); break;
            case 19: editTeam(*career.myTeam); break;
            case 20: cout << "Volviendo al menu principal." << endl; break;
            default: break;
        }
        if (careerChoice != 20) pauseForContinue();
    } while (careerChoice != 20);
}

void GameController::runQuickGame() {
    Career& career = engine_.career();
    cout << "\nConfiguracion activa: " << game_settings::settingsSummary(settings_) << endl;
    if (career.divisions.empty()) {
        cout << "No se encontraron divisiones disponibles." << endl;
        return;
    }

    cout << "\nSelecciona la division para Juego Rapido:" << endl;
    for (size_t i = 0; i < career.divisions.size(); ++i) {
        cout << i + 1 << ". " << career.divisions[i].display << endl;
    }
    int divisionChoice = readInt("Elige una division: ", 1, static_cast<int>(career.divisions.size()));
    const string divisionId = career.divisions[static_cast<size_t>(divisionChoice - 1)].id;
    auto teams = career.getDivisionTeams(divisionId);
    if (teams.empty()) {
        cout << "No hay equipos para esta division." << endl;
        return;
    }

    cout << "\nEquipos de " << divisionDisplay(divisionId) << ":" << endl;
    for (size_t i = 0; i < teams.size(); ++i) {
        cout << i + 1 << ". " << teams[i]->name << endl;
    }
    int teamChoice = readInt("Elige un numero de equipo: ", 1, static_cast<int>(teams.size()));
    Team myTeam = *teams[static_cast<size_t>(teamChoice - 1)];
    myTeam.resetSeasonStats();

    Team opponent = myTeam;
    if (teams.size() > 1) {
        int oppIndex = teamChoice - 1;
        while (oppIndex == teamChoice - 1) {
            oppIndex = randInt(0, static_cast<int>(teams.size()) - 1);
        }
        opponent = *teams[static_cast<size_t>(oppIndex)];
    } else {
        opponent.name = "Rival";
    }

    int gameChoice = 0;
    do {
        displayGameMenu();
        gameChoice = readInt("Elige una opcion: ", 1, 8);
        switch (gameChoice) {
            case 1: viewTeam(myTeam); break;
            case 2: addPlayer(myTeam); break;
            case 3: trainPlayer(myTeam); break;
            case 4: changeTactics(myTeam); break;
            case 5:
                game_settings::applyQuickMatchDifficulty(myTeam, opponent, settings_);
                engine_.runQuickMatch(myTeam, opponent, game_settings::isDetailedSimulation(settings_));
                break;
            case 6: {
                string fname = readLine("Ingresa ruta de archivo o carpeta del equipo: ");
                if (!loadTeamFromInputPath(fname, myTeam)) {
                    cout << "No se pudo cargar el equipo." << endl;
                } else {
                    cout << "Equipo cargado correctamente." << endl;
                }
                break;
            }
            case 7: editTeam(myTeam); break;
            case 8: cout << "Volviendo al menu principal." << endl; break;
            default: break;
        }
        if (gameChoice != 8) pauseForContinue();
    } while (gameChoice != 8);
}
