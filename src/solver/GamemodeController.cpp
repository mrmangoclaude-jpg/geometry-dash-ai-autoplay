#include "GamemodeController.hpp"
#include <cmath>

GamemodeController& GamemodeController::get() {
    static GamemodeController instance;
    return instance;
}

GamemodeType GamemodeController::detectGamemode(
    bool isShip, bool isBall, bool isBird, 
    bool isDart, bool isRobot, bool isSpider, bool isSwing
) {
    if (isShip) return GamemodeType::Ship;
    if (isBall) return GamemodeType::Ball;
    if (isBird) return GamemodeType::UFO;
    if (isDart) return GamemodeType::Wave;
    if (isRobot) return GamemodeType::Robot;
    if (isSpider) return GamemodeType::Spider;
    if (isSwing) return GamemodeType::Swing;
    return GamemodeType::Cube;
}

int GamemodeController::evaluateAction(const PlayerStateSnapshot& state) {
    switch (state.mode) {
        case GamemodeType::Cube:
            return evaluateCube(state);
        case GamemodeType::Ship:
            return evaluateShip(state);
        case GamemodeType::Ball:
            return evaluateBall(state);
        case GamemodeType::UFO:
            return evaluateUFO(state);
        case GamemodeType::Wave:
            return evaluateWave(state);
        case GamemodeType::Robot:
            return evaluateRobot(state);
        case GamemodeType::Spider:
            return evaluateSpider(state);
        case GamemodeType::Swing:
            return evaluateSwing(state);
        default:
            return evaluateCube(state);
    }
}

int GamemodeController::evaluateCube(const PlayerStateSnapshot& state) {
    // 1. In air near a ring / jump orb: trigger tap only if simulated trajectory survives
    if (!state.isOnGround && state.ringInReach) {
        if (state.orbIsSafeToTap && !state.isHolding) {
            return 1;
        }
        if (state.isHolding) {
            return 0;
        }
    }

    // 2. On ground
    if (state.isOnGround) {
        // Jump only when the obstacle enters the optimal clearance window (40 to 75 units):
        // Prevents premature jumps (jumping 130 units away lands directly into the spike)
        if (state.hazardInFront && state.obstacleDistX <= 75.0f && !state.isHolding) {
            return 1;
        }
        if (state.isHolding) {
            return 0; // Release so we can jump again next time
        }
    } else {
        // While in air, release hold so buffer doesn't misfire
        if (state.isHolding && !state.ringInReach) {
            return 0;
        }
    }
    return -1;
}

int GamemodeController::evaluateShip(const PlayerStateSnapshot& state) {
    // Corridor centering: stay balanced between floor and ceiling
    // If getting close to floor or descending rapidly: push
    if (state.floorDist < 45.0f || (state.floorDist < 70.0f && state.yVelocity < -2.0f)) {
        return 1;
    }
    // If getting close to ceiling or ascending too high: release
    if (state.ceilingDist < 45.0f || (state.ceilingDist < 70.0f && state.yVelocity > 2.0f)) {
        return 0;
    }
    return -1;
}

int GamemodeController::evaluateBall(const PlayerStateSnapshot& state) {
    // Ball can only switch gravity when firmly on a surface
    if (state.isOnGround) {
        // If an obstacle is approaching on current surface: flip gravity!
        if (state.hazardInFront && state.obstacleDistX < 80.0f && !state.isHolding) {
            return 1;
        }
        if (state.isHolding) {
            return 0;
        }
    } else if (state.isHolding) {
        return 0;
    }
    return -1;
}

int GamemodeController::evaluateUFO(const PlayerStateSnapshot& state) {
    // UFO gives an upward jump impulse per click
    if (state.ceilingDist < 35.0f) {
        return 0; // Don't tap into ceiling spikes!
    }
    // Tap to jump when falling low or hazard ahead
    if (state.floorDist < 50.0f || (state.hazardInFront && state.obstacleDistX < 65.0f && state.floorDist < 80.0f)) {
        if (!state.isHolding) {
            return 1;
        } else {
            return 0; // Quick release to allow next flutter tap
        }
    }
    if (state.isHolding) {
        return 0;
    }
    return -1;
}

int GamemodeController::evaluateWave(const PlayerStateSnapshot& state) {
    // Wave moves strictly 45 degrees up when held, down when released
    // Inverted gravity swaps these
    bool movingUp = state.isHolding;
    if (state.isUpsideDown) movingUp = !movingUp;

    // Safety margin from ceiling and floor
    if (state.floorDist < 30.0f) {
        return state.isUpsideDown ? 0 : 1; // Rise up away from floor
    }
    if (state.ceilingDist < 30.0f) {
        return state.isUpsideDown ? 1 : 0; // Dive down away from ceiling
    }

    // Centering between hazards
    if (state.floorDist < state.ceilingDist - 15.0f && !movingUp) {
        return state.isUpsideDown ? 0 : 1;
    } else if (state.ceilingDist < state.floorDist - 15.0f && movingUp) {
        return state.isUpsideDown ? 1 : 0;
    }

    return -1;
}

int GamemodeController::evaluateRobot(const PlayerStateSnapshot& state) {
    // Robot has variable jump height based on hold duration
    if (state.isOnGround && state.hazardInFront && state.obstacleDistX < 90.0f) {
        return 1; // Start holding
    }
    // Release hold once high enough or after cleared obstacle
    if (!state.isOnGround && state.isHolding) {
        if (state.floorDist > 90.0f || state.ceilingDist < 30.0f) {
            return 0;
        }
    }
    return -1;
}

int GamemodeController::evaluateSpider(const PlayerStateSnapshot& state) {
    // Spider instantly teleports to the nearest ceiling/floor surface
    if (state.isOnGround) {
        if (state.hazardInFront && state.obstacleDistX < 85.0f && !state.isHolding) {
            return 1; // Teleport to opposite surface
        }
        if (state.isHolding) {
            return 0;
        }
    } else if (state.ringInReach && !state.isHolding) {
        return 1;
    } else if (state.isHolding) {
        return 0;
    }
    return -1;
}

int GamemodeController::evaluateSwing(const PlayerStateSnapshot& state) {
    // Swing Copter: Each click inverts mid-air gravity!
    // If heading down towards floor hazards, tap to invert upward
    if (state.floorDist < 45.0f && state.yVelocity < 0.0f && !state.isHolding) {
        return 1;
    }
    // If heading up towards ceiling hazards, tap to invert downward
    if (state.ceilingDist < 45.0f && state.yVelocity > 0.0f && !state.isHolding) {
        return 1;
    }
    // Instant release so next tap can flip gravity again
    if (state.isHolding) {
        return 0;
    }
    return -1;
}
