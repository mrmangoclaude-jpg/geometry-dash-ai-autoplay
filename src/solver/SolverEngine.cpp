#include "SolverEngine.hpp"
#include "../macro/MacroManager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <array>

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/EffectGameObject.hpp>
using namespace geode::prelude;
#endif

SolverEngine& SolverEngine::get() {
    static SolverEngine instance;
    return instance;
}

void SolverEngine::startSolving(int levelId, const std::string& levelName) {
#if __has_include(<Geode/Geode.hpp>)
    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
    if (MacroManager::get().hasSavedMacro(levelId, collectCoins)) {
        auto saved = MacroManager::get().loadMacro(levelId, collectCoins);
        if (saved.has_value()) {
            startPlaying(*saved);
            auto playLayer = PlayLayer::get();
            if (playLayer && playLayer->m_gameState.m_levelTime > 0.05) {
                playLayer->resetLevel();
            }
            return;
        }
    }
#endif

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
    auto playLayer = PlayLayer::get();
    if (playLayer) {
        solveMathematically(playLayer);
    }
#endif
}

#if __has_include(<Geode/Geode.hpp>)
namespace {
    enum class SimMode { Cube, Ship, Ball, UFO, Wave, Robot, Spider };

    struct MoveTrigger {
        float triggerX;
        int targetGroup;
        float dx;
        float dy;
        float duration;
    };

    struct LevelObj {
        int uid;
        int id;
        float x, y, w, h;
        float baseX, baseY;
        bool isHazard;
        bool isSolid;
        bool isSlope;
        bool isStopDash;
        bool isYellowPad, isPinkPad, isRedPad, isGravPad, isSpiderPad, isPad;
        bool isYellowOrb, isPinkOrb, isRedOrb, isGravOrb, isGreenOrb, isDashOrb, isSpiderOrb, isOrb;
        bool isPortalCube, isPortalShip, isPortalBall, isPortalUFO, isPortalWave, isPortalRobot, isPortalSpider;
        bool isPortalGravNorm, isPortalGravInv, isPortalGravToggle;
        bool isPortalMini, isPortalNormalSize;
        float speedVal;
        std::vector<MoveTrigger> moveTriggers;
    };

    inline void evalObjPos(const LevelObj& o, float px, float vx, float& outX, float& outY) {
        outX = o.baseX;
        outY = o.baseY;
        if (o.moveTriggers.empty()) return;
        float speedUnitsPerSec = std::max(100.0f, vx * 60.0f);
        for (const auto& mt : o.moveTriggers) {
            if (px > mt.triggerX) {
                float elapsed = (px - mt.triggerX) / speedUnitsPerSec;
                float prog = (mt.duration <= 0.001f) ? 1.0f : std::clamp(elapsed / mt.duration, 0.0f, 1.0f);
                outX += mt.dx * prog;
                outY += mt.dy * prog;
            }
        }
    }

    struct ActionNode {
        int parentIdx = -1;
        PlayerAction action;
    };

    struct SimPlayer {
        float px = 0.0f;
        float py = 105.0f;
        float vy = 0.0f;
        float vx = 5.193f;
        float gravDir = 1.0f;
        SimMode mode = SimMode::Cube;
        bool mini = false;
        bool grounded = true;
        bool dashing = false;
        bool holding = false;
        bool dead = false;
        int holdFramesRemaining = 0;
        int robotBoostFrames = 0;
        int lastTapFrame = -100;
        int lastOrb = -1;
        int lastPad = -1;
        int lastPortal = -1;
        int actionIdx = -1;

        uint32_t beamKey() const {
            uint32_t ky = static_cast<uint32_t>(static_cast<int>(std::round(py / 5.0f)) & 0x3FF);
            uint32_t kv = static_cast<uint32_t>(static_cast<int>(std::round(vy / 1.5f)) & 0x3F);
            uint32_t flags = (grounded ? 1U : 0U)
                           | ((gravDir < 0.0f ? 1U : 0U) << 1)
                           | ((holding ? 1U : 0U) << 2)
                           | ((dashing ? 1U : 0U) << 3)
                           | (static_cast<uint32_t>(mode) << 4);
            return (ky << 14) | (kv << 8) | flags;
        }
    };

