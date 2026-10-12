#include "../solver/SolverEngine.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto state = SolverEngine::get().getState();
        if (state == SolverState::Solving) {
            // Pure in-memory corridor solver (Instant < 20ms, 0 FPS drop, no visual death resets)
            SolverEngine::get().solveMathematically(this);
            return;
        }

        // Normal 1x playback or idle
        SolverEngine::get().onFrameTick(this);
        GJBaseGameLayer::update(dt);
    }
};
#endif
