#include "solver/SolverEngine.hpp"
#include "macro/MacroManager.hpp"
#include "ui/AutoPlayPopup.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
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
                    if (MacroManager::get().hasSavedMacro(levelId)) {
                        auto saved = MacroManager::get().loadMacro(levelId);
                        if (saved.has_value()) {
                            SolverEngine::get().startPlaying(*saved);
                            playLayer->resetLevel();
                        }
                    } else {
                        SolverEngine::get().startSolving(levelId, playLayer->m_level->m_levelName);
                    }
                }
                return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);
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
