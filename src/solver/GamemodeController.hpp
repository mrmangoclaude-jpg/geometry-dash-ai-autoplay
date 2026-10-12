#pragma once

#include <cstdint>

enum class GamemodeType {
    Cube,
    Ship,
    Ball,
    UFO,
    Wave,
    Robot,
    Spider,
    Swing
};

struct PlayerStateSnapshot {
    GamemodeType mode = GamemodeType::Cube;
    float posX = 0.0f;
    float posY = 0.0f;
    float yVelocity = 0.0f;
    bool isOnGround = false;
    bool isUpsideDown = false;
    bool isDashing = false;
    bool isHolding = false;
    
    // Corridor boundary sensor results
    float ceilingDist = 100.0f;
    float floorDist = 100.0f;
    float obstacleDistX = 200.0f;
    bool hazardInFront = false;
    bool ringInReach = false;
};

class GamemodeController {
public:
    static GamemodeController& get();

    // Determine current gamemode from player attributes
    GamemodeType detectGamemode(
        bool isShip, bool isBall, bool isBird, 
        bool isDart, bool isRobot, bool isSpider, bool isSwing
    );

    // Evaluate whether the bot should push, release, or maintain input
    // Returns: 1 = Push, 0 = Release, -1 = Maintain current state
    int evaluateAction(const PlayerStateSnapshot& state);

private:
    int evaluateCube(const PlayerStateSnapshot& state);
    int evaluateShip(const PlayerStateSnapshot& state);
    int evaluateBall(const PlayerStateSnapshot& state);
    int evaluateUFO(const PlayerStateSnapshot& state);
    int evaluateWave(const PlayerStateSnapshot& state);
    int evaluateRobot(const PlayerStateSnapshot& state);
    int evaluateSpider(const PlayerStateSnapshot& state);
    int evaluateSwing(const PlayerStateSnapshot& state);
};
