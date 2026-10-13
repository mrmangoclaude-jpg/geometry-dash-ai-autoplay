#pragma once

#include "../macro/MacroTypes.hpp"
#include <vector>
#include <string>
#include <atomic>
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

class SolverEngine {
private:
    SolverState m_state = SolverState::Idle;
    std::atomic<bool> m_isSolving{false};
    int m_currentLevelId = 0;
    std::string m_currentLevelName = "";
    
    // Playback data
    MacroData m_activeMacro;
    size_t m_playbackIndex = 0;
    size_t m_currentTick = 0;
    bool m_injecting = false;
    bool m_isHolding = false;

    // Visualizer
    std::vector<std::pair<float, float>> m_solvedTrajectoryPath;
    void* m_activeDrawNode = nullptr;

    SolverEngine() = default;

public:
    static SolverEngine& get();

    void solveLevelAsync(int levelId, const std::string& levelName, void* gameLayerPtr);
    void stopSolving();

    void startPlaying(const MacroData& macro);
    void stopPlaying();

    void onProcessCommands(void* gameLayerPtr);
    void onAfterProcessCommands();
    void onLevelReset();
    void onPlayerDeath(void* playerPtr);
    void onLevelComplete();

    bool shouldBlockUserInput() const {
        return m_state == SolverState::Playing && !m_injecting;
    }

    void renderTrajectory(void* drawNodePtr);
    void clearTrajectoryNode() { m_activeDrawNode = nullptr; }

    SolverState getState() const { return m_state; }
    size_t getCurrentTick() const { return m_currentTick; }
    const MacroData& getActiveMacro() const { return m_activeMacro; }
    bool isSolving() const { return m_isSolving.load(); }
};
