#include "../solver/SolverEngine.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayBaseGameLayer, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        SolverEngine::get().onProcessCommands(this);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        SolverEngine::get().onAfterProcessCommands();
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (SolverEngine::get().shouldBlockUserInput()) {
            return; // Block human clicks during bot playback to prevent accidental desync
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};
#endif