    void stepSim(SimPlayer& s, const std::vector<LevelObj>& objs, bool wantHold, float groundY) {
        if (s.dead) return;
        float prevY = s.py;
        bool freshPress = wantHold && !s.holding;
        float playerHalfH = s.mini ? 9.0f : 15.0f;

        auto itStart = std::lower_bound(objs.begin(), objs.end(), s.px - 45.0f,
            [](const LevelObj& o, float val) { return o.x < val; });

        // 1. Check portals, S-blocks & pads
        for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
            const auto& o = *it;
            float ox, oy;
            evalObjPos(o, s.px, s.vx, ox, oy);

            if (std::abs(s.px - ox) < 16.0f && std::abs(s.py - oy) < std::max(28.0f, o.h * 0.6f)) {
                if (o.speedVal > 0.0f) s.vx = o.speedVal;
                if (o.isStopDash) s.dashing = false;
                if (o.isPortalShip) s.mode = SimMode::Ship;
                else if (o.isPortalCube) s.mode = SimMode::Cube;
                else if (o.isPortalBall) s.mode = SimMode::Ball;
                else if (o.isPortalUFO) s.mode = SimMode::UFO;
                else if (o.isPortalWave) s.mode = SimMode::Wave;
                else if (o.isPortalRobot) s.mode = SimMode::Robot;
                else if (o.isPortalSpider) s.mode = SimMode::Spider;

                if (o.isPortalMini) s.mini = true;
                else if (o.isPortalNormalSize) s.mini = false;

                if (o.isPortalGravNorm) s.gravDir = 1.0f;
                else if (o.isPortalGravInv) s.gravDir = -1.0f;
                else if (o.isPortalGravToggle && o.uid != s.lastPortal) {
                    s.lastPortal = o.uid;
                    s.gravDir = -s.gravDir;
                }
            }

            if (o.isPad && o.uid != s.lastPad && std::abs(s.px - ox) < 16.0f && std::abs(s.py - oy) < 20.0f) {
                s.lastPad = o.uid;
                s.dashing = false;
                s.robotBoostFrames = 0;
                float miniMult = s.mini ? 0.8f : 1.0f;
                if (o.isYellowPad) { s.vy = 14.35f * miniMult * s.gravDir; s.grounded = false; }
                else if (o.isPinkPad) { s.vy = 9.35f * miniMult * s.gravDir; s.grounded = false; }
                else if (o.isRedPad) { s.vy = 18.2f * miniMult * s.gravDir; s.grounded = false; }
                else if (o.isGravPad || o.isSpiderPad) { s.gravDir = -s.gravDir; s.vy = 7.5f * s.gravDir; s.grounded = false; }
            }
        }

