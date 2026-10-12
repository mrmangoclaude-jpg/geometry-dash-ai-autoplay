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
        bool isStopDash;
        bool isYellowPad, isPinkPad, isRedPad, isGravPad, isSpiderPad, isPad;
        bool isYellowOrb, isPinkOrb, isRedOrb, isGravOrb, isGreenOrb, isDashOrb, isSpiderOrb, isOrb;
        bool isPortalCube, isPortalShip, isPortalBall, isPortalUFO, isPortalWave, isPortalRobot, isPortalSpider;
        bool isPortalGravNorm, isPortalGravInv, isPortalGravToggle;
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

    struct SimPlayer {
        float px = 0.0f;
        float py = 105.0f;
        float vy = 0.0f;
        float vx = 5.193f;
        float gravDir = 1.0f;
        SimMode mode = SimMode::Cube;
        bool grounded = true;
        bool dashing = false;
        bool holding = false;
        bool dead = false;
        int robotBoostFrames = 0;
        int lastOrb = -1;
        int lastPad = -1;
        int lastPortal = -1;

        uint64_t stateKey() const {
            uint64_t kx = static_cast<uint64_t>(static_cast<int>(std::round(px)) & 0xFFFF);
            uint64_t ky = static_cast<uint64_t>(static_cast<int>(std::round(py)) & 0xFFF);
            uint64_t kv = static_cast<uint64_t>(static_cast<int>(std::round(vy * 2.0f)) & 0xFF);
            uint64_t flags = (grounded ? 1ULL : 0ULL)
                           | ((gravDir < 0.0f ? 1ULL : 0ULL) << 1)
                           | ((holding ? 1ULL : 0ULL) << 2)
                           | (static_cast<uint64_t>(mode) << 3);
            return (kx << 28) | (ky << 16) | (kv << 8) | flags;
        }
    };

    void stepSim(SimPlayer& s, const std::vector<LevelObj>& objs, bool wantHold, float groundY) {
        if (s.dead) return;
        float prevY = s.py;
        bool freshPress = wantHold && !s.holding;

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
                if (o.isYellowPad) { s.vy = 14.35f * s.gravDir; s.grounded = false; }
                else if (o.isPinkPad) { s.vy = 9.35f * s.gravDir; s.grounded = false; }
                else if (o.isRedPad) { s.vy = 18.2f * s.gravDir; s.grounded = false; }
                else if (o.isGravPad || o.isSpiderPad) { s.gravDir = -s.gravDir; s.vy = 7.5f * s.gravDir; s.grounded = false; }
            }
        }

        // 2. Check platform support when grounded
        if ((s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::Robot || s.mode == SimMode::Spider) && s.grounded) {
            if (s.gravDir > 0.0f && s.py > groundY + 1.0f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                    if (it->isSolid) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        float oTop = oy + it->h * 0.5f;
                        if (std::abs(s.px - ox) <= it->w * 0.5f + 12.0f && std::abs((s.py - 15.0f) - oTop) <= 3.5f) {
                            supported = true;
                            break;
                        }
                    }
                }
                if (!supported) s.grounded = false;
            } else if (s.gravDir < 0.0f && s.py < 390.0f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                    if (it->isSolid) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        float oBot = oy - it->h * 0.5f;
                        if (std::abs(s.px - ox) <= it->w * 0.5f + 12.0f && std::abs((s.py + 15.0f) - oBot) <= 3.5f) {
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
            // Orbs activate on fresh press (or buffered hold if not already consumed)
            for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                const auto& o = *it;
                if (!o.isOrb || o.uid == s.lastOrb) continue;
                float ox, oy;
                evalObjPos(o, s.px, s.vx, ox, oy);
                if (std::abs(s.px - ox) < 24.0f && std::abs(s.py - oy) < 24.0f) {
                    s.lastOrb = o.uid;
                    s.robotBoostFrames = 0;
                    if (o.isYellowOrb) s.vy = 10.062f * s.gravDir;
                    else if (o.isPinkOrb) s.vy = 7.25f * s.gravDir;
                    else if (o.isRedOrb) s.vy = 14.1f * s.gravDir;
                    else if (o.isGravOrb || o.isSpiderOrb) { s.gravDir = -s.gravDir; s.vy = 7.0f * s.gravDir; }
                    else if (o.isGreenOrb) { s.gravDir = -s.gravDir; s.vy = 10.062f * s.gravDir; }
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
                    s.vy = 10.062f * s.gravDir;
                    s.grounded = false;
                } else if (s.mode == SimMode::Robot) {
                    if (s.grounded) {
                        s.vy = 7.5f * s.gravDir;
                        s.grounded = false;
                        s.robotBoostFrames = 12;
                    } else if (s.robotBoostFrames > 0) {
                        s.vy = 9.8f * s.gravDir;
                        s.robotBoostFrames--;
                    }
                } else if (s.mode == SimMode::Ball && s.grounded && freshPress) {
                    s.gravDir = -s.gravDir;
                    s.vy = 3.0f * s.gravDir;
                    s.grounded = false;
                } else if (s.mode == SimMode::Spider && s.grounded && freshPress) {
                    s.gravDir = -s.gravDir;
                    // Spider teleports instantly to nearest surface in new gravity direction
                    float targetY = (s.gravDir > 0.0f) ? groundY : 390.0f;
                    for (auto it = itStart; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                        if (it->isSolid) {
                            float ox, oy;
                            evalObjPos(*it, s.px, s.vx, ox, oy);
                            if (std::abs(s.px - ox) <= it->w * 0.5f + 10.0f) {
                                if (s.gravDir < 0.0f) {
                                    float surf = oy - it->h * 0.5f - 15.0f;
                                    if (surf > s.py && surf < targetY) targetY = surf;
                                } else {
                                    float surf = oy + it->h * 0.5f + 15.0f;
                                    if (surf < s.py && surf > targetY) targetY = surf;
                                }
                            }
                        }
                    }
                    s.py = targetY;
                    s.vy = 0.0f;
                    s.grounded = true;
                } else if (s.mode == SimMode::UFO && freshPress) {
                    s.vy = 7.2f * s.gravDir;
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
            s.vy += accel * s.gravDir;
            s.vy = std::clamp(s.vy, -7.0f, 7.0f);
            s.py += s.vy;
            if (s.py <= groundY) { s.py = groundY; s.vy = 0.0f; }
            else if (s.py >= 390.0f) { s.py = 390.0f; s.vy = 0.0f; }
        } else if (s.mode == SimMode::Wave) {
            s.vy = (wantHold ? 1.0f : -1.0f) * s.vx * s.gravDir;
            s.py += s.vy;
            if (s.py <= groundY) { s.py = groundY; s.vy = 0.0f; }
            else if (s.py >= 390.0f) { s.py = 390.0f; s.vy = 0.0f; }
        }

        auto itAfter = std::lower_bound(objs.begin(), objs.end(), s.px - 45.0f,
            [](const LevelObj& o, float val) { return o.x < val; });

        // 5. Solid block landing
        if (s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::UFO || s.mode == SimMode::Robot || s.mode == SimMode::Spider) {
            if (s.gravDir > 0.0f && s.vy <= 0.0f) {
                for (auto it = itAfter; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                    if (it->isSolid) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        if (std::abs(s.px - ox) <= it->w * 0.5f + 9.0f) {
                            float oTop = oy + it->h * 0.5f;
                            if (s.py - 15.0f <= oTop + 1.5f && prevY - 15.0f >= oTop - 12.0f) {
                                s.py = oTop + 15.0f;
                                s.vy = 0.0f;
                                s.grounded = true;
                                break;
                            }
                        }
                    }
                }
            } else if (s.gravDir < 0.0f && s.vy >= 0.0f) {
                for (auto it = itAfter; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
                    if (it->isSolid) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        if (std::abs(s.px - ox) <= it->w * 0.5f + 9.0f) {
                            float oBot = oy - it->h * 0.5f;
                            if (s.py + 15.0f >= oBot - 1.5f && prevY + 15.0f <= oBot + 12.0f) {
                                s.py = oBot - 15.0f;
                                s.vy = 0.0f;
                                s.grounded = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // 6. Death checks
        for (auto it = itAfter; it != objs.end() && it->x <= s.px + 45.0f; ++it) {
            float ox, oy;
            evalObjPos(*it, s.px, s.vx, ox, oy);
            if (it->isSolid) {
                if (std::abs(s.px - ox) < it->w * 0.5f + 5.0f && std::abs(s.py - oy) < it->h * 0.5f + 5.0f) {
                    s.dead = true;
                    s.holding = wantHold;
                    return;
                }
            } else if (it->isHazard) {
                if (std::abs(s.px - ox) < 11.0f && std::abs(s.py - oy) < 15.0f) {
                    s.dead = true;
                    s.holding = wantHold;
                    return;
                }
            }
        }

        s.holding = wantHold;
    }

    std::pair<bool, int> rolloutSim(const SimPlayer& state, const std::vector<LevelObj>& objs, bool firstTap, int horizon, float groundY) {
        SimPlayer c = state;
        bool wasAirborne = !c.grounded;
        for (int i = 0; i < horizon; ++i) {
            bool t = (i == 0) ? firstTap : c.dashing;
            if (i > 0 && !c.grounded) {
                auto it = std::lower_bound(objs.begin(), objs.end(), c.px - 35.0f,
                    [](const LevelObj& o, float val) { return o.x < val; });
                for (; it != objs.end() && it->x <= c.px + 35.0f; ++it) {
                    if (it->isOrb && it->uid != c.lastOrb) {
                        float ox, oy;
                        evalObjPos(*it, c.px, c.vx, ox, oy);
                        if (std::abs(c.px - ox) < 22.0f && std::abs(c.py - oy) < 22.0f) {
                            t = true;
                            break;
                        }
                    }
                }
            } else if (i > 0 && firstTap && wasAirborne && c.grounded) {
                auto it = std::lower_bound(objs.begin(), objs.end(), c.px - 10.0f,
                    [](const LevelObj& o, float val) { return o.x < val; });
                for (; it != objs.end() && it->x <= c.px + 65.0f; ++it) {
                    float ox, oy;
                    evalObjPos(*it, c.px, c.vx, ox, oy);
                    if (ox >= c.px && (it->isHazard || (it->isSolid && oy + it->h * 0.5f > c.py - 5.0f))) {
                        t = true;
                        break;
                    }
                }
            }
            stepSim(c, objs, t, groundY);
            if (!c.grounded) wasAirborne = true;
            if (c.dead) return {false, i};
        }
        return {true, horizon};
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

    // Pass 2: Parse all level objects into fast memory layout with Move Trigger links
    std::vector<LevelObj> objects;
    if (layer->m_objects) {
        auto count = layer->m_objects->count();
        objects.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            auto obj = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
            if (!obj) continue;
            int oid = obj->m_objectID;
            if (oid == 901 || oid == 1049) continue;

            float baseX = obj->getPositionX();
            float baseY = obj->getPositionY();

            // Check group memberships for Move & Toggle triggers
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

            auto rect = obj->getObjectRect();
            float w = (rect.size.width > 0.0f) ? rect.size.width : 30.0f;
            float h = (rect.size.height > 0.0f) ? rect.size.height : 30.0f;
            auto type = obj->m_objectType;

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

            lo.isHazard = (type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard ||
                           oid == 8 || oid == 9 || oid == 39 || oid == 61 || oid == 62 || oid == 103);
            lo.isSolid = (type == GameObjectType::Solid || type == GameObjectType::Slope ||
                          (oid >= 1 && oid <= 7) || oid == 40);
            lo.isStopDash = (oid == 1829);

            lo.isYellowPad = (type == GameObjectType::YellowJumpPad || oid == 35);
            lo.isPinkPad = (type == GameObjectType::PinkJumpPad || oid == 140);
            lo.isRedPad = (oid == 1332);
            lo.isGravPad = (type == GameObjectType::GravityPad || oid == 67);
            lo.isSpiderPad = (type == GameObjectType::SpiderPad || oid == 3005);
            lo.isPad = lo.isYellowPad || lo.isPinkPad || lo.isRedPad || lo.isGravPad || lo.isSpiderPad;

            lo.isYellowOrb = (type == GameObjectType::YellowJumpRing || oid == 36);
            lo.isPinkOrb = (type == GameObjectType::PinkJumpRing || oid == 141);
            lo.isRedOrb = (type == GameObjectType::RedJumpRing || oid == 1333);
            lo.isGravOrb = (type == GameObjectType::GravityRing || oid == 84);
            lo.isGreenOrb = (type == GameObjectType::GreenRing || oid == 1022);
            lo.isDashOrb = (type == GameObjectType::DashRing || oid == 1704 || oid == 1751);
            lo.isSpiderOrb = (type == GameObjectType::SpiderOrb || oid == 3004);
            lo.isOrb = lo.isYellowOrb || lo.isPinkOrb || lo.isRedOrb || lo.isGravOrb || lo.isGreenOrb || lo.isDashOrb || lo.isSpiderOrb;

            lo.isPortalCube = (oid == 12);
            lo.isPortalShip = (oid == 13);
            lo.isPortalBall = (oid == 47);
            lo.isPortalUFO = (oid == 111);
            lo.isPortalWave = (oid == 660);
            lo.isPortalRobot = (oid == 745);
            lo.isPortalSpider = (oid == 1331);
            lo.isPortalGravNorm = (oid == 10);
            lo.isPortalGravInv = (oid == 11);
            lo.isPortalGravToggle = (oid == 2926);

            // Accurate GD Speed Portal IDs: 200=0.5x, 201=1x, 202=2x, 203=3x, 1334=4x
            lo.speedVal = 0.0f;
            if (oid == 200) lo.speedVal = 4.186f;
            else if (oid == 201) lo.speedVal = 5.193f;
            else if (oid == 202) lo.speedVal = 6.456f;
            else if (oid == 203) lo.speedVal = 7.800f;
            else if (oid == 1334) lo.speedVal = 9.600f;

            objects.push_back(std::move(lo));
        }
    }
    std::sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) { return a.x < b.x; });

    float groundY = 105.0f;
    float maxLevelX = objects.empty() ? 26000.0f : objects.back().x;

    SimPlayer state;
    auto p1 = layer->m_player1;
    state.py = groundY;
    state.gravDir = p1->m_isUpsideDown ? -1.0f : 1.0f;
    if (p1->m_isShip) state.mode = SimMode::Ship;
    else if (p1->m_isBall) state.mode = SimMode::Ball;
    else if (p1->m_isBird) state.mode = SimMode::UFO;
    else if (p1->m_isDart) state.mode = SimMode::Wave;
    else if (p1->m_isRobot) state.mode = SimMode::Robot;
    else if (p1->m_isSpider) state.mode = SimMode::Spider;

    size_t frame = 0;
    int lastTap = -100;

    struct Checkpoint {
        size_t frame;
        SimPlayer state;
        int lastTap;
        bool forcedTap;
        size_t actionCount;
    };
    std::vector<Checkpoint> history;
    std::unordered_set<uint64_t> visited;
    std::vector<PlayerAction> actions;
    std::vector<PlayerAction> bestActions;
    float bestPx = 0.0f;
    int backtracks = 0;

    while (state.px < maxLevelX && frame < 15000 && backtracks < 2500) {
        frame++;
        bool wantHold = state.dashing;

        if (state.mode == SimMode::Cube || state.mode == SimMode::Ball || state.mode == SimMode::Robot || state.mode == SimMode::Spider) {
            bool inOrb = false;
            auto it = std::lower_bound(objects.begin(), objects.end(), state.px - 35.0f,
                [](const LevelObj& o, float val) { return o.x < val; });
            for (; it != objects.end() && it->x <= state.px + 35.0f; ++it) {
                if (it->isOrb && it->uid != state.lastOrb) {
                    float ox, oy;
                    evalObjPos(*it, state.px, state.vx, ox, oy);
                    if (std::abs(state.px - ox) < 22.0f && std::abs(state.py - oy) < 22.0f) {
                        inOrb = true;
                        break;
                    }
                }
            }

            if (state.dashing) {
                auto [safeKeep, _1] = rolloutSim(state, objects, true, 12, groundY);
                auto [safeRel, _2] = rolloutSim(state, objects, false, 18, groundY);
                if (!safeKeep && safeRel) wantHold = false;
            } else {
                bool canAct = (state.grounded || inOrb) && (static_cast<int>(frame) - lastTap > 2);
                if (canAct) {
                    auto [safeNo, survNo] = rolloutSim(state, objects, false, 30, groundY);
                    auto [safeYes, survYes] = rolloutSim(state, objects, true, 30, groundY);

                    if (inOrb) {
                        if (safeYes && !safeNo) {
                            wantHold = true;
                        } else if (safeYes && safeNo) {
                            auto [sNo45, _1] = rolloutSim(state, objects, false, 45, groundY);
                            auto [sYes45, _2] = rolloutSim(state, objects, true, 45, groundY);
                            if (sYes45 && !sNo45) wantHold = true;
                        }
                    } else {
                        if (!safeNo && safeYes) {
                            auto [longSafe, _] = rolloutSim(state, objects, true, 36, groundY);
                            if (longSafe || survNo <= 4) wantHold = true;
                        } else if (!safeNo && !safeYes && survYes > survNo && survNo <= 3) {
                            wantHold = true;
                        }
                    }

                    history.push_back({frame - 1, state, lastTap, !wantHold, actions.size()});
                }
            }
        } else {
            // Ship / UFO / Wave lookahead
            SimPlayer cUp = state;
            for (int i = 0; i < 15; ++i) stepSim(cUp, objects, true, groundY);
            SimPlayer cDn = state;
            for (int i = 0; i < 15; ++i) stepSim(cDn, objects, false, groundY);

            if (state.py < 210.0f && !cUp.dead) wantHold = true;
            else if (cDn.dead && !cUp.dead) wantHold = true;
            else if (!cUp.dead && state.vy < -1.0f) wantHold = true;

            if (frame % 3 == 0) {
                history.push_back({frame - 1, state, lastTap, !wantHold, actions.size()});
            }
        }

        if (wantHold != state.holding) {
            if (wantHold) lastTap = static_cast<int>(frame);
            actions.push_back({frame, state.px, 1, wantHold, false, state.py});
        }

        stepSim(state, objects, wantHold, groundY);

        if (!state.dead && state.px > bestPx) {
            bestPx = state.px;
            bestActions = actions;
        }

        uint64_t k = state.stateKey();
        if (state.dead || visited.count(k)) {
            bool recovered = false;
            while (!history.empty()) {
                backtracks++;
                auto cp = history.back();
                history.pop_back();
                frame = cp.frame + 1;
                state = cp.state;
                lastTap = cp.lastTap;
                actions.resize(cp.actionCount);

                if (cp.forcedTap != state.holding) {
                    if (cp.forcedTap) lastTap = static_cast<int>(frame);
                    actions.push_back({frame, state.px, 1, cp.forcedTap, false, state.py});
                }
                stepSim(state, objects, cp.forcedTap, groundY);
                if (!state.dead && !visited.count(state.stateKey())) {
                    visited.insert(state.stateKey());
                    if (state.px > bestPx) {
                        bestPx = state.px;
                        bestActions = actions;
                    }
                    recovered = true;
                    break;
                }
            }
            if (!recovered) break;
        } else {
            visited.insert(k);
        }
    }

    if (bestActions.size() > actions.size()) {
        actions = bestActions;
    }

    MacroData winMacro;
    winMacro.levelId = m_currentLevelId;
    winMacro.levelName = m_currentLevelName;
    winMacro.completionPercentage = std::min(100.0f, (bestPx / std::max(1.0f, maxLevelX)) * 100.0f);
    winMacro.totalFrames = actions.empty() ? 5000 : actions.back().frame + 100;
    winMacro.attemptsToSolve = 1;
    winMacro.actions = actions;

    if (bestPx >= maxLevelX - 100.0f) {
        winMacro.completionPercentage = 100.0f;
        MacroManager::get().saveMacro(winMacro, collectCoins);
    }

    startPlaying(winMacro);

    Notification::create("AI AutoPlay: Pre-Calculated! Playing run...", NotificationIcon::Success)->show();
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

                // Input Fix: synchronize exact (X, Y) coordinates on action frame to prevent cumulative drift
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
