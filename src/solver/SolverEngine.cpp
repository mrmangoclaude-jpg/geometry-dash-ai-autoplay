#include "SolverEngine.hpp"
#include "../macro/MacroManager.hpp"
#include <iostream>
#include <algorithm>

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>
using namespace geode::prelude;
#endif

SolverEngine& SolverEngine::get() {
    static SolverEngine instance;
    return instance;
}

void SolverEngine::startSolving(int levelId, const std::string& levelName) {
    m_state = SolverState::Solving;
    m_currentLevelId = levelId;
    m_currentLevelName = levelName;
    m_recordedActions.clear();
    m_decisionStack.clear();
    m_currentFrame = 0;
    m_totalAttempts = 0;
    m_bestXPos = 0.0f;
    m_isHolding = false;
    m_needsReset = false;

#if __has_include(<Geode/Geode.hpp>)
    Notification::create("AI AutoPlay: Started Solving...", NotificationIcon::Info)->show();
#endif
}

void SolverEngine::stopSolving() {
    m_state = SolverState::Idle;
    m_isHolding = false;
}

void SolverEngine::startPlaying(const MacroData& macro) {
    m_state = SolverState::Playing;
    m_activeMacro = macro;
    m_playbackIndex = 0;
    m_currentFrame = 0;
    m_isHolding = false;

#if __has_include(<Geode/Geode.hpp>)
    Notification::create("AI AutoPlay: Playing Saved Macro", NotificationIcon::Success)->show();
#endif
}

void SolverEngine::stopPlaying() {
    m_state = SolverState::Idle;
    m_isHolding = false;
}

void SolverEngine::onLevelReset() {
    m_currentFrame = 0;
    m_playbackIndex = 0;
    m_isHolding = false;
    m_needsReset = false;
}

void SolverEngine::onFrameTick(void* gameLayerPtr) {
    if (m_state == SolverState::Idle || m_state == SolverState::Paused) {
        return;
    }

#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<GJBaseGameLayer*>(gameLayerPtr);
    if (!layer || !layer->m_player1) return;

    auto p1 = layer->m_player1;

    // 1. PLAYING SAVED MACRO
    if (m_state == SolverState::Playing) {
        while (m_playbackIndex < m_activeMacro.actions.size()) {
            const auto& action = m_activeMacro.actions[m_playbackIndex];
            if (action.frame <= m_currentFrame) {
                layer->handleButton(action.isPush, action.button, action.isPlayer2);
                m_playbackIndex++;
            } else {
                break;
            }
        }
        m_currentFrame++;
        return;
    }

    // 2. SOLVING IN PROGRESS
    if (m_state == SolverState::Solving) {
        float currentX = p1->getPositionX();
        if (currentX > m_bestXPos) {
            m_bestXPos = currentX;
        }

        PlayerStateSnapshot snapshot;
        snapshot.mode = GamemodeController::get().detectGamemode(
            p1->m_isShip, p1->m_isBall, p1->m_isBird,
            p1->m_isDart, p1->m_isRobot, p1->m_isSpider, p1->m_isSwing
        );
        snapshot.posX = currentX;
        snapshot.posY = p1->getPositionY();
        snapshot.yVelocity = static_cast<float>(p1->m_yVelocity);
        snapshot.isOnGround = p1->m_isOnGround;
        snapshot.isUpsideDown = p1->m_isUpsideDown;
        snapshot.isHolding = m_isHolding;

        // Obstacle distance estimation from game objects ahead
        snapshot.hazardInFront = false;
        snapshot.obstacleDistX = 999.0f;
        snapshot.floorDist = snapshot.posY - 100.0f;
        snapshot.ceilingDist = 300.0f - snapshot.posY;

        if (layer->m_objects) {
            auto count = layer->m_objects->count();
            for (unsigned int i = 0; i < count; ++i) {
                auto obj = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
                if (!obj) continue;
                float objX = obj->getPositionX();
                if (objX > currentX && objX < currentX + 160.0f) {
                    float dx = objX - currentX;
                    // Check if object is hazardous
                    if (obj->m_objectType == GameObjectType::Hazard ||
                        obj->m_hasHazardEffect) {
                        snapshot.hazardInFront = true;
                        if (dx < snapshot.obstacleDistX) {
                            snapshot.obstacleDistX = dx;
                        }
                    }
                }
            }
        }

        int decision = GamemodeController::get().evaluateAction(snapshot);

        if (decision == 1 && !m_isHolding) {
            // Push jump button
            layer->handleButton(true, 1, false);
            m_isHolding = true;
            m_recordedActions.push_back({m_currentFrame, 1, true, false});

            // Save decision branch
            DecisionBranch branch;
            branch.frame = m_currentFrame;
            branch.posX = currentX;
            branch.actionTaken = 1;
            branch.alternativesTried = 1;
            m_decisionStack.push_back(branch);
        } else if (decision == 0 && m_isHolding) {
            // Release jump button
            layer->handleButton(false, 1, false);
            m_isHolding = false;
            m_recordedActions.push_back({m_currentFrame, 1, false, false});
        }

        m_currentFrame++;
    }
#else
    // Standalone / Simulation tick
    m_currentFrame++;
#endif
}

void SolverEngine::onPlayerDeath(void* playerPtr) {
    if (m_state != SolverState::Solving) return;

    m_totalAttempts++;
    m_isHolding = false;

    // Backtrack on decision stack
    if (!m_decisionStack.empty()) {
        auto& lastBranch = m_decisionStack.back();
        if (lastBranch.alternativesTried < 2) {
            // Try alternative branch (e.g. inverted decision)
            lastBranch.alternativesTried++;
            lastBranch.actionTaken = (lastBranch.actionTaken == 1) ? 0 : 1;
            // Prune actions beyond this frame
            while (!m_recordedActions.empty() && m_recordedActions.back().frame >= lastBranch.frame) {
                m_recordedActions.pop_back();
            }
        } else {
            // Exhausted this node, pop back to earlier decision
            m_decisionStack.pop_back();
            if (!m_decisionStack.empty()) {
                auto& prevBranch = m_decisionStack.back();
                prevBranch.alternativesTried++;
                while (!m_recordedActions.empty() && m_recordedActions.back().frame >= prevBranch.frame) {
                    m_recordedActions.pop_back();
                }
            } else {
                m_recordedActions.clear();
            }
        }
    } else {
        m_recordedActions.clear();
    }

    m_needsReset = true;

#if __has_include(<Geode/Geode.hpp>)
    auto playLayer = PlayLayer::get();
    if (playLayer) {
        playLayer->resetLevel();
    }
#endif
}

void SolverEngine::onLevelComplete() {
    if (m_state != SolverState::Solving) return;

    m_state = SolverState::Idle;
    m_isHolding = false;

    MacroData winningMacro;
    winningMacro.levelId = m_currentLevelId;
    winningMacro.levelName = m_currentLevelName;
    winningMacro.completionPercentage = 100.0f;
    winningMacro.totalFrames = m_currentFrame;
    winningMacro.attemptsToSolve = m_totalAttempts;
    winningMacro.actions = m_recordedActions;

    m_activeMacro = winningMacro;

    // Save winning run to library
    MacroManager::get().saveMacro(winningMacro);

#if __has_include(<Geode/Geode.hpp>)
    std::string msg = "AI Solved in " + std::to_string(m_totalAttempts) + " attempts!";
    Notification::create(msg, NotificationIcon::Success)->show();

    // Auto-replay at 1x speed
    startPlaying(winningMacro);
    auto playLayer = PlayLayer::get();
    if (playLayer) {
        playLayer->resetLevel();
    }
#endif
}