        // 2. Check platform support when grounded
        if ((s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::Robot || s.mode == SimMode::Spider) && s.grounded) {
            if (s.gravDir > 0.0f && s.py > groundY + 1.0f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                    if (it->isSolid || it->isSlope) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        float oTop = oy + it->h * 0.5f;
                        if (std::abs(s.px - ox) <= it->w * 0.5f + 12.0f && std::abs((s.py - playerHalfH) - oTop) <= 4.5f) {
                            supported = true;
                            break;
                        }
                    }
                }
                if (!supported) s.grounded = false;
            } else if (s.gravDir < 0.0f && s.py < 390.0f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                    if (it->isSolid || it->isSlope) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        float oBot = oy - it->h * 0.5f;
                        if (std::abs(s.px - ox) <= it->w * 0.5f + 12.0f && std::abs((s.py + playerHalfH) - oBot) <= 4.5f) {
                            supported = true;
                            break;
                        }
                    }
                }
                if (!supported) s.grounded = false;
            }
        }

        // 3. Apply input
        if (!wantHold) {
            s.dashing = false;
            s.robotBoostFrames = 0;
        }
        if (wantHold) {
            bool hitOrb = false;
            float miniMult = s.mini ? 0.8f : 1.0f;
            for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                const auto& o = *it;
                if (!o.isOrb || o.uid == s.lastOrb) continue;
                float ox, oy;
                evalObjPos(o, s.px, s.vx, ox, oy);
                if (std::abs(s.px - ox) < 24.0f && std::abs(s.py - oy) < 24.0f) {
                    s.lastOrb = o.uid;
                    s.robotBoostFrames = 0;
                    if (o.isYellowOrb) s.vy = 10.062f * miniMult * s.gravDir;
                    else if (o.isPinkOrb) s.vy = 7.25f * miniMult * s.gravDir;
                    else if (o.isRedOrb) s.vy = 14.1f * miniMult * s.gravDir;
                    else if (o.isGravOrb || o.isSpiderOrb) { s.gravDir = -s.gravDir; s.vy = 7.0f * s.gravDir; }
                    else if (o.isGreenOrb) { s.gravDir = -s.gravDir; s.vy = 10.062f * miniMult * s.gravDir; }
                    else if (o.isDashOrb) {
                        if (o.id == 1751) s.gravDir = -s.gravDir;
                        s.vy = 0.0f;
                        s.dashing = true;
                    }
                    s.grounded = false;
                    hitOrb = true;
                    break;
                }
            }
            if (!hitOrb) {
                if (s.mode == SimMode::Cube && s.grounded) {
                    s.vy = 10.062f * miniMult * s.gravDir;
                    s.grounded = false;
                } else if (s.mode == SimMode::Robot) {
                    if (s.grounded && freshPress) {
                        s.vy = 7.5f * miniMult * s.gravDir;
                        s.grounded = false;
                        s.robotBoostFrames = 12;
                    } else if (s.robotBoostFrames > 0) {
                        s.vy = 9.8f * miniMult * s.gravDir;
                        s.robotBoostFrames--;
                    }
                } else if (s.mode == SimMode::Ball && s.grounded && freshPress) {
                    s.gravDir = -s.gravDir;
                    s.vy = 3.0f * s.gravDir;
                    s.grounded = false;
                } else if (s.mode == SimMode::Spider && s.grounded && freshPress) {
                    s.gravDir = -s.gravDir;
                    float targetY = (s.gravDir > 0.0f) ? groundY : 390.0f;
                    for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                        if (it->isSolid) {
                            float ox, oy;
                            evalObjPos(*it, s.px, s.vx, ox, oy);
                            if (std::abs(s.px - ox) <= it->w * 0.5f + 10.0f) {
                                if (s.gravDir < 0.0f) {
                                    float surf = oy - it->h * 0.5f - playerHalfH;
                                    if (surf > s.py && surf < targetY) targetY = surf;
                                } else {
                                    float surf = oy + it->h * 0.5f + playerHalfH;
                                    if (surf < s.py && surf > targetY) targetY = surf;
                                }
                            }
                        }
                    }
                    s.py = targetY;
                    s.vy = 0.0f;
                    s.grounded = true;
                } else if (s.mode == SimMode::UFO && freshPress) {
                    s.vy = 7.2f * miniMult * s.gravDir;
                    s.grounded = false;
                }
            }
        }

        // 4. Integrate motion
        const float gravity = -0.77614f;
        s.px += s.vx;
        if (s.dashing) {
            s.vy = 0.0f;
        } else if (s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::UFO || s.mode == SimMode::Robot || s.mode == SimMode::Spider) {
            if (!s.grounded) {
                s.py += s.vy;
                s.vy += gravity * s.gravDir;
                if (std::abs(s.vy) > 15.6f) s.vy = (s.vy > 0.0f) ? 15.6f : -15.6f;
                if (s.gravDir > 0.0f && s.py <= groundY) {
                    s.py = groundY;
                    s.vy = 0.0f;
                    s.grounded = true;
                }
            }
        } else if (s.mode == SimMode::Ship) {
            float accel = wantHold ? 0.45f : -0.45f;
            if (s.mini) accel *= 1.18f;
            s.vy += accel * s.gravDir;
            s.vy = std::clamp(s.vy, -7.0f, 7.0f);
            s.py += s.vy;
            if (s.py <= groundY) { s.py = groundY; s.vy = 0.0f; }
            else if (s.py >= 500.0f) { s.py = 500.0f; s.vy = 0.0f; }
        } else if (s.mode == SimMode::Wave) {
            float waveMult = s.mini ? 2.0f : 1.0f;
            s.vy = (wantHold ? 1.0f : -1.0f) * s.vx * waveMult * s.gravDir;
            s.py += s.vy;
            if (s.py <= groundY) { s.py = groundY; s.vy = 0.0f; }
            else if (s.py >= 500.0f) { s.py = 500.0f; s.vy = 0.0f; }
        }

        auto itAfter = std::lower_bound(objs.begin(), objs.end(), s.px - 45.0f,
            [](const LevelObj& o, float val) { return o.x < val; });

        // 5. Solid block & Slope landing
        for (auto it = itAfter; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
            if (!it->isSolid && !it->isSlope) continue;
            float ox, oy;
            evalObjPos(*it, s.px, s.vx, ox, oy);
            if (std::abs(s.px - ox) > it->w * 0.5f + 9.0f) continue;

            if (it->isSlope) {
                float oTop = oy + it->h * 0.5f;
                float oBot = oy - it->h * 0.5f;
                if (s.gravDir > 0.0f && s.py - playerHalfH <= oTop && s.py + playerHalfH >= oBot) {
                    float t = std::clamp((s.px - (ox - it->w * 0.5f)) / std::max(1.0f, it->w), 0.0f, 1.0f);
                    float surfY = oBot + t * it->h + playerHalfH;
                    if (s.py <= surfY + 4.0f) {
                        s.py = surfY;
                        if (s.vy < 4.5f) s.vy = 4.5f;
                        s.grounded = true;
                    }
                }
                continue;
            }

            if (s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::UFO || s.mode == SimMode::Robot || s.mode == SimMode::Spider || s.mode == SimMode::Ship) {
                if (s.gravDir > 0.0f && s.vy <= 0.1f) {
                    float oTop = oy + it->h * 0.5f;
                    if (s.py - playerHalfH <= oTop + 2.5f && prevY - playerHalfH >= oTop - 14.0f) {
                        s.py = oTop + playerHalfH;
                        s.vy = 0.0f;
                        s.grounded = true;
                    }
                } else if (s.gravDir < 0.0f && s.vy >= -0.1f) {
                    float oBot = oy - it->h * 0.5f;
                    if (s.py + playerHalfH >= oBot - 2.5f && prevY + playerHalfH <= oBot + 14.0f) {
                        s.py = oBot - playerHalfH;
                        s.vy = 0.0f;
                        s.grounded = true;
                    }
                }
            }
        }

        // 6. Accurate GD Inner-Hitbox Death checks
        for (auto it = itAfter; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
            if (!it->isSolid && !it->isHazard) continue;
            float ox, oy;
            evalObjPos(*it, s.px, s.vx, ox, oy);
            if (it->isSolid) {
                // GD uses a smaller inner hitbox for solid block crashes so standing on top/bottom never kills
                float crashW = it->w * 0.5f + 2.0f;
                float crashH = std::max(2.0f, it->h * 0.5f - 3.0f);
                if (std::abs(s.px - ox) < crashW && std::abs(s.py - oy) < crashH) {
                    s.dead = true;
                    s.holding = wantHold;
                    return;
                }
            } else if (it->isHazard) {
                float hazW = std::clamp(it->w * 0.45f, 4.0f, 12.0f) + 5.0f;
                float hazH = std::clamp(it->h * 0.45f, 4.0f, 14.0f) + 6.0f;
                if (std::abs(s.px - ox) < hazW && std::abs(s.py - oy) < hazH) {
                    s.dead = true;
                    s.holding = wantHold;
                    return;
                }
            }
        }

        s.holding = wantHold;
    }
}
#endif

