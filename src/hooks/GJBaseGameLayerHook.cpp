#include "../solver/SolverEngine.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto state = SolverEngine::get().getState();
        if (state == SolverState::Solving) {
            // Headless fixed 60Hz physics sub-stepping:
            // Prevents tunneling and physics glitches caused by oversized dt
            const float fixedDt = 1.0f / 60.0f;
            const int subSteps = 80; // 80x headless simulation per frame

            for (int i = 0; i < subSteps; ++i) {
                SolverEngine::get().onFrameTick(this);
                GJBaseGameLayer::update(fixedDt);
                if (SolverEngine::get().getState() != SolverState::Solving) {
                    break;
                }
            }
            return;
        }

        // Normal 1x playback or idle
        SolverEngine::get().onFrameTick(this);
        GJBaseGameLayer::update(dt);
    }
};
#endif
