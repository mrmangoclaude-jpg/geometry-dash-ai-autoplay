#include "../solver/SolverEngine.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayBaseGameLayer, GJBaseGameLayer) {
    void processQueuedButtons(float dt, bool clearInputQueue) {
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        SolverEngine::get().onProcessButtons(this);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        SolverEngine::get().onFrameTick(this);
    }
};
#endif
