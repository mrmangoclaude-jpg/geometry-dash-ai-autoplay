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
    // If this level already has a verified solved macro, immediately play it!
    if (MacroManager::get().hasSavedMacro(levelId)) {
        auto saved = MacroManager::get().loadMacro(levelId);
        if (saved.has_value()) {
            startPlaying(*saved);
#if __has_include(<Geode/Geode.hpp>)
            Notification::create("AI AutoPlay: Level Solved! Playing run...", NotificationIcon::Success)->show();
            auto playLayer = PlayLayer::get();
            if (playLayer) {
                playLayer->resetLevel();
            }
#endif
            return;
        }
    }

    m_state = SolverState::Solving;
    m_currentLevelId = levelId;
    m_currentLevelName = levelName;
    m_recordedActions.clear();
    m_verifiedBestActions.clear();
    m_decisionStack.clear();
    m_currentFrame = 0;
    m_replayIndex = 0;
    m_totalAttempts = 0;
    m_bestXPos = 0.0f;
    m_currentX = 0.0f;
    m_isHolding = false;
    m_needsReset = false;

#if __has_include(<Geode/Geode.hpp>)
    Notification::create("AI AutoPlay: Solving Headless in Background...", NotificationIcon::Info)->show();
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
    m_replayIndex = 0;
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
    float currentX = p1->getPositionX();
    float currentY = p1->getPositionY();
    m_currentX = currentX;

    // 1. PLAYING SAVED MACRO
    if (m_state == SolverState::Playing) {
        float levelTime = 0.0f;
        if (layer) {
            levelTime = layer->m_gameState.m_levelTime;
        }

        while (m_playbackIndex < m_activeMacro.actions.size()) {
            const auto& action = m_activeMacro.actions[m_playbackIndex];
            bool shouldTrigger = false;
            if (action.xPos > 0.0f) {
                // Spatial position trigger: 100% FPS-proof (triggers at exact obstacle coordinate even on 44 FPS)
                shouldTrigger = (currentX >= action.xPos - 3.5f);
            } else {
                // Physics time clock trigger (converts actual in-game levelTime to 60Hz ticks)
                float physicsFrame = levelTime * 60.0f;
                shouldTrigger = (physicsFrame >= static_cast<float>(action.frame));
            }

            if (shouldTrigger) {
                layer->handleButton(action.isPush, action.button, action.isPlayer2);
                m_isHolding = action.isPush;
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
        if (currentX > m_bestXPos) {
            m_bestXPos = currentX;
        }

        // Check if we can replay a verified safe action from previous best runs
        if (m_replayIndex < m_verifiedBestActions.size()) {
            const auto& act = m_verifiedBestActions[m_replayIndex];
            if (act.frame <= m_currentFrame) {
                layer->handleButton(act.isPush, act.button, act.isPlayer2);
                m_isHolding = act.isPush;
                m_recordedActions.push_back(act);
                m_replayIndex++;
            }
            m_currentFrame++;
            return;
        }

        // Beyond known territory: Frontier exploration with Trajectory Forecaster!
        PlayerStateSnapshot snapshot;
        snapshot.mode = GamemodeController::get().detectGamemode(
            p1->m_isShip, p1->m_isBall, p1->m_isBird,
            p1->m_isDart, p1->m_isRobot, p1->m_isSpider, p1->m_isSwing
        );
        snapshot.posX = currentX;
        snapshot.posY = currentY;
        snapshot.yVelocity = static_cast<float>(p1->m_yVelocity);
        snapshot.isOnGround = p1->m_isOnGround;
        snapshot.isUpsideDown = p1->m_isUpsideDown;
        snapshot.isHolding = m_isHolding;

        snapshot.hazardInFront = false;
        snapshot.obstacleDistX = 999.0f;
        snapshot.floorDist = snapshot.posY - 105.0f;
        snapshot.ceilingDist = 320.0f - snapshot.posY;

        // Parse nearby objects within [currentX - 40, currentX + 350]
        struct DetectedObject {
            float x, y, width, height;
            GameObjectType type;
            bool isHazard;
            bool isSolid;
            bool isRing;
        };
        std::vector<DetectedObject> nearby;
        nearby.reserve(64);

        if (layer->m_objects) {
            auto count = layer->m_objects->count();
            for (unsigned int i = 0; i < count; ++i) {
                auto obj = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
                if (!obj) continue;
                float ox = obj->getPositionX();
                if (ox >= currentX - 40.0f && ox <= currentX + 350.0f) {
                    float oy = obj->getPositionY();
                    auto type = obj->m_objectType;
                    bool hazard = (type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard);
                    bool solid = (type == GameObjectType::Solid || type == GameObjectType::Slope);
                    bool ring = (type == GameObjectType::YellowJumpRing || type == GameObjectType::PinkJumpRing ||
                                 type == GameObjectType::GravityRing || type == GameObjectType::GreenRing ||
                                 type == GameObjectType::RedJumpRing || type == GameObjectType::DashRing ||
                                 type == GameObjectType::SpiderOrb || type == GameObjectType::TeleportOrb);
                    
                    auto rect = obj->getObjectRect();
                    float w = (rect.size.width > 0.0f) ? rect.size.width : 30.0f;
                    float h = (rect.size.height > 0.0f) ? rect.size.height : 30.0f;
                    nearby.push_back({ox, oy, w, h, type, hazard, solid, ring});
                }
            }
        }

        // Constant speed: exact 60Hz physics (vx = 311.58/60, vy0 = 603.72/60, g = -2794.1/3600)
        const float vx = 5.193f;
        const float vy0 = 10.062f;
        const float gravity = -0.77614f;

        // A. Ground Threat Detection (Will we crash or die without jumping?)
        for (int step = 1; step <= 26; ++step) {
            float sx = currentX + step * vx;
            float sy = currentY;
            float pL = sx - 9.0f, pR = sx + 9.0f;
            float pB = sy - 12.0f, pT = sy + 12.0f;

            for (const auto& o : nearby) {
                float oL = o.x - o.width * 0.5f;
                float oR = o.x + o.width * 0.5f;
                float oB = o.y - o.height * 0.5f;
                float oT = o.y + o.height * 0.5f;

                bool overlap = (pL < oR && pR > oL && pB < oT && pT > oB);
                if (overlap) {
                    if (o.isHazard) {
                        snapshot.hazardInFront = true;
                        float dx = o.x - currentX;
                        if (dx < snapshot.obstacleDistX) snapshot.obstacleDistX = dx;
                        if (step <= 4) snapshot.groundHasImmediateThreat = true;
                    } else if (o.isSolid && oT > currentY - 6.0f) {
                        // Running into front/side of a block!
                        snapshot.hazardInFront = true;
                        float dx = o.x - currentX;
                        if (dx < snapshot.obstacleDistX) snapshot.obstacleDistX = dx;
                        if (step <= 4) snapshot.groundHasImmediateThreat = true;
                    }
                }
            }
        }

        // B. Jump Arc Forecaster: simulate full 26 frames of trajectory
        bool simJumpSafe = true;
        float simY = currentY;
        float simVy = vy0;

        for (int k = 1; k <= 26; ++k) {
            float sx = currentX + k * vx;
            simVy += gravity;
            simY += simVy;
            if (simY < currentY) simY = currentY;

            float pL = sx - 8.0f, pR = sx + 8.0f;
            float pB = simY - 10.0f, pT = simY + 10.0f;

            for (const auto& o : nearby) {
                float oL = o.x - o.width * 0.5f;
                float oR = o.x + o.width * 0.5f;
                float oB = o.y - o.height * 0.5f;
                float oT = o.y + o.height * 0.5f;

                bool overlap = (pL < oR && pR > oL && pB < oT && pT > oB);
                if (overlap) {
                    if (o.isHazard) {
                        simJumpSafe = false;
                        break;
                    } else if (o.isSolid) {
                        if (simY < oT - 5.0f && simVy > 0.0f) {
                            simJumpSafe = false;
                            break;
                        }
                    }
                }
            }
            if (!simJumpSafe) break;

            // Landing footprint check (k >= 22)
            if (k >= 22) {
                for (const auto& o : nearby) {
                    if (o.isHazard) {
                        float oL = o.x - o.width * 0.5f;
                        float oR = o.x + o.width * 0.5f;
                        if (sx > oL - 8.0f && sx < oR + 8.0f && simY <= o.y + o.height * 0.5f + 8.0f) {
                            simJumpSafe = false;
                            break;
                        }
                    }
                }
            }
        }
        snapshot.jumpIsSafe = simJumpSafe;

        // C. Jump Ring / Orb in reach check
        for (const auto& o : nearby) {
            if (o.isRing) {
                float dist = std::hypot(o.x - currentX, o.y - currentY);
                if (dist < 38.0f) {
                    snapshot.ringInReach = true;
                    snapshot.orbIsSafeToTap = true;
                    break;
                }
            }
        }

        int decision = GamemodeController::get().evaluateAction(snapshot);

        if (decision == 1 && !m_isHolding) {
            layer->handleButton(true, 1, false);
            m_isHolding = true;
            m_recordedActions.push_back({m_currentFrame, currentX, 1, true, false});
        } else if (decision == 0 && m_isHolding) {
            layer->handleButton(false, 1, false);
            m_isHolding = false;
            m_recordedActions.push_back({m_currentFrame, currentX, 1, false, false});
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

    // Check if this run advanced the frontier
    if (m_currentX > m_bestXPos + 10.0f) {
        m_bestXPos = m_currentX;
        // Keep verified actions up to 10 frames before the death point
        m_verifiedBestActions = m_recordedActions;
        while (!m_verifiedBestActions.empty() && m_verifiedBestActions.back().frame + 10 >= m_currentFrame) {
            m_verifiedBestActions.pop_back();
        }
    } else {
        // Did not advance: prune back one action from verified run
        if (!m_verifiedBestActions.empty()) {
            m_verifiedBestActions.pop_back();
        }
    }

    m_recordedActions.clear();
    m_replayIndex = 0;
    m_currentFrame = 0;
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
