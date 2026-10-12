#include "../solver/SolverEngine.hpp"
#include "../macro/MacroManager.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        if (level) {
            int levelId = level->m_levelID;
            if (MacroManager::get().hasSavedMacro(levelId)) {
                Notification::create("AI AutoPlay: Saved run available! (Press 'G' to Play)", NotificationIcon::Info)->show();
                
                bool autoPlay = Mod::get()->getSettingValue<bool>("auto_play_saved");
                if (autoPlay) {
                    auto saved = MacroManager::get().loadMacro(levelId);
                    if (saved.has_value()) {
                        SolverEngine::get().startPlaying(*saved);
                    }
                }
            }
        }
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        SolverEngine::get().onLevelReset();
    }

    void levelComplete() {
        if (SolverEngine::get().getState() == SolverState::Solving) {
            SolverEngine::get().onLevelComplete();
            return;
        }
        PlayLayer::levelComplete();
    }

    void onQuit() {
        SolverEngine::get().stopSolving();
        SolverEngine::get().stopPlaying();
        PlayLayer::onQuit();
    }
};
#endif
