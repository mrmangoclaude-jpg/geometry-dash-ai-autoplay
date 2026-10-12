#include "MacroManager.hpp"
#include <fstream>
#include <iostream>

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
using namespace geode::prelude;
#endif

MacroManager& MacroManager::get() {
    static MacroManager instance;
    return instance;
}

void MacroManager::setCustomDirectory(const std::filesystem::path& path) {
    m_customPath = path;
}

std::filesystem::path MacroManager::getMacrosDirectory() {
    if (!m_customPath.empty()) {
        std::filesystem::create_directories(m_customPath);
        return m_customPath;
    }

#if __has_include(<Geode/Geode.hpp>)
    auto dir = Mod::get()->getSaveDir() / "saved_autoplays";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
#else
    auto dir = std::filesystem::current_path() / "saved_autoplays";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
#endif
}

static std::string sanitizeLevelName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('_');
        }
    }
    return out.empty() ? "unnamed" : out;
}

std::filesystem::path MacroManager::getMacroPathForLevel(int levelId, bool coins, const std::string& levelName) {
    std::string base = (levelId > 0 || levelName.empty())
        ? std::to_string(levelId)
        : ("local_" + sanitizeLevelName(levelName));
    std::string filename = base + (coins ? "_coins.json" : ".json");
    return getMacrosDirectory() / filename;
}

bool MacroManager::saveMacro(const MacroData& macro, bool coins) {
    if (macro.actions.empty()) return false;
    try {
        auto path = getMacroPathForLevel(macro.levelId, coins, macro.levelName);
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << macro.serialize();
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<MacroData> MacroManager::loadMacro(int levelId, bool coins, const std::string& levelName) {
    try {
        if (coins) {
            auto pathCoins = getMacroPathForLevel(levelId, true, levelName);
            if (std::filesystem::exists(pathCoins)) {
                std::ifstream file(pathCoins);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
                    auto data = MacroData::deserialize(content);
                    if (!data.actions.empty()) return data;
                }
            }
        }
        auto path = getMacroPathForLevel(levelId, false, levelName);
        if (!std::filesystem::exists(path)) return std::nullopt;

        std::ifstream file(path);
        if (!file.is_open()) return std::nullopt;

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        auto data = MacroData::deserialize(content);
        if (data.actions.empty()) return std::nullopt;
        return data;
    } catch (...) {
        return std::nullopt;
    }
}

bool MacroManager::hasSavedMacro(int levelId, bool coins, const std::string& levelName) {
    return loadMacro(levelId, coins, levelName).has_value();
}

std::vector<MacroData> MacroManager::getAllSavedMacros() {
    std::vector<MacroData> results;
    try {
        auto dir = getMacrosDirectory();
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
                    results.push_back(MacroData::deserialize(content));
                }
            }
        }
    } catch (...) {}
    return results;
}

bool MacroManager::deleteMacro(int levelId, bool coins, const std::string& levelName) {
    try {
        auto path = getMacroPathForLevel(levelId, coins, levelName);
        if (std::filesystem::exists(path)) {
            return std::filesystem::remove(path);
        }
    } catch (...) {}
    return false;
}
