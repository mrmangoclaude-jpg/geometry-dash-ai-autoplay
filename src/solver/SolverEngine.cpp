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
#include <Geode/binding/LevelSettingsObject.hpp>
using namespace geode::prelude;
#endif

SolverEngine& SolverEngine::get() {
    static SolverEngine instance;
    return instance;
}

void SolverEngine::startSolving(int levelId, const std::string& levelName, void* gameLayerPtr) {
#if __has_include(<Geode/Geode.hpp>)
    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
    if (MacroManager::get().hasSavedMacro(levelId, collectCoins, levelName)) {
        auto saved = MacroManager::get().loadMacro(levelId, collectCoins, levelName);
        if (saved.has_value()) {
            startPlaying(*saved);
            auto playLayer = gameLayerPtr ? static_cast<PlayLayer*>(gameLayerPtr) : PlayLayer::get();
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
    void* targetLayer = gameLayerPtr ? gameLayerPtr : static_cast<void*>(PlayLayer::get());
    if (targetLayer) {
        solveMathematically(targetLayer);
    }
#endif
}

#if __has_include(<Geode/Geode.hpp>)
namespace {
    enum class SimMode : uint8_t { Cube = 0, Ship = 1, Ball = 2, UFO = 3, Wave = 4, Robot = 5, Spider = 6 };

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
        int orient;
        float x, y, w, h;
        float baseX, baseY;
        bool isHazard;
        bool isSaw;
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

    inline bool classifyKnownObject(int oid, float rot, float& w, float& h, bool& isSolid, bool& isSlope, bool& isHazard, bool& isSaw) {
        bool rot90 = (static_cast<int>(std::round(std::abs(rot) / 90.0f)) % 2) == 1;

        // 30x30 standard solid blocks (including 467, 469, 470, 471 outline blocks)
        if ((oid >= 1 && oid <= 7) || oid == 63 || (oid >= 69 && oid <= 72) || (oid >= 74 && oid <= 78) ||
            (oid >= 81 && oid <= 83) || (oid >= 90 && oid <= 96) || (oid >= 116 && oid <= 119) ||
            oid == 121 || oid == 122 || oid == 146 || (oid >= 160 && oid <= 163) || (oid >= 165 && oid <= 169) ||
            oid == 173 || oid == 175 || (oid >= 207 && oid <= 210) || oid == 212 || oid == 213 ||
            (oid >= 247 && oid <= 250) || (oid >= 252 && oid <= 258) || oid == 260 || oid == 261 ||
            (oid >= 263 && oid <= 265) || (oid >= 267 && oid <= 272) || oid == 274 || oid == 275 ||
            oid == 467 || oid == 469 || oid == 470 || oid == 471) {
            isSolid = true; w = 30.0f; h = 30.0f; return true;
        }
        // 15x15 mini solid blocks
        if (oid == 64 || oid == 195 || oid == 206 || oid == 220 || oid == 661) {
            isSolid = true; w = 15.0f; h = 15.0f; return true;
        }
        // 30x14 slabs
        if (oid == 40 || oid == 147 || oid == 215 || oid == 369 || oid == 370) {
            isSolid = true; w = rot90 ? 14.0f : 30.0f; h = rot90 ? 30.0f : 14.0f; return true;
        }
        // 30x15 slabs
        if (oid == 662 || oid == 663 || oid == 664) {
            isSolid = true; w = rot90 ? 15.0f : 30.0f; h = rot90 ? 30.0f : 15.0f; return true;
        }
        // 30x5 thin solid barriers
        if (oid == 468 || oid == 475) {
            isSolid = true; w = rot90 ? 5.0f : 30.0f; h = rot90 ? 30.0f : 5.0f; return true;
        }
        // 30x16 slabs
        if (oid == 62 || oid == 65 || oid == 66 || oid == 68) {
            isSolid = true; w = rot90 ? 16.0f : 30.0f; h = rot90 ? 30.0f : 16.0f; return true;
        }
        // 30x30 slopes
        switch (oid) {
            case 289: case 294: case 299: case 305: case 309: case 315:
            case 321: case 326: case 331: case 337: case 343: case 349:
            case 353: case 371: case 483: case 492: case 651: case 665:
            case 673: case 709: case 711: case 726: case 728: case 886:
                isSlope = true; w = 30.0f; h = 30.0f; return true;
            default: break;
        }
        // 60x30 slopes
        switch (oid) {
            case 291: case 295: case 301: case 307: case 311: case 317:
            case 323: case 327: case 333: case 339: case 345: case 351:
            case 355: case 367: case 372: case 484: case 493: case 652:
            case 666: case 674: case 710: case 712: case 727: case 729: case 887:
                isSlope = true; w = rot90 ? 30.0f : 60.0f; h = rot90 ? 60.0f : 30.0f; return true;
            default: break;
        }
        // Known rectangular hazards (spikes)
        float hw = 0.0f, hh = 0.0f;
        switch (oid) {
            case 720: hw = 2.4f; hh = 3.2f; break;
            case 61:  hw = 9.0f; hh = 7.2f; break;
            case 365: case 667: hw = 9.0f; hh = 6.0f; break;
            case 392: case 458: case 459: hw = 2.6f; hh = 4.8f; break;
            case 8: case 144: case 177: case 216: hw = 6.0f; hh = 12.0f; break;
            case 103: case 145: case 218: hw = 4.0f; hh = 7.6f; break;
            case 39: case 205: case 217: hw = 6.0f; hh = 5.6f; break;
            case 9: hw = 9.0f; hh = 10.8f; break;
            default: break;
        }
        if (hw > 0.0f) {
            isHazard = true;
            w = rot90 ? hh : hw;
            h = rot90 ? hw : hh;
            return true;
        }
        // Known sawblades (circular hazards)
        float sawDiam = 0.0f;
        switch (oid) {
            case 88: case 186: case 740: sawDiam = 64.6f; break;
            case 89:  sawDiam = 43.2f; break;
            case 98:  sawDiam = 24.0f; break;
            case 397: sawDiam = 57.8f; break;
            case 398: sawDiam = 34.88f; break;
            case 399: sawDiam = 25.8f; break;
            case 675: sawDiam = 64.0f; break;
            case 676: sawDiam = 35.02f; break;
            case 677: sawDiam = 24.96f; break;
            case 678: sawDiam = 60.8f; break;
            case 679: sawDiam = 37.08f; break;
            case 680: sawDiam = 21.6f; break;
            default: break;
        }
        if (sawDiam > 0.0f) {
            isHazard = true;
            isSaw = true;
            w = sawDiam;
            h = sawDiam;
            return true;
        }
        return false;
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
        float floorY = 90.0f;
        float ceilY = 1200.0f;
        SimMode mode = SimMode::Cube;
        bool mini = false;
        bool grounded = true;
        bool dashing = false;
        bool holding = false;
        bool hasBufferedInput = false;
        bool dead = false;
        int robotBoostFrames = 0;
        int lastOrb = -1;
        int lastPad = -1;
        int lastPortal = -1;
        int actionIdx = -1;

        uint32_t beamKey() const {
            uint32_t ky = static_cast<uint32_t>(static_cast<int>(std::round(py)) & 0x3FFF);
            uint32_t kv = static_cast<uint32_t>(static_cast<int>(std::round(vy * 2.0f)) & 0xFF);
            bool useHoldBit = holding && (mode == SimMode::Cube || mode == SimMode::Ball || mode == SimMode::UFO || mode == SimMode::Robot || mode == SimMode::Spider);
            uint32_t flags = (grounded ? 1U : 0U)
                           | ((gravDir < 0.0f ? 1U : 0U) << 1)
                           | ((useHoldBit ? 1U : 0U) << 2)
                           | (static_cast<uint32_t>(mode) << 3);
            return (ky << 16) | (kv << 8) | flags;
        }
    };

    void stepSim(SimPlayer& s, const std::vector<LevelObj>& objs, bool wantHold) {
        if (s.dead) return;
        float prevY = s.py;
        bool freshPress = wantHold && !s.holding;
        if (freshPress) {
            s.hasBufferedInput = true;
        } else if (!wantHold) {
            s.hasBufferedInput = false;
        }

        float playerHalfW = (s.mode == SimMode::Wave) ? (s.mini ? 3.0f : 5.0f) : (s.mini ? 9.0f : 15.0f);
        float playerHalfH = playerHalfW;

        auto itStart = std::lower_bound(objs.begin(), objs.end(), s.px - 55.0f,
            [](const LevelObj& o, float val) { return o.x < val; });

        // 1. Portals, S-blocks & pads
        for (auto it = itStart; it != objs.end() && it->x <= s.px + 55.0f; ++it) {
            const auto& o = *it;
            float ox, oy;
            evalObjPos(o, s.px, s.vx, ox, oy);

            if (std::abs(s.px - ox) < std::max(18.0f, o.w * 0.5f + playerHalfW) &&
                std::abs(s.py - oy) < std::max(55.0f, o.h * 0.5f) + playerHalfH) {
                if (o.speedVal > 0.0f) s.vx = o.speedVal;
                if (o.isStopDash) s.dashing = false;

                bool isModePortal = false;
                if (o.isPortalShip) { s.mode = SimMode::Ship; isModePortal = true; }
                else if (o.isPortalCube) { s.mode = SimMode::Cube; isModePortal = true; }
                else if (o.isPortalBall) { s.mode = SimMode::Ball; isModePortal = true; }
                else if (o.isPortalUFO) { s.mode = SimMode::UFO; isModePortal = true; }
                else if (o.isPortalWave) { s.mode = SimMode::Wave; isModePortal = true; }
                else if (o.isPortalRobot) { s.mode = SimMode::Robot; isModePortal = true; }
                else if (o.isPortalSpider) { s.mode = SimMode::Spider; isModePortal = true; }

                if (isModePortal && o.uid != s.lastPortal) {
                    s.lastPortal = o.uid;
                    if (s.mode == SimMode::Ship || s.mode == SimMode::UFO || s.mode == SimMode::Wave) {
                        float fl = std::max(90.0f, std::floor((oy - 240.0f + 1.0f) / 30.0f) * 30.0f + 90.0f);
                        s.floorY = fl;
                        s.ceilY = fl + 300.0f;
                    } else if (s.mode == SimMode::Ball || s.mode == SimMode::Spider) {
                        float fl = std::max(90.0f, std::floor((oy - 210.0f + 1.0f) / 30.0f) * 30.0f + 90.0f);
                        s.floorY = fl;
                        s.ceilY = fl + 240.0f;
                    } else {
                        s.floorY = 90.0f;
                        s.ceilY = 1200.0f;
                    }
                }

                if (o.isPortalMini) s.mini = true;
                else if (o.isPortalNormalSize) s.mini = false;

                if (o.isPortalGravNorm) {
                    if (s.gravDir < 0.0f) { s.gravDir = 1.0f; s.vy *= 0.5f; s.grounded = false; }
                } else if (o.isPortalGravInv) {
                    if (s.gravDir > 0.0f) { s.gravDir = -1.0f; s.vy *= 0.5f; s.grounded = false; }
                } else if (o.isPortalGravToggle && o.uid != s.lastPortal) {
                    s.lastPortal = o.uid;
                    s.gravDir = -s.gravDir;
                    s.vy *= 0.5f;
                    s.grounded = false;
                }
            }

            if (o.isPad && o.uid != s.lastPad &&
                std::abs(s.px - ox) < (o.w * 0.5f + playerHalfW) &&
                std::abs(s.py - oy) < (o.h * 0.5f + playerHalfH + 4.0f)) {
                s.lastPad = o.uid;
                s.dashing = false;
                s.robotBoostFrames = 0;
                float miniMult = s.mini ? 0.8f : 1.0f;
                if (o.isYellowPad) {
                    float vyPad = (s.mode == SimMode::Ball) ? 8.65f : ((s.mode == SimMode::UFO) ? 7.2f : 15.94f);
                    s.vy = vyPad * miniMult * s.gravDir;
                    s.grounded = false;
                } else if (o.isPinkPad) {
                    float vyPad = (s.mode == SimMode::Ball) ? 6.0f : 10.39f;
                    s.vy = vyPad * miniMult * s.gravDir;
                    s.grounded = false;
                } else if (o.isRedPad) {
                    float vyPad = (s.mode == SimMode::Ball) ? 10.8f : 20.22f;
                    s.vy = vyPad * miniMult * s.gravDir;
                    s.grounded = false;
                } else if (o.isGravPad || o.isSpiderPad) {
                    s.gravDir = -s.gravDir;
                    float vyPad = (s.mode == SimMode::Ball) ? 3.5f : 6.44f;
                    s.vy = -vyPad * miniMult * s.gravDir;
                    s.grounded = false;
                }
            }
        }

        // Recompute playerHalfW/H in case mode/mini changed
        playerHalfW = (s.mode == SimMode::Wave) ? (s.mini ? 3.0f : 5.0f) : (s.mini ? 9.0f : 15.0f);
        playerHalfH = playerHalfW;
        float floorCenterY = s.floorY + playerHalfH;
        float ceilCenterY = s.ceilY - playerHalfH;

        // 2. Check platform support when grounded
        if ((s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::Robot || s.mode == SimMode::Spider) && s.grounded) {
            if (s.gravDir > 0.0f && s.py > floorCenterY + 1.5f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 55.0f; ++it) {
                    if (it->isSolid || it->isSlope) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        float oTop = oy + it->h * 0.5f;
                        if (std::abs(s.px - ox) <= it->w * 0.5f + playerHalfW && std::abs((s.py - playerHalfH) - oTop) <= 5.0f) {
                            supported = true;
                            break;
                        }
                    }
                }
                if (!supported) s.grounded = false;
            } else if (s.gravDir < 0.0f && s.py < ceilCenterY - 1.5f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 55.0f; ++it) {
                    if (it->isSolid || it->isSlope) {
                        float ox, oy;
                        evalObjPos(*it, s.px, s.vx, ox, oy);
                        float oBot = oy - it->h * 0.5f;
                        if (std::abs(s.px - ox) <= it->w * 0.5f + playerHalfW && std::abs((s.py + playerHalfH) - oBot) <= 5.0f) {
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
            if (s.hasBufferedInput) {
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 55.0f; ++it) {
                    const auto& o = *it;
                    if (!o.isOrb || o.uid == s.lastOrb) continue;
                    float ox, oy;
                    evalObjPos(o, s.px, s.vx, ox, oy);
                    if (std::abs(s.px - ox) < (18.0f + playerHalfW) && std::abs(s.py - oy) < (18.0f + playerHalfH)) {
                        s.lastOrb = o.uid;
                        s.hasBufferedInput = false;
                        s.robotBoostFrames = 0;
                        if (o.isYellowOrb) {
                            float vyOrb = (s.mode == SimMode::Ball) ? 7.05f : 11.18f;
                            s.vy = vyOrb * miniMult * s.gravDir;
                        } else if (o.isPinkOrb) {
                            float vyOrb = (s.mode == SimMode::Ball) ? 5.42f : ((s.mode == SimMode::UFO) ? 4.22f : ((s.mode == SimMode::Ship) ? 3.72f : 8.05f));
                            s.vy = vyOrb * miniMult * s.gravDir;
                        } else if (o.isRedOrb) {
                            float vyOrb = (s.mode == SimMode::Ball) ? 9.32f : 15.2f;
                            s.vy = vyOrb * miniMult * s.gravDir;
                        } else if (o.isGravOrb || o.isSpiderOrb) {
                            s.gravDir = -s.gravDir;
                            float vyOrb = (s.mode == SimMode::Ball) ? 2.82f : 4.45f;
                            s.vy = -vyOrb * miniMult * s.gravDir;
                        } else if (o.isGreenOrb) {
                            s.gravDir = -s.gravDir;
                            float vyOrb = (s.mode == SimMode::Ball) ? 6.92f : 10.97f;
                            s.vy = vyOrb * miniMult * s.gravDir;
                        } else if (o.isDashOrb) {
                            if (o.id == 1751) s.gravDir = -s.gravDir;
                            s.vy = 0.0f;
                            s.dashing = true;
                        }
                        s.grounded = false;
                        hitOrb = true;
                        break;
                    }
                }
            }
            if (!hitOrb) {
                if (s.mode == SimMode::Cube && s.grounded) {
                    s.vy = 11.18f * miniMult * s.gravDir;
                    s.grounded = false;
                    s.hasBufferedInput = false;
                } else if (s.mode == SimMode::Robot) {
                    if (s.grounded && s.hasBufferedInput) {
                        s.vy = 7.5f * miniMult * s.gravDir;
                        s.grounded = false;
                        s.hasBufferedInput = false;
                        s.robotBoostFrames = 12;
                    } else if (s.robotBoostFrames > 0) {
                        s.vy = 9.8f * miniMult * s.gravDir;
                        s.robotBoostFrames--;
                    }
                } else if (s.mode == SimMode::Ball && s.grounded && s.hasBufferedInput) {
                    s.gravDir = -s.gravDir;
                    s.vy = -3.02f * miniMult * s.gravDir;
                    s.grounded = false;
                    s.hasBufferedInput = false;
                } else if (s.mode == SimMode::Spider && s.grounded && s.hasBufferedInput) {
                    s.gravDir = -s.gravDir;
                    s.hasBufferedInput = false;
                    float targetY = (s.gravDir > 0.0f) ? floorCenterY : ceilCenterY;
                    for (auto it = itStart; it != objs.end() && it->x <= s.px + 55.0f; ++it) {
                        if (it->isSolid) {
                            float ox, oy;
                            evalObjPos(*it, s.px, s.vx, ox, oy);
                            if (std::abs(s.px - ox) <= it->w * 0.5f + playerHalfW - 1.0f) {
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
                } else if (s.mode == SimMode::UFO && s.hasBufferedInput) {
                    s.vy = (s.mini ? 5.98f : 6.18f) * s.gravDir;
                    s.grounded = false;
                    s.hasBufferedInput = false;
                }
            }
        }

        // 4. Integrate motion
        s.px += s.vx;
        if (s.dashing) {
            s.vy = 0.0f;
        } else if (s.mode == SimMode::Cube || s.mode == SimMode::Robot) {
            const float gravity = -0.9582f;
            if (!s.grounded) {
                s.py += s.vy;
                s.vy += gravity * s.gravDir;
                if (std::abs(s.vy) > 15.0f) s.vy = (s.vy > 0.0f) ? 15.0f : -15.0f;
                if (s.gravDir > 0.0f && s.py <= floorCenterY) {
                    s.py = floorCenterY;
                    s.vy = 0.0f;
                    s.grounded = true;
                } else if (s.py >= ceilCenterY || (s.gravDir < 0.0f && s.py <= floorCenterY)) {
                    s.dead = true;
                    s.holding = wantHold;
                    return;
                }
            }
        } else if (s.mode == SimMode::Ball) {
            const float gravity = -0.4657f;
            if (!s.grounded) {
                s.py += s.vy;
                s.vy += gravity * s.gravDir;
                if (std::abs(s.vy) > 13.5f) s.vy = (s.vy > 0.0f) ? 13.5f : -13.5f;
                if (s.gravDir > 0.0f && s.py <= floorCenterY) {
                    s.py = floorCenterY; s.vy = 0.0f; s.grounded = true;
                } else if (s.gravDir < 0.0f && s.py >= ceilCenterY) {
                    s.py = ceilCenterY; s.vy = 0.0f; s.grounded = true;
                } else if (s.gravDir > 0.0f && s.py >= ceilCenterY) {
                    s.py = ceilCenterY; s.vy = 0.0f;
                } else if (s.gravDir < 0.0f && s.py <= floorCenterY) {
                    s.py = floorCenterY; s.vy = 0.0f;
                }
            }
        } else if (s.mode == SimMode::UFO) {
            float gravity = (s.vy * s.gravDir > 1.72f) ? -0.464f : -0.310f;
            if (s.mini) gravity *= 1.18f;
            if (!s.grounded) {
                s.py += s.vy;
                s.vy += gravity * s.gravDir;
                float maxDown = s.mini ? 6.77f : 5.76f;
                float maxUp = s.mini ? 8.47f : 7.20f;
                if (s.gravDir > 0.0f) s.vy = std::clamp(s.vy, -maxDown, maxUp);
                else s.vy = std::clamp(s.vy, -maxUp, maxDown);
            }
            if (s.py <= floorCenterY) {
                s.py = floorCenterY; s.vy = 0.0f;
                if (s.gravDir > 0.0f) s.grounded = true;
            } else if (s.py >= ceilCenterY) {
                s.py = ceilCenterY; s.vy = 0.0f;
                if (s.gravDir < 0.0f) s.grounded = true;
            }
        } else if (s.mode == SimMode::Ship) {
            float accel = wantHold
                ? ((s.vy * s.gravDir <= 1.72f) ? 0.388f : 0.310f)
                : ((s.vy * s.gravDir >= 1.72f) ? -0.372f : -0.248f);
            if (s.mini) accel *= 1.18f;
            s.vy += accel * s.gravDir;
            float maxDown = s.mini ? 6.77f : 5.76f;
            float maxUp = s.mini ? 8.47f : 7.20f;
            if (s.gravDir > 0.0f) s.vy = std::clamp(s.vy, -maxDown, maxUp);
            else s.vy = std::clamp(s.vy, -maxUp, maxDown);
            s.py += s.vy;
            if (s.py <= floorCenterY) {
                s.py = floorCenterY; s.vy = 0.0f;
                if (s.gravDir > 0.0f) s.grounded = true;
            } else if (s.py >= ceilCenterY) {
                s.py = ceilCenterY; s.vy = 0.0f;
                if (s.gravDir < 0.0f) s.grounded = true;
            }
        } else if (s.mode == SimMode::Wave) {
            float waveMult = s.mini ? 2.0f : 1.0f;
            s.vy = (wantHold ? 1.0f : -1.0f) * s.vx * waveMult * s.gravDir;
            s.py += s.vy;
            if (s.py <= floorCenterY) {
                s.py = floorCenterY; s.vy = 0.0f;
            } else if (s.py >= ceilCenterY) {
                s.py = ceilCenterY; s.vy = 0.0f;
            }
        }

        auto itAfter = std::lower_bound(objs.begin(), objs.end(), s.px - 55.0f,
            [](const LevelObj& o, float val) { return o.x < val; });

        // 5. Solid block & Slope collisions
        for (auto it = itAfter; it != objs.end() && it->x <= s.px + 55.0f; ++it) {
            if (!it->isSolid && !it->isSlope) continue;
            float ox, oy;
            evalObjPos(*it, s.px, s.vx, ox, oy);
            float w = it->w, h = it->h;
            if (std::abs(s.px - ox) > w * 0.5f + playerHalfW) continue;

            float oTop = oy + h * 0.5f;
            float oBot = oy - h * 0.5f;

            if (it->isSlope) {
                int orient = it->orient;
                float t = std::clamp((s.px - (ox - w * 0.5f)) / std::max(1.0f, w), 0.0f, 1.0f);
                float diagY = (orient == 0 || orient == 2) ? (oBot + t * h) : (oTop - t * h);
                if (orient == 0 || orient == 1) { // Floor slope (solid below diagY)
                    if (s.py + playerHalfH > oBot && s.py - playerHalfH <= diagY + 4.0f) {
                        if (s.mode == SimMode::Wave) {
                            if (s.py - playerHalfH < diagY - 2.0f) {
                                s.dead = true; s.holding = wantHold; return;
                            }
                        } else if (prevY + playerHalfH <= oBot + 4.0f && s.py < oy) {
                            if (s.mode == SimMode::Ship || s.mode == SimMode::Ball || s.mode == SimMode::UFO) {
                                s.py = oBot - playerHalfH;
                                if (s.vy > 0.0f) s.vy = 0.0f;
                                if (s.gravDir < 0.0f) s.grounded = true;
                            } else {
                                s.dead = true; s.holding = wantHold; return;
                            }
                        } else {
                            s.py = diagY + playerHalfH;
                            if (s.gravDir > 0.0f) {
                                s.grounded = true;
                                if (orient == 0) s.vy = std::max(s.vy, s.vx * (h / w) * 0.9f);
                                else if (s.vy <= 0.1f) s.vy = -s.vx * (h / w) * 0.75f;
                            } else {
                                if (s.vy < 0.0f) s.vy = 0.0f;
                            }
                        }
                    }
                } else { // Ceiling slope (orient 2, 3: solid above diagY)
                    if (s.py - playerHalfH < oTop && s.py + playerHalfH >= diagY - 4.0f) {
                        if (s.mode == SimMode::Wave) {
                            if (s.py + playerHalfH > diagY + 2.0f) {
                                s.dead = true; s.holding = wantHold; return;
                            }
                        } else if (prevY - playerHalfH >= oTop - 4.0f && s.py > oy) {
                            if (s.gravDir > 0.0f && s.vy <= 0.1f) {
                                s.py = oTop + playerHalfH; s.vy = 0.0f; s.grounded = true;
                            }
                        } else {
                            s.py = diagY - playerHalfH;
                            if (s.gravDir < 0.0f) {
                                s.grounded = true;
                                if (orient == 3) s.vy = std::min(s.vy, -s.vx * (h / w) * 0.9f);
                                else if (s.vy >= -0.1f) s.vy = s.vx * (h / w) * 0.75f;
                            } else {
                                if (s.vy > 0.0f) s.vy = 0.0f;
                            }
                        }
                    }
                }
                continue;
            }

            if (s.mode != SimMode::Wave && std::abs(s.px - ox) <= w * 0.5f + playerHalfW - 1.0f) {
                float clip = (s.mode == SimMode::Ship || s.mode == SimMode::UFO) ? 7.0f : 10.0f;
                if (s.gravDir > 0.0f) {
                    if (s.vy <= 0.1f && (s.py - playerHalfH) <= oTop + 2.5f && (prevY - playerHalfH) >= oTop - clip) {
                        s.py = oTop + playerHalfH;
                        s.vy = 0.0f;
                        s.grounded = true;
                    } else if ((s.mode == SimMode::Ship || s.mode == SimMode::Ball || s.mode == SimMode::UFO) &&
                               s.vy >= -0.1f && (s.py + playerHalfH) >= oBot - 2.5f && (prevY + playerHalfH) <= oBot + clip) {
                        s.py = oBot - playerHalfH;
                        s.vy = 0.0f;
                    }
                } else {
                    if (s.vy >= -0.1f && (s.py + playerHalfH) >= oBot - 2.5f && (prevY + playerHalfH) <= oBot + clip) {
                        s.py = oBot - playerHalfH;
                        s.vy = 0.0f;
                        s.grounded = true;
                    } else if ((s.mode == SimMode::Ship || s.mode == SimMode::Ball || s.mode == SimMode::UFO) &&
                               s.vy <= 0.1f && (s.py - playerHalfH) <= oTop + 2.5f && (prevY - playerHalfH) >= oTop - clip) {
                        s.py = oTop + playerHalfH;
                        s.vy = 0.0f;
                    }
                }
            }
        }

        // 6. Fatal collision checks (Solids & Hazards)
        for (auto it = itAfter; it != objs.end() && it->x <= s.px + 55.0f; ++it) {
            if (!it->isSolid && !it->isHazard) continue;
            float ox, oy;
            evalObjPos(*it, s.px, s.vx, ox, oy);
            float w = it->w, h = it->h;
            if (it->isSolid) {
                float innerHalf = (s.mode == SimMode::Wave && s.mini) ? 3.0f : 4.5f;
                if (std::abs(s.px - ox) < (w * 0.5f + innerHalf) && std::abs(s.py - oy) < (h * 0.5f + innerHalf)) {
                    s.dead = true; s.holding = wantHold; return;
                }
            } else if (it->isHazard) {
                if (it->isSaw) {
                    float radius = w * 0.5f + 3.0f; // Safety margin so the cube jumps before touching teeth
                    float dx = std::max(0.0f, std::abs(s.px - ox) - playerHalfW * 0.85f);
                    float dy = std::max(0.0f, std::abs(s.py - oy) - playerHalfH * 0.85f);
                    if (dx * dx + dy * dy < radius * radius) {
                        s.dead = true; s.holding = wantHold; return;
                    }
                } else {
                    if (std::abs(s.px - ox) < (w * 0.5f + playerHalfW + 2.0f) && std::abs(s.py - oy) < (h * 0.5f + playerHalfH + 2.0f)) {
                        s.dead = true; s.holding = wantHold; return;
                    }
                }
            }
        }

        s.holding = wantHold;
    }
}
#endif

void SolverEngine::solveMathematically(void* gameLayerPtr) {
#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<PlayLayer*>(gameLayerPtr);
    if (!layer || !layer->m_player1) {
        m_state = SolverState::Idle;
        return;
    }

    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
    if (MacroManager::get().hasSavedMacro(m_currentLevelId, collectCoins, m_currentLevelName)) {
        auto saved = MacroManager::get().loadMacro(m_currentLevelId, collectCoins, m_currentLevelName);
        if (saved.has_value()) {
            startPlaying(*saved);
            if (layer->m_gameState.m_levelTime > 0.05) {
                layer->resetLevel();
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

    // Pass 2: Parse all interactive & physical objects
    std::vector<LevelObj> objects;
    if (layer->m_objects) {
        auto count = layer->m_objects->count();
        objects.reserve(count / 2 + 16);
        for (unsigned int i = 0; i < count; ++i) {
            auto obj = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
            if (!obj || obj == layer->m_anticheatSpike) continue;
            if (obj->m_isDisabled || obj->m_isDisabled2 || obj->m_isGroupDisabled) continue;

            int oid = obj->m_objectID;
            if (oid == 901 || oid == 1049) continue;

            float baseX = obj->getPositionX();
            float baseY = obj->getPositionY();
            float rot = obj->getRotation();
            bool fx = obj->isFlipX();
            bool fy = obj->isFlipY();
            bool rot90 = (static_cast<int>(std::round(std::abs(rot) / 90.0f)) % 2) == 1;

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

            auto type = obj->m_objectType;
            LevelObj lo{};
            lo.uid = static_cast<int>(i);
            lo.id = oid;
            lo.baseX = baseX;
            lo.baseY = baseY;
            lo.x = baseX;
            lo.y = baseY;
            lo.w = 30.0f;
            lo.h = 30.0f;
            lo.moveTriggers = std::move(objMoves);

            // Ignore non-interactive portals (mirror, dual, teleport)
            if (oid == 29 || oid == 30 || oid == 45 || oid == 46 || oid == 59 || oid == 60 || oid == 747 || oid == 749) {
                continue;
            }

            // Always ignore decorative / passable blocks (e.g. fake blocks in The Lightning Road)
            if (type == GameObjectType::Decoration || type == GameObjectType::EnterEffectObject) {
                if (oid != 200 && oid != 201 && oid != 202 && oid != 203 && oid != 1334 && oid != 1829) {
                    continue;
                }
            }

            bool known = classifyKnownObject(oid, rot, lo.w, lo.h, lo.isSolid, lo.isSlope, lo.isHazard, lo.isSaw);
            if (!known) {
                auto rect = obj->getObjectRect();
                if (rect.size.width > 0.1f && rect.size.height > 0.1f) {
                    lo.w = rect.size.width;
                    lo.h = rect.size.height;
                }
                lo.isSlope = (type == GameObjectType::Slope && !obj->m_slopeIsHazard);
                lo.isSaw = (type == GameObjectType::AnimatedHazard);
                lo.isHazard = (type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard ||
                               (type == GameObjectType::Slope && obj->m_slopeIsHazard));
                lo.isSolid = (type == GameObjectType::Solid);
                if (lo.isHazard && !lo.isSaw && lo.w >= 24.0f && lo.h >= 24.0f) {
                    lo.w = rot90 ? 12.0f : 6.0f;
                    lo.h = rot90 ? 6.0f : 12.0f;
                }
            }

            // Filter out any anticheat spike at the spawn point (x < 80)
            if (lo.isHazard && baseX < 80.0f) {
                continue;
            }

            if (lo.isSlope) {
                int orient = ((static_cast<int>(std::round(rot / 90.0f)) % 4) + 4) % 4;
                if (fx && fy) orient = (orient + 2) % 4;
                else if (fx) orient = (orient + 1) % 4;
                else if (fy) orient = (orient + 3) % 4;
                lo.orient = orient;
            } else {
                lo.orient = 0;
            }

            lo.isStopDash = (oid == 1829);

            lo.isYellowPad = (type == GameObjectType::YellowJumpPad || oid == 35);
            lo.isPinkPad = (type == GameObjectType::PinkJumpPad || oid == 140);
            lo.isRedPad = (type == GameObjectType::RedJumpPad || oid == 1332);
            lo.isGravPad = (type == GameObjectType::GravityPad || oid == 67);
            lo.isSpiderPad = (type == GameObjectType::SpiderPad || oid == 3005);
            lo.isPad = lo.isYellowPad || lo.isPinkPad || lo.isRedPad || lo.isGravPad || lo.isSpiderPad;
            if (lo.isPad) {
                lo.w = 25.0f;
                lo.h = 5.0f;
            }

            lo.isYellowOrb = (type == GameObjectType::YellowJumpRing || oid == 36);
            lo.isPinkOrb = (type == GameObjectType::PinkJumpRing || oid == 141);
            lo.isRedOrb = (type == GameObjectType::RedJumpRing || oid == 1333);
            lo.isGravOrb = (type == GameObjectType::GravityRing || oid == 84);
            lo.isGreenOrb = (type == GameObjectType::GreenRing || oid == 1022);
            lo.isDashOrb = (type == GameObjectType::DashRing || type == GameObjectType::GravityDashRing || oid == 1704 || oid == 1751);
            lo.isSpiderOrb = (type == GameObjectType::SpiderOrb || oid == 3004);
            lo.isOrb = lo.isYellowOrb || lo.isPinkOrb || lo.isRedOrb || lo.isGravOrb || lo.isGreenOrb || lo.isDashOrb || lo.isSpiderOrb;
            if (lo.isOrb) {
                lo.w = 36.0f;
                lo.h = 36.0f;
            }

            lo.isPortalCube = (type == GameObjectType::CubePortal || oid == 12);
            lo.isPortalShip = (type == GameObjectType::ShipPortal || oid == 13);
            lo.isPortalBall = (type == GameObjectType::BallPortal || oid == 47);
            lo.isPortalUFO = (type == GameObjectType::UfoPortal || oid == 111);
            lo.isPortalWave = (type == GameObjectType::WavePortal || oid == 660);
            lo.isPortalRobot = (type == GameObjectType::RobotPortal || oid == 745);
            lo.isPortalSpider = (type == GameObjectType::SpiderPortal || oid == 1331);
            lo.isPortalGravInv = (type == GameObjectType::NormalGravityPortal || oid == 10);
            lo.isPortalGravNorm = (type == GameObjectType::InverseGravityPortal || oid == 11);
            lo.isPortalGravToggle = (type == GameObjectType::GravityTogglePortal || oid == 2926);
            lo.isPortalMini = (type == GameObjectType::MiniSizePortal || oid == 101);
            lo.isPortalNormalSize = (type == GameObjectType::RegularSizePortal || oid == 99);

            lo.speedVal = 0.0f;
            if (oid == 200) lo.speedVal = 4.186f;
            else if (oid == 201) lo.speedVal = 5.193f;
            else if (oid == 202) lo.speedVal = 6.456f;
            else if (oid == 203) lo.speedVal = 7.800f;
            else if (oid == 1334) lo.speedVal = 9.600f;

            if (lo.isPortalGravNorm || lo.isPortalGravInv || lo.isPortalGravToggle) {
                lo.w = rot90 ? 75.0f : 25.0f;
                lo.h = rot90 ? 25.0f : 75.0f;
            } else if (lo.isPortalCube || lo.isPortalShip || lo.isPortalBall || lo.isPortalUFO ||
                       lo.isPortalWave || lo.isPortalRobot || lo.isPortalSpider ||
                       lo.isPortalMini || lo.isPortalNormalSize) {
                lo.w = rot90 ? 86.0f : 34.0f;
                lo.h = rot90 ? 34.0f : 86.0f;
            } else if (lo.speedVal > 0.0f) {
                lo.w = rot90 ? 56.0f : 50.0f;
                lo.h = rot90 ? 50.0f : 56.0f;
            }

            if (!lo.moveTriggers.empty()) {
                float estX, estY;
                evalObjPos(lo, baseX, 5.193f, estX, estY);
                lo.x = estX;
                lo.y = estY;
            }

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

    float maxLevelX = objects.empty() ? 5000.0f : (objects.back().x + 300.0f);

    SimPlayer initial;
    initial.px = 0.0f;
    initial.py = 105.0f;
    initial.vy = 0.0f;
    initial.vx = 5.193f;
    initial.gravDir = 1.0f;
    initial.mode = SimMode::Cube;
    initial.mini = false;
    initial.grounded = true;
    initial.floorY = 90.0f;
    initial.ceilY = 1200.0f;

    if (layer->m_levelSettings) {
        auto ls = layer->m_levelSettings;
        initial.mini = ls->m_startMini;
        initial.gravDir = ls->m_isFlipped ? -1.0f : 1.0f;
        switch (ls->m_startMode) {
            case 1: initial.mode = SimMode::Ship; initial.ceilY = 390.0f; break;
            case 2: initial.mode = SimMode::Ball; initial.ceilY = 330.0f; break;
            case 3: initial.mode = SimMode::UFO;  initial.ceilY = 390.0f; break;
            case 4: initial.mode = SimMode::Wave; initial.ceilY = 390.0f; break;
            case 5: initial.mode = SimMode::Robot; break;
            case 6: initial.mode = SimMode::Spider; initial.ceilY = 330.0f; break;
            default: initial.mode = SimMode::Cube; break;
        }
        switch (static_cast<int>(ls->m_startSpeed)) {
            case 1: initial.vx = 4.186f; break; // 0.5x
            case 2: initial.vx = 6.456f; break; // 2x
            case 3: initial.vx = 7.800f; break; // 3x
            case 4: initial.vx = 9.600f; break; // 4x
            default: initial.vx = 5.193f; break; // 1x
        }
    }

    // Parallel Wavefront Beam Search with Stratified Vertical-Band Pruning
    std::vector<ActionNode> actionPool;
    actionPool.reserve(65536);

    auto recordTransition = [&](SimPlayer& s, size_t f, float xAtAction, float yAtAction, bool newHold) {
        if (newHold != s.holding) {
            int newIdx = static_cast<int>(actionPool.size());
            actionPool.push_back({s.actionIdx, PlayerAction{f, xAtAction, 1, newHold, false, yAtAction}});
            s.actionIdx = newIdx;
        }
    };

    std::vector<SimPlayer> beam;
    beam.push_back(initial);

    SimPlayer bestOverall = initial;
    size_t bestFrame = 0;

    const size_t maxFrames = 20000;
    const size_t maxBeamWidth = 256;

    for (size_t frame = 1; frame <= maxFrames; ++frame) {
        if (beam.empty()) break;
        if (beam[0].px >= maxLevelX) {
            bestOverall = beam[0];
            bestFrame = frame;
            break;
        }

        std::vector<SimPlayer> nextBeam;
        nextBeam.reserve(beam.size() * 2);
        std::unordered_set<uint32_t> seenBuckets;
        seenBuckets.reserve(beam.size() * 2);

        for (const auto& cur : beam) {
            bool nearOrb = false;
            if ((cur.mode == SimMode::Cube || cur.mode == SimMode::Ball || cur.mode == SimMode::Spider) && !cur.grounded && !cur.dashing) {
                auto itO = std::lower_bound(objects.begin(), objects.end(), cur.px - 40.0f,
                    [](const LevelObj& o, float val) { return o.x < val; });
                for (; itO != objects.end() && itO->x <= cur.px + 45.0f; ++itO) {
                    if (itO->isOrb) { nearOrb = true; break; }
                }
            }

            for (int w = 0; w <= 1; ++w) {
                bool wantHold = (w == 1);
                if (wantHold && (cur.mode == SimMode::Cube || cur.mode == SimMode::Ball || cur.mode == SimMode::Spider) &&
                    !cur.grounded && !cur.dashing && !nearOrb) {
                    continue;
                }
                SimPlayer cB = cur;
                recordTransition(cB, frame, cur.px, cur.py, wantHold);
                stepSim(cB, objects, wantHold);
                if (!cB.dead) {
                    uint32_t k = cB.beamKey();
                    if (seenBuckets.insert(k).second) {
                        nextBeam.push_back(cB);
                    }
                }
            }
        }

        if (nextBeam.empty()) break;

        if (nextBeam.size() > maxBeamWidth) {
            std::sort(nextBeam.begin(), nextBeam.end(), [](const SimPlayer& a, const SimPlayer& b) {
                bool aG = a.grounded && (a.mode == SimMode::Cube || a.mode == SimMode::Ball || a.mode == SimMode::Robot || a.mode == SimMode::Spider);
                bool bG = b.grounded && (b.mode == SimMode::Cube || b.mode == SimMode::Ball || b.mode == SimMode::Robot || b.mode == SimMode::Spider);
                if (aG != bG) return aG > bG;
                return std::abs(a.vy) < std::abs(b.vy);
            });

            std::unordered_map<int, int> bandCounts;
            struct ScoredIdx { int rank; size_t idx; };
            std::vector<ScoredIdx> scored;
            scored.reserve(nextBeam.size());
            for (size_t i = 0; i < nextBeam.size(); ++i) {
                int band = static_cast<int>(nextBeam[i].py / 10.0f);
                int cnt = bandCounts[band]++;
                scored.push_back({cnt, i});
            }
            std::stable_sort(scored.begin(), scored.end(), [](const ScoredIdx& a, const ScoredIdx& b) {
                return a.rank < b.rank;
            });
            std::vector<SimPlayer> pruned;
            pruned.reserve(maxBeamWidth);
            for (size_t i = 0; i < maxBeamWidth; ++i) {
                pruned.push_back(nextBeam[scored[i].idx]);
            }
            nextBeam = std::move(pruned);
        }

        beam = std::move(nextBeam);
        if (beam[0].px > bestOverall.px) {
            bestOverall = beam[0];
            bestFrame = frame;
        }
    }

    // Reconstruct chronological action sequence from bestOverall.actionIdx
    std::vector<PlayerAction> actions;
    m_solvedTrajectoryPath.clear();
    int curIdx = bestOverall.actionIdx;
    while (curIdx >= 0 && curIdx < static_cast<int>(actionPool.size())) {
        actions.push_back(actionPool[curIdx].action);
        if (actionPool[curIdx].action.xPos > 0.0f) {
            m_solvedTrajectoryPath.push_back({actionPool[curIdx].action.xPos, actionPool[curIdx].action.yPos});
        }
        curIdx = actionPool[curIdx].parentIdx;
    }
    std::reverse(actions.begin(), actions.end());
    std::reverse(m_solvedTrajectoryPath.begin(), m_solvedTrajectoryPath.end());

    // Guarantee the button is NEVER left held down at the end of the macro
    if (!actions.empty() && actions.back().isPush) {
        const auto& lastAct = actions.back();
        actions.push_back({lastAct.frame + 3, lastAct.xPos + 15.0f, 1, false, false, 0.0f});
    }

    log::info("SOLVER: '{}' (ID {}) solved to x={:.1f}/{:.1f} ({:.1f}%) with {} actions",
        m_currentLevelName, m_currentLevelId, bestOverall.px, maxLevelX,
        std::min(100.0f, (bestOverall.px / std::max(1.0f, maxLevelX)) * 100.0f), actions.size());

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
    } else {
        log::warn("SOLVER DIAGNOSTIC: '{}' (ID {}) stopped early at {:.1f}% (x={:.1f}/{:.1f})",
            m_currentLevelName, m_currentLevelId, winMacro.completionPercentage, bestOverall.px, maxLevelX);
    }

    startPlaying(winMacro);

    Notification::create("AI AutoPlay: Level Solved! Playing run...", NotificationIcon::Success)->show();
    if (layer->m_gameState.m_levelTime > 0.05) {
        layer->resetLevel();
    }
#endif
}

void SolverEngine::stopSolving() {
    m_state = SolverState::Idle;
    m_isHolding = false;
    m_activeDrawNode = nullptr;
    m_solvedTrajectoryPath.clear();
}

void SolverEngine::startPlaying(const MacroData& macro) {
    m_state = SolverState::Playing;
    m_activeMacro = macro;
    m_playbackIndex = 0;
    m_currentFrame = 0;
    m_isHolding = false;

    if (m_solvedTrajectoryPath.empty() && !macro.actions.empty()) {
        m_solvedTrajectoryPath.reserve(macro.actions.size());
        for (const auto& act : macro.actions) {
            if (act.xPos > 0.0f) {
                m_solvedTrajectoryPath.push_back({act.xPos, act.yPos});
            }
        }
    }

#if __has_include(<Geode/Geode.hpp>)
    Notification::create("AI AutoPlay: Playing Macro", NotificationIcon::Success)->show();
#endif
}

void SolverEngine::stopPlaying() {
    m_state = SolverState::Idle;
    m_isHolding = false;
    m_activeDrawNode = nullptr;
    m_solvedTrajectoryPath.clear();
    m_activeMacro = {};
    m_playbackIndex = 0;
}

void SolverEngine::onLevelReset() {
    m_currentFrame = 0;
    m_playbackIndex = 0;
    m_replayIndex = 0;
    m_isHolding = false;
    m_needsReset = false;
}

void SolverEngine::onProcessButtons(void* gameLayerPtr) {
    if (m_state != SolverState::Playing) return;

#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<GJBaseGameLayer*>(gameLayerPtr);
    if (!layer || !layer->m_player1) return;

    auto p1 = layer->m_player1;
    if (p1->m_isDead) return;

    float currentX = p1->getPositionX();
    float levelTime = layer->m_gameState.m_levelTime;
    bool actedP1 = false;
    bool actedP2 = false;

    while (m_playbackIndex < m_activeMacro.actions.size()) {
        const auto& action = m_activeMacro.actions[m_playbackIndex];
        bool shouldTrigger = false;
        if (action.xPos > 0.0f) {
            shouldTrigger = (currentX >= action.xPos - 0.75f);
        } else {
            float physicsFrame = levelTime * 60.0f;
            shouldTrigger = (physicsFrame >= static_cast<float>(action.frame));
        }

        if (!shouldTrigger) {
            break;
        }

        if (action.isPlayer2) {
            if (actedP2) break;
        } else {
            if (actedP1) break;
        }

        auto targetPlayer = (action.isPlayer2 && layer->m_player2) ? layer->m_player2 : p1;
        if (action.isPush) {
            targetPlayer->pushButton(PlayerButton::Jump);
        } else {
            targetPlayer->releaseButton(PlayerButton::Jump);
        }

        layer->handleButton(action.isPush, action.button, !action.isPlayer2);
        if (!action.isPlayer2) {
            m_isHolding = action.isPush;
            actedP1 = true;
        } else {
            actedP2 = true;
        }
        m_playbackIndex++;
    }
#endif
}

void SolverEngine::onFrameTick(void* gameLayerPtr) {
    if (m_state != SolverState::Playing) {
        return;
    }

#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<GJBaseGameLayer*>(gameLayerPtr);
    if (!layer || !layer->m_player1) return;

    auto p1 = layer->m_player1;
    if (p1->m_isDead) return;

    m_currentX = p1->getPositionX();
    m_currentFrame++;
#else
    m_currentFrame++;
#endif
}

void SolverEngine::renderTrajectory(void* drawNodePtr) {
    m_activeDrawNode = drawNodePtr;
#if __has_include(<Geode/Geode.hpp>)
    if (!drawNodePtr) return;
    auto drawNode = static_cast<CCDrawNode*>(drawNodePtr);
    if (!drawNode->getParent()) return;
    drawNode->clear();

    if (m_solvedTrajectoryPath.size() < 2) return;

    ccColor4F pathColor = (m_activeMacro.completionPercentage >= 99.0f)
        ? ccc4f(0.0f, 1.0f, 0.35f, 0.75f)
        : ccc4f(1.0f, 0.75f, 0.0f, 0.75f);

    size_t step = (m_solvedTrajectoryPath.size() > 300) ? (m_solvedTrajectoryPath.size() / 300 + 1) : 1;
    size_t count = 0;
    for (size_t i = 0; i + step < m_solvedTrajectoryPath.size() && count < 300; i += step, ++count) {
        CCPoint pA = {m_solvedTrajectoryPath[i].first, m_solvedTrajectoryPath[i].second};
        CCPoint pB = {m_solvedTrajectoryPath[i+step].first, m_solvedTrajectoryPath[i+step].second};
        if (std::isnan(pA.x) || std::isnan(pA.y) || std::isnan(pB.x) || std::isnan(pB.y)) continue;
        if (std::isinf(pA.x) || std::isinf(pA.y) || std::isinf(pB.x) || std::isinf(pB.y)) continue;
        if (std::abs(pB.x - pA.x) < 0.5f && std::abs(pB.y - pA.y) < 0.5f) continue;
        drawNode->drawSegment(pA, pB, 1.5f, pathColor);
    }

    if (m_lastDeathX > 0.0f && m_lastDeathY > 0.0f) {
        drawNode->drawDot({m_lastDeathX, m_lastDeathY}, 6.0f, ccc4f(1.0f, 0.15f, 0.15f, 1.0f));
    }
#endif
}

void SolverEngine::onPlayerDeath(void* playerPtr) {
    m_isHolding = false;
#if __has_include(<Geode/Geode.hpp>)
    if (!playerPtr) return;
    auto player = static_cast<PlayerObject*>(playerPtr);
    m_lastDeathX = player->getPositionX();
    m_lastDeathY = player->getPositionY();

    if (m_state == SolverState::Playing) {
        bool debugLogs = Mod::get()->getSettingValue<bool>("debug_logs");
        if (debugLogs) {
            float expX = 0.0f, expY = 0.0f;
            bool expPush = false;
            if (m_playbackIndex < m_activeMacro.actions.size()) {
                const auto& act = m_activeMacro.actions[m_playbackIndex];
                expX = act.xPos;
                expY = act.yPos;
                expPush = act.isPush;
            }
            log::warn("[AutoPlay Debug] Player DIED at x={:.2f}, y={:.2f}, vy={:.2f}, grav={:.1f}",
                m_lastDeathX, m_lastDeathY, player->m_yVelocity, player->m_gravity);
            log::warn("[AutoPlay Debug] Playback index: {}/{} | Next act: x={:.2f}, y={:.2f}, push={}",
                m_playbackIndex, m_activeMacro.actions.size(), expX, expY, expPush);
            log::warn("[AutoPlay Debug] Delta to planned action: dx={:.2f}, dy={:.2f}",
                m_lastDeathX - expX, m_lastDeathY - expY);
        }

        Notification::create(fmt::format("AutoPlay: Died at x={:.0f}, y={:.0f}", m_lastDeathX, m_lastDeathY), NotificationIcon::Error)->show();

        if (m_activeDrawNode) {
            auto drawNode = static_cast<CCDrawNode*>(m_activeDrawNode);
            if (drawNode->getParent()) {
                drawNode->drawDot({m_lastDeathX, m_lastDeathY}, 6.0f, ccc4f(1.0f, 0.15f, 0.15f, 1.0f));
            }
        }
    }
#endif
}

void SolverEngine::onLevelComplete() {
    m_state = SolverState::Idle;
    m_isHolding = false;
}
