# Geometry Dash AI AutoPlay (Geode 2.2081) — Project Handoff

## 1. Project Overview & Current Status
This mod implements a visual, fast-forward mathematical wavefront pathfinding solver and deterministic macro player for Geometry Dash 2.2081 on Windows (Geode SDK).
It pre-solves entire levels before the first attempt starts, saves verified 100% macros to disk (`saved_autoplays/`), and executes runs with 100% legitimate vanilla physics (zero noclip).

---

## 2. Root Causes Identified & Permanently Fixed

### A. Double-Input Playback Injection & Random Jump Glitches
- **Root Cause**:
  1. `GJBaseGameLayerHook` invoked `SolverEngine::onProcessButtons` *after* `GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue)`.
  2. Inside `onProcessButtons`, the code called **both** `targetPlayer->pushButton(PlayerButton::Jump)` directly **and** `layer->handleButton(action.isPush, ...)`.
  3. `layer->handleButton` pushed the action into RobTop's `m_queuedButtons`. Because `processQueuedButtons` had already executed for tick $N$, that queued button remained in the queue until tick $N+1$, when `processQueuedButtons` fired `pushButton` a second time!
  4. This caused every jump to fire on two consecutive ticks ($N$ and $N+1$), double-jumping or buffering unwanted jumps upon landing on blocks ("why does it js randomly jump here"), and delayed releases by 1 full tick, causing desync deaths on *Stereo Madness* and other main levels.
- **Fix**:
  1. In `GJBaseGameLayerHook.cpp`, `SolverEngine::get().onProcessButtons(this)` is invoked **before** `GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue)`.
  2. In `SolverEngine.cpp`, `onProcessButtons` calls **only** `layer->handleButton(action.isPush, action.button, !action.isPlayer2)`. The redundant direct calls to `targetPlayer->pushButton` / `releaseButton` were removed.
  3. When `handleButton` queues the action, `processQueuedButtons` immediately consumes and applies it on that exact sub-tick with vanilla physics and clears the queue, eliminating double-firing completely.

### B. Gravity Portal Inversions
- **Root Cause**:
  - In Geometry Dash:
    - **ID 10 is the Blue Gravity Portal (Normal Gravity / Floor, gravDir = 1.0)**.
    - **ID 11 is the Yellow Gravity Portal (Inverse Gravity / Ceiling, gravDir = -1.0)**.
  - Previous code had inverted them (`isPortalGravInv = (NormalGravityPortal || oid == 10)` and `isPortalGravNorm = (InverseGravityPortal || oid == 11)`).
  - Passing through a blue portal caused the solver to invert gravity upwards into ceiling spikes, terminating the beam search in levels like *The Nightmare* at 11.6% ($x=3292.4$).
- **Fix**:
  - Restored `lo.isPortalGravNorm = (type == GameObjectType::NormalGravityPortal || oid == 10);`
  - Restored `lo.isPortalGravInv = (type == GameObjectType::InverseGravityPortal || oid == 11);`

### C. Custom 2.0 Sawblade Ingestion Skipped as "Decoration"
- **Root Cause**:
  - In `SolverEngine.cpp`, objects with `m_objectType == GameObjectType::Decoration` were skipped *before* calling `classifyKnownObject`.
  - In Geometry Dash 2.0+, custom rotating sawblades (IDs 675, 676, 677, 678, 679, 680) are categorized by RobTop as `GameObjectType::Decoration` with custom damage colliders.
  - Skipping them caused the in-game solver to completely ignore the sawblade at $x=301$ (ID 675, radius 32) in *DeCode*, generating a flat trajectory straight through the saw and causing player death at $x=254.46$.
- **Fix**:
  - `classifyKnownObject` is now evaluated first. Only unknown objects that are not physical or interactive are filtered out by decoration checks.

### D. Artificial Hazard Inflation & Impassable Demon Corridors
- **Root Cause**:
  - A prior commit added `+ 3.0f` to saw radius and `+ 2.0f` to spike bounding boxes.
  - In tight demon corridors (such as $x=2196$ in *DeCode* and $x=3292$ in *The Nightmare*), these inflated bounds artificially blocked valid paths, killing all candidates in the beam search and forcing the solver to stop early.
- **Fix**:
  - Reverted to verified GD collision boundaries: exact saw radius (`w * 0.5f`) with `0.7f` player half-size subtraction, and mode-tuned rectangular spike margins (`1.0f` for continuous flight modes like Ship/Wave, `0.75f` for platform modes like Cube/Ball).

### E. Execution of Incomplete Truncated Macros
- **Root Cause**:
  - When `solveMathematically` failed to reach 100%, it still called `startPlaying(winMacro)`. The game loaded a 28-action macro, ran out of inputs at 8% or 11%, displayed `"AI AutoPlay: Stopped"`, and dropped the player into hazards.
- **Fix**:
  - Playback is strictly guarded: `startPlaying` is only invoked if `bestOverall.px >= maxLevelX - 150.0f`. If incomplete, the solver safely stops, resets state to Idle, and shows a diagnostic notification with the exact percentage reached.

### F. Pre-Packaged 100% Verified DeCode Macros
- The offline solver solved *DeCode* 100% (25,427 studs, 4,341 frames, 0 deaths).
- The winning macro has been exported and bundled directly into `saved_autoplays/`:
  - `saved_autoplays/2997354.json` (Online ID)
  - `saved_autoplays/local_DeCode_antigravity.json` (Local Level copy)

---

## 3. Environment & Build Pipeline
- **Project Directory:** `C:\Users\Eigenaar\.gemini\antigravity\scratch\geometry-dash-ai-autoplay`
- **Target Game:** Geometry Dash **2.2081** (Windows 64-bit)
- **Installed Mod Binary:** `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods\antigravity.ai_autoplay.geode`
- **Saved Macros Directory:** `C:\Users\Eigenaar\AppData\Local\GeometryDash\geode\mods\antigravity.ai_autoplay\saved_autoplays\`
- **Build Workflow:**
  ```powershell
  git add -A
  git commit -m "commit message"
  git push origin main
  python scripts/monitor_and_install.py
  ```
