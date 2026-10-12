#include "../solver/SolverEngine.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayBaseGameLayer, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto state = SolverEngine::get().getState();
        if (state == SolverState::Solving) {
            SolverEngine::get().solveMathematically(this);
        }
        // Run on every 240Hz physics substep so framerate (60/109/144/240 FPS) never causes drift
        SolverEngine::get().onFrameTick(this);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
#endif
