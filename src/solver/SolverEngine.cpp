#include "SolverEngine.hpp"
#include "LevelExporter.hpp"
#include "../macro/MacroManager.hpp"
#include "dpcore.hpp"

#include <thread>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <algorithm>

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;
#endif

SolverEngine& SolverEngine::get() {
    static SolverEngine instance;
    return instance;
}

void SolverEngine::startPlaying(const MacroData& macro) {
    m_state = SolverState::Playing;
    m_activeMacro = macro;
    m_playbackIndex = 0;
    m_currentTick = 0;
    m_injecting = false;
    m_isHolding = false;

#if __has_include(<Geode/Geode.hpp>)
    log::info("[AI AutoPlay] Playing macro for level {}: {} actions, total frames {}",
              macro.levelName, macro.actions.size(), macro.totalFrames);
#endif
}

void SolverEngine::stopPlaying() {
    m_state = SolverState::Idle;
    m_activeMacro = {};
    m_playbackIndex = 0;
    m_currentTick = 0;
    m_injecting = false;
    m_isHolding = false;
}

void SolverEngine::stopSolving() {
    m_isSolving = false;
    m_state = SolverState::Idle;
}

void SolverEngine::onLevelReset() {
    m_currentTick = 0;
    m_playbackIndex = 0;
    m_injecting = false;
    m_isHolding = false;
}

void SolverEngine::onProcessCommands(void* gameLayerPtr) {
    if (m_state != SolverState::Playing) return;

#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<GJBaseGameLayer*>(gameLayerPtr);
    if (!layer || !layer->m_player1) return;

    auto p1 = layer->m_player1;
    if (p1->m_isDead) return;

    // Inject all actions scheduled up to this physics substep tick
    while (m_playbackIndex < m_activeMacro.actions.size()
           && m_activeMacro.actions[m_playbackIndex].frame <= m_currentTick) {
        const auto& act = m_activeMacro.actions[m_playbackIndex];

        m_injecting = true;
        layer->handleButton(act.isPush, act.button, !act.isPlayer2);
        m_injecting = false;

        if (!act.isPlayer2) {
            m_isHolding = act.isPush;
        }

        m_playbackIndex++;
    }
#endif
}

void SolverEngine::onAfterProcessCommands() {
    if (m_state == SolverState::Playing) {
        ++m_currentTick;
    }
}

void SolverEngine::onPlayerDeath(void* playerPtr) {
#if __has_include(<Geode/Geode.hpp>)
    if (m_state == SolverState::Playing) {
        log::warn("[AI AutoPlay] Player died at tick {}, playback index {}/{}",
                  m_currentTick, m_playbackIndex, m_activeMacro.actions.size());
    }
#endif
    m_isHolding = false;
}

void SolverEngine::onLevelComplete() {
#if __has_include(<Geode/Geode.hpp>)
    log::info("[AI AutoPlay] Level completed 100%!");
    geode::Notification::create("AI AutoPlay: Level Complete 100%!", geode::NotificationIcon::Success)->show();
#endif
}

void SolverEngine::solveLevelAsync(int levelId, const std::string& levelName, void* gameLayerPtr) {
    if (m_isSolving.load()) return;

#if __has_include(<Geode/Geode.hpp>)
    auto layer = static_cast<GJBaseGameLayer*>(gameLayerPtr);
    if (!layer || !layer->m_objects) return;

    m_isSolving = true;
    m_state = SolverState::Solving;
    m_currentLevelId = levelId;
    m_currentLevelName = levelName;

    // 1. Export live level objects from PlayLayer memory to CSV
    std::string csv = LevelExporter::exportLevelCsv(layer);
    float maxX = LevelExporter::getMaxLevelX(layer);

    log::info("[AI AutoPlay] Exported level CSV ({} bytes), maxX: {:.1f}", csv.size(), maxX);
    geode::Notification::create("AI AutoPlay: Solving level in background...", geode::NotificationIcon::Loading)->show();

    std::thread([this, csv, levelId, levelName, maxX]() {
        long long horizon = static_cast<long long>(maxX) + 2000;
        if (horizon < 3000) horizon = 3000;

        auto tempDir = std::filesystem::temp_directory_path();
        std::string planPath = (tempDir / ("autoplay_plan_" + std::to_string(levelId) + ".txt")).string();

        std::vector<std::string> args = {
            "--out", planPath,
            "--horizon", std::to_string(horizon),
            "--threads", "4",
            "--groupfire"
        };

        log::info("[AI AutoPlay] Running reachability DP solver (horizon: {}, out: {})...", horizon, planPath);
        int rc = dpcore::solveInProcess(csv, args);
        auto outcome = dpcore::outcome();
        log::info("[AI AutoPlay] Solver finished: rc={}, verdict={}, deepX={:.1f}, workStates={}",
                  rc, outcome.verdict, outcome.deepX, outcome.workStates);

        bool solved = false;
        MacroData newMacro;
        newMacro.levelId = levelId;
        newMacro.levelName = levelName;
        newMacro.completionPercentage = 100.0f;

        if (rc >= 0 && std::filesystem::exists(planPath)) {
            std::ifstream infile(planPath);
            std::string line;
            while (std::getline(infile, line)) {
                if (line.rfind("input=", 0) != 0) continue;
                int t = 0, d = 0;
                if (sscanf(line.c_str() + 6, "%d,%d", &t, &d) == 2) {
                    PlayerAction act;
                    act.frame = static_cast<size_t>(t);
                    act.isPush = (d & 1) != 0;
                    act.button = 1;
                    act.isPlayer2 = false;
                    newMacro.actions.push_back(act);
                }
            }
            if (!newMacro.actions.empty()) {
                newMacro.totalFrames = newMacro.actions.back().frame + 100;
                solved = true;
            }
        }

        m_isSolving = false;

        if (solved) {
            MacroManager::get().saveMacro(newMacro);
            log::info("[AI AutoPlay] Successfully solved level! Level: {} (ID {}), Actions: {}, Total Frames: {}",
                      newMacro.levelName, newMacro.levelId, newMacro.actions.size(), newMacro.totalFrames);

            geode::Loader::get()->queueInMainThread([this, newMacro]() {
                startPlaying(newMacro);
                geode::Notification::create("AI AutoPlay: Level Solved 100%!", geode::NotificationIcon::Success)->show();
            });
        } else {
            log::warn("[AI AutoPlay] Solver could not find 100% path on initial pass.");
            geode::Loader::get()->queueInMainThread([this]() {
                m_state = SolverState::Idle;
                geode::Notification::create("AI AutoPlay: Could not solve completely", geode::NotificationIcon::Warning)->show();
            });
        }
    }).detach();
#endif
}

void SolverEngine::renderTrajectory(void* drawNodePtr) {
    // Optional debug trajectory rendering
}
