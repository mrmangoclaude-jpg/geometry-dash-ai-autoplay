# Geometry Dash AI AutoPlay (Geode 2.2081) — Project Handoff

## 1. Project Overview & Current Status
This mod implements a visual, fast-forward mathematical wavefront pathfinding solver and deterministic macro recorder/player for Geometry Dash 2.2081 on Windows (Geode SDK).
It pre-solves entire levels before the first attempt starts, saves macros to disk (`saved_autoplays/`), and executes runs with 100% legitimate physics (zero noclip).

---

## 2. Recent Critical Bug Fixes & Architectural Upgrades

### A. In-Game Playback Input Injection (Fixing DeCode In-Game Deaths)
- **Root Cause**:
  1. `GJBaseGameLayerHook` previously executed inputs in `processCommands` *after* `GJBaseGameLayer::processCommands` had already finished `processQueuedButtons`, `m_player1->update(dt)`, and `checkCollisions` for that tick. Input was therefore delayed by a full tick.
  2. `if (!isHalfTick)` skipped half of the 240Hz sub-ticks.
  3. `p1->setPosition({action.xPos, action.yPos})` attempted coordinate snapping during active gameplay. In GD 2.2, snapping position without updating internal velocities and physics caches causes immediate penetration deaths in `checkCollisions`.
- **Solution (Reverse-Engineered from Eclipse & xdBot)**:
  - Disassembled `eclipse.eclipse-menu.geode` and `zilko.xdbot.geode`. Both production bots hook **`GJBaseGameLayer::processQueuedButtons(float dt, bool clearInputQueue)`**.
  - In `processQueuedButtons`, we call `orig(dt, clearInputQueue)` and immediately inject pending inputs:
    ```cpp
    targetPlayer->pushButton(PlayerButton::Jump); // or releaseButton
    layer->handleButton(action.isPush, action.button, !action.isPlayer2);
    ```
  - Input is processed natively before `PlayerObject::update(dt)` runs.
  - Position snapping (`setPosition`) has been completely removed.

### B. Portal ID Inversions & Passable Object Classification
- **Gravity Portals (IDs 10 & 11)**:
  - In GD, **ID 10 is the Blue Gravity Portal (Inverts Gravity to Ceiling)** and **ID 11 is the Yellow Gravity Portal (Normal Floor Gravity)**.
  - `SolverEngine` previously had these inverted (`isPortalGravNorm = oid == 10`), which caused the player in levels like *The Nightmare* to remain on the floor at $x=3315$ and collide with spikes. This has been corrected.
- **Passable / Fake Blocks (The Lightning Road)**:
  - Levels like *THE LIGHTNING ROAD* use decorative passable blocks with solid IDs (e.g. block 5 with `GameObjectType::Decoration`).
  - `classifyKnownObject` now respects `m_objectType == GameObjectType::Decoration` and ignores fake blocks completely.
- **Non-Interactive Portals**:
  - Mirror Portals (IDs 29, 30, 45, 46), Dual Portals (IDs 59, 60), and Teleport Portals (IDs 747, 749) are now safely treated as passthrough decorations.

### C. In-Game Debug Visualizer ("Show Trajectory") & Telemetry
- **Visual Trajectory Node**:
  - `PlayLayerHook` attaches a `CCDrawNode` (`autoplay-trajectory-node`) to `m_objectLayer` at z-order 9999 when `"show_trajectory"` is enabled in mod settings.
  - Renders the solved trajectory as a glowing line (green for 100% completion, orange for partial).
- **Death Marker & Notifications**:
  - When the player dies during playback, a red dot is placed at `(deathX, deathY)`, an on-screen toast is shown (`AutoPlay: Died at x=... y=...`), and detailed telemetry (velocity, gravity, distance from expected macro action) is printed to the Geode console.

### D. Dangling Pointer Crash on Exit/Re-Enter & Hazard Margin Fix
- **Root Cause of Crash on Level Re-Entry**:
  - `SolverEngine::m_activeDrawNode` previously held a raw `void*` pointer to the `CCDrawNode` instance created in `PlayLayer`.
  - When the player exited a level and re-entered, the previous `PlayLayer` and its children were deallocated, but `m_activeDrawNode` remained non-null in the singleton `SolverEngine`.
  - Upon re-entering, `PlayLayer::init()` called `resetLevel()`, which invoked `SolverEngine::onLevelReset()` -> `renderTrajectory(m_activeDrawNode)` on the destroyed node.
  - Cocos2d-x attempted to write vertices to the deallocated `m_pTriangles` buffer (pointing to `0x0`), triggering `EXCEPTION_ACCESS_VIOLATION (0xC0000005)` in `CCDrawNode::drawSegmentEx`.
  - **Fix**:
    1. `SolverEngine::clearTrajectoryNode()` is called on `onQuit()` and in `PlayLayerHook`'s lifecycle.
    2. `stopPlaying()` and `stopSolving()` reset `m_activeDrawNode = nullptr` and clear path memory.
    3. `onLevelReset()` no longer calls `renderTrajectory` directly; rendering is safely mediated by `PlayLayerHook` via `getOrCreateTrajectoryNode()`.
    4. `renderTrajectory()` guards against detached nodes (`!drawNode->getParent()`), checks NaNs/Infs, skips zero-length segments, and caps segments to 300 points to prevent buffer overflows.
- **Root Cause of Death at x=254.46 in DeCode**:
  - The offline macro `local_DeCode_antigravity.json` had its opening jump at `x = 264.84` due to aggressive grounded-state preference in beam sorting and lack of hazard safety buffers.
  - The cube physically collided with the floor sawblade (ID 675, center at $x=301$, radius 32) at $x = 254.46$ before reaching the scheduled jump point.
  - **Fix**: Added a 3.0-stud collision safety buffer to circular sawblades (`radius = w * 0.5f + 3.0f`) and 2.0 studs to hazards, forcing the solver to initiate jumps at $x \approx 244..249$ with ample clearance. Purged the outdated pre-generated macro from disk.

---

## 3. Environment & Build Pipeline
- **Project Directory:** `C:\Users\Eigenaar\.gemini\antigravity\scratch\geometry-dash-ai-autoplay`
- **Target Game:** Geometry Dash **2.2081** (Windows 64-bit)
- **Installed Mod Binary:** `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods\antigravity.ai_autoplay.geode`
- **Saved Macros Directory:** `C:\Users\Eigenaar\AppData\Local\GeometryDash\geode\mods\antigravity.ai_autoplay\saved_autoplays\`
- **Build Command:**
  ```powershell
  git add -A
  git commit -m "commit message"
  git push origin main
  python scripts/monitor_and_install.py
  ```