void SolverEngine::solveMathematically(void* gameLayerPtr) {
#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<GJBaseGameLayer*>(gameLayerPtr);
    if (!layer || !layer->m_player1) {
        m_state = SolverState::Idle;
        return;
    }

    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
    if (MacroManager::get().hasSavedMacro(m_currentLevelId, collectCoins)) {
        auto saved = MacroManager::get().loadMacro(m_currentLevelId, collectCoins);
        if (saved.has_value()) {
            startPlaying(*saved);
            auto playLayer = PlayLayer::get();
            if (playLayer && playLayer->m_gameState.m_levelTime > 0.05) {
                playLayer->resetLevel();
            }
            return;
        }
    }

    // Pass 1: Collect Move Triggers (ID 901) & Toggle Triggers (ID 1049)
    std::unordered_map<int, std::vector<MoveTrigger>> groupMoveTriggers;
    std::unordered_map<int, float> disabledGroupX;

    if (layer->m_objects) {
        auto count = layer->m_objects->count();
        for (unsigned int i = 0; i < count; ++i) {
            auto obj = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
            if (!obj) continue;
            int oid = obj->m_objectID;
            if (oid == 901) {
                auto eff = static_cast<EffectGameObject*>(obj);
                if (!eff->m_isSpawnTriggered && !eff->m_isTouchTriggered && eff->m_targetGroupID > 0) {
                    MoveTrigger mt{};
                    mt.triggerX = obj->getPositionX();
                    mt.targetGroup = eff->m_targetGroupID;
                    mt.dx = eff->m_moveOffset.x;
                    mt.dy = eff->m_moveOffset.y;
                    mt.duration = std::max(0.0f, eff->m_duration);
                    groupMoveTriggers[mt.targetGroup].push_back(mt);
                }
            } else if (oid == 1049) {
                auto eff = static_cast<EffectGameObject*>(obj);
                if (!eff->m_isSpawnTriggered && !eff->m_isTouchTriggered && eff->m_targetGroupID > 0 && !eff->m_activateGroup) {
                    float tx = obj->getPositionX();
                    auto it = disabledGroupX.find(eff->m_targetGroupID);
                    if (it == disabledGroupX.end() || tx < it->second) {
                        disabledGroupX[eff->m_targetGroupID] = tx;
                    }
                }
            }
        }
    }

    // Pass 2: Parse all interactive & physical objects (filtering out Decoration!)
    std::vector<LevelObj> objects;
    if (layer->m_objects) {
        auto count = layer->m_objects->count();
        objects.reserve(count / 2 + 16);
        for (unsigned int i = 0; i < count; ++i) {
            auto obj = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
            if (!obj) continue;
            if (obj->m_isDisabled || obj->m_isDisabled2 || obj->m_isGroupDisabled) continue;

            int oid = obj->m_objectID;
            if (oid == 901 || oid == 1049) continue;

            auto type = obj->m_objectType;
            // Skip pure decoration objects so online levels with 20,000+ deco objects solve cleanly & fast
            if (type == GameObjectType::Decoration || type == GameObjectType::EnterEffectObject) {
                if (oid != 200 && oid != 201 && oid != 202 && oid != 203 && oid != 1334 && oid != 1829) {
                    continue;
                }
            }

            auto rect = obj->getObjectRect();
            float w = (rect.size.width > 0.1f) ? rect.size.width : 30.0f;
            float h = (rect.size.height > 0.1f) ? rect.size.height : 30.0f;
            float baseX = (rect.size.width > 0.1f) ? rect.getMidX() : obj->getPositionX();
            float baseY = (rect.size.height > 0.1f) ? rect.getMidY() : obj->getPositionY();

            bool toggledOff = false;
            std::vector<MoveTrigger> objMoves;
            if (obj->m_groups && obj->m_groupCount > 0) {
                int gCount = std::min<int>(obj->m_groupCount, 10);
                for (int g = 0; g < gCount; ++g) {
                    int gid = (*obj->m_groups)[g];
                    if (gid <= 0) continue;
                    auto dtIt = disabledGroupX.find(gid);
                    if (dtIt != disabledGroupX.end() && dtIt->second <= baseX + 15.0f) {
                        toggledOff = true;
                        break;
                    }
                    auto mtIt = groupMoveTriggers.find(gid);
                    if (mtIt != groupMoveTriggers.end()) {
                        for (const auto& mt : mtIt->second) {
                            objMoves.push_back(mt);
                        }
                    }
                }
            }
            if (toggledOff) continue;

            LevelObj lo{};
            lo.uid = static_cast<int>(i);
            lo.id = oid;
            lo.baseX = baseX;
            lo.baseY = baseY;
            lo.x = baseX;
            lo.y = baseY;
            lo.w = w;
            lo.h = (oid == 40) ? 15.0f : h;
            lo.moveTriggers = std::move(objMoves);

            if (!lo.moveTriggers.empty()) {
                float estX, estY;
                evalObjPos(lo, baseX, 5.193f, estX, estY);
                lo.x = estX;
                lo.y = estY;
            }

            lo.isSlope = (type == GameObjectType::Slope && !obj->m_slopeIsHazard);
            lo.isHazard = (type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard ||
                           (type == GameObjectType::Slope && obj->m_slopeIsHazard) ||
                           oid == 8 || oid == 9 || oid == 39 || oid == 61 || oid == 62 || oid == 103);
            lo.isSolid = (type == GameObjectType::Solid);
            lo.isStopDash = (oid == 1829);

            lo.isYellowPad = (type == GameObjectType::YellowJumpPad || oid == 35);
            lo.isPinkPad = (type == GameObjectType::PinkJumpPad || oid == 140);
            lo.isRedPad = (type == GameObjectType::RedJumpPad || oid == 1332);
            lo.isGravPad = (type == GameObjectType::GravityPad || oid == 67);
            lo.isSpiderPad = (type == GameObjectType::SpiderPad || oid == 3005);
            lo.isPad = lo.isYellowPad || lo.isPinkPad || lo.isRedPad || lo.isGravPad || lo.isSpiderPad;

            lo.isYellowOrb = (type == GameObjectType::YellowJumpRing || oid == 36);
            lo.isPinkOrb = (type == GameObjectType::PinkJumpRing || oid == 141);
            lo.isRedOrb = (type == GameObjectType::RedJumpRing || oid == 1333);
            lo.isGravOrb = (type == GameObjectType::GravityRing || oid == 84);
            lo.isGreenOrb = (type == GameObjectType::GreenRing || oid == 1022);
            lo.isDashOrb = (type == GameObjectType::DashRing || type == GameObjectType::GravityDashRing || oid == 1704 || oid == 1751);
            lo.isSpiderOrb = (type == GameObjectType::SpiderOrb || oid == 3004);
            lo.isOrb = lo.isYellowOrb || lo.isPinkOrb || lo.isRedOrb || lo.isGravOrb || lo.isGreenOrb || lo.isDashOrb || lo.isSpiderOrb;

            lo.isPortalCube = (type == GameObjectType::CubePortal || oid == 12);
            lo.isPortalShip = (type == GameObjectType::ShipPortal || oid == 13);
            lo.isPortalBall = (type == GameObjectType::BallPortal || oid == 47);
            lo.isPortalUFO = (type == GameObjectType::UfoPortal || oid == 111);
            lo.isPortalWave = (type == GameObjectType::WavePortal || oid == 660);
            lo.isPortalRobot = (type == GameObjectType::RobotPortal || oid == 745);
            lo.isPortalSpider = (type == GameObjectType::SpiderPortal || oid == 1331);
            lo.isPortalGravNorm = (type == GameObjectType::NormalGravityPortal || oid == 10);
            lo.isPortalGravInv = (type == GameObjectType::InverseGravityPortal || oid == 11);
            lo.isPortalGravToggle = (type == GameObjectType::GravityTogglePortal || oid == 2926);
            lo.isPortalMini = (type == GameObjectType::MiniSizePortal || oid == 101);
            lo.isPortalNormalSize = (type == GameObjectType::RegularSizePortal || oid == 99);

            lo.speedVal = 0.0f;
            if (oid == 200) lo.speedVal = 4.186f;
            else if (oid == 201) lo.speedVal = 5.193f;
            else if (oid == 202) lo.speedVal = 6.456f;
            else if (oid == 203) lo.speedVal = 7.800f;
            else if (oid == 1334) lo.speedVal = 9.600f;

            bool isPortal = lo.isPortalCube || lo.isPortalShip || lo.isPortalBall || lo.isPortalUFO ||
                            lo.isPortalWave || lo.isPortalRobot || lo.isPortalSpider ||
                            lo.isPortalGravNorm || lo.isPortalGravInv || lo.isPortalGravToggle ||
                            lo.isPortalMini || lo.isPortalNormalSize;

            if (!lo.isSolid && !lo.isSlope && !lo.isHazard && !lo.isPad && !lo.isOrb && !isPortal && !lo.isStopDash && lo.speedVal == 0.0f) {
                continue;
            }

            objects.push_back(std::move(lo));
        }
    }
    std::sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) { return a.x < b.x; });

    float groundY = 105.0f;
    float maxLevelX = objects.empty() ? 5000.0f : (objects.back().x + 300.0f);

    SimPlayer initial;
    auto p1 = layer->m_player1;
    initial.px = std::max(0.0f, p1->getPositionX());
    initial.py = std::max(groundY, p1->getPositionY());
    initial.gravDir = p1->m_isUpsideDown ? -1.0f : 1.0f;
    if (p1->m_isShip) initial.mode = SimMode::Ship;
    else if (p1->m_isBall) initial.mode = SimMode::Ball;
    else if (p1->m_isBird) initial.mode = SimMode::UFO;
    else if (p1->m_isDart) initial.mode = SimMode::Wave;
    else if (p1->m_isRobot) initial.mode = SimMode::Robot;
    else if (p1->m_isSpider) initial.mode = SimMode::Spider;

    if (p1->m_playerSpeed <= 0.75f) initial.vx = 4.186f;
    else if (p1->m_playerSpeed <= 0.95f) initial.vx = 5.193f;
    else if (p1->m_playerSpeed <= 1.15f) initial.vx = 6.456f;
    else if (p1->m_playerSpeed <= 1.40f) initial.vx = 7.800f;
    else initial.vx = 9.600f;

    // Parallel Wavefront Beam Search across the entire level!
    std::vector<ActionNode> actionPool;
    actionPool.reserve(32768);

    auto recordTransition = [&](SimPlayer& s, size_t f, float xAtAction, bool newHold) {
        if (newHold != s.holding) {
            int newIdx = static_cast<int>(actionPool.size());
            // Set yPos = 0.0f for solver-generated actions so real 240Hz GD physics controls vertical arc
            actionPool.push_back({s.actionIdx, PlayerAction{f, xAtAction, 1, newHold, false, 0.0f}});
            s.actionIdx = newIdx;
        }
    };

    std::vector<SimPlayer> beam;
    beam.push_back(initial);

    SimPlayer bestOverall = initial;
    size_t bestFrame = 0;

    const size_t maxFrames = 18000;
    const size_t maxBeamWidth = 48;

    for (size_t frame = 1; frame <= maxFrames; ++frame) {
        if (beam.empty()) break;
        if (beam[0].px >= maxLevelX) {
            bestOverall = beam[0];
            bestFrame = frame;
            break;
        }

        std::vector<SimPlayer> nextBeam;
        nextBeam.reserve(maxBeamWidth * 2);
        std::unordered_set<uint32_t> seenBuckets;
        seenBuckets.reserve(maxBeamWidth * 2);

        for (const auto& cur : beam) {
            bool inOrb = false;
            bool inDashOrb = false;
            auto it = std::lower_bound(objects.begin(), objects.end(), cur.px - 35.0f,
                [](const LevelObj& o, float val) { return o.x < val; });
            for (; it != objects.end() && it->x <= cur.px + 35.0f; ++it) {
                if (it->isOrb && it->uid != cur.lastOrb) {
                    float ox, oy;
                    evalObjPos(*it, cur.px, cur.vx, ox, oy);
                    if (std::abs(cur.px - ox) < 24.0f && std::abs(cur.py - oy) < 24.0f) {
                        inOrb = true;
                        if (it->isDashOrb) inDashOrb = true;
                        break;
                    }
                }
            }

            if (cur.mode == SimMode::Cube || cur.mode == SimMode::Ball || cur.mode == SimMode::Robot || cur.mode == SimMode::Spider) {
                // Branch A: Default continuation (release after holdFramesRemaining expires, or keep holding if dashing)
                {
                    SimPlayer cNo = cur;
                    bool holdNo = cNo.dashing;
                    if (!cNo.dashing && cNo.holdFramesRemaining > 0) {
                        holdNo = true;
                        cNo.holdFramesRemaining--;
                    }
                    // If an unactivated orb is right ahead, ensure we release 1 frame early so we can tap the orb
                    if (inOrb && cNo.holding && !cNo.dashing) {
                        holdNo = false;
                        cNo.holdFramesRemaining = 0;
                    }
                    float actX = cNo.px;
                    recordTransition(cNo, frame, actX, holdNo);
                    stepSim(cNo, objects, holdNo, groundY);
                    if (!cNo.dead) {
                        uint32_t k = cNo.beamKey();
                        if (seenBuckets.insert(k).second) {
                            nextBeam.push_back(cNo);
                        }
                    }
                }

                // Branch B: Active decision (Jump from ground, Activate Orb, Extend Robot jump, or Release Dash)
                bool canJumpOrOrb = (cur.grounded || inOrb) && !cur.holding && (static_cast<int>(frame) - cur.lastTapFrame >= 2);
                bool canRobotHold = (cur.mode == SimMode::Robot && cur.robotBoostFrames > 0 && cur.holding);
                bool canReleaseDash = cur.dashing;

                if (canJumpOrOrb || canRobotHold || canReleaseDash) {
                    SimPlayer cYes = cur;
                    bool holdYes = !canReleaseDash;
                    if (holdYes) {
                        cYes.lastTapFrame = static_cast<int>(frame);
                        cYes.holdFramesRemaining = inDashOrb ? 0 : 2;
                    }
                    float actX = cYes.px;
                    recordTransition(cYes, frame, actX, holdYes);
                    stepSim(cYes, objects, holdYes, groundY);
                    if (!cYes.dead) {
                        uint32_t k = cYes.beamKey();
                        if (seenBuckets.insert(k).second) {
                            nextBeam.push_back(cYes);
                        }
                    }
                }
            } else {
                // Ship / UFO / Wave: explore both Hold and Release branches
                bool tryBoth = (cur.mode == SimMode::Wave) || (cur.mode == SimMode::Ship) ||
                               (cur.mode == SimMode::UFO && (!cur.holding && static_cast<int>(frame) - cur.lastTapFrame >= 3));

                // Prefer keeping current hold state first, then toggling on decision frames
                bool firstChoice = cur.holding;
                if (cur.mode == SimMode::UFO) firstChoice = false;

                for (int b = 0; b < (tryBoth ? 2 : 1); ++b) {
                    bool wantHold = (b == 0) ? firstChoice : !firstChoice;
                    SimPlayer cBranch = cur;
                    if (wantHold && !cBranch.holding) {
                        cBranch.lastTapFrame = static_cast<int>(frame);
                    }
                    float actX = cBranch.px;
                    recordTransition(cBranch, frame, actX, wantHold);
                    stepSim(cBranch, objects, wantHold, groundY);
                    if (!cBranch.dead) {
                        uint32_t k = cBranch.beamKey();
                        if (seenBuckets.insert(k).second) {
                            nextBeam.push_back(cBranch);
                        }
                    }
                }
            }
        }

        if (nextBeam.empty()) break;

        // If nextBeam exceeds maxBeamWidth, keep a diverse spread of heights + grounded states
        if (nextBeam.size() > maxBeamWidth) {
            std::sort(nextBeam.begin(), nextBeam.end(), [](const SimPlayer& a, const SimPlayer& b) {
                if (a.grounded != b.grounded) return a.grounded > b.grounded;
                return std::abs(a.py - 180.0f) < std::abs(b.py - 180.0f);
            });
            nextBeam.resize(maxBeamWidth);
        }

        beam = std::move(nextBeam);
        if (beam[0].px > bestOverall.px) {
            bestOverall = beam[0];
            bestFrame = frame;
        }
    }

    // Reconstruct chronological action sequence from bestOverall.actionIdx
    std::vector<PlayerAction> actions;
    int curIdx = bestOverall.actionIdx;
    while (curIdx >= 0 && curIdx < static_cast<int>(actionPool.size())) {
        actions.push_back(actionPool[curIdx].action);
        curIdx = actionPool[curIdx].parentIdx;
    }
    std::reverse(actions.begin(), actions.end());

    // Guarantee the button is NEVER left held down at the end of the macro!
    if (!actions.empty() && actions.back().isPush) {
        const auto& lastAct = actions.back();
        actions.push_back({lastAct.frame + 3, lastAct.xPos + 15.0f, 1, false, false, 0.0f});
    }

    MacroData winMacro;
    winMacro.levelId = m_currentLevelId;
    winMacro.levelName = m_currentLevelName;
    winMacro.completionPercentage = std::min(100.0f, (bestOverall.px / std::max(1.0f, maxLevelX)) * 100.0f);
    winMacro.totalFrames = bestFrame + 100;
    winMacro.attemptsToSolve = 1;
    winMacro.actions = actions;

    if (bestOverall.px >= maxLevelX - 150.0f && !actions.empty()) {
        winMacro.completionPercentage = 100.0f;
        MacroManager::get().saveMacro(winMacro, collectCoins);
    }

    startPlaying(winMacro);

    Notification::create("AI AutoPlay: Level Solved! Playing run...", NotificationIcon::Success)->show();
    auto playLayer = PlayLayer::get();
    if (playLayer && playLayer->m_gameState.m_levelTime > 0.05) {
        playLayer->resetLevel();
    }
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
    Notification::create("AI AutoPlay: Playing Macro", NotificationIcon::Success)->show();
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
    // Strictly O(1) macro playback during gameplay — ZERO object scanning or mid-game calculations!
    if (m_state != SolverState::Playing) {
        return;
    }

