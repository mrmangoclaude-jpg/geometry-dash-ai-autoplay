#include "../solver/SolverEngine.hpp"
#include "../macro/MacroManager.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>

using namespace geode::prelude;

class $modify(AutoPlayPlayLayer, PlayLayer) {
    CCDrawNode* getOrCreateTrajectoryNode() {
        if (!m_objectLayer) return nullptr;
        auto node = static_cast<CCDrawNode*>(m_objectLayer->getChildByID("autoplay-trajectory-node"_spr));
        if (!node) {
            node = CCDrawNode::create();
            node->setID("autoplay-trajectory-node"_spr);
            m_objectLayer->addChild(node, 9999);
        }
        return node;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (m_level) {
            int levelId = m_level->m_levelID;
            std::string levelName = m_level->m_levelName;
            bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
            bool showTraj = Mod::get()->getSettingValue<bool>("show_trajectory");

            CCDrawNode* trajNode = showTraj ? getOrCreateTrajectoryNode() : nullptr;

            if (MacroManager::get().hasSavedMacro(levelId, collectCoins, levelName)) {
                auto saved = MacroManager::get().loadMacro(levelId, collectCoins, levelName);
                if (saved.has_value()) {
                    SolverEngine::get().startPlaying(*saved);
                    if (trajNode) {
                        SolverEngine::get().renderTrajectory(trajNode);
                    }
                }
            } else {
                // Pre-solve the entire level (including Move Triggers) BEFORE gameplay starts!
                SolverEngine::get().startSolving(levelId, levelName, this);
                if (trajNode) {
                    SolverEngine::get().renderTrajectory(trajNode);
                }
            }
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        SolverEngine::get().onLevelReset();
        bool showTraj = Mod::get()->getSettingValue<bool>("show_trajectory");
        if (showTraj) {
            CCDrawNode* trajNode = getOrCreateTrajectoryNode();
            if (trajNode) {
                SolverEngine::get().renderTrajectory(trajNode);
            }
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
        SolverEngine::get().clearTrajectoryNode();
        SolverEngine::get().stopSolving();
        SolverEngine::get().stopPlaying();
        PlayLayer::onQuit();
    }
};
#endif
