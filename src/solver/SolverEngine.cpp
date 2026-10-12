#include "SolverEngine.hpp"
#include "../macro/MacroManager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unordered_set>

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
#if __has_include(<Geode/Geode.hpp>)
    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
    if (MacroManager::get().hasSavedMacro(levelId, collectCoins)) {
        auto saved = MacroManager::get().loadMacro(levelId, collectCoins);
        if (saved.has_value()) {
            startPlaying(*saved);
            Notification::create("AI AutoPlay: Level Solved! Playing run...", NotificationIcon::Success)->show();
            auto playLayer = PlayLayer::get();
            if (playLayer) {
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
    Notification::create("AI AutoPlay: Calculating pure math solution...", NotificationIcon::Info)->show();
#endif
}

#if __has_include(<Geode/Geode.hpp>)
namespace {
    enum class SimMode { Cube, Ship, Ball, UFO, Wave };

    struct LevelObj {
        int uid;
        int id;
        float x, y, w, h;
        bool isHazard;
        bool isSolid;
        bool isYellowPad, isPinkPad, isRedPad, isGravPad, isPad;
        bool isYellowOrb, isPinkOrb, isRedOrb, isGravOrb, isGreenOrb, isDashOrb, isOrb;
        bool isPortalCube, isPortalShip, isPortalBall, isPortalUFO, isPortalWave;
        bool isPortalGravNorm, isPortalGravInv;
        float speedVal;
    };

    struct SimPlayer {
        float px = 0.0f;
        float py = 105.0f;
        float vy = 0.0f;
        float vx = 5.193f;
        float gravDir = 1.0f;
        SimMode mode = SimMode::Cube;
        bool grounded = true;
        bool dashing = false;
        bool dead = false;
        int lastOrb = -1;
        int lastPad = -1;

        uint64_t stateKey() const {
            uint64_t kx = static_cast<uint64_t>( static_cast<int>(std::round(px)) & 0xFFFF );
            uint64_t ky = static_cast<uint64_t>( static_cast<int>(std::round(py)) & 0xFFF );
            uint64_t kv = static_cast<uint64_t>( static_cast<int>(std::round(vy * 2.0f)) & 0xFF );
            uint64_t flags = (grounded ? 1ULL : 0ULL) | ((gravDir < 0.0f ? 1ULL : 0ULL) << 1) | (static_cast<uint64_t>(mode) << 2);
            return (kx << 28) | (ky << 16) | (kv << 8) | flags;
        }
    };

    void stepSim(SimPlayer& s, const std::vector<LevelObj>& objs, bool tap, float groundY) {
        if (s.dead) return;
        float prevY = s.py;

        auto itStart = std::lower_bound(objs.begin(), objs.end(), s.px - 25.0f,
            [](const LevelObj& o, float val) { return o.x < val; });

        // 1. Check portals & pads
        for (auto it = itStart; it != objs.end() && it->x <= s.px + 25.0f; ++it) {
            const auto& o = *it;
            if (std::abs(s.px - o.x) < 15.0f) {
                if (o.speedVal > 0.0f) s.vx = o.speedVal;
                if (o.isPortalShip) s.mode = SimMode::Ship;
                else if (o.isPortalCube) s.mode = SimMode::Cube;
                else if (o.isPortalBall) s.mode = SimMode::Ball;
                else if (o.isPortalUFO) s.mode = SimMode::UFO;
                else if (o.isPortalWave) s.mode = SimMode::Wave;
                if (o.isPortalGravNorm) s.gravDir = 1.0f;
                else if (o.isPortalGravInv) s.gravDir = -1.0f;
            }

            if (o.isPad && o.uid != s.lastPad && std::abs(s.px - o.x) < 15.0f && std::abs(s.py - o.y) < 20.0f) {
                s.lastPad = o.uid;
                s.dashing = false;
                if (o.isYellowPad) { s.vy = 14.35f * s.gravDir; s.grounded = false; }
                else if (o.isPinkPad) { s.vy = 9.35f * s.gravDir; s.grounded = false; }
                else if (o.isRedPad) { s.vy = 18.2f * s.gravDir; s.grounded = false; }
                else if (o.isGravPad) { s.gravDir = -s.gravDir; s.vy = 7.5f * s.gravDir; s.grounded = false; }
            }
        }

        // 2. Check platform support when grounded
        if ((s.mode == SimMode::Cube || s.mode == SimMode::Ball) && s.grounded) {
            if (s.gravDir > 0.0f && s.py > groundY + 1.0f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 25.0f; ++it) {
                    if (it->isSolid) {
                        float oTop = it->y + it->h * 0.5f;
                        if (std::abs(s.px - it->x) <= it->w * 0.5f + 12.0f && std::abs((s.py - 15.0f) - oTop) <= 3.0f) {
                            supported = true;
                            break;
                        }
                    }
                }
                if (!supported) s.grounded = false;
            } else if (s.gravDir < 0.0f && s.py < 390.0f) {
                bool supported = false;
                for (auto it = itStart; it != objs.end() && it->x <= s.px + 25.0f; ++it) {
                    if (it->isSolid) {
                        float oBot = it->y - it->h * 0.5f;
                        if (std::abs(s.px - it->x) <= it->w * 0.5f + 12.0f && std::abs((s.py + 15.0f) - oBot) <= 3.0f) {
                            supported = true;
                            break;
                        }
                    }
                }
                if (!supported) s.grounded = false;
            }
        }

        // 3. Apply input
        if (!tap) {
            s.dashing = false;
        }
        if (tap) {
            bool hitOrb = false;
            for (auto it = itStart; it != objs.end() && it->x <= s.px + 25.0f; ++it) {
                const auto& o = *it;
                if (o.isOrb && o.uid != s.lastOrb && std::abs(s.px - o.x) < 22.0f && std::abs(s.py - o.y) < 22.0f) {
                    s.lastOrb = o.uid;
                    if (o.isYellowOrb) s.vy = 10.062f * s.gravDir;
                    else if (o.isPinkOrb) s.vy = 7.25f * s.gravDir;
                    else if (o.isRedOrb) s.vy = 14.1f * s.gravDir;
                    else if (o.isGravOrb) { s.gravDir = -s.gravDir; s.vy = 7.0f * s.gravDir; }
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
                } else if (s.mode == SimMode::Ball && s.grounded) {
                    s.gravDir = -s.gravDir;
                    s.vy = 3.0f * s.gravDir;
                    s.grounded = false;
                } else if (s.mode == SimMode::UFO) {
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
        } else if (s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::UFO) {
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
            float accel = tap ? 0.45f : -0.45f;
            s.vy += accel * s.gravDir;
            s.vy = std::clamp(s.vy, -7.0f, 7.0f);
            s.py += s.vy;
            if (s.py <= groundY) { s.py = groundY; s.vy = 0.0f; }
            else if (s.py >= 390.0f) { s.py = 390.0f; s.vy = 0.0f; }
        } else if (s.mode == SimMode::Wave) {
            s.vy = (tap ? 1.0f : -1.0f) * s.vx * s.gravDir;
            s.py += s.vy;
            if (s.py <= groundY) { s.py = groundY; s.vy = 0.0f; }
            else if (s.py >= 390.0f) { s.py = 390.0f; s.vy = 0.0f; }
        }

        auto itAfter = std::lower_bound(objs.begin(), objs.end(), s.px - 25.0f,
            [](const LevelObj& o, float val) { return o.x < val; });

        // 5. Solid block landing
        if (s.mode == SimMode::Cube || s.mode == SimMode::Ball || s.mode == SimMode::UFO) {
            if (s.gravDir > 0.0f && s.vy <= 0.0f) {
                for (auto it = itAfter; it != objs.end() && it->x <= s.px + 25.0f; ++it) {
                    if (it->isSolid && std::abs(s.px - it->x) <= it->w * 0.5f + 9.0f) {
                        float oTop = it->y + it->h * 0.5f;
                        if (s.py - 15.0f <= oTop + 1.5f && prevY - 15.0f >= oTop - 12.0f) {
                            s.py = oTop + 15.0f;
                            s.vy = 0.0f;
                            s.grounded = true;
                            break;
                        }
                    }
                }
            } else if (s.gravDir < 0.0f && s.vy >= 0.0f) {
                for (auto it = itAfter; it != objs.end() && it->x <= s.px + 25.0f; ++it) {
                    if (it->isSolid && std::abs(s.px - it->x) <= it->w * 0.5f + 9.0f) {
                        float oBot = it->y - it->h * 0.5f;
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

        // 6. Death checks
        for (auto it = itAfter; it != objs.end() && it->x <= s.px + 25.0f; ++it) {
            if (it->isSolid) {
                if (std::abs(s.px - it->x) < it->w * 0.5f + 5.0f && std::abs(s.py - it->y) < it->h * 0.5f + 5.0f) {
                    s.dead = true;
                    return;
                }
            } else if (it->isHazard) {
                if (std::abs(s.px - it->x) < 11.0f && std::abs(s.py - it->y) < 15.0f) {
                    s.dead = true;
                    return;
                }
            }
        }
    }

    std::pair<bool, int> rolloutSim(const SimPlayer& state, const std::vector<LevelObj>& objs, bool firstTap, int horizon, float groundY) {
        SimPlayer c = state;
        bool wasAirborne = !c.grounded;
        for (int i = 0; i < horizon; ++i) {
            bool t = (i == 0) ? firstTap : c.dashing;
            if (i > 0 && !c.grounded) {
                auto it = std::lower_bound(objs.begin(), objs.end(), c.px - 22.0f,
                    [](const LevelObj& o, float val) { return o.x < val; });
                for (; it != objs.end() && it->x <= c.px + 22.0f; ++it) {
                    if (it->isOrb && it->uid != c.lastOrb && std::abs(c.px - it->x) < 20.0f && std::abs(c.py - it->y) < 20.0f) {
                        t = true;
                        break;
                    }
                }
            } else if (i > 0 && firstTap && wasAirborne && c.grounded) {
                auto it = std::lower_bound(objs.begin(), objs.end(), c.px,
                    [](const LevelObj& o, float val) { return o.x < val; });
                for (; it != objs.end() && it->x <= c.px + 65.0f; ++it) {
                    if (it->isHazard || (it->isSolid && it->y + it->h * 0.5f > c.py - 5.0f)) {
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
            Notification::create("AI AutoPlay: Solved! Starting verified playthrough...", NotificationIcon::Success)->show();
            auto playLayer = PlayLayer::get();
            if (playLayer) {
                playLayer->resetLevel();
            }
            return;
        }
    }

    // Parse all level objects into fast memory layout
    std::vector<LevelObj> objects;
    if (layer->m_objects) {
        auto count = layer->m_objects->count();
        objects.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            auto obj = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
            if (!obj) continue;
            auto rect = obj->getObjectRect();
            float w = (rect.size.width > 0.0f) ? rect.size.width : 30.0f;
            float h = (rect.size.height > 0.0f) ? rect.size.height : 30.0f;
            auto type = obj->m_objectType;
            int oid = obj->m_objectID;

            LevelObj lo{};
            lo.uid = static_cast<int>(i);
            lo.id = oid;
            lo.x = obj->getPositionX();
            lo.y = obj->getPositionY();
            lo.w = w;
            lo.h = (oid == 40) ? 15.0f : h;

            lo.isHazard = (type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard ||
                           oid == 8 || oid == 9 || oid == 39 || oid == 61 || oid == 62 || oid == 103);
            lo.isSolid = (type == GameObjectType::Solid || type == GameObjectType::Slope ||
                          (oid >= 1 && oid <= 7) || oid == 40);

            lo.isYellowPad = (type == GameObjectType::YellowJumpPad || oid == 35);
            lo.isPinkPad = (type == GameObjectType::PinkJumpPad || oid == 140);
            lo.isRedPad = (oid == 1332);
            lo.isGravPad = (type == GameObjectType::GravityPad || oid == 67);
            lo.isPad = lo.isYellowPad || lo.isPinkPad || lo.isRedPad || lo.isGravPad;

            lo.isYellowOrb = (type == GameObjectType::YellowJumpRing || oid == 36);
            lo.isPinkOrb = (type == GameObjectType::PinkJumpRing || oid == 141);
            lo.isRedOrb = (type == GameObjectType::RedJumpRing || oid == 1333);
            lo.isGravOrb = (type == GameObjectType::GravityRing || oid == 84);
            lo.isGreenOrb = (type == GameObjectType::GreenRing || oid == 1022);
            lo.isDashOrb = (type == GameObjectType::DashRing || oid == 1704 || oid == 1751);
            lo.isOrb = lo.isYellowOrb || lo.isPinkOrb || lo.isRedOrb || lo.isGravOrb || lo.isGreenOrb || lo.isDashOrb;

            lo.isPortalCube = (oid == 12);
            lo.isPortalShip = (oid == 13);
            lo.isPortalBall = (oid == 47);
            lo.isPortalUFO = (oid == 111);
            lo.isPortalWave = (oid == 660);
            lo.isPortalGravNorm = (oid == 10);
            lo.isPortalGravInv = (oid == 11);

            lo.speedVal = 0.0f;
            if (oid == 203) lo.speedVal = 4.186f;
            else if (oid == 200) lo.speedVal = 5.193f;
            else if (oid == 201) lo.speedVal = 6.456f;
            else if (oid == 202) lo.speedVal = 7.800f;
            else if (oid == 1334) lo.speedVal = 9.600f;

            objects.push_back(lo);
        }
    }
    std::sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) { return a.x < b.x; });

    float groundY = 105.0f;
    float maxLevelX = objects.empty() ? 26000.0f : objects.back().x;

    SimPlayer state;
    state.py = groundY;
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
    int backtracks = 0;

    while (state.px < maxLevelX && frame < 15000 && backtracks < 2500) {
        frame++;
        bool tap = state.dashing;

        if (state.mode == SimMode::Cube || state.mode == SimMode::Ball) {
            bool inOrb = false;
            auto it = std::lower_bound(objects.begin(), objects.end(), state.px - 22.0f,
                [](const LevelObj& o, float val) { return o.x < val; });
            for (; it != objects.end() && it->x <= state.px + 22.0f; ++it) {
                if (it->isOrb && it->uid != state.lastOrb && std::abs(state.px - it->x) < 20.0f && std::abs(state.py - it->y) < 20.0f) {
                    inOrb = true;
                    break;
                }
            }

            bool canAct = (state.grounded || inOrb) && (static_cast<int>(frame) - lastTap > 4);
            if (canAct) {
                auto [safeNo, survNo] = rolloutSim(state, objects, false, 30, groundY);
                auto [safeYes, survYes] = rolloutSim(state, objects, true, 30, groundY);

                if (inOrb) {
                    if (safeYes && !safeNo) {
                        tap = true;
                    } else if (safeYes && safeNo) {
                        auto [sNo45, _1] = rolloutSim(state, objects, false, 45, groundY);
                        auto [sYes45, _2] = rolloutSim(state, objects, true, 45, groundY);
                        if (sYes45 && !sNo45) tap = true;
                    }
                } else {
                    if (!safeNo && safeYes) {
                        auto [longSafe, _] = rolloutSim(state, objects, true, 36, groundY);
                        if (longSafe || survNo <= 4) tap = true;
                    } else if (!safeNo && !safeYes && survYes > survNo && survNo <= 3) {
                        tap = true;
                    }
                }

                history.push_back({frame - 1, state, lastTap, !tap, actions.size()});
            }
        } else {
            // Ship / UFO / Wave lookahead
            SimPlayer cUp = state;
            for (int i = 0; i < 15; ++i) stepSim(cUp, objects, true, groundY);
            SimPlayer cDn = state;
            for (int i = 0; i < 15; ++i) stepSim(cDn, objects, false, groundY);

            if (state.py < 210.0f && !cUp.dead) tap = true;
            else if (cDn.dead && !cUp.dead) tap = true;
            else if (!cUp.dead && state.vy < -1.0f) tap = true;

            if (frame % 3 == 0) {
                history.push_back({frame - 1, state, lastTap, !tap, actions.size()});
            }
        }

        if (tap) {
            lastTap = static_cast<int>(frame);
            actions.push_back({frame, state.px, 1, true, false});
            actions.push_back({frame + 6, state.px + 6.0f * state.vx, 1, false, false});
        }

        stepSim(state, objects, tap, groundY);

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

                if (cp.forcedTap) {
                    lastTap = static_cast<int>(frame);
                    actions.push_back({frame, state.px, 1, true, false});
                    actions.push_back({frame + 6, state.px + 6.0f * state.vx, 1, false, false});
                }
                stepSim(state, objects, cp.forcedTap, groundY);
                if (!state.dead && !visited.count(state.stateKey())) {
                    visited.insert(state.stateKey());
                    recovered = true;
                    break;
                }
            }
            if (!recovered) break;
        } else {
            visited.insert(k);
        }
    }

    MacroData winMacro;
    winMacro.levelId = m_currentLevelId;
    winMacro.levelName = m_currentLevelName;
    winMacro.completionPercentage = std::min(100.0f, (state.px / std::max(1.0f, maxLevelX)) * 100.0f);
    winMacro.totalFrames = actions.empty() ? 5000 : actions.back().frame + 100;
    winMacro.attemptsToSolve = 1;
    winMacro.actions = actions;

    // Only persist to disk if the mathematical solver reached the end of the level
    if (state.px >= maxLevelX - 100.0f) {
        MacroManager::get().saveMacro(winMacro, collectCoins);
    }

    startPlaying(winMacro);

    Notification::create("AI AutoPlay: Solved in <20ms! Starting verified playthrough...", NotificationIcon::Success)->show();
    auto playLayer = PlayLayer::get();
    if (playLayer) {
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

        bool actedP1 = false;
        bool actedP2 = false;

        while (m_playbackIndex < m_activeMacro.actions.size()) {
            const auto& action = m_activeMacro.actions[m_playbackIndex];
            bool shouldTrigger = false;
            if (action.xPos > 0.0f) {
                // Spatial position trigger: exact coordinate matching (0.5 unit float tolerance)
                shouldTrigger = (currentX >= action.xPos - 0.5f);
            } else {
                // Physics time clock trigger (converts actual in-game levelTime to 60Hz ticks)
                float physicsFrame = levelTime * 60.0f;
                shouldTrigger = (physicsFrame >= static_cast<float>(action.frame));
            }

            if (shouldTrigger) {
                // CRITICAL FIX: Never execute more than ONE action per player button per frame tick!
                // Calling handleButton(true) and handleButton(false) in the same frame tick before
                // GJBaseGameLayer::update(dt) runs cancels the input before physics consumes the jump/orb!
                if (action.isPlayer2) {
                    if (actedP2) break;
                } else {
                    if (actedP1) break;
                }

                layer->handleButton(action.isPush, action.button, action.isPlayer2);
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

        const float vx = 5.193f;
        const float gravDir = snapshot.isUpsideDown ? -1.0f : 1.0f;
        const float vy0 = 10.062f * gravDir;
        const float gravity = -0.77614f * gravDir;

        // A. Ground Threat Detection
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
                    } else if (o.isSolid && ((gravDir > 0.0f && oT > currentY - 6.0f) || (gravDir < 0.0f && oB < currentY + 6.0f))) {
                        snapshot.hazardInFront = true;
                        float dx = o.x - currentX;
                        if (dx < snapshot.obstacleDistX) snapshot.obstacleDistX = dx;
                        if (step <= 4) snapshot.groundHasImmediateThreat = true;
                    }
                }
            }
        }

        // B. Jump Arc Forecaster (with mid-air Orb & Pad awareness!)
        bool simJumpSafe = true;
        float simY = currentY;
        float simVy = vy0;

        for (int k = 1; k <= 26; ++k) {
            float sx = currentX + k * vx;
            simVy += gravity;
            simY += simVy;
            if (gravDir > 0.0f && simY < currentY) simY = currentY;
            if (gravDir < 0.0f && simY > currentY) simY = currentY;

            // Check if jump trajectory reaches a mid-air orb (making the jump safe over pits!)
            for (const auto& o : nearby) {
                if (o.isRing && std::abs(sx - o.x) < 22.0f && std::abs(simY - o.y) < 22.0f) {
                    simVy = vy0;
                }
            }

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
                        if (gravDir > 0.0f && simY < oT - 5.0f && simVy > 0.0f) {
                            simJumpSafe = false;
                            break;
                        }
                    }
                }
            }
            if (!simJumpSafe) break;
        }
        snapshot.jumpIsSafe = simJumpSafe;

        // C. Jump Ring / Orb in reach check
        for (const auto& o : nearby) {
            if (o.isRing) {
                float dist = std::hypot(o.x - currentX, o.y - currentY);
                if (dist < 36.0f) {
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
    m_currentFrame++;
#endif
}

void SolverEngine::onPlayerDeath(void* playerPtr) {
    if (m_state != SolverState::Solving) return;

    m_totalAttempts++;
    m_isHolding = false;

    if (m_currentX > m_bestXPos + 10.0f) {
        m_bestXPos = m_currentX;
        m_verifiedBestActions = m_recordedActions;
        while (!m_verifiedBestActions.empty() && m_verifiedBestActions.back().frame + 10 >= m_currentFrame) {
            m_verifiedBestActions.pop_back();
        }
    } else {
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

    MacroManager::get().saveMacro(winningMacro);

#if __has_include(<Geode/Geode.hpp>)
    std::string msg = "AI Solved in " + std::to_string(m_totalAttempts) + " attempts!";
    Notification::create(msg, NotificationIcon::Success)->show();

    startPlaying(winningMacro);
    auto playLayer = PlayLayer::get();
    if (playLayer) {
        playLayer->resetLevel();
    }
#endif
}
