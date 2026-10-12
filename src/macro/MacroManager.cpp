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

std::filesystem::path MacroManager::getMacroPathForLevel(int levelId) {
    return getMacrosDirectory() / (std::to_string(levelId) + ".json");
}

bool MacroManager::saveMacro(const MacroData& macro) {
    try {
        auto path = getMacroPathForLevel(macro.levelId);
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << macro.serialize();
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<MacroData> MacroManager::loadMacro(int levelId) {
    try {
        auto path = getMacroPathForLevel(levelId);
        if (!std::filesystem::exists(path)) return std::nullopt;

        std::ifstream file(path);
        if (!file.is_open()) return std::nullopt;

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return MacroData::deserialize(content);
    } catch (...) {
        return std::nullopt;
    }
}

bool MacroManager::hasSavedMacro(int levelId) {
    auto path = getMacroPathForLevel(levelId);
    return std::filesystem::exists(path);
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

bool MacroManager::deleteMacro(int levelId) {
    try {
        auto path = getMacroPathForLevel(levelId);
        if (std::filesystem::exists(path)) {
            return std::filesystem::remove(path);
        }
    } catch (...) {}
    return false;
}
