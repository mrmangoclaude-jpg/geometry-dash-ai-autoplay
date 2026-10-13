#include "../solver/SolverEngine.hpp"
#include "../macro/MacroManager.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>

using namespace geode::prelude;

class $modify(AutoPlayPlayLayer, PlayLayer) {
    struct Fields {
        CCDrawNode* m_trajectoryNode = nullptr;
    };

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (m_level) {
            int levelId = m_level->m_levelID;
            std::string levelName = m_level->m_levelName;
            bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
            bool showTraj = Mod::get()->getSettingValue<bool>("show_trajectory");

            if (showTraj && m_objectLayer) {
                if (!m_fields->m_trajectoryNode) {
                    m_fields->m_trajectoryNode = CCDrawNode::create();
                    m_fields->m_trajectoryNode->setID("autoplay-trajectory-node"_spr);
                    m_objectLayer->addChild(m_fields->m_trajectoryNode, 9999);
                }
            }

            if (MacroManager::get().hasSavedMacro(levelId, collectCoins, levelName)) {
                auto saved = MacroManager::get().loadMacro(levelId, collectCoins, levelName);
                if (saved.has_value()) {
                    SolverEngine::get().startPlaying(*saved);
                    if (m_fields->m_trajectoryNode) {
                        SolverEngine::get().renderTrajectory(m_fields->m_trajectoryNode);
                    }
                }
            } else {
                // Pre-solve the entire level (including Move Triggers) BEFORE gameplay starts!
                SolverEngine::get().startSolving(levelId, levelName, this);
                if (m_fields->m_trajectoryNode) {
                    SolverEngine::get().renderTrajectory(m_fields->m_trajectoryNode);
                }
            }
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        SolverEngine::get().onLevelReset();
        if (m_fields->m_trajectoryNode) {
            SolverEngine::get().renderTrajectory(m_fields->m_trajectoryNode);
        }
    }

    void levelComplete() {
        if (SolverEngine::get().getState() == SolverState::Solving) {
            SolverEngine::get().onLevelComplete();
            return;
        }
        PlayLayer::levelComplete();
    }

    void onQuit() {
        m_fields->m_trajectoryNode = nullptr;
        SolverEngine::get().stopSolving();
        SolverEngine::get().stopPlaying();
        PlayLayer::onQuit();
    }
};
#endif
