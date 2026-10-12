#include "../solver/SolverEngine.hpp"
#include "../macro/MacroManager.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>

using namespace geode::prelude;

class $modify(AutoPlayPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (m_level) {
            int levelId = m_level->m_levelID;
            bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
            bool autoPlay = Mod::get()->getSettingValue<bool>("auto_play_saved");

            if (MacroManager::get().hasSavedMacro(levelId, collectCoins)) {
                Notification::create("AI AutoPlay: Saved run ready! (Press 'G' to toggle)", NotificationIcon::Info)->show();
                if (autoPlay) {
                    auto saved = MacroManager::get().loadMacro(levelId, collectCoins);
                    if (saved.has_value()) {
                        SolverEngine::get().startPlaying(*saved);
                    }
                }
            } else if (autoPlay) {
                // Pre-solve the entire level (including Move Triggers) BEFORE gameplay starts!
                SolverEngine::get().startSolving(levelId, m_level->m_levelName);
            }
        }
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
