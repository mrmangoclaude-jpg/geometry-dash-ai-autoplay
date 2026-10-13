#include "solver/SolverEngine.hpp"
#include "macro/MacroManager.hpp"
#include "ui/AutoPlayPopup.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
        if (isKeyDown && !isKeyRepeat && key == enumKeyCodes::KEY_G) {
            auto playLayer = PlayLayer::get();
            if (playLayer && playLayer->m_level) {
                auto state = SolverEngine::get().getState();
                if (state == SolverState::Solving || state == SolverState::Playing) {
                    SolverEngine::get().stopSolving();
                    SolverEngine::get().stopPlaying();
                    Notification::create("AI AutoPlay: Stopped", NotificationIcon::Info)->show();
                } else {
                    int levelId = playLayer->m_level->m_levelID;
                    std::string levelName = playLayer->m_level->m_levelName;
                    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
                    if (MacroManager::get().hasSavedMacro(levelId, collectCoins, levelName)) {
                        auto saved = MacroManager::get().loadMacro(levelId, collectCoins, levelName);
                        if (saved.has_value()) {
                            SolverEngine::get().startPlaying(*saved);
                            playLayer->resetLevel();
                        }
                    } else {
                        SolverEngine::get().solveLevelAsync(levelId, levelName, playLayer);
                        playLayer->resetLevel();
                    }
                }
                return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
    }
};

$on_mod(Loaded) {
    log::info("AI AutoPlay mod loaded successfully!");
}

#else

int main() {
    return 0;
}

#endif
