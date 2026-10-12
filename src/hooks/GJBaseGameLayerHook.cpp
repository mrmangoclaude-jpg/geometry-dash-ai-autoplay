#include "../solver/SolverEngine.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto state = SolverEngine::get().getState();
        if (state == SolverState::Solving) {
            float speedMult = static_cast<float>(Mod::get()->getSettingValue<double>("solve_speed"));
            if (speedMult < 1.0f) speedMult = 25.0f;
            dt *= speedMult;
        }

        GJBaseGameLayer::update(dt);
        SolverEngine::get().onFrameTick(this);
    }
};
#endif
