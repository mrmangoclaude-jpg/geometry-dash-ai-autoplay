#include "../src/macro/MacroTypes.hpp"
#include "../src/macro/MacroManager.hpp"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "[TEST] Running MacroData serialization test..." << std::endl;

    MacroData original;
    original.levelId = 128;
    original.levelName = "Stereo Madness";
    original.completionPercentage = 100.0f;
    original.totalFrames = 1420;
    original.attemptsToSolve = 4;
    original.actions = {
        {120, 1, true, false},
        {135, 1, false, false},
        {240, 1, true, false},
        {255, 1, false, false},
        {350, 1, true, true},
        {365, 1, false, true}
    };

    std::string jsonStr = original.serialize();
    MacroData deserialized = MacroData::deserialize(jsonStr);

    assert(deserialized.levelId == original.levelId);
    assert(deserialized.levelName == original.levelName);
    assert(deserialized.totalFrames == original.totalFrames);
    assert(deserialized.attemptsToSolve == original.attemptsToSolve);
    assert(deserialized.actions.size() == original.actions.size());

    for (size_t i = 0; i < original.actions.size(); ++i) {
        assert(deserialized.actions[i] == original.actions[i]);
    }

    std::cout << "[TEST] MacroManager file system test..." << std::endl;
    auto& manager = MacroManager::get();
    auto testDir = std::filesystem::temp_directory_path() / "gd_ai_test_macros";
    manager.setCustomDirectory(testDir);

    assert(manager.saveMacro(original));
    assert(manager.hasSavedMacro(128));

    auto loaded = manager.loadMacro(128);
    assert(loaded.has_value());
    assert(loaded->levelName == "Stereo Madness");
    assert(loaded->actions.size() == 6);

    auto all = manager.getAllSavedMacros();
    assert(all.size() >= 1);

    assert(manager.deleteMacro(128));
    assert(!manager.hasSavedMacro(128));

    std::filesystem::remove_all(testDir);
    std::cout << "[PASS] Macro serialization & storage tests passed!" << std::endl;
    return 0;
}
