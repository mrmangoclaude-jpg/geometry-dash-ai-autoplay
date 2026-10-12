#pragma once

#include "MacroTypes.hpp"
#include <filesystem>
#include <optional>
#include <vector>

class MacroManager {
private:
    std::filesystem::path m_customPath;
    MacroManager() = default;

public:
    static MacroManager& get();

    void setCustomDirectory(const std::filesystem::path& path);
    std::filesystem::path getMacrosDirectory();
    std::filesystem::path getMacroPathForLevel(int levelId);

    bool saveMacro(const MacroData& macro);
    std::optional<MacroData> loadMacro(int levelId);
    bool hasSavedMacro(int levelId);
    std::vector<MacroData> getAllSavedMacros();
    bool deleteMacro(int levelId);
};
