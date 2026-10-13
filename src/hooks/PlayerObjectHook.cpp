#include "../solver/SolverEngine.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(AutoPlayPlayerObject, PlayerObject) {
    void playDeathEffect() {
        SolverEngine::get().onPlayerDeath(this);
        if (SolverEngine::get().getState() == SolverState::Solving) {
            // Suppress standard death sequence animation and delay
            return;
        }

        PlayerObject::playDeathEffect();
    }
};
#endif
