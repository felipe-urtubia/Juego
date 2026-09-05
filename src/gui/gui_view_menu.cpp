#include "gui/gui_view_builders.h"

#ifdef _WIN32

#include "engine/game_settings.h"
#include "utils/utils.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace {

struct SaveBrowserSlot {
    std::string path;
    std::string club = "Sin club";
    std::string division = "Sin division";
    std::string manager = "Sin manager";
    std::string seasonWeek = "Sin fecha";
    std::string modified = "Sin fecha de archivo";
    bool hasBackup = false;
};

std::string mainMenuSavePath(const gui_win32::AppState& state) {
    std::string savePath = state.career.saveFile.empty() ? std::string("saves/career_save.txt") : state.career.saveFile;
    if (!pathExists(savePath) && savePath == "saves/career_save.txt" && pathExists("career_save.txt")) {
        savePath = "career_save.txt";
    }
    return savePath;
}

bool mainMenuHasSave(const gui_win32::AppState& state) {
    return pathExists(mainMenuSavePath(state));
}

std::string mainMenuSaveStateLabel(const gui_win32::AppState& state) {
    if (state.career.myTeam) return "Sesion activa";
    return mainMenuHasSave(state) ? "Guardado listo" : "Sin guardado";
}

std::string mainMenuBackupStateLabel(const gui_win32::AppState& state) {
    const std::string savePath = mainMenuSavePath(state);
    return pathExists(savePath + ".bak") ? "Backup disponible" : "Sin backup";
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string firstField(const std::string& text, char delimiter) {
    const size_t pos = text.find(delimiter);
    return trim(pos == std::string::npos ? text : text.substr(0, pos));
}

std::string saveModifiedLabel(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data{};
    const std::string resolved = resolveProjectPath(path);
    if (!GetFileAttributesExW(gui_win32::utf8ToWide(resolved).c_str(), GetFileExInfoStandard, &data)) {
        return "Sin fecha de archivo";
    }
    FILETIME localTime{};
    SYSTEMTIME systemTime{};
    if (!FileTimeToLocalFileTime(&data.ftLastWriteTime, &localTime) ||
        !FileTimeToSystemTime(&localTime, &systemTime)) {
        return "Sin fecha de archivo";
    }
    char buffer[32]{};
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%02u-%02u-%04u %02u:%02u",
                  static_cast<unsigned>(systemTime.wDay),
                  static_cast<unsigned>(systemTime.wMonth),
                  static_cast<unsigned>(systemTime.wYear),
                  static_cast<unsigned>(systemTime.wHour),
                  static_cast<unsigned>(systemTime.wMinute));
    return buffer;
#else
    (void)path;
    return "Fecha no disponible";
#endif
}

bool parseSaveSlot(const std::string& path, SaveBrowserSlot& slot) {
    std::vector<std::string> lines;
    if (!pathExists(path) || !readTextFileLines(path, lines)) return false;

    bool hasSeason = false;
    bool hasClub = false;
    slot = SaveBrowserSlot{};
    slot.path = path;
    slot.modified = saveModifiedLabel(path);
    slot.hasBackup = pathExists(path + ".bak");

    for (const std::string& rawLine : lines) {
        const std::string line = trim(rawLine);
        if (startsWith(line, "SEASON ")) {
            std::istringstream in(line);
            std::string seasonLabel;
            std::string weekLabel;
            int season = 0;
            int week = 0;
            in >> seasonLabel >> season >> weekLabel >> week;
            if (season > 0 && week > 0) {
                slot.seasonWeek = "Temp " + std::to_string(season) + " / Sem " + std::to_string(week);
                hasSeason = true;
            }
        } else if (startsWith(line, "DIVISION ")) {
            slot.division = divisionDisplay(trim(line.substr(9)));
        } else if (startsWith(line, "MYTEAM ")) {
            slot.club = trim(line.substr(7));
            hasClub = !slot.club.empty();
        } else if (startsWith(line, "MANAGER ")) {
            slot.manager = firstField(line.substr(8), '|');
            if (slot.manager.empty()) slot.manager = "Sin manager";
        }
    }

    return hasSeason && hasClub;
}

void addUniqueCandidate(std::vector<std::string>& candidates, const std::string& path) {
    if (path.empty()) return;
    if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) {
        candidates.push_back(path);
    }
}

