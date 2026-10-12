#pragma once

#include "../macro/MacroTypes.hpp"
#include "GamemodeController.hpp"
#include <vector>
#include <string>
#include <deque>

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#endif

enum class SolverState {
    Idle,
    Solving,
    Playing,
    Paused
};

struct DecisionBranch {
    size_t frame = 0;
    float posX = 0.0f;
    int actionTaken = 0;     // 1 = push, 0 = release
    int alternativesTried = 0; // count of explored options
    float bestProgress = 0.0f;
};

class SolverEngine {
private:
    SolverState m_state = SolverState::Idle;
    int m_currentLevelId = 0;
    std::string m_currentLevelName = "";
    
    // Playback data
    MacroData m_activeMacro;
    size_t m_playbackIndex = 0;

    // Solving state & Frontier memory
    std::vector<PlayerAction> m_recordedActions;
    std::vector<PlayerAction> m_verifiedBestActions;
    std::deque<DecisionBranch> m_decisionStack;
    size_t m_currentFrame = 0;
    size_t m_replayIndex = 0;
    int m_totalAttempts = 0;
    float m_bestXPos = 0.0f;
    float m_currentX = 0.0f;
    bool m_isHolding = false;
    bool m_needsReset = false;

    SolverEngine() = default;

public:
    static SolverEngine& get();

    void startSolving(int levelId, const std::string& levelName);
    void stopSolving();
    
    void startPlaying(const MacroData& macro);
    void stopPlaying();

    void onFrameTick(void* gameLayerPtr);
    void onPlayerDeath(void* playerPtr);
    void onLevelComplete();
    void onLevelReset();

    SolverState getState() const { return m_state; }
    int getTotalAttempts() const { return m_totalAttempts; }
    float getBestXPos() const { return m_bestXPos; }
    size_t getCurrentFrame() const { return m_currentFrame; }
    bool shouldReset() const { return m_needsReset; }
    void clearResetFlag() { m_needsReset = false; }
    
    const MacroData& getActiveMacro() const { return m_activeMacro; }
};
