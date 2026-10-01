#include "gui/gui_audio.h"

#ifdef _WIN32

#include "utils/utils.h"

#include <mmsystem.h>

#include <sstream>
#include <vector>

namespace gui_win32 {

namespace {

constexpr wchar_t kMenuMusicAlias[] = L"cf_menu_music";
constexpr wchar_t kLegacyMenuMusicFileName[] = L"Los Miserables - El Crack  Video Oficial (HD Remastered).mp3";
constexpr char kThemeManifestPath[] = "assets/audio/menu_themes.csv";

struct MenuThemeAsset {
    std::string id;
    std::string fileName;
    std::string title;
    std::string license;
    std::string usage;
    std::string notes;
};

bool fileExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring canonicalPath(const std::wstring& path) {
    wchar_t buffer[MAX_PATH]{};
    DWORD length = GetFullPathNameW(path.c_str(), MAX_PATH, buffer, nullptr);
    if (length == 0 || length >= MAX_PATH) return path;
    return std::wstring(buffer, buffer + length);
}

std::wstring directoryOf(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return std::wstring();
    return path.substr(0, slash);
}

std::wstring joinWidePath(const std::wstring& base, const std::wstring& child) {
    if (base.empty()) return child;
    if (base.back() == L'\\' || base.back() == L'/') return base + child;
    return base + L"\\" + child;
}

std::wstring executableDirectory() {
    wchar_t buffer[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return std::wstring();
    return directoryOf(std::wstring(buffer, buffer + length));
}

std::wstring currentDirectoryPath() {
    wchar_t buffer[MAX_PATH]{};
    DWORD length = GetCurrentDirectoryW(MAX_PATH, buffer);
    if (length == 0 || length >= MAX_PATH) return std::wstring();
    return std::wstring(buffer, buffer + length);
}

bool sendMci(const std::wstring& command) {
    return mciSendStringW(command.c_str(), nullptr, 0, nullptr) == 0;
}

std::vector<std::string> manifestCandidates() {
    return {
        kThemeManifestPath,
        joinPath("..", kThemeManifestPath),
        joinPath("..", joinPath("..", kThemeManifestPath))
    };
}

std::vector<MenuThemeAsset> loadThemeManifest() {
    std::vector<MenuThemeAsset> themes;
    std::vector<std::string> lines;
    std::string manifestPath;
    for (const auto& candidate : manifestCandidates()) {
        if (readTextFileLines(candidate, lines)) {
            manifestPath = candidate;
            break;
        }
    }

    if (manifestPath.empty()) {
        themes.push_back({"el-crack",
                          "assets/audio/Los Miserables - El Crack  Video Oficial (HD Remastered).mp3",
                          "El Crack",
                          "Dominio publico (segun confirmacion del usuario)",
                          "Frontend principal",
                          "Fallback integrado cuando no aparece el manifiesto."});
        return themes;
    }

    for (size_t i = 1; i < lines.size(); ++i) {
        const std::string line = trim(lines[i]);
        if (line.empty()) continue;
        const std::vector<std::string> cols = splitCsvLine(line);
        if (cols.size() < 6) continue;
        MenuThemeAsset asset;
        asset.id = toLower(trim(cols[0]));
        asset.fileName = trim(cols[1]);
        asset.title = trim(cols[2]);
        asset.license = trim(cols[3]);
        asset.usage = trim(cols[4]);
        asset.notes = trim(cols[5]);
        if (!asset.id.empty() && !asset.fileName.empty()) themes.push_back(asset);
    }

    if (themes.empty()) {
        themes.push_back({"el-crack",
                          "assets/audio/Los Miserables - El Crack  Video Oficial (HD Remastered).mp3",
                          "El Crack",
                          "Dominio publico (segun confirmacion del usuario)",
                          "Frontend principal",
                          "Fallback integrado cuando el manifiesto queda vacio."});
    }
    return themes;
}

const MenuThemeAsset* themeForSettings(const AppState& state, const std::vector<MenuThemeAsset>& themes) {
    const std::string desired = toLower(trim(state.settings.menuThemeId));
    for (const auto& theme : themes) {
        if (theme.id == desired) return &theme;
    }
    return themes.empty() ? nullptr : &themes.front();
}

std::wstring resolveThemePath(const AppState& state) {
    const std::vector<MenuThemeAsset> themes = loadThemeManifest();
    const MenuThemeAsset* theme = themeForSettings(state, themes);
    if (!theme) return std::wstring();

    const std::wstring exeDir = executableDirectory();
    const std::wstring cwd = currentDirectoryPath();
    const std::wstring wideFile = utf8ToWide(theme->fileName);
    const std::wstring projectFile = utf8ToWide(resolveProjectPath(theme->fileName));
    const std::wstring projectLegacy =
        utf8ToWide(resolveProjectPath("Los Miserables - El Crack  Video Oficial (HD Remastered).mp3"));
    const std::vector<std::wstring> candidates = {
        canonicalPath(utf8ToWide(theme->fileName)),
        canonicalPath(projectFile),
        canonicalPath(joinWidePath(cwd, wideFile)),
        canonicalPath(joinWidePath(exeDir, wideFile)),
        canonicalPath(joinWidePath(exeDir, utf8ToWide(std::string("..\\") + theme->fileName))),
        canonicalPath(joinWidePath(exeDir, utf8ToWide(std::string("..\\..\\") + theme->fileName))),
        canonicalPath(projectLegacy),
        joinWidePath(cwd, kLegacyMenuMusicFileName),
        joinWidePath(exeDir, kLegacyMenuMusicFileName),
        canonicalPath(joinWidePath(exeDir, std::wstring(L"..\\") + kLegacyMenuMusicFileName)),
        canonicalPath(joinWidePath(exeDir, std::wstring(L"..\\..\\") + kLegacyMenuMusicFileName))
    };

    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        const std::wstring normalized = canonicalPath(candidate);
        if (fileExists(normalized)) return normalized;
    }
    return std::wstring();
}

int volumeToMciScale(const AppState&, int volume) {
    const int clamped = game_settings::clampVolume(volume);
    return (clamped * 1000) / 100;
}

void setDeviceVolume(AppState& state, int volume) {
    if (!state.menuMusicOpened) return;
    std::wostringstream command;
    command << L"setaudio " << kMenuMusicAlias << L" volume to " << volumeToMciScale(state, volume);
    if (sendMci(command.str())) {
        state.menuMusicAppliedVolume = game_settings::clampVolume(volume);
    }
}

void fadeDeviceVolume(AppState& state, int targetVolume) {
    if (!state.menuMusicOpened) return;
    const int clampedTarget = game_settings::clampVolume(targetVolume);
    // Sleeping in this path freezes the Win32 message loop while settings or pages refresh.
    setDeviceVolume(state, clampedTarget);
}

void closeMenuMusicDevice(AppState& state) {
    if (state.menuMusicOpened) {
        sendMci(L"close " + std::wstring(kMenuMusicAlias));
    }
    state.menuMusicOpened = false;
    state.menuMusicPlaying = false;
    state.menuMusicAppliedVolume = -1;
}

void stopMenuMusic(AppState& state) {
    if (!state.menuMusicOpened || !state.menuMusicPlaying) return;
    if (state.settings.menuAudioFade) fadeDeviceVolume(state, 0);
    sendMci(L"stop " + std::wstring(kMenuMusicAlias));
    sendMci(L"seek " + std::wstring(kMenuMusicAlias) + L" to start");
    state.menuMusicPlaying = false;
}

void ensureMenuMusicOpened(AppState& state) {
    const std::wstring desiredPath = resolveThemePath(state);
    if (desiredPath.empty()) {
        state.menuMusicMissingReported = true;
        return;
    }

    if (state.menuMusicOpened && canonicalPath(state.menuMusicPath) != canonicalPath(desiredPath)) {
        closeMenuMusicDevice(state);
    }

    if (state.menuMusicOpened) return;

    state.menuMusicPath = desiredPath;
    std::wstring openCommand = L"open \"" + state.menuMusicPath + L"\" type mpegvideo alias " + std::wstring(kMenuMusicAlias);
    if (!sendMci(openCommand)) {
        state.menuMusicMissingReported = true;
        state.menuMusicPath.clear();
        return;
    }

    state.menuMusicOpened = true;
    state.menuMusicAppliedVolume = -1;
    setDeviceVolume(state, state.settings.menuAudioFade ? 0 : state.settings.volume);
}

}  // namespace

void syncMenuMusicForPage(AppState& state) {
    const bool onMainMenu = state.currentPage == GuiPage::MainMenu;
    const bool onFrontendPage =
        isFrontMenuPage(state.currentPage) || state.currentPage == GuiPage::NewCareer;
    const bool shouldPlay = game_settings::shouldPlayMenuMusic(state.settings, onMainMenu, onFrontendPage);
    if (!shouldPlay) {
        stopMenuMusic(state);
        return;
    }

    ensureMenuMusicOpened(state);
    if (!state.menuMusicOpened) return;
    if (!state.menuMusicPlaying) {
        if (state.settings.menuAudioFade) setDeviceVolume(state, 0);
        if (sendMci(L"play " + std::wstring(kMenuMusicAlias) + L" repeat")) {
            state.menuMusicPlaying = true;
        }
    }
    fadeDeviceVolume(state, state.settings.volume);
}

void refreshMenuMusicVolume(AppState& state) {
    if (!state.menuMusicOpened) return;
    fadeDeviceVolume(state, state.settings.volume);
}

void shutdownMenuMusic(AppState& state) {
    stopMenuMusic(state);
    closeMenuMusicDevice(state);
}

}  // namespace gui_win32

#endif