void addSaveFolderCandidates(std::vector<std::string>& candidates) {
#ifdef _WIN32
    const std::string savesRoot = resolveProjectPath("saves");
    WIN32_FIND_DATAW findData{};
    HANDLE handle = FindFirstFileW(gui_win32::utf8ToWide(joinPath(savesRoot, "*.txt")).c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE) return;
    do {
        const std::string name = gui_win32::wideToUtf8(findData.cFileName);
        if (name == "." || name == "..") continue;
        const std::string lowerName = toLower(name);
        if (lowerName.find("validation") != std::string::npos || lowerName.find("test_") == 0) continue;
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        addUniqueCandidate(candidates, joinPath("saves", name));
    } while (FindNextFileW(handle, &findData));
    FindClose(handle);
#else
    (void)candidates;
#endif
}

std::vector<SaveBrowserSlot> discoverSaveSlots(const gui_win32::AppState& state) {
    std::vector<std::string> candidates;
    addUniqueCandidate(candidates, state.career.saveFile.empty() ? std::string("saves/career_save.txt") : state.career.saveFile);
    addUniqueCandidate(candidates, "saves/career_save.txt");
    addUniqueCandidate(candidates, "career_save.txt");
    addSaveFolderCandidates(candidates);

    std::vector<SaveBrowserSlot> slots;
    for (const std::string& path : candidates) {
        SaveBrowserSlot slot;
        if (parseSaveSlot(path, slot)) slots.push_back(slot);
    }
    std::sort(slots.begin(), slots.end(), [](const SaveBrowserSlot& a, const SaveBrowserSlot& b) {
        return a.path < b.path;
    });
    return slots;
}

const SaveBrowserSlot* selectedSlot(const std::vector<SaveBrowserSlot>& slots, const std::string& selectedPath) {
    for (const SaveBrowserSlot& slot : slots) {
        if (slot.path == selectedPath) return &slot;
    }
    return slots.empty() ? nullptr : &slots.front();
}

}  // namespace

