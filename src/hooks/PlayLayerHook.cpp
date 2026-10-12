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
            std::string levelName = m_level->m_levelName;
            bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");

            if (MacroManager::get().hasSavedMacro(levelId, collectCoins, levelName)) {
                auto saved = MacroManager::get().loadMacro(levelId, collectCoins, levelName);
                if (saved.has_value()) {
                    SolverEngine::get().startPlaying(*saved);
                }
            } else {
                // Pre-solve the entire level (including Move Triggers) BEFORE gameplay starts!
                SolverEngine::get().startSolving(levelId, levelName, this);
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