#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<GJBaseGameLayer*>(gameLayerPtr);
    if (!layer || !layer->m_player1) return;

    auto p1 = layer->m_player1;
    if (p1->m_isDead) return;

    float currentX = p1->getPositionX();
    float currentY = p1->getPositionY();
    m_currentX = currentX;

    if (m_playbackIndex < m_activeMacro.actions.size()) {
        float levelTime = layer->m_gameState.m_levelTime;
        bool actedP1 = false;
        bool actedP2 = false;

        while (m_playbackIndex < m_activeMacro.actions.size()) {
            const auto& action = m_activeMacro.actions[m_playbackIndex];
            bool shouldTrigger = false;
            if (action.xPos > 0.0f) {
                shouldTrigger = (currentX >= action.xPos - 0.25f);
            } else {
                float physicsFrame = levelTime * 60.0f;
                shouldTrigger = (physicsFrame >= static_cast<float>(action.frame));
            }

            if (shouldTrigger) {
                if (action.isPlayer2) {
                    if (actedP2) break;
                } else {
                    if (actedP1) break;
                }

                // Input Fix: synchronize exact (X, Y) coordinates on pre-recorded 240Hz frameFixes
                if (action.xPos > 0.0f && action.yPos > 0.0f && !action.isPlayer2) {
                    float dx = std::abs(currentX - action.xPos);
                    float dy = std::abs(currentY - action.yPos);
                    if (dx < 15.0f && dy < 35.0f) {
                        p1->setPosition({action.xPos, action.yPos});
                        currentX = action.xPos;
                        currentY = action.yPos;
                    }
                }

                layer->handleButton(action.isPush, action.button, !action.isPlayer2);
                if (!action.isPlayer2) {
                    m_isHolding = action.isPush;
                    actedP1 = true;
                } else {
                    actedP2 = true;
                }
                m_playbackIndex++;
            } else {
                break;
            }
        }
    }
    m_currentFrame++;
#else
    m_currentFrame++;
#endif
}

void SolverEngine::onPlayerDeath(void* playerPtr) {
    m_isHolding = false;
}

void SolverEngine::onLevelComplete() {
    m_state = SolverState::Idle;
    m_isHolding = false;
}
