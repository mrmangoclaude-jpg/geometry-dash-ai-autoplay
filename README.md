# AI AutoPlay — Geometry Dash Geode Mod

An autonomous pathfinding solver and macro replay engine built for Geometry Dash 2.2074 on Windows using the Geode SDK.

---

## Features

- **Visual Fast-Forward Solver**: Watch the AI explore levels at 25x–50x speed with instant respawn until it discovers the optimal path to 100%.
- **Supports All 8 Gamemodes**: Handles Cube, Ship, Ball, UFO, Wave, Robot, Spider, and Swing Copter with specialized corridor-centering and edge-detection logic.
- **Persistent Level Library**: Solved winning runs are automatically serialized and saved to disk by Level ID in JSON format (`saved_autoplays/<levelId>.json`).
- **1-Click AutoPlay**: Automatically replays saved winning macros at normal 1x speed with 100% frame accuracy.
- **In-Game Keybinds & Pause Menu**:
  - **`G` Key**: Instant toggle to solve current level or play saved macro.
  - **Pause Menu Button**: Opens the full AI AutoPlay Hub popup to view attempt stats, manage macros, or clear saved runs.

---

## Directory Structure

```text
geometry-dash-ai-autoplay/
├── mod.json                     # Geode mod manifest & settings
├── CMakeLists.txt               # CMake build configuration
├── about.md                     # Geode in-game description
├── README.md                    # Project documentation
├── src/
│   ├── main.cpp                 # Mod entrypoint & keyboard dispatcher hook ('G' key)
│   ├── macro/
│   │   ├── MacroTypes.hpp       # PlayerAction and MacroData structures with JSON serde
│   │   ├── MacroManager.hpp     # Disk storage & library manager header
│   │   └── MacroManager.cpp     # Macro saving, loading, and directory resolution
│   ├── solver/
│   │   ├── GamemodeController.hpp # 8-gamemode physics classifier & sensor evaluation
│   │   ├── GamemodeController.cpp # Flight corridor & jump edge logic
│   │   ├── SolverEngine.hpp     # Fast-forward pathfinder & backtracking stack
│   │   └── SolverEngine.cpp     # Decision tree exploration & win/death hooks
│   ├── hooks/
│   │   ├── GJBaseGameLayerHook.cpp # Delta speedhack & input injection
│   │   ├── PlayerObjectHook.cpp    # Instant death interception (skips death lag)
│   │   ├── PlayLayerHook.cpp       # Level lifecycle & auto-detection of saved runs
│   │   └── PauseLayerHook.cpp      # Pause menu button injection
│   └── ui/
│       ├── AutoPlayPopup.hpp    # Hub modal interface header
│       └── AutoPlayPopup.cpp    # Status card, solve button, and macro browser
└── tests/
    ├── test_macro.py            # Macro serialization & file storage test
    ├── test_gamemodes.py        # Gamemode physics evaluation test
    ├── test_solver_backtrack.py # Branch-and-bound backtracking test
    └── run_all_tests.py         # Full test suite runner
```

---

## Building the Mod

1. Open a terminal in this directory:
   ```powershell
   cd "C:\Users\Eigenaar\.gemini\antigravity\scratch\geometry-dash-ai-autoplay"
   ```
2. Build with Geode CLI:
   ```powershell
   geode build
   ```
   *The generated `.geode` package will automatically be placed into your Geometry Dash `geode/mods/` folder.*

---

## In-Game Usage

1. Launch Geometry Dash and open any level (official or custom online level).
2. Press **`G`** on your keyboard (or press Escape -> click the green AI AutoPlay button -> click **Solve Level**).
3. The game will visually accelerate to fast-forward speed as the bot explores paths.
4. As soon as the bot reaches 100%, the level will automatically reset and play through smoothly at normal speed.
5. Next time you enter that level, the mod detects your saved run and gives you instant 1-click AutoPlay!