namespace gui_win32 {

GuiPageModel buildMainMenuModel(AppState& state) {
    GuiPageModel model;
    const std::string savePath = mainMenuSavePath(state);
    const std::string saveState = mainMenuSaveStateLabel(state);
    const std::string backupState = mainMenuBackupStateLabel(state);

    model.title = game_settings::gameTitle();
    model.breadcrumb = "Inicio > Menu principal";
    model.infoLine = "Gestiona tu club. Toma decisiones. Escribe tu historia.";
    model.summary.title = "FrontMenuOverview";
    model.detail.title = "FrontMenuProfile";
    model.feed.title = "FrontMenuRoadmap";
    model.metrics = {
        {"Continuar", saveState, kThemeAccentBlue},
        {"Guardado", mainMenuHasSave(state) ? "Disponible" : "No existe", kThemeAccentGreen},
        {"Dificultad", game_settings::difficultyLabel(state.settings.difficulty), kThemeAccent},
        {"Velocidad", game_settings::simulationSpeedLabel(state.settings.simulationSpeed), kThemeWarning},
        {"Audio", game_settings::menuMusicModeLabel(state.settings.menuMusicMode), kThemeAccentGreen}
    };

    model.summary.content =
        "Centro del manager\r\n"
        "- Nueva carrera inicia una historia desde cero.\r\n"
        "- Continuar retoma la sesion activa o el ultimo guardado disponible.\r\n"
        "- Cargar partida abre el gestor de guardados.\r\n"
        "- Opciones concentra audio, simulacion, idioma y perfil visual.\r\n"
        "- Creditos presenta la informacion del proyecto.\r\n\r\n"
        "Guardado visible\r\n"
        "- Principal: " + saveState + " (" + savePath + ")\r\n"
        "- Respaldo: " + backupState + "\r\n"
        "- El frontend mantiene las acciones reales del juego.";

    std::ostringstream detail;
    detail << "Guardado\r\n";
    detail << "Ruta principal: " << savePath << "\r\n";
    detail << "Estado: " << saveState << "\r\n";
    detail << "Respaldo: " << backupState << "\r\n\r\n";
    detail << "Volumen: " << game_settings::volumeLabel(state.settings.volume) << "\r\n";
    detail << "Dificultad: " << game_settings::difficultyLabel(state.settings.difficulty) << "\r\n";
    detail << game_settings::difficultyDescription(state.settings.difficulty) << "\r\n\r\n";
    detail << "Velocidad de simulacion: " << game_settings::simulationSpeedLabel(state.settings.simulationSpeed) << "\r\n";
    detail << game_settings::simulationSpeedDescription(state.settings.simulationSpeed) << "\r\n\r\n";
    detail << "Modo de simulacion: " << game_settings::simulationModeLabel(state.settings.simulationMode) << "\r\n";
    detail << game_settings::simulationModeDescription(state.settings.simulationMode) << "\r\n\r\n";
    detail << "Idioma: " << game_settings::languageLabel(state.settings.language) << "\r\n";
    detail << "Texto: " << game_settings::textSpeedLabel(state.settings.textSpeed) << "\r\n";
    detail << "Visual: " << game_settings::visualProfileLabel(state.settings.visualProfile) << "\r\n";
    detail << "Musica: " << game_settings::menuMusicModeLabel(state.settings.menuMusicMode) << " | "
           << game_settings::menuAudioFadeLabel(state.settings.menuAudioFade) << "\r\n\r\n";
    detail << "Perfil persistente listo para reabrir el centro del club sin perder contexto.";
    model.detail.content = detail.str();

    model.feed.lines = {
        "Guardado principal: " + savePath + " | " + saveState,
        "Respaldo automatico: " + backupState,
        "Estado actual: portada completa con carrera, guardado y configuracion en tiempo real.",
        "Navegacion GUI: Tab, Enter, flechas, numeros, F11 y Esc desde ajustes o creditos.",
        "Continuar usa el flujo real y Guardados abre el selector de saves.",
        "Simular guarda automaticamente antes de avanzar la semana."
    };
    return model;
}

GuiPageModel buildSavesPageModel(AppState& state) {
    GuiPageModel model;
    model.title = "Guardados";
    model.breadcrumb = "Inicio > Guardados";
    model.infoLine = "Administra los guardados detectados en la carpeta saves.";
    model.summary.title = "SaveBrowserOverview";
    model.detail.title = "SaveBrowserDetail";
    model.feed.title = "SaveBrowserList";

    const std::vector<SaveBrowserSlot> slots = discoverSaveSlots(state);
    state.saveSlotPaths.clear();
    for (const SaveBrowserSlot& slot : slots) {
        state.saveSlotPaths.push_back(slot.path);
    }
    if (slots.empty()) {
        state.selectedSavePath.clear();
    } else if (state.selectedSavePath.empty() ||
               std::find(state.saveSlotPaths.begin(), state.saveSlotPaths.end(), state.selectedSavePath) == state.saveSlotPaths.end()) {
        state.selectedSavePath = slots.front().path;
    }

    const SaveBrowserSlot* selected = selectedSlot(slots, state.selectedSavePath);
    model.metrics = {
        {"Guardados", std::to_string(slots.size()), slots.empty() ? kThemeDanger : kThemeAccentGreen},
        {"Seleccion", selected ? selected->club : "Sin seleccion", selected ? kThemeAccentBlue : kThemeDanger},
        {"Backup", selected && selected->hasBackup ? "Disponible" : "No disponible", selected && selected->hasBackup ? kThemeAccentGreen : kThemeWarning},
        {"Sesion", state.career.myTeam ? "Activa" : "Libre", state.career.myTeam ? kThemeAccentBlue : kThemeAccent},
        {"Ruta", selected ? selected->path : "Sin guardado", selected ? kThemeWarning : kThemeDanger}
    };

    std::ostringstream summary;
    summary << "Centro de guardados\r\n\r\n";
    summary << "Guardados detectados: " << slots.size() << "\r\n";
    summary << "Carpetas revisadas: saves/ y guardado legado de la raiz.\r\n\r\n";
    summary << "Acciones\r\n";
    summary << "- Abrir seleccionado carga el save marcado en la lista.\r\n";
    summary << "- Borrar seleccionado elimina el save y su backup .bak.\r\n";
    summary << "- Volver regresa al menu principal.\r\n\r\n";
    if (state.career.myTeam) {
        summary << "Hay una carrera activa en memoria. Para borrar un guardado debes salir de esa carrera o volver a la portada sin sesion activa.";
    } else if (selected) {
        summary << "Seleccion actual: " << selected->club << " (" << selected->path << ").";
    } else {
        summary << "No hay guardados validos para cargar.";
    }
    model.summary.content = summary.str();

    std::ostringstream detail;
    if (selected) {
        detail << "Club: " << selected->club << "\r\n";
        detail << "Division: " << selected->division << "\r\n";
        detail << "Manager: " << selected->manager << "\r\n";
        detail << "Fecha carrera: " << selected->seasonWeek << "\r\n";
        detail << "Archivo: " << selected->path << "\r\n";
        detail << "Modificado: " << selected->modified << "\r\n";
        detail << "Backup: " << (selected->hasBackup ? selected->path + ".bak" : std::string("No disponible")) << "\r\n\r\n";
        detail << "Pulsa Abrir seleccionado para cargar esta carrera.";
    } else {
        detail << "No se encontraron archivos de carrera validos.\r\n\r\n";
        detail << "Crea una carrera nueva y usa Guardar para generar saves/career_save.txt.";
    }
    model.detail.content = detail.str();

    if (slots.empty()) {
        model.feed.lines.push_back("Sin guardados validos en saves/.");
    } else {
        for (const SaveBrowserSlot& slot : slots) {
            model.feed.lines.push_back(slot.club + " | " + slot.division + " | " + slot.seasonWeek +
                                      " | " + slot.manager + " | " + slot.modified +
                                      (slot.hasBackup ? " | backup" : "") +
                                      " | " + slot.path);
        }
    }
    return model;
}

GuiPageModel buildSettingsPageModel(AppState& state) {
    GuiPageModel model;
    model.title = "Configuraciones";
    model.breadcrumb = "Inicio > Configuraciones";
    model.infoLine = state.settingsDirty
        ? "Cabina inicial de ajustes. Hay cambios pendientes por aplicar o restaurar."
        : "Cabina inicial de ajustes. Cambia el tono del juego antes de entrar al centro del club.";
    model.summary.title = "SettingsDesk";
    model.detail.title = "SettingsImpact";
    model.feed.title = "SettingsReturn";
    model.metrics = {
        {"Volumen", game_settings::volumeLabel(state.settings.volume), kThemeAccentGreen},
        {"Dificultad", game_settings::difficultyLabel(state.settings.difficulty), kThemeAccent},
        {"Velocidad", game_settings::simulationSpeedLabel(state.settings.simulationSpeed), kThemeWarning},
        {"Visual", game_settings::visualProfileLabel(state.settings.visualProfile), kThemeAccentBlue},
        {"Musica", game_settings::menuMusicModeLabel(state.settings.menuMusicMode), kThemeAccent}
    };

    model.summary.content =
        "[Volumen]\r\n"
        "Se mantiene como control de frontend aunque el juego aun no tenga audio real.\r\n\r\n"
        "[Dificultad]\r\n"
        "Afecta el arranque de nuevas carreras y el contexto del proyecto.\r\n\r\n"
        "[Velocidad de simulacion]\r\n"
        "Define el ritmo percibido del frontend y deja lista la estructura para futuras animaciones o pausas.\r\n\r\n"
        "[Modo de simulacion]\r\n"
        "Controla si el juego prioriza una lectura rapida o una salida mas detallada.\r\n\r\n"
        "[Idioma, texto y visual]\r\n"
        "Preparan accesibilidad, densidad y tono visual del frontend.\r\n\r\n"
        "[Musica del frontend]\r\n"
        "Permite silenciar el tema o extenderlo a portada, ajustes y creditos.";

    std::ostringstream detail;
    detail << "Volumen: " << game_settings::volumeLabel(state.settings.volume) << "\r\n";
    detail << "Dificultad: " << game_settings::difficultyLabel(state.settings.difficulty) << "\r\n";
    detail << game_settings::difficultyDescription(state.settings.difficulty) << "\r\n\r\n";
    detail << "Velocidad: " << game_settings::simulationSpeedLabel(state.settings.simulationSpeed) << "\r\n";
    detail << game_settings::simulationSpeedDescription(state.settings.simulationSpeed) << "\r\n\r\n";
    detail << "Simulacion: " << game_settings::simulationModeLabel(state.settings.simulationMode) << "\r\n";
    detail << game_settings::simulationModeDescription(state.settings.simulationMode) << "\r\n\r\n";
    detail << "Idioma: " << game_settings::languageLabel(state.settings.language) << "\r\n";
    detail << game_settings::languageDescription(state.settings.language) << "\r\n\r\n";
    detail << "Texto: " << game_settings::textSpeedLabel(state.settings.textSpeed) << "\r\n";
    detail << game_settings::textSpeedDescription(state.settings.textSpeed) << "\r\n\r\n";
    detail << "Visual: " << game_settings::visualProfileLabel(state.settings.visualProfile) << "\r\n";
    detail << game_settings::visualProfileDescription(state.settings.visualProfile) << "\r\n\r\n";
    detail << "Musica: " << game_settings::menuMusicModeLabel(state.settings.menuMusicMode) << "\r\n";
    detail << game_settings::menuMusicModeDescription(state.settings.menuMusicMode) << "\r\n";
    detail << game_settings::menuAudioFadeDescription(state.settings.menuAudioFade) << "\r\n\r\n";
    detail << (state.settingsDirty
                   ? "Hay cambios pendientes. Aplicar guarda en disco; Restaurar vuelve al ultimo estado aplicado."
                   : "Ajustes aplicados. Puedes cambiar valores y confirmar solo cuando estes conforme.");
    model.detail.content = detail.str();

    model.feed.lines = {
        std::string("Volumen actual: ") + game_settings::volumeLabel(state.settings.volume),
        std::string("Dificultad actual: ") + game_settings::difficultyLabel(state.settings.difficulty),
        std::string("Velocidad actual: ") + game_settings::simulationSpeedLabel(state.settings.simulationSpeed),
        std::string("Modo actual: ") + game_settings::simulationModeLabel(state.settings.simulationMode),
        std::string("Idioma / texto: ") + game_settings::languageLabel(state.settings.language) + " / " + game_settings::textSpeedLabel(state.settings.textSpeed),
        std::string("Visual / musica: ") + game_settings::visualProfileLabel(state.settings.visualProfile) + " / " + game_settings::menuMusicModeLabel(state.settings.menuMusicMode),
        state.settingsDirty ? "Pendiente: usa Aplicar ajustes o Restaurar." : "Estado: ajustes aplicados y guardados."
    };
    return model;
}

GuiPageModel buildCreditsPageModel(AppState& state) {
    GuiPageModel model;
    model.title = "Creditos";
    model.breadcrumb = "Inicio > Creditos";
    model.infoLine = "Identidad del proyecto, base tecnica y direccion actual de Chilean Footballito.";
    model.summary.title = "FrontMenuOverview";
    model.detail.title = "FrontMenuProfile";
    model.feed.title = "FrontMenuRoadmap";
    model.metrics = {
        {"Motor", "C++17", kThemeAccentBlue},
        {"Frontend", "Win32 + CLI", kThemeAccent},
        {"Audio", game_settings::menuThemeLabel(state.settings), kThemeAccentGreen},
        {"Visual", game_settings::visualProfileLabel(state.settings.visualProfile), kThemeWarning},
        {"Estado", "Escalable", kThemeAccentBlue}
    };

    model.summary.content =
        "Chilean Footballito\r\n"
        "Simulador de gestion futbolistica construido en C++ con una arquitectura modular que une carrera, staff, scouting, mercado, motor de partido y frontend compartido.\r\n\r\n"
        "La GUI Win32 y la CLI consumen la misma capa de servicios, settings y persistencia.";

    std::ostringstream detail;
    detail << "Tecnologia\r\n";
    detail << "- C++17\r\n";
    detail << "- GUI Win32 nativa\r\n";
    detail << "- CLI compartida para validacion y depuracion\r\n";
    detail << "- Persistencia propia para carrera y configuracion\r\n\r\n";
    detail << "Tema del menu\r\n";
    detail << "- Track activo: " << game_settings::menuThemeLabel(state.settings) << "\r\n";
    detail << "- Alcance: " << game_settings::menuMusicModeLabel(state.settings.menuMusicMode) << "\r\n";
    detail << "- Transicion: " << game_settings::menuAudioFadeLabel(state.settings.menuAudioFade) << "\r\n\r\n";
    detail << "Objetivo\r\n";
    detail << "Seguir acercando el proyecto a una experiencia profunda tipo manager game.";
    model.detail.content = detail.str();

    model.feed.lines = {
        "Arquitectura preparada para continuar, cargar, creditos ampliados y opciones de video.",
        "Settings persistentes unificados entre consola, GUI, audio y timing del frontend.",
        "Audio y assets del menu ya viven en assets/audio con manifiesto documentado.",
        "Volver te devuelve limpio a la portada principal."
    };
    return model;
}

}  // namespace gui_win32

#endif
