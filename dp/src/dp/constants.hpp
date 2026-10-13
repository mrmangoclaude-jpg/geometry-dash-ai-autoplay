#pragma once
#include "dp/modifiers.hpp"

namespace dp {

constexpr double kFloorY = 105.0;   // cube rest height on the ground line
// ...which is a REST HEIGHT, not the ground. The ground plane is at 90 and the
// rest height is 90 + the player's half, so a mini player rests at 99, not 105.
// Measured on lv11 t=9793: an inverted mini ship descending at vy = -3.424 is
// stopped dead at y = 99.000 and held there. The code below therefore uses
// kGroundY + pHalf everywhere it used to use kFloorY.
constexpr double kGroundY = kFloorY - 15.0;   // 90.0
constexpr double kCubeHalf = 15.0;  // outer box (landing)
// Mini. Measured on lv11: the cube crosses the mini portal (rect x[4409.5,
// 4440.5], so the boxes touch at x = 4394.5) at t=3384 and immediately starts
// falling from its resting y=165, then settles at **y=159.000** on the same
// continuous platform whose top is 150. 159 - 150 = 9, i.e. the half size goes
// 15 -> 9 (0.6x). Without this the model kept standing at 165 on a surface GD
// had already dropped away from.
constexpr double kMiniHalf = 9.0;
constexpr double kMiniScale = 0.6;
// Mini jumps LOWER. Measured on lv11 t=3528: same tick, same x, both grounded
// at y=159, GD leaves with vy = 8.9440 and the model left with 11.1800.
// 8.944 / 11.18 = 0.800 exactly. Gravity is NOT scaled -- both sides step
// -0.2160 per tick from there, and the traces stay exactly 2.2360 apart in vy
// until the landing. Without this the model floats 40 px above GD through the
// whole mini section and every hazard test after it is answered for the wrong
// player.
// UNVERIFIED for mini: terminal velocity, and the orb/pad impulses. Measure
// them the same way when a plan first depends on one.
constexpr double kCubeJumpMini = 8.944;
// ...and the same 0.800 applies to the launch impulses. Measured by injecting
// the mini cube onto lv11's yellow pad (x[7888.5,7913.5], t=6076): GD returned
// vy = 12.800, and the full-size pad is 16.0 -- 12.800 / 16.0 = 0.800 exactly,
// the same factor the jump showed. Two independent measurements agreeing to
// four digits is a rule, so it is applied to the orbs too.
// NOTE the orb side is INFERRED from those two, not measured directly. lv11's
// yellow orb at (8055,255) is the first plan that will depend on it: if the
// factor is wrong there, the GD replay diverges at that orb and says so.
// (This is separate from the per-MODE ratios -- ball is 0.600/0.700 -- which
// really do differ per constant and must stay measured one by one.)
constexpr double kMiniImpulse = 0.800;
constexpr double kCubeInner = 4.5;  // side/ceiling kill box
// collidedWithObjectInternal's reach for the flight modes (ship / UFO / swing): a body within it
// of a face is landed or stopped there, not crushed (the other modes use 10). The --preslopesolid
// thin solid uses the same numbers; --shipslopecap derives a ship's flat-side distance from a
// spiked ramp from it, min(kCubeInner, half - 6): 4.5 full, 3.0 mini, both as measured.
constexpr double kFlyThinTol = 6.0;
// ---- ROBOT (mode 5) -------------------------------------------------------
// Read out of PlayerObject::updateJump (win RVA 0x38b900), not fitted. The
// robot is a cube with three differences, all of which are literal branches in
// that function:
//
//  1. GRAVITY x0.9. updateJump picks a per-mode scale (xmm7) right after the
//     mode flags: ball 0.6, spider 0.6, ROBOT 0.9, everything else 1.0, and it
//     multiplies the `dt * gravity` term that is subtracted from m_yVelocity.
//     It is NOT applied to the jump.
//  2. JUMP x0.5. `if (m_isRobot) jumpHeight *= 0.5` sits immediately after the
//     jump height is read (m_gravity's sibling double at this+0x7c0), before
//     the mini factor. So a robot tap leaves at half a cube's jump for the
//     same speed, and mini still multiplies by 0.800 on top.
//  3. HOLD = HOVER, not thrust. While the button stays held after the jump,
//     updateJump ADDS exactly the same `sign * dt * gravity * 0.9` it is about
//     to subtract, so vy is constant -- the robot floats. The budget is a
//     double at this+0x830 that is zeroed when the jump fires and grows by
//     `dt * 0.1` per tick while the hover runs; the hover stops once it
//     reaches 1.5. With dt = kCubeG / 0.9581990 = 0.225422 (the same dt that
//     makes the cube's gravity -0.216) that is 1.5 / (0.1 * 0.225422) = 66.5,
//     i.e. 67 ticks.
//     Releasing the button sets a flag (this+0x99c, cleared only by the next
//     jump), so a release ENDS the hover permanently -- re-pressing in mid-air
//     does not resume it.
//
// Also from the same function: the robot's jump needs m_jumpBuffered as well as
// the hold, i.e. a fresh press. Holding through a landing does NOT re-jump.
// The model's cube already tests the rising edge (`input && !s.action`), so
// that comes for free.
// CONFIRMED WITH THE GAME (2026-09-03), after step.hpp had spent two weeks
// asserting the opposite on the strength of a citation that no longer resolves:
// on calib_holdjump_robot (flat ground, one press at t=30, never released) the
// robot jumps at t=31 and is still standing on the floor at t=600, while the
// cube on the identical rig bounces five times in the same window. The gate is
// +0x986 next to m_jumpBuffered (updateJump 0x38ba3c..0x38ba51), cleared by the
// arm that jumps (0x38c728) and restored only by releaseButton (0x398260).
constexpr double kRobotGScale = 0.9;
constexpr double kRobotJumpScale = 0.5;
constexpr int kRobotHoverTicks = 67;
// ---- SPIDER (mode 6) ------------------------------------------------------
// Same shape as the robot: the mode table in updateJump gives it the BALL's
// gravity scale (0.6), and its action is not a jump at all -- it is
// PlayerObject::spiderTestJump, a teleport to the surface on the other side.
//
// Measured on lv22 (injected onto the spider section at x~1,010, full size):
//   gravity  -0.129 / tick, i.e. 0.216 x 0.600 -- the ball's number to three
//            digits, on 13 consecutive airborne ticks
//   rest     y = 163.500 on a floor whose top is 150, and y = 286.500 under a
//            ceiling whose underside is 300. Both are 13.5 off the surface,
//            not the 15 every other ground mode uses.
//   teleport t=262 y=163.5 upright -> t=263 y=286.5 upsideDown, one tick, and
//            the gravity is flipped. The target was the block at (1035,315),
//            whose x range starts 9.5 px to the RIGHT of the player, so the
//            search is at least as wide as the player's own box.
//   gate     a tap in MID-AIR does nothing (13 ticks of undisturbed free fall
//            after one). GD only calls spiderTestJump from the grounded branch
//            of updateJump, same as the cube's jump.
// Rings in flight: GD closes the CONTACT path in ship/UFO/wave/swing
// (playerTouchedRing's last gate), so there a ring fires only from the press
// itself. (--no-ringmode, held-fires-in-every-mode, is gone since the flag clean-up.)
// --oriringnow: a ROTATED ring (Obj::oriented) is decided by the row after the
// press alone, not by whichever of the press row and that row is nearer (the
// ring loop's `usePre`). On since 2026-09-21, and always on since the flag
// clean-up. The measurement is at the ring loop.
// --swingpushtol: the swing's push-out onto a face needs the face within
// kLandTol, as GD's face pick does; beyond it the contact is a side hit.
// On by default since 2026-10 (was opt-in); see the push-out branch in step.hpp.
// (The switch is gone since the 0.4.0 clean-up; its on behaviour is fixed.)
// A ring's press latch is GD's +0x986, which every consumer clears, so the
// grounded jump spends the press too. (--no-pressspent, the pre-2026-09-05
// `!s.ringHold` gate, is gone since the flag clean-up.)
// Deaths during the PLAN RECONSTRUCTION, which is a different question from
// deaths during the search. 2026-09-08, lv22@5400 at cap 8000: the search never
// put a state on the spike uid 5897 (0 of ~1,478 evaluations), the reconstructed
// plan walked into it (16 hits), the sub-step loop exited on `!dead` so DIE ran
// -- and the run still reported SOLVED and wrote a trace past the death. So a
// SOLVED verdict does not mean the plan survives its own replay, and nothing
// counted how often that happens.
//
// ONLY the reconstruction is counted, deliberately. The search runs on eight
// threads and a plain counter there would be a data race; `g_inRecon` is written
// once, between the search and the reconstruction, and only read during the
// search, so the branch below costs a predictable read and no synchronisation.
inline bool g_inRecon = false;      // set after "reconstructing plan"
inline long long g_dieRecon = 0;    // DIEs while g_inRecon
// ...and split by cause, because the total is not a number about physics.
// `out-of-play` (step.hpp:8211) is a SEARCH PRUNE -- the upright escapee bound,
// not a kill -- so a run reporting n=666 last=out-of-play may be 666 prunes and
// zero kills, or 665 and one. The total plus the last cause cannot tell those
// apart, and reading a whole census off the last entry is the mistake this file
// records elsewhere as "the last witness is not the population".
inline std::vector<std::pair<const char*, long long>> g_dieReconWhy;
// The same question asked where it can be answered: the witness resim's own
// `rdead`, which is the final plan's single walk. g_dieRecon counts every DIE
// the reconstruction runs; these count only the walk, so they say whether the
// PLAN dies. Contiguity (last - first + 1 == dead) separates "one corpse
// re-dying every tick" from "several deaths", which share a total.
inline long long g_resimDead = 0;
// The plain head bonk's discriminant is the id-1859 arm (State::armT), not the `robot ||
// mini cube` proxy: always so since 2026-09-26 (it was --bonkarm). See the bonk gate in
// step.hpp; the search key reads armT for it too.
// --witnessframe: the witness walk binds the geometry of the frame the
// call STARTS in, as --replay does, instead of frame 0. Not print-only: the walk also
// fills modeAt, which sets the emitted plan's edge latencies. See cli.hpp at rLf.
// Always on; the switch is gone since the flag clean-up.
// --vetophys (default off): a SOLVED whose correctly bound witness walk dies of a
// physical cause (hazard / solid-side / crush) is published as the PARTIAL at that
// death. See the demotion after the witness walk in cli.hpp.
inline bool g_vetoPhys = false;
// --resimladder (default on since 2026-10-10; --no-resimladder is the off arm): a SOLVED whose
// witness walk dies of physics is not handed out as it
// is. The call searches again from the walk's own state a few ticks before that death (the rungs
// below, then the next death if the walk moves on), splices the answer onto the plan's prefix, and
// walks the whole plan again from t0; it keeps the plan whose walk dies latest, or is clean. The
// repair loop otherwise flies the plan, GD dies where the walk did (a death the search sent), and
// re-anchors there -- a round per death. A second search from the SAME anchor returns the same
// plan (the 2026-09-28 preflight: the search's representative is far from the walk by then), so
// the new search starts at the death, from the exact state. See cli.hpp after the witness walk.
inline bool g_resimLadder = true;
constexpr int kResimLadderMax = 4;                        // searches per call
constexpr long long kResimLadderRungs[] = {6, 24, 96, 240};   // ticks before the walk's death
// --resimladderpartial (default off, and nothing without --resimladder): the ladder also takes a
// PARTIAL whose walk dies short of its frontier, and keeps a PARTIAL answer for any call. Without
// it only a SOLVED (or joined) plan is laddered, and only a SOLVED answer is kept. Measured on
// a custom level, new base 9d1f41ba: without the ladder 305 s, with it and this
// 509-517 s (two builds), a wall at t~10,300 that cost a 102 s section solve.
inline bool g_resimLadderPartial = false;
// ...held by the cap ladder (cliMain) around an attempt whose answer the ladder may still replace:
// such an attempt's verdict is the ladder's to read, so a PARTIAL is left as it is and a SOLVED
// keeps only a SOLVED. Not reset per call; cliMain sets and clears it. On that level the same
// PARTIAL was laddered once per attempt, seven times over, at about 7 s each.
inline bool g_resimLadderHold = false;
// --dropnocollide: do not ingest id-1910 as a collidable solid.
//
// Measured (ledger run D): GD sets [obj+0x515] on lv22's uid 4705 (id 1910) and
// collidedWithObjectInternal returns false for it regardless of geometry -- 50 of 81
// hbin rows had the player's 9x9 inner box inside the object, one at full penetration,
// and hit was 0 on every one. Both neighbouring id-1 solids read [obj+0x515] == 0 and
// collide normally. The model ingests 4705 as an ordinary 15x15 solid and kills at
// t=4,889 on an object the game will never touch.
//
// The discriminator here is the ID, not the flag: +0x515 is NOT exported to objrects
// (it is absent from solver.hpp's column list), so the level data cannot express the
// real condition. id 1910 occurs exactly once in the corpus -- this object -- so the
// exclusion is one object wide, not a class rule. Match on the id COLUMN only: the same
// dump carries an unrelated uid 1910 (id 1268, type 20), and a loose numeric match would
// silently drop the wrong row. Always on; the switch is gone since the flag clean-up.
inline constexpr int kNoCollideId = 1910;
// --verdictinfo (default off): when a SOLVED plan's own witness walk died, print one
// `vinfo:` line carrying the verdict beside that death -- tick, cause, uid, object,
// frame. Print only. The verdict is NOT changed and the plan is NOT withheld: the
// direction is "send the plan, attach the known death as information" (audit 05:02),
// and the SOLVED/PARTIAL definition stays exactly where it is.
//
// Why it is worth a line at all: 23 of 423 witness walks in the 2026-09-08 cold run
// died, all on lv20/lv22, and none of those deaths matched GD's within +-30 ticks
// (0/18). So the witness death is not evidence the route dies -- which is precisely
// why this is information attached to a plan rather than a reason to demote it.
inline bool g_verdictInfo = false;
inline float g_resimPX = 0.f;   // the player's world x at the witness's first death
// ...and what killed it, taken at the FIRST dying tick. Without this, "the
// model died and GD died" can only be matched on the fact of a death, and two
// deaths at different places in the level read as agreement -- the model would
// be credited with knowing something it did not know.
inline const char* g_resimWhy = nullptr;
// ...and WHO, taken at the same tick. A cause string says `cube/hazard`; it does
// not say which object, and on lv20 the answer decides an open question: the
// model-visible hazards near the death (type 2/47 -- level_loader.hpp:923 emits
// nothing else) are all static, all `groups=0`, and none of them overlaps the
// player's box, so the killer has to be moving geometry. Naming it turns that
// from an elimination into an identification.
// The search's side of the same question the resim's `trig=` answers. The two
// walks apply triggers with different masks -- the group's (cli.hpp:3026) and
// the state's own (cli.hpp:4066) -- so a moving object can be in one place for
// the search and another for the resim, and on lv20 that is the whole
// disagreement. -1 = off; set to the tick to look at.
inline long long g_trigDbgT = -1;
// --coindbg <tick>: where the model put each coin at that tick, and which ones
// its mask left switched off (cli.hpp). The mod prints GD's own answer as
// `coinlive:`, so the pair SAYS whether the two disagree about a coin's
// position, where otherwise it can only be inferred from a refused credit.
// -1 = off.
inline long long g_coinDbgT = -1;
// One "I have this one" line per coin under --coindbg, not one per state: the
// search reaches the same collection from thousands of branches.
inline unsigned char g_coinSaid[8] = {0, 0, 0, 0, 0, 0, 0, 0};
// ...and one per pickup, for the same reason (g_collect is capped at 16).
inline unsigned char g_itemSaid[16] = {};
// --coindbg: the CLOSEST any state got to each coin while inside its x bound.
// "Never collected" has three readings -- the x bound was never entered, or it
// was entered but no state came within the y bound, or a state was inside both
// and the collect still did not happen -- and the layer lines cannot separate
// them, because they print a min/max ENVELOPE of y rather than whether any
// state sits in the coin's band. This is the number that does. -1 = the x bound
// was never entered.
inline double g_coinNearDy[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
inline double g_coinNearX[8] = {};
inline double g_coinNearY[8] = {};
inline long long g_coinNearT[8] = {};
// ...AND THE DENOMINATOR, because "never entered" is the shape of 0 this
// project keeps mis-reading: it reads the same whether no state entered the
// bound or the search never got that far. g_coinSeen is how many states were
// examined inside the x bound (0 is the interesting case) and g_coinProbeMaxX
// is the deepest x the coin test itself ever saw, so a 0 can be told from a
// "not yet" on the line that reports it.
inline long long g_coinSeen[8] = {};
inline double g_coinProbeMaxX = -1e18;
// How often does the group's collapse of the firing tick actually collapse
// anything? `gFire` is max(trigT) over the members (cli.hpp:2974) and the group
// key already pins `trig`, so the masks always agree -- the spread is in trigT
// alone. A group whose members share one trigT is not approximated at all.
// Counting the passes separately from the firings is the lesson of `!c.flip`,
// where a gate was entered 593 times and fired zero.
inline long long g_gfireGroups = 0;   // groups placed
inline long long g_gfireSpread = 0;   // ...of which the members disagree on trigT
inline long long g_gfireSum = 0;      // sum of (max-min) over those
inline int g_gfireMax = 0;            // widest spread seen, in ticks
inline int g_resimUid = -1;
inline float g_resimObjX = 0.f, g_resimObjY = 0.f;
inline TouchMask g_resimTrig{};  // the walk's own trigger mask at that tick
// ...and the FRAME the walk was in. g_resimObjX/Y are read in whatever frame
// the resim currently occupies (the loop re-binds rLf = &frameLevel(L,
// c.frame)), so two walks that report different positions may be reporting the
// same place seen from different frames -- on lv22 the two sides' killers turn
// out to differ by exactly a quarter turn, which was inferred from the numbers
// rather than measured. Reading the frame is what makes it a measurement.
inline int g_resimFrame = -1;
inline long long g_resimTicks = 0;
inline int g_resimFirst = -1;
inline int g_resimLast = -1;
inline void noteReconDeath(const char* why) {
    ++g_dieRecon;
    for (auto& e : g_dieReconWhy)
        if (e.first == why || (e.first && why && !std::strcmp(e.first, why))) {
            ++e.second;
            return;
        }
    g_dieReconWhy.emplace_back(why, 1LL);
}
// --hazdbg <uid>: trace ONE object down the ground-mode hazard loop in
// step.hpp, gate by gate. -1 = off, which is the default, so the shipped search
// is untouched and quick_regress stays byte-identical. A uid rather than a
// blanket switch because the search evaluates many states per tick and an
// unconditional print buries the answer.
inline int g_hazDbgUid = -1;        // --hazdbg
// --waveobbaabb: the WAVE's two-stage hazard test, as GD's danger loop runs it --
// stage 1 the player's rect against the object's rect (both axis-aligned), stage 2
// the player's box TURNED by its sprite rotation against the object's turned box.
// Without it, stage 1 of a turned hazard is the unturned player square against the
// turned box (hazardHit), which is stricter than both stages together once the wave
// is tilted. See the wave's hazard branch in step.hpp. On by default; --no-waveobbaabb is
// the off arm.
inline bool g_waveObbAabb = true;
// --wavehazendpoint: the wave's HAZARD test at the tick's end position only, not at the
// samples between the endpoints -- what --hazendpoint (speed.hpp) does for the cube and
// the flying body, whose game sweep found no kill on an interior-only overlap. The wave's
// branch is separate code and kept the sweep. Solids keep it here too. On by default;
// --no-wavehazendpoint is the off arm.
inline bool g_waveHazEndpoint = true;
// --waveoneway: a one-way plate (type 21) does not kill the wave, as it does not kill the
// flying body (step.hpp, fly/solid-side). On by default; --no-waveoneway is the off arm.
inline bool g_waveOneway = true;
// --tplatchexact: a teleport with a latch bit is judged "already fired" by the bit alone,
// not also by "was inside its box at the previous position" (step.hpp, portal pass).
// Off by default.
inline bool g_tpLatchExact = false;
// --tpseen: a teleport not judged by its latch bit is "already fired" when its own firing test
// passed on the previous tick (State::portSeen, the mode portals' three slots), not when the
// previous position lies inside its box. The position stand-in re-tests that position with this
// tick's rotation: on a custom level a mini ship tilting 1.6 deg a tick met a 747 (rot -1) whose
// oriented test missed by 0.010 px at t=8,507 (rotation 3.58) and passed at 8,508; the stand-in
// re-read 8,507 at 5.22 deg as inside, so the model never teleported, where GD did at 8,508
// (y 164.0 -> 182.865, the exit). That level's 25 teleports have no latch bit (128 bits, all taken
// by gravity portals). On by default since 2026-10; --no-tpseen turns it off.
inline bool g_tpSeen = true;
// --dyngplatch: a gravity portal carried by moving geometry (a grouped one, routed to L.dyn) gets a
// latch bit too (level_loader.hpp, after the static ones and before the teleports' --tplatch bits), so
// it is spent on its first overlap like the static ones and an anchor's portal= payload can name it.
// Without a bit it re-fires on every overlap. A custom level: a yellow gravity portal in a group
// (35 x 35 turned 45) overlaps an upside-down cube for three ticks, spent without effect; a gravity
// orb inside it turns the cube upright on the next tick, and GD keeps it upright through the rest of
// the overlap. The anchor's payload named the portal spent, the model had no bit to set, flipped the
// cube back on the next tick and the loop paid three iterations there. On by default since 2026-10;
// --no-dyngplatch turns it off.
inline bool g_dynGpLatch = true;
// --tpspawn: on the first step after the spawn a teleport is not judged "already fired" by the
// player having been inside its box at the spawn (step.hpp, portal pass). On by default;
// --no-tpspawn is the off arm.
inline bool g_tpSpawn = true;
// --padtouch: a pad touched exactly (|d| equal to the half sum) is a contact (step.hpp, pad
// pass). On by default; --no-padtouch is the off arm.
inline bool g_padTouch = true;
// --padrefill (default on since 2026-10): a pad that the pad memory (State::usedPad, four slots) does not hold,
// that the body already overlapped at the previous tick's position, and that one body could overlap
// together with a pad the memory does hold, is taken as fired then and remembered, not fired
// (step.hpp, pad pass). The last condition keeps a body that ARRIVED inside a pad out of it: the
// same level teleports at t=2,024 into an upside-down yellow pad, and GD fires it on 2,025 (the
// landing counts from the next tick) -- "inside at the previous tick" alone took that as old. GD's latch is per pad for the whole attempt; the
// memory forgets a pad once x is clear of it, and a pad that fired while all four slots were taken
// was never remembered at all -- so it fired again the moment a slot came free while the body was
// still inside it. A custom level's stack of six sideways yellow pads (two columns, three rows,
// 30 px apart): GD fires them once at t=3,442 (vy 16) and the body rises on through the stack;
// the model filled its slots with the first four and fired the fifth again at t=3,454 (vy 16 where
// GD has 13.42). Gravity and spider pads are left out (their facing gates can refuse a pad on one
// tick and take it on the next). Pads in the moving set are NOT left out -- that stack is in it
// with a one-row recording -- so a pad that moves into a body is tested at its current position
// against the body's previous one, which can take its first contact as an old one.
inline bool g_padRefill = true;
// --spiderorbface (default on since 2026-10): a spider orb (id 3004, type 43) jumps from the gravity of its own
// facing, not from the player's (step.hpp, ring pass). PlayerObject::ringJump (0x398c00), its 0x2b
// branch (from 0x399017, flipGravity at 0x39902a): unless sideways, when GameObject::isFacingDown(orb)
// differs from the player's +0x9bf it calls flipGravity first, and then spiderTestJump -- so the body ends on the far side of
// the ORB's facing with the opposite gravity, whatever gravity it came in with. The model always
// jumped from the body's own gravity, which is the same thing only when the two agree (lv22's two
// orbs). A custom level t=12,877: an upside-down cube presses on an upright spider orb (uid 816);
// GD turns it upright and jumps it UP to the underside of block 5631 (y 1755, still upside down
// after), the model jumped it DOWN to y 1635. A sideways section (isFacingLeft) is left as it was,
// and so is an orb with no surface on the far side (GD's flipGravity still runs there).
inline bool g_spiderOrbFace = true;
// --dashground (default on since 2026-10): a dash does not end when the body lands -- it lasts while the button is
// held, and while it lasts the body does not jump (step.hpp, the jump gate and the grounded branch).
// PlayerObject::updateJump (0x38b900) guards every jump arm with !m_isDashing (+0x9e4), and the
// collision side calls stopDashing only on a side contact (collidedWithObjectInternal, the two
// calls followed by +0x985 = 0); landing on a surface leaves the dash on. The model ended the dash
// with the landing (its dash line lives in the airborne branch only), and a cube still holding the
// button re-jumped on the next tick. A custom level t=9,897: an upside-down cube takes a dash ring,
// dashes down onto block 24042 at 9,906 and holds the button to 9,909; GD stays on the block (vy 0,
// onGround 1), the model jumped off it at 9,907 (vy -11.23).
inline bool g_dashGround = true;
// --spiderpad: the 2.2 spider pad (id 3005, GameObjectType 44) is loaded as a pad and fires in the
// pad pass: the gravity turned toward the pad's facing, then the spider's teleport (step.hpp, pad
// pass). On by default; --no-spiderpad is the off arm, under which the object is not loaded at
// all. The MOD's pad recorder follows the same answer (hooks_player.cpp, solverSpiderPad).
inline bool g_spiderPad = true;
// --gravtoggle: the gravity toggle portal (GameObjectType 42, id 2926) is a gravity portal that
// asks for the other gravity (level_loader.hpp, the portal bucket; step.hpp, the portal pass).
// On by default since 2026-10-05 (the disassembly, two injections, a 19-unit rig that GD and the
// model flip on the same 36 ticks, a kitref of 15 fixed and 0 broken in 1,773 episodes);
// --no-gravtoggle is the off arm, where the loader leaves the portal out.
inline bool g_gravToggle = true;
inline bool isGravPortalType(int type) {
    return type == 3 || type == 4 || (g_gravToggle && type == 42);
}
// --teleporttrig: the teleport trigger (id 3022) moves player 1 when its x is crossed: to its
// target's position, x and y unless kept, with the gravity its mode asks for (modifiers.hpp,
// TpTrig; step.hpp, the end of the tick). On by default since 2026-10-06 (the disassembly and an
// 18-unit rig the model follows; on the corpus neither a first divergence nor a clear moved). One
// body in a plain frame moving right; a sideways one, a touch- or spawn-started one and one in a
// dual or a turned frame stay out. --no-teleporttrig is the off arm, where the trigger is not
// loaded.
inline bool g_teleportTrig = true;
// --tpsideways (default on since 2026-10): ...and one whose target lies AHEAD in x is loaded too, x and all
// (level_loader.hpp; the end-of-tick move in step.hpp already writes xAbs, as a teleport portal's
// does). An x-crossing trigger fires on the same tick for every state that crosses it at the same
// x, so the jump moves the whole layer alike. A custom level's uid 151 at x=2,355 sends the ball to
// (2,445, 585), the ship portal: GD's row at t=1,760 jumps 90.6 px in x and 168.7 in y, and the
// model, which left the trigger out, ran 88 px behind GD from there -- every flight's deadband was
// placed at GD's x and never met the model's path. A target BEHIND stays out (crossing it again
// would fire the trigger again in the model, which fires once).
inline bool g_tpSideways = true;
// --tpportalx (default on since 2026-10): a teleport portal of the 2902 family whose exit lies AHEAD in x moves x
// too. teleportPlayer (0x20fdb0) puts the body on the exit object's real position, x and y, unless the
// portal keeps x (+0x771); only the 747 keeps the body's x by construction. The x is written at the end
// of the tick, where the teleport orb and trigger write theirs: the collision pass that fired it walks
// the sections it started in. A custom level's 2902 at x=27 sends the cube to (1,695, 1,155) -- GD's
// row 2 reads exactly that -- and the model, which moved y alone, ran on from x=0 a level below. GD
// moves nothing on row 1 (x stays 0 there), so such a portal fires from the second step of an attempt
// on, where --tpspawn lets a spawn-covered 747 fire on the first. One body in a plain frame moving
// right, as the trigger; an exit behind stays out (as --tpsideways leaves a trigger's).
inline bool g_tpPortalX = true;
// --widescan (default on since 2026-10): the x-window scan over a sorted-by-cx object list (state.hpp
// XSlice::forRange) moves its cursor and stops by each object's own edges, which follow cx only
// for objects of about one width: a wider one later in cx can still reach back into the window,
// and one behind the cursor can still reach forward into it. Under the flag the scan is exact: an
// object with a half width under kWideScanHw is caught by a rewind and a scan kWideScanHw past the
// break, and a wider one by its own list with a range test. A custom level's floor uid 3379 (2,250 px
// wide, centre x=2,835): GD's cube walks on it at x=2,061, and the model, whose scan never reached it,
// fell through. What the old scan reached is visited first and in the same order.
inline bool g_wideScan = true;
constexpr double kWideScanHw = 40.0;
// --p2xoff (default on since 2026-10): the second dual body keeps its own x (State::dx2, its offset from player 1).
// A teleport trigger moves player 1 only: EffectGameObject::triggerObject's case for id 3022 calls
// GJBaseGameLayer::teleportPlayer with no player, and teleportPlayer takes player 1 for that
// (layer+0xda0). A custom level's dual spider section: a trigger at x=16,815 sends player 1 back to
// 16,695 and the second body runs on at 16,815, 120 px ahead of player 1 for the rest of the dual,
// where the model, which has one x, met every block, orb and ceiling of player 2's at player 1's x.
// fixup.hpp stepBoth steps the second half at x + dx2 and moves dx2 by however much farther the one
// half went than the other (a teleport), a dual portal starts it at 0, and --startp2dx seeds it at
// an anchor; it is read only while the pair is dual. Frame 0 only, as measured. The key takes dx2 when it is not 0, so every state of a run
// without the flag (dx2 always 0) keys as before.
inline bool g_p2xOff = true;
// --startp2dx X: the anchor's dx2 (GD's p2x - x), read only under --p2xoff and only for a dual anchor.
inline double g_startP2dx = 0.0;
// --p2solo: the second dual body ends the dual on a solo portal the way the game does -- player 1
// takes the second body's attributes at that point of its pass (fixup.hpp stepBoth; step.hpp, the
// portal pass). On by default since 2026-10-07 (--no-p2solo is the off arm, where the second half's solo
// touch is dropped). GD: toggleDualMode 0x2168d0 with the second body as the toucher calls player 1's
// copyAttributes 0x3a04a0 from it. Measured on a custom level at t=10,012: p1 comes out at p2's y
// (1,279.130) wherever p1 itself is (GD probe, p1's y injected to 1,252 / 1,256 / 1,272: the same y).
// kitref A/B on 223 kits (2,162 episodes): 13 fail -> pass, no pass -> fail; quick unchanged.
inline bool g_p2Solo = true;
// --dualendrot (default on since 2026-10): ...and a WAVE hands over its sprite angle too. copyAttributes leaves the rotation
// alone, but toggleDualMode's last block sets player 1's rotation to the second body's (vt +0x150 with vt +0x158)
// when player 1 -- a wave once the second body's mode bytes are copied -- did not end the dual itself and the
// second body is a wave. The tick's updateShipRotation then turns that angle along the second body's move, its
// last position (+0x9f8) having been copied too. A custom level t=12,463: a mini wave pair, the second body
// (going up, -35.35) takes a dual-off portal; GD's player 1 comes out at -38.16 = -35.35 eased toward -63.43,
// where the model kept player 1's 33.0 and eased it to +36.04; three ticks later a 747 turned 45 degrees met the
// model's square turned the other way and teleported it a tick early.
inline bool g_dualEndRot = true;
// --p1soloend: a solo portal player 1 takes ends the dual before the second body's collisions
// (fixup.hpp stepBoth): the game checks player 2 only while the dual is on, so that body's portals,
// its gravity reaching player 1, and its death do not happen on the tick. A custom level t=10,012
// (a cube pair, p1 upside down, p2 upright): p1 enters the solo, inverse
// gravity and robot portals at x=17,665 together; GD's player 1 stays upside down with vy -0.862,
// where the model also ran p2's pass, whose inverse gravity portal set p1 upright and halved its vy.
// On by default since 2026-10-06; --no-p1soloend is the off arm.
inline bool g_p1SoloEnd = true;
// --gravtrig: the gravity trigger (id 2066) scales gravity from the tick after the one whose move
// crosses it: every mode's gravity step, not what a press does (modifiers.hpp, GravTrig). On by
// default since 2026-10-07 (kitref 62 FAIL->PASS and 0 PASS->FAIL, all on the four 2.2 levels with a
// 2066; one cold clear with --tporder); --no-gravtrig is the off arm, where the trigger is not loaded.
inline bool g_gravTrig = true;
// --followy: Follow Player Y (id 1814) moves its group toward player 1's y every tick from the one
// after the tick whose move crosses it, before the player moves -- a share of the gap set by its
// speed, cut by its max speed, plus its offset (modifiers.hpp, FollowY; step.hpp, the top of
// stepOne; State::followF). On by default since 2026-10-06 (the disassembly, a rig the model
// follows to its end, a kitref of 1 fixed and 0 broken); a delayed one, and one more than a state's
// two slots, stay NOT modelled. --no-followy is the off arm, where the group sits where the
// recording put it.
inline bool g_followYOn = true;
// --teleportorb: the teleport orb (GameObjectType 46, id 3027) is loaded as a ring; firing it
// does not jump but puts the body at its target at the end of the tick -- x and y unless kept,
// the gravity its mode asks for, vy as it was and the ground it stood on still under it, so a
// press it leaves acts on the next tick (level_loader.hpp, the orb bucket; step.hpp, applyOrb and
// the end of stepOne). On by default since 2026-10-06 (the disassembly, a 17-unit rig the model
// follows but for four rows, each back on the next); measured with one body in a plain frame, and
// an orb's own force is reported, not modelled. --no-teleportorb is the off arm, where the orb is
// not loaded.
inline bool g_teleportOrb = true;
// --tppush: a teleport sets the body's velocity after the move when its static force (345) or its
// redirected force (347) is on -- the portals, and the teleport trigger under its own flag; an
// orb's is reported, not read (modifiers.hpp, TpForce; step.hpp, tpForceVy). On by default since
// 2026-10-06 (the disassembly and a 30-unit rig the model follows to its end; one body in a plain
// frame). --no-tppush is the off arm, where only lv22's gravity-switching 747 gets its exit vy
// (the fixed +18 in the portal pass).
inline bool g_tpPush = true;
// Whether the last objrects load had the tp* force columns (level_loader.hpp); without them
// --tppush reads no force and the portal pass keeps the fixed +18.
inline bool g_tpForceCol = false;
// --tpkeepy: a teleport portal with ignoreY (353) keeps the body's height and only does the rest:
// the gravity, the force, x where it moves x (step.hpp, the portal pass). On by default since
// 2026-10-06 (calib_tpportal's three units); --no-tpkeepy is the off arm, where the loader prints
// the portal as NOT modelled and the body goes to the exit's y.
inline bool g_tpKeepY = true;
// --tporder: on a teleport's tick, the landing site's portals, pads, orbs and speed
// portals fire there when GD's collision pass reaches them AFTER the teleport, in the pass's own order
// (step.hpp, gdTeleAfter): the 3x3 section cells around where the pass began -- read once
// (checkCollisions 0x2141a6-0x214257), walked x-major then y (0x2142f0-0x21443c) -- a cell's objects in
// ascending uid, teleportPlayer called inside that walk (collisionCheckObjects 0x21557a) with the rect
// re-read after it (0x215e78). Cells are 100 units in x, and in y only with dynamicLevelHeight on
// (loadLevelSettings 0x23ae17 writes 0 otherwise, so every object is in row 0; read from the running
// game: lv20 y=0, a dynamicLevelHeight level y=0.01). Without it the gate is the uid alone, which is
// that order whenever x and y cells do not split the two: right for lv20 and lv21 (y one row), wrong
// for a dynamicLevelHeight level's 747 from y 598 to 985 (cells 5 -> 9, outside the walk): GD flips,
// halves -15 to -7.5, takes the pad's -16 and turns mini on the tick after the teleport, and the model
// did it on the arrival tick in the order pad -> portal (vy +8). On by default since 2026-10-07 (one
// cold clear the flag made, quick moving only lv22's 2902 toward gdref); --no-tporder is the off arm.
inline bool g_tpOrder = true;
// --boxparts (default on since 2026-10): a touch-only object whose box chain moves it in two or more parts takes
// the per-part formula (level_loader.hpp, `boxParts`; dynamics.hpp, plainAt) instead of the summed
// offset, so its motion does not depend on a recording that ends at an attempt's death.
inline bool g_boxParts = true;
// --moverextend (default on since 2026-10): a mover whose last three rows are consecutive ticks of one constant velocity and end at the recording's own last tick (the attempt's death) goes on at that velocity past it (dynamics.hpp prepareExtend / seek). --rotextend (rotation, load time) and --slideparts (eased autonomous moves) are the other two tails; this one reads the rows only.
inline bool g_moverExtend = true;
// --moverextendvel (default off): the FALLBACK hypothesis of the same tail, terminal velocity with no cap and no Move data behind it. --moverextend is the Move-data form: it needs one of the object's own linear Moves with exactly that step and goes on only for the Move's remaining duration.
inline bool g_moverExtendVel = false;
// --recendextend (default on since 2026-10): past the recording's own last row (the attempt's death) a mover
// whose last three rows are consecutive ticks of one steady step in position and angle goes on by that step for at most kRecEndExtendTicks, then
// holds (dynamics.hpp prepareRecEnd). Only where nothing else continues it. The game kills in the step after the last
// row with the object already moved: a custom level's spike rides two Moves and three Rotates about moving centres,
// and held at its last row it sat 2 px behind, so the model flew on 24 ticks past the game's death and no verdict
// could be recorded.
inline bool g_recEndExtend = true;
inline constexpr int kRecEndExtendTicks = 2;
// --recendresid (default on since 2026-10): ...and where the autonomous Moves' own tail is what continues it (dynamics.hpp, the
// parts tail), the part of that last step the Moves do not explain goes on as well, for the same ticks. The same
// custom level's spike: its x follows the player through a lockToPlayerX Move, which the parts tail does not carry,
// so the tail moved it 0.15 px in y and left x at the last row, 2.36 px a tick behind, and the model died a tick
// after the game. --recendextend alone does not reach it: the tail has already moved it off the last row.
// Unlike --recendextend's step it is not held past those ticks (the tail alone answers from there).
inline bool g_recEndResid = true;
// --flipcollide (default on since 2026-10): a swing that flipped gravity this tick meets the solids with the new
// gravity's sign; the flyers' solid loop measured the penetrations with the tick's opening sign while
// it picked the faces by the new one (step.hpp, the flyers' solid loop).
inline bool g_flipCollide = true;
// --mpushflip (default on since 2026-10): the moving-push launch from last tick's c6 is mirrored under flipped
// gravity (prev < -5, clamp -20, prev <= vy), as postCollision's 0x9bf branch is (step.hpp,
// the --mpushlaunch block).
inline bool g_mpushFlip = true;
// --swingbandceil (default on since 2026-10): a flipped swing lands on the band's top as any flyer does on its
// gravity side; the swing was exempt from the band's ceiling in both gravities (step.hpp, the
// flyers' band clamp).
inline bool g_swingBandCeil = true;
// --slopestick (default on since 2026-10): GD's moving-ramp re-land -- a ramp the upright cube last landed on that
// moved down this tick takes the body again, up to min(|dy|, 1.25) px above its seat (step.hpp, after
// the slope law's seat; PlayerObject::postCollision 0x38ea58-0x38eb57).
inline bool g_slopeStick = true;
// ...the ramp factor its jump bonus reads (slopes.hpp slopeRampFactor): the seat is not counted, so it is taken as held past
// the 0.1 s ramp-up (24 ticks), as a seat on a ramp that carries the body along is.
inline constexpr int kSlopeStickRideTicks = 24;
// --hazrectgate (default on since 2026-10): the first stage of a turned hazard's test is the two rects, the player's against the
// object's recorded rect, as in the game's danger loop (checkCollisions: rect against rect, then box against box for an
// object that uses its turned box). It was the turned box against the player's UNturned square, which is neither stage
// and is stricter than the game when the player is turned (object.hpp hazardStage1).
inline bool g_hazRectGate = true;
// --ctlautotail (default on since 2026-10): the --slideparts tail for a touch-controlled object. Past its recording's last
// row, an object that a touch chain controls but does not move (offset 0, e.g. a Rotate whose box the state
// has not entered) goes on by each of its autonomous Moves' own eased increment from that Move's crossing,
// as an autonomous object already does (dynamics.hpp, applyTriggers). Where it applies it replaces
// --moverextend's tail for that object (the parts give the same step for a lone linear Move, and also
// cover overlapping and eased ones).
inline bool g_ctlAutoTail = true;
// --headbandfresh (default on since 2026-10): the level's first step does not hold a resting flyer at vy 0 (step.hpp, the
// flight branch's rest). GD's collision pass compares the band's top with the value it kept from the last pass
// (layer+0x764, 0 on a fresh layer), so a fresh attempt's first pass sees the band move and gravity stays;
// the mod's cfg headclear bit 4 gives every head flight that fresh value.
inline bool g_headBandFresh = true;
// --fixgravthr: on a level with fixGravityBug=1, a single upside-down ship or UFO picks its
// acceleration with the upright threshold (step.hpp, thrFlip). On by default;
// --no-fixgravthr is the off arm.
inline bool g_fixGravThr = true;
// --expinzero: ExpIn easing returns 0 at u == 0 (dynamics.hpp gdEase). From PR #2. On by default since
// 2026-10; --no-expinzero is the off arm.
inline bool g_expInZero = true;
// --touchoff: a touch-triggered trigger does not fire while an x-crossing Toggle has switched off
// a group it sits in (step.hpp markTouched, TouchTrig::ownTog). GD (2.2081): toggleGroup
// (0x223bc0) keeps a per-object count of the groups switched off and sets the byte at +0x28e
// while it is negative, and collisionCheckObjects (0x214960) skips an object with that byte set
// at the top of its loop, before it hands a type-0x14 object to playerTouchedTrigger. SubZero
// 4003 is built on it: the twelve touch triggers of group 4 are switched off at x=1,815 and back
// on at x=2,535, so the player crosses them on the way right without effect and sets them off on
// the way back. On by default; --no-touchoff is the off arm.
inline bool g_touchOwnOff = true;
// --tapdisarm (default on since 2026-10): an x-crossing trigger that a box's chain has switched off before the player crosses
// it does not fire (dynamics.hpp autoDisarmed), and a Toggle's target group is not walked for the triggers it
// holds (triggers.hpp, the box chain). GD (2.2081): checkSpawnObjects (0x21a8f0) walks the x-sorted triggers
// with a cursor and calls triggerObject only for an object whose +0x28e (toggleGroup's "switched off") is
// clear; the cursor moves on either way, so a trigger passed while off never fires. A Toggle (1049) only calls
// toggleGroup (triggerObject case 0x419) -- it spawns nothing -- while a Tap (1595) switching its group ON goes
// through toggleGroupTriggered, which spawns the group as well. A custom level: a press past the Tap at x
// 37,200 spawns a Toggle that switches off the group holding the rig's 60-degree Rotate and the Toggle that
// brings its spikes in, and GD's cube passes where the model's met the spikes.
inline bool g_tapDisarm = true;
// --boxarm (default on since 2026-10): --tapdisarm the other way round. An x-crossing Move that an x-crossing Toggle has
// switched off by its crossing (triggers.hpp offAtCrossing, which used to drop it) is kept, marked
// AutoTrig::offInit, and fires only in a state where a toggle block (1594, "activate group") switching its group
// back on fired before the crossing (dynamics.hpp autoFireT, the same g_autoDisarm ties). Same GD reading as
// --tapdisarm (checkSpawnObjects skips a trigger whose +0x28e is set). A custom level's dual-wave corridor:
// Toggles at x 19,395 switch off seven groups of paired Moves (the corridor's floor and ceiling, locked to the
// player's x, +-30..90 each), and a press inside the box before each pair switches it back on. The model never
// fired them and put a ceiling block 105 px above GD's (cy 375 against 270), so the wave GD seats under it
// flew up through it.
inline bool g_boxArm = true;
// --nanslope (default on since 2026-10): a ramp whose line the dump cannot give -- a scale-0 object, w = h = 0 with sy0 / sy1
// printed "nan" -- is left out of the level (level_loader.hpp). Left in, its m = (sy1 - sy0) / (sx1 - sx0) is
// NaN, `m == 0` does not skip it and every bound test against NaN is false, so the anchor's ride guess
// (cli.hpp rideAtAnchor) seated any grounded body on it: a custom level's two such ramps (x 14,490 and
// 21,030) gave an anchor at x 3,316 onSlope = 1 with slopeM = NaN, and the ramp release one tick later
// stamped vy = NaN.
inline bool g_nanSlope = true;
inline int g_nanSlopeDropped = 0;   // how many the loader left out (printed after the load)
// --flipbonk: the cube's head bonk under an armed 1859 (step.hpp, the CEILING STOP branch) applies
// to a flipped cube too, whose head meets a block's top -- on the tick the arm is touched only.
// On by default; --no-flipbonk is the off arm.
inline bool g_flipBonk = true;
// --multiorb: a ring with GD's Multi Activate (Obj::multiAct, objrects' `multiact`) is not spent by
// firing: a fresh press fires it again while the player is still inside it (step.hpp, the ring
// pick and flyEarlyRing). Needs a dump with the column. On by default; --no-multiorb is the off arm.
inline bool g_multiOrb = true;
// --spiderband: a spider and the band's edges as GD's passes read them (step.hpp): the collision
// pass (GJBaseGameLayer::checkCollisions 0x2137f0) runs before the camera moves, so it clamps to
// the band of the end of the previous tick (the recorded row t-1, not t+1); a teleport from a press
// held in the air is taken in that same update, while a press on this tick is taken in the button
// pass after the camera has moved (row t); and when the band moved this tick the clamp gives the
// spider its velocity back (setYVelocity after hitGround), so it is carried along the edge. The
// button pass also comes after the clamp: a fresh press's teleport is not clamped on its own tick,
// and a fresh press on the tick the clamp seats the spider teleports on that tick. A spider under
// the band's ceiling is no escapee (the ceiling seats it and its press brings it down).
// On by default; --no-spiderband is the off arm. Measured on the nine resims of one SubZero 4002
// cold that reach the moving band:
// without the three flags all nine die on the escapee prune before GD does; with --spidercoyote and
// --spiderspend, eight keep |y - GD| <= 0.01 to the end -- four die on GD's own death tick, four
// seven ticks early on a moving saw -- and one parts at a ramp under the carried spider.
inline bool g_spiderBand = true;
// --spidercoyote: --upsidecoyote's linger for the spider (step.hpp, ogLingerGate). GD's
// playerIsFallingBugged (0x39a430) tests `vy > 2g` for an upside-down player, so updateJump leaves
// m_isOnGround (+0x9c1) set on a flipped spider until it falls faster than 2g, touching or not, and
// a press teleports it. SubZero 4002 t=24,877..24,885 (a flipped spider chasing a rising band
// edge): og 1 at vy 1.129 .. 1.903 with the edge 0.1-0.24 px ahead, og 0 once vy(t-1) = 2.032.
// On by default; --no-spidercoyote is the off arm.
inline bool g_spiderCoyote = true;
// --spiderspend: m_jumpBuffered (+0x985) as the spider's two press consumers clear it (step.hpp).
// spiderTestJumpInternal clears it (0x39505e) and the ring check reads its mirror (+0x989, copied
// in update), so a button held on from a press teleport fires no ring until it is pressed again
// (State::pressSpent); and ringJump clears it (0x399601 / 0x39999b), so a press a ring took in the
// air is not kept for a teleport on landing (State::pSpiderTap). SubZero 4002: t=25,320..25,321 GD's
// spider teleports down onto the band floor and, still held, passes a ring without firing it (the
// model rang at 7.826 = 11.18 x 0.7); t=25,414 a ring takes the air press and GD's spider lands at
// 25,475 and stays, held, where the model teleported at 25,476.
// On by default; --no-spiderspend is the off arm.
inline bool g_spiderSpend = true;
// --spiderpresslate: a spider's fresh press is taken in the button pass, after the collision pass,
// so it cannot teleport the spider out of a death that pass deals on the same tick (step.hpp, the
// spider branch steps the tick without the press first and dies with it). SubZero 4002 t=22,530:
// the spider walks into the side of a 3 px wall (uid 18247) on the tick it presses; GD dies there
// at y 1,483.5, the model teleported to 1,696.5 and flew on.
// On by default; --no-spiderpresslate is the off arm.
inline bool g_spiderPressLate = true;
// --spiderpadfirst: ...and a pad the collision pass fires on that tick launches the spider before the
// press is seen, so the button pass finds it airborne: no teleport, and the press is an air press,
// kept while held (State::pSpiderTap). step.hpp reads it off the probe --spiderpresslate steps
// (g_probePadFired). A custom level's spider runs on the floor at 103.5 into a red pad (29 x 7 at
// (165, 92.5)): GD teleports it to the ceiling at 346.5 for a press on any of the six ticks before
// the pad's, and for a press on the pad's own tick leaves at vy 12.000 exactly as with no press (a
// one-press sweep over the nine ticks), where the model teleported. A spider pad (type 44), which
// teleports and lands the body itself, is left out; so is the dual, which the probe does not step.
// On by default since 2026-10; --no-spiderpadfirst turns it off.
inline bool g_spiderPadFirst = true;
// --spiderbandback: a spider teleport that finds nothing lands on the band's face even when the
// spider is already past it (step.hpp, spiderTargetGd's fallback).
// On by default; --no-spiderbandback is the off arm.
inline bool g_spiderBandBack = true;
// --spiderslopeline (on by default since 2026-10): a ramp the spider's teleport finds is met at its LINE when the
// ramp faces the search -- GD's slopeYPos at the spider's x, carried past the ramp's own span -- not at
// its rect. spiderTestJumpInternal (0x3943f0): for a type-0x19 candidate, ceiling-like = direction < 7
// and bit `dir` of 0x6a (dirs 1, 3, 5, 6); when that differs from the spider's upsideDown it takes
// slopeYPos, otherwise the rect's face as for a block. A custom level t=5,972: an upright mini spider
// at x=6,523.47 taps under ramp uid 43737 (dir 6, line 515.2 at its left edge x=6,530.6 rising 2 a
// pixel); the line carried 7.13 px left is 500.94, and GD's spider goes to 500.94 - 8.1 = 492.840,
// airborne (the line is not there), where the model seated it under the rect at 507.1.
inline bool g_spiderSlopeLine = true;
// --spiderbandground: a spider's tap takes the band as a wall also when the band's floor is the
// ground, so an upright spider with nothing overhead lands under the band's ceiling (step.hpp,
// tapBandWall). On by default since 2026-10-06 (a custom level's spider wall, where the game flies
// 33 variants of the tap the model lost; kitref unchanged on the spider kits); --no-spiderbandground
// is the off arm, where such a tap finds no face.
inline bool g_spiderBandGround = true;
// --nocolone (on by default since 2026-10-07; --no-nocolone is the off arm): a MOVING solid (it has a
// recorded timeline) whose [obj+0x515] byte is set
// is loaded as a one-way top instead of being dropped (level_loader.hpp), so the cube can stand on it
// and a snap seed can find it. Static ones stay dropped.
inline bool g_noColOneWay = true;
// --slamdedup (default on since 2026-10): the toward-the-player step a recAuto object takes from a touched box (dynamics.hpp,
// dy < 0) is skipped when the recording already holds that touch (its anchor within 8 ticks of the fire tick).
// Contract: only the state's FIRST touch of the object (the earliest fire among the boxes it touches) is skipped, so
// a nearby touch of another box keeps its step. Known hole: the recording holds a tick, not the box that moved the
// object; if the recording's touch is box A and the state touches only B, B is skipped as if it were A.
inline bool g_slamDedup = true;
// --slambox (default on since 2026-10; replaces --slamdedup's rule on every call that carries --rectouch): the step is skipped
// only for a box the recorded attempt itself fired, entered by the state on the recorded entry's own tick
// (dynamics.hpp slamBoxRecorded, from --rectouch, which the mod passes only under this flag, on its anchored calls). A custom level's plate moved by three boxes: the state
// entered box -9 on the recording's own tick and box +300 later; the tick-only rule saw two hits and gave the -9
// a second time, and the plate and the player ran 9 px low to an early death.
inline bool g_slamBox = true;
// --holddedup (default on since 2026-10): the punch hold (a recAuto object's recording is held min(60, t - F) ticks per touched
// dy > 0 box) is skipped for a touch within 16 ticks of the recording's own motion anchor (dynamics.hpp).
inline bool g_holdDedup = true;
// --spiderinner: a full-size spider's kill box is 0.3 x its own 27 px box, half 4.05 (kSpiderInner),
// not the cube's 4.5 (step.hpp, pInner). collidedWithObjectInternal takes the inner rect from the
// player's getObjectRect(0.3, 0.3) (vf +0x488 at 0x393658), and the spider's box is 27 (kSpiderHalf).
// The crush test (step.hpp, crushHalf) reads the same box and takes the same number.
// Measured in GD on lv21 t=8,821 (worker 99): the spider walking into the left face (11,460) of the
// static block uid 12007 with the press at 8,820 removed and x shifted at 8,820 -- it lives at
// x 11,455.9268 and dies at 11,455.9770, so the half is in [4.023, 4.073). With 4.5 the model's
// --spiderpresslate probe killed it a tick early and lv21's quick sections lost the teleport GD
// takes there. The mini spider keeps 4.5 (not measured).
// On by default; --no-spiderinner is the off arm.
inline bool g_spiderInner = true;
constexpr double kSpiderInner = 4.05;
// --rotboost: a gameplay rotation that swaps the vertical and horizontal axes raises GD's
// velocity-limit exemption (State::boost). rotateGameplay's swap block writes [+0x952] = 1 at
// 0x399faa unconditionally and lowers it again at 0x39a00d only when the trigger's Dont Slide
// (property 585) is set. No 2900 in official level 22 sets 585. On by default since 2026-10-06;
// --no-rotboost is the off arm.
inline bool g_rotBoost = true;
// --rotspeed: a SPEED portal its group turns is tested as the turned box too (dynamics.hpp,
// turnedBox), as the mode and gravity portals are. The recording's w/h are the turned box's
// axis-aligned bound, and the speed pass tested that bound. Measured on a custom level: a 2x
// portal (51x56) turning through 331-333 degrees records a 71.7x73.7 bound; GD switches the speed
// at x 43.3 from its centre and not at 45.2, the bound's test at 49.1 -- three ticks early, every
// attempt through it. On by default since 2026-10-06; --no-rotspeed is the off arm.
inline bool g_rotSpeed = true;
// --bonkouter: the armed plain head bonk (step.hpp, `bonk`) tests x on the player's outer box rather
// than the inner kill box. On by default since 2026-10-06; --no-bonkouter is the off arm.
inline bool g_bonkOuter = true;
// --spidercrushgrace: the spider-teleport grace (State::spiderJumpT) that spares the side kill
// spares the crush test too (step.hpp, crushHalf). GD has one inner-box kill block in
// collidedWithObjectInternal, with the grace in front of it; the model has two tests, and only the
// side test had the grace. SubZero 4002 it23: the spider teleports to y 1,516.5 at t=23,510 with a
// 3x30 wall (uid 17671) 0.2 px left of its centre; the model's crush test killed it on that tick,
// and GD lives to 23,514. On by default; --no-spidercrushgrace is the off arm.
inline bool g_spiderCrushGrace = true;
inline thread_local bool g_inPressProbe = false;
inline thread_local bool g_probePadFired = false;   // --spiderpadfirst: a pad fired inside the probe
// --ridehold: the moving-support ride (step.hpp, the support scan) does not re-seat the player down
// onto a moving face while a support that did not move holds its foot higher.
// On by default; --no-ridehold is the off arm.
inline bool g_rideHold = true;
// --stickfallback (on by default since 2026-10): a rider whose descending support runs out from under it lands on
// another solid that descends with it, when that one's top is within 5 dt (1.25 px) of the foot.
// GD's stick re-land (PlayerObject::postCollision 0x38e85b-0x38ea48): when the solid the rider is
// tied to (+0x608) moved down this tick and the foot is within 5 dt of its top, the rider re-lands
// (collidedWithObject, dt 0) on it if the two still meet with its top raised by 5 dt, and otherwise
// on +0x600, the first solid the solid pass found moving down with its rect raised by 5 dt over the
// player (GJBaseGameLayer::checkCollisions, the 5.0 * dt at 0x2145a0). A custom level: a mini cube
// rides a 30x3 slab sinking 0.08 px a tick; at t=12,130 its left edge passes the slab's right edge
// by 0.88 px and GD seats it on the block beside, 0.96 px below and in the same group (hbox: the
// dt 0 hit on uid 79866), where the model fell. step.hpp, the support scan.
// The same re-land bounds the other side for the flyers: their support scan held a grounded flyer on
// a face that moved down by any amount (0.6 + |dcy|), where GD re-lands only within the 5 dt; under
// this flag a descending face holds a flyer only inside it (step.hpp, the flyers' support scan). A
// custom level: a mini swing on a block that drops 2.165 px in one tick, which GD lets go of.
inline bool g_stickFallback = true;
// --ridebound (on by default since 2026-10): the same 5 dt bound on the ground scan (step.hpp, the support scan that
// feeds the ride re-seat): a face that moved down holds a standing cube family rider only while the
// foot is within 5 dt (1.25 px) of its new top, where `0.6 + |dcy|` held it on a face of any speed and
// the ride then re-seated it there. A custom level: at x=17,700 a Move of duration 0 drops the floor
// group 300 px on one tick; GD's cube, standing on it, goes to y 1244.952 with vy -0.215 (the gravity
// step) and falls, where the model put it on the new face 300 px down. The slow descents the ride was
// measured on stay inside the bound (lv22's elastic settle, 0.04 px a tick; its ball-corridor floor,
// 0.838 px on the first step; a robot's eased platform, 0.171).
inline bool g_rideBound = true;
// --graceendonly (on by default since 2026-10): the flip grace's re-seat (step.hpp, the cube's solid side test,
// flipGraceSeat) only where the tick ENDS with the inner box in the solid. The side test sweeps
// kSubSteps points from the last position to this one, and a diagonal step can clip a corner at a
// point the body never occupied; a kill there is the sweep's known over-kill, but the re-seat moves
// the body onto the face as if GD had resolved it, and GD tests the end of the tick only. A custom
// level, t=12,727: a flipped mini cube 13 ticks after its flip steps from (16,621.5, 620.4) to
// (16,623.4, 619.0) past the corner of a 22x43 block at (16,607.5, 593.8); the inner box meets it at
// the sweep's midpoint only (x 15.92 > 11 + 4.5 at the end), the model sat the cube on the block's
// top (624.26), and GD carried on to 619.000 and died on a spike at t=12,730.
inline bool g_graceEndOnly = true;
// --ballscaleair (on by default since 2026-10): a size portal re-stakes a ball's ground rate (step.hpp, --ballstake
// at the size portal) also while the stake in force is an AIR rate at the old size. togglePlayerScale
// (0x3a0cba) gates the re-stake on +0x728, which runBallRotation sets and runBallRotation2 (the air
// stake: a ring, a flip) leaves alone -- so a ball that has rolled since it became one still has it up
// after an air stake, and the portal puts it on the new size's ground rate in flight. The model read
// the gate off the stake's value, which misses exactly that. A custom level, t=3,560: a ball spinning
// at -1.4274 (0.7's full-size air rate) takes a mini portal in the air and GD's row steps +2.519
// (0.7's mini ground rate, the sign runBallRotation writes); the model kept -1.4274, ran 34.8 deg
// behind, and its turned box missed a rotated wave portal at t=3,725 that GD's took a tick earlier.
// The air rate alone does not say whether +0x728 is up: ringJump's stake leaves it as it was, but a
// flip (a tap, a gravity portal, a ring that turns the gravity, a dual partner call) goes through
// flipGravity, which lowers it before its own stake, as toggleRollMode does on the way into and out of
// the ball. So the flag carries GD's +0x728 per body (State::ball728): the ground stakes raise it,
// those lower it, and the air re-stake is refused while it is known to be down. Another custom level,
// t=1,818: a ball that a pad and two speed portals had staked on the ground flips at 1,814 (+1.7708 a
// tick, the air rate) and takes a mini portal in the air; GD's rot keeps +1.7708, the model re-staked,
// ran 63.6 deg off, and its turned box met a gravity portal at 1,897, a tick before GD's. At an
// --start anchor the bit is not known (the dump has no column for it) and the stake's value decides,
// as before.
inline bool g_ballScaleAir = true;
// --slopeunchanged (on by default since 2026-10): a flipped ship's continuing ride is refused on a tick whose seat
// target equals the last tick's (step.hpp, at the end of stepOne, after the solids). GD refuses an unchanged target on a
// continuing contact (0x390415, the --ceilcont note); past a ramp's end the target is clamped and so
// unchanged, the ride ends, and postCollision launches the body off the exit -- and the next tick,
// not riding, it lands on the ramp afresh. A custom level, t=10,485..: a flipped ship under a block
// whose bottom is below the ceiling ramp's line, past the ramp's end: GD alternates seat-and-clamp
// (vy 0, onGround 1, onSlope 1) with release (vy 4.734, onGround 0, onSlope 0) every tick, y 400
// throughout; the model seated every tick and kept vy 2.0, and the fixup loop chased the alternation
// four ticks per iteration (five iterations).
inline bool g_slopeUnchanged = true;
// --thinlandjump (on by default since 2026-10): a cube or robot that a ramp's thin solid (its wall or its 1 px top /
// bottom strip, preSlopeCollision's probes) lands on the press tick jumps on that tick, as the ordinary
// landing on a solid's face already does (step.hpp, `solid/land`). The strip goes through the same
// collidedWithObjectInternal, and GD runs the buttons after the collision pass. A custom level,
// t=12,507: a flipped mini cube at speed 1.3, airborne, reaches the bottom strip of a floor ramp
// (bottom 660.76) with a fresh press; GD's row is y=651.760 (= 660.76 - 9) AND vy=-8.984 (= 11.230 x
// 0.8, flipped), never a vy=0 tick. The model landed it there and sat, a gravity step behind for the
// whole arc.
inline bool g_thinLandJump = true;
// --apexreacq (on by default since 2026-10): the slope law (step.hpp, its `!s.onSlope` gate) also runs on a floor
// ramp whose high end is an apex the body has passed while it rides the floor ramp that falls away
// from that apex, in the law's continuing form (m_wasOnSlope; the drop of a target equal to the last
// one). A custom level, t=10,401..10,405, GD's slp: lines: a flipped ship pinned under a block at
// y 400 over an apex at x 12,900 (a ramp rising to 390.519, the next falling from it). On the tick its
// ride down the far side has left it at 399.140, the near ramp, past its end and still overlapping,
// takes it at its clamped end target 405.519 (oS0=0 was0=1) and the block puts it back on 400 with
// onGround 1; on the next tick that target is unchanged and the ride is refused (vy 4.734). The model
// tried the law only when not riding, and the ship hung at 399.12. With it on, --slopeunchanged also
// reads "unchanged" against the target of the ramp ridden last tick, which is what GD compares with
// (the target the last seat wrote): the near ramp's first tick is then a landing, not a release.
inline bool g_apexReacq = true;
// --followsection (on by default since 2026-10): an object a Follow Player Y moves stays in the collision section it
// was put in at load, so on a level with dynamicLevelHeight (100-unit rows) the game's collision pass
// only meets it while that load-time cell is in the 3x3 around the player (step.hpp, the follow copies).
// processPlayerFollowActions (0x22ded0) adds to the object's y and marks it dirty; nothing re-buckets
// it. A custom level, t=2,570: a ship at y 698 enters the x span of a gravity portal that follows its y
// (at 700.4, load-time y 825, row 8 against the player's row 6). GD's candidate list (cfg watchuid,
// `ccl:` has=0, n=1 from t=2,566 to 2,574) does not hold it and the ship is not flipped -- in all 43
// attempts started from the head. The 25 attempts flown from a checkpoint all flip at 2,570: the
// checkpoint load re-buckets the object where it stands. The model, which flipped, now matches the head.
inline bool g_followSection = true;
// --bigsection (on by default since 2026-10): ...and the same holds for every collider, not only a followed one.
// GJBaseGameLayer::addToSection (0x226500) files an object in ONE cell, from its position (+0x3b8 x,
// +0x3c0 y) times the layer's factors (+0x36a0 / +0x36a4: 0.01 each, y 0 without dynamicLevelHeight),
// so the part of a big object that lies past the 3x3 cells around the player is never met. A custom
// level (dynamicLevelHeight), t=12,463: a ship at y 826 (row 8) enters the x span of a yellow gravity
// portal stretched to 25 x 733 whose centre is at y 657 (row 6). GD's candidate list (cfg watchuid,
// `ccl:` has=0 on all 247 cell lines of t=12,455..12,495) never holds it and the ship crosses the portal
// unflipped; the model flipped it on the first tick of overlap, in every recorded iteration that got
// there (28 recorded first divergences sit on that tick or the next). Static
// colliders whose half extent passes kBigSecHalf only (modifiers.hpp); moved objects are re-filed by
// the game when they move and keep the windows' own positions.
// ...and the GROUPED colliders (Dynamics) the same way, their cell read from where the windows place them
// (a Move re-files what it moves; a circle reaches its radius). The same custom level as --recendtail's,
// t=12,249: a saw scaled about six times (id 1705, radius 32.3 x 5.94 = 192) in group 1, centre y 1,115
// (row 11), and a swing at y 980 (row 9) 175 px from it -- GD flies on, the model died on the anchor's
// first tick and every search from that anchor came back empty.
// ...EXCEPT an object with Extended Collision (GameObject+0x280, property 511; Obj::extCol from the
// objrects `extcol` column): addToSection puts it in an always-checked list (+0x35e0) instead of a cell,
// and checkCollisions tests that list every tick after the 3x3 cells. Another custom level, t=2: the
// cube starts on a floor block scaled 75 times (2,250 px, centre x 2,835, eleven cells ahead) with the
// flag; GD stands on it (snapped to it from t=2), the model without the exception fell through it.
// A dump without the column cannot tell (the loader says so) and takes every big object as filed.
inline bool g_bigSection = true;
// --crushhead (on by default since 2026-10): a moving solid whose underside comes DOWN onto an upright cube's head
// is a head collision too, when the 1859 arm is up. The bonk below takes only a head rising into the
// face (vpNow > 0, the head under the face at the start of the tick), so a ceiling that descends onto
// a falling cube left it inside the block untouched. A custom level: block uid 7169 (30 x 30) rides the
// player's x and drops 5 px a tick (a Move of -120 over 24 ticks); the cube, inside an 1859 that rides
// with it, falls at -1.08. On GD's death tick (9,836 in the replayed plan) the block's underside (1275)
// is 3.53 px into the head (1278.53, hitboxtrace's prect of the collision pass): GD puts the cube at
// 1260.000 = underside - 15 (the killer line's py), and the hazard pass then finds it on the spike under
// it (top 1248). The model left the cube inside the block and met the spike two ticks after GD.
// vy: PlayerObject::collidedWithObjectInternal's upright head arm (the one that calls didHitHead) sets
// vy = min(vy, +0x630 - 1). +0x630 is a partner's velocity, written only when the object this pass meets
// moved WITH the body's gravity (up, for an upright body: local_190 = its delta over dt). PlayerObject::
// postCollision moves it to +0x638 and zeroes it every tick, so for a block coming down onto the head it
// is 0 and the result is min(vy, -1). The killer lines of two plans agree: -1.296 where the gravity
// step was -1.296, -1.000 where it was -0.864. A rising partner met earlier in the same pass would leave
// +0x630 non-zero; that is not modelled here.
// Scoped to what was measured: cube, upright, armed. A cube under a descending ceiling WITHOUT the arm
// is not measured (GD kills a rising unarmed cube at a ceiling; whether the face's own motion changes
// that is open) and is left as the model had it.
inline bool g_crushHead = true;
// --dashhead (on by default since 2026-10): a dash that carries the head up into a face counts as rising for the
// head collision. The bonk's gate is vpNow > 0, and a dash moves along its line with vy 0, so a cube
// dashing up under an armed ceiling went on through the face. A custom level: a green dash ring
// (id 1704, rot -8, (5,909, 585)) sits between two blocks that close on it, each with an 1859 arm; the
// cube stands on the lower one with its head at the upper one's underside (606.0 against 606.046). In
// GD, for a press anywhere from 10 ticks before to 1 tick before the cube's box clears the upper block,
// the cube stays at y 591.000 and rises along the dash line (0.1825 a tick, tan 8) only from the first
// tick its box is clear (x 5,939.35): the head is held under the face while the dash goes on. The
// model rose on the tick after the press, up to 2 ticks before GD.
inline bool g_dashHead = true;
// --portorder (on by default since 2026-10): the portals of a tick are taken in the collision pass's own order --
// the section cells x-major then y (y only with dynamicLevelHeight), a cell's objects in ascending uid
// (gdTeleAfter's walk) -- where the window listed the static ones by x and appended the moving ones
// after them. It decides which box a stacked portal sees: a rotated portal re-reads the player's box,
// so a portal the pass reaches after a mode portal meets the new mode's box and one it reaches before
// meets the old. A custom level t=7,849: a wave enters a UFO portal (uid 21444, x 8201, cell 82) with a
// mini portal (uid 21434, x 8197.22, cell 81, rot 10, in a moving group) on it. GD's candidate list
// holds the mini from t=7,846 on and fires it at 7,850 only (ccl: act 0 at 7,846-7,849, 1 at 7,850;
// vsize 1 at 7,849), so its UFO flaps at full size (vy 6.871); the model reached the mini after the
// UFO, tested it with the UFO's half 15, made the body mini at 7,849 and flapped at 6.648.
// A SIZE portal is not a mode portal here: togglePlayerScale leaves the box stale for the rest of the
// pass (step.hpp, at the size portal). Another custom level, t=1,180: a mini UFO meets a size portal
// (uid 4000) and an inverse gravity portal (uid 4017), both turned 45 deg on the same centre; GD turns
// the body full size on 1,180 and flips it on 1,181, and with the new box the model had flipped it on
// 1,180.
inline bool g_portOrder = true;
// --slidemini (on by default since 2026-10): the dart slide arm's push-out (step.hpp, the wave's solid loop) holds a
// MINI wave too, at the solid's face + 3.0 -- the half of the mini wave's 6x6 rect, the one its hazard
// test was bisected on -- where the full wave's is + 5. The sweep that measured the arm had no mini
// sample. A custom level, t=7,001..: an armed mini wave (a 1755 box over the spot) dives onto a 1.5 px
// slab whose top is 270.185; GD holds it at 273.185 with onGround 1 and zig-zags it off that line
// (273.185 / 275.781 every tick), and the model flew through to 272.150.
inline bool g_slideMini = true;
// --apexstick (on by default since 2026-10): the ride's stick (step.hpp, stickHere) holds a flipped ship to the far
// ramp of an apex -- the floor ramp starting at the high end of the one it rode last tick, past that end
// -- only within 1.0 of the seat, not kStickGap: GD's contact on another ramp than the current one has
// the fresh band. A custom level, GD's slp: lines for the far ramp with m_wasOnSlope set and the button
// down: gap 0.841 above the seat -> seated (t=10,402 of one route), 2.130 -> not (t=10,412 of another,
// the ship launched off the near ramp's refused contact at 4.734 instead), 8.684 -> not.
inline bool g_apexStick = true;
// --ceilstrict (on by default since 2026-10): a flipped body exactly touching the band's ceiling (y + half == the
// ceiling) is neither clamped nor grounded; GD's test is strict. A custom level, t=12,783: a flipped
// mini ball (dual, half 9) under a band ceiling of 840 moves 830.3758545 + 2.774 x 0.225, which is
// 831.000 in float -- exactly 840 - 9. GD's row keeps vy 2.774 and onGround 0, and grounds it (vy 0) on
// 12,784 when it would pass the line; the model clamped and grounded it on 12,783. The other arrivals
// at a ball's band edge in six runs of the level (163 rows) ground on the arrival tick.
inline bool g_ceilStrict = true;
// A mode portal seats on its own tick: GD runs the activation pass before the
// solid pass. (--no-portalseat is gone since the flag clean-up.)
// Force fields add AFTER the terminal clamp, unclamped (PlayerObject::update, not
// updateJump). (--no-forceorder, the pre-2026-09-05 order, is gone since the flag clean-up.)
// A same-frame 2900 turn that changes the polarity halves vy: flipGravity's
// mulsd 0.5 @0x39a2dc. (--no-rot2900halve is gone since the flag clean-up.)
// A gravity portal latches on its first OVERLAP (hasBeenActivated).
// (--no-portallatch, fire on every pass, is gone since the flag clean-up.)
// WHICH HALF stepOne is running, for the --slopedbg prints only. Nothing reads
// it as physics. A per-tick diagnostic line cannot be counted on a dual level
// without it: stepOne runs twice with swapHalves between, so a portal each half
// passes ONCE looks exactly like one the same body passed twice, and reading it
// the second way is what put a retracted witness into 283e8a4 (see 87049e5).
inline int g_halfNow = 0;
// A ceiling ramp acquires from below: GD's underside gate takes the player
// under the line within tol_u (the site in step.hpp). (--no-ceilseat is
// gone since the flag clean-up.)

// A ramp already governing the surface vetoes the solid (slopeVetoesSolid:
// 1,673 of 1,673 rig ticks, all 14 corpus ticks). (--no-slopeveto is
// gone since the flag clean-up.)

// A FRESH ramp contact passes GD's inset-rect reach test first (0x38fc0e,
// 0x38fc3c-0x38fc7b; slopeWouldAcquire). (--no-slopefreshrect is
// gone since the flag clean-up.)

// --no-slopeseat: a ramp seats the player at the surface sampled at an x
// CLAMPED into the ramp's span, plus/minus a flat player half (the
// pre-2026-09-06 behaviour). GD's own seat is
// `slopeYPos(centreX) -/+ playerRect.h/(2 cos t)` -- the line EXTRAPOLATED past
// the span, with the bounds applied to the TARGET y instead
// (0x38fd42-0x38fdf0, re-evaluated on every acquiring tick 0x39072c) -- see
// slopeSeatTarget in slopes.hpp for the formula, the RVAs and the lv16 t=9,241
// witness. The two agree everywhere except on the ENTRY side of a ramp.
// (The switch is gone since the 0.4.0 clean-up; its on behaviour is fixed.)

// The UFO's floor acquisition allowance used to be kShipLandTol (6.0), before
// 2026-09-06 (the --no-ufolandtol arm, gone since the flag clean-up). That 6.0 was never measured -- the
// site's own comment said so ("kShipLandTol (6.0) stays for the UFO, whose own
// acquisition has NOT BEEN MEASURED this way"), the ship having been measured
// down to 0.001 while the UFO was left alone.
//
// GD's floor gate is `acquire <=> s*targetY > s*y - (extraTol ? tol : 0)`
// (@0x38fea5) and its only widths are 0 / 1 fresh / 2 continuing, selected by
// extraTol and m_wasOnSlope. 6.0 is not among them at any setting.
//
// Witness lv19 t=14,587 (UFO, uid 9522): targetY 315.928 against a free y of
// 317.298, so gap = 1.370. GD refuses -- `315.928 > 316.298` is false -- and
// seats a tick later once the line rises to meet the player. The model admits
// it through the main test (1.370 <= 6.0) and pulls the player 1.370 px DOWN
// onto a line it has not reached, one tick early. The 1.370 also settles which
// width is live: 2.0 would admit it, so fresh/continuing is load-bearing here.
//
// The corpus reach was counted before the change: of 3,424 UFO rows, 1,612
// carry the 6.0, and the rows this narrows are 4 -- of which exactly ONE is
// actually seated (the witness). No changed row is admitted by another
// disjunct, so nothing keeps its seat by a second path, and no UFO row anywhere
// has a gap in (0, 1.0], which is the only interval where "assume the band
// applies" and "clamp to zero" could have disagreed.

// A NORMAL-SIZE UFO that flaps while seated on a floor ramp leaves at this
// value instead of its plain 6.871. Measured on the calibration rig
// `calib_ufojump` (14 cells, worker 99): the two control cells, pressing
// on the flat run-up, return 6.8710 and 6.6480 exactly, and every one of the
// six normal sweep cells returns 8.0000 -- at |m| 0.5, 1 and 2, and at both 1
// and 3 ramps. So it is an ASSIGNMENT, not a bonus added to the jump:
//   * gradient does not enter it (three gradients, one value),
//   * ride time does not enter it (ramp factor 0.674 vs 1.000, same value),
//     which refutes every slopeRampFactor-shaped hypothesis outright,
//   * and it is not the cube's on-ramp bonus (kSlopeJumpBonus), whose |m| ratio
//     is 1.4192 where the measured ratio is 1.0000.
// The bonus this implies, 1.129, is well under the 0.4x cap (2.748), so the cap
// is not what flattens it.
//
// MINI IS DELIBERATELY NOT FIXED and stays wrong. The same rig gives a mini
// 9.3680 at |m| 1 and 2 and 8.1470 at |m| 0.5, against the model's unchanged
// 6.6480. Two rules fit those three points equally well -- a threshold at
// |m| >= 1, and the ramp's width -- and they cannot be told apart, because the
// 22 levels contain exactly three slope shapes ((30,30), (30,60), (60,30)), in
// which |m| >= 1 <=> w = 30 without exception. Separating them needs a 60x60 or
// a 30x15 slope, i.e. an object these levels never use. Guessing between the
// two would put a width rule into the model under a gradient's name.
// (The --no-uforampflap arm is gone since the flag clean-up.)

// --shipheldflap: a ship->UFO portal buffers the flap for a held button too, as
// wave->UFO already does. Measured on lv14 (2026-09-18, cfg `presstrace`): the
// press at t=14,189 sets +0x985/+0x986, the SHIP's updates leave +0x986 alone for
// two ticks, and the UFO's first update after the portal (id111 at 18461) consumes
// it and flaps (vy 0.788 -> 6.648); released before the portal, +0x986 is cleared
// with the button and nothing fires. The model buffered only a fresh edge on the
// portal tick, so a press one tick before the portal was lost.
// A press that STARTED in a consuming mode is excluded through pressSpent: on the
// rig calib_heldflap (2026-09-18) a grounded cube pressed two ticks before the ship
// portal jumps, +0x986 goes to 0, and held through ship into UFO it stays 0 and
// nothing flaps; pressed inside the ship, the same hold flaps (vy 6.871) on the
// UFO's first update. wave->UFO has no such gate and is unmeasured for it.
// On since 2026-09-19, with the anchor's press-latch payload that feeds it;
// always on since the flag clean-up.

// --wavespentgate: the same gate on the wave->UFO buffered flap, which is on by
// default. The wave twin of calib_heldflap (2026-09-18, presstrace): pressed inside
// the wave and held, the UFO's first update flaps (6.871); pressed as a grounded
// cube (the jump spends +0x986) and held through the wave, +0x986 stays 0 and the
// UFO does not flap (vy 1.298 -> 1.212) -- where the model flapped to 6.871.
// On since 2026-09-19; always on since the flag clean-up.

// The slope-exit launch fires only off a ride that became a landing (before
// 2026-09-06 any CONTACT did; the --no-ridelandlaunch arm is gone since the flag clean-up).
// GD's launch comes out of the ride; a contact that never landed has no ride to
// launch from. Witness lv19 t=14,633: the UFO meets the ramp at svy +6.323,
// fails the |vy| <= 5.0 hitGround gate (which this model already implements at
// the seat, keeping vy and leaving grounded clear), GD flies straight on, and
// the model launches anyway on leaving at 14,643 -- -2.156 that then persists
// exactly, the signature of one velocity overwrite.
//
// The gate is State::rideLanded and NOT `grounded`: the seat sets grounded only
// `if (rideLands && !(flipForRide && ridesTop))`, so a flipped rider on a floor
// ramp's top lands without it. Gating on grounded would change 2 rows and break
// lv16 t=8,913, whose launch GD makes and whose model value already matches.
// With rideLanded the reach is one row, and lv16 8,913 and lv17 18,573 both
// keep theirs.
constexpr double kUfoRampFlap = 8.0;

// GD resolves ramp-then-solid (the model's own order, every solid first, was the
// --no-rampfirst arm, gone since the flag clean-up). GJBaseGameLayer::checkCollisions (0x2137f0) is two passes:
// the bucket scan resolves slopes (type 0x19) and teleports (0x1c) in place,
// while solids (type 0 / 0x15) and hazards are only pushed onto a list and
// resolved afterwards through PlayerObject::collidedWithObject (0x214687), so
// in GD a solid always sees the seat the ramp already wrote. See the rule at
// the end of the slope block in step.hpp for the witness (lv16 t=4,142) and
// for what it deliberately does not cover.

// A ramp contact takes GD's own V3/V4/V5 velocity writes (slopeNudge in
// slopes.hpp). (--no-slopenudge, the pre-2026-09-06 +-2.0 / kShipRampG / walkIn0
// rules, is gone since the flag clean-up.)

// The flight loop's mover-catch (`fly/mpush`) seats within GD's reach-back
// `kShipLandTol + |dcy|/0.25`. (--no-mpushreach, any penetration, is
// gone since the flag clean-up.)

// GD's velocity-limit exemption (State::boost) is carried by ship, UFO and
// swing (swing only before 2026-09-06; --no-boostlatch is gone since the flag clean-up). The byte [player+0x952] has no
// mode test at any of its ten writers and is read by the ship's acceleration
// selector (0x38c5be / 0x38c5d8) and by the terminal clamp shared by ship, UFO
// and swing (0x38ca9f), so the real scope is those three -- see boostLatchMode
// in slopes.hpp for the reading and the sites in step.hpp for the consumers.

// When several rings touch on the same tick GD fires the one touched FIRST (the
// lowest uid, before 2026-09-06, was --no-ringfirsttouch, gone since the flag clean-up). GD's container
// `PlayerObject +0xa38` (m_touchedRings) is appended to only on a ring's FIRST
// contact (playerTouchedRing 0x217e40, containsObject 0x217ea1 / addObject
// 0x217eb5) and is pruned in place, order preserved, at every tick's head
// (resetTouchedRings(p,0) 0x3982e0), so pushButton's forward walk (0x3980b7)
// fires the FIRST-TOUCHED ring; ascending uid is only the same-tick tie-break
// (the bucket sort 0x2143a6 / comparator 0x205210). See the site in step.hpp.

// (--no-padobb, the raw player rotation for a rotated pad, is gone since the flag clean-up.)

// The BALL's hang window takes one tick's movement |useDx| as the grace on its
// exit end (a constant 1.5 before 2026-09-06; --no-ballcorng is gone since the flag clean-up), which is what the ship (r68/r74) and the swing (r71c/r74) already
// use. Two witnesses bracket the grace from opposite sides and leave no
// constant available:
//   lv17 t=18,571/18,572  full ball,  m=-1, x1=24,120, xoff 6.213, dx 1.29825
//                         last ride cx 24,113.838 / first drop 24,115.137
//                         => 0.051 <= G < 1.350
//   lv16 t=13,116/13,117  mini ball,  m=-1, x1=19,872, xoff 3.728, dx 1.614258
//                         last ride cx 19,869.660 / first drop 19,871.273
//                         => 1.388 <= G < 3.001
// The two intervals are disjoint, and |useDx| lands inside both, so these two
// ticks do not merely permit a dx-proportional grace, they require one. The
// reverse-travel mirror (okLo) is NOT converted: it has zero witnesses in the
// whole corpus, and an untested arm is better left where it was measured to be
// harmless. See the site in step.hpp.

// A pad's same-tick rotation step turns toward the gravity at the moment GD
// called runNormalRotation (116 witnesses, the site in step.hpp). (--no-padspinpre,
// the end-of-tick gravity, is gone since the flag clean-up.)

// --no-satrotraw: the four oriented-contact sites in step.hpp advance `s.rot`
// by one spin step before handing it to the SAT (the pre-2026-09-06 behaviour)
// instead of passing the state's rotation as it stands. The sites are the
// gravity-portal pass, the portal pass, the pad loop and the speed portals;
// they all carried the same expression
//     pRot* += (s.rotNeg ? 1.0 : -1.0) * (s.mini ? 2.25 : 1.7307692);
//
// IT WAS A SIGN INVERSION AGAINST THE MODEL'S OWN ROTATION LAW. The cube's law
// (step.hpp, the `spinNow && !c.grounded` branch) writes
//     c.rot = c.rot + (spinSign ? -rate : rate)
// with `spinSign` = `rotSignNow` = `s.rotNeg` on a plain airborne tick -- THE
// SAME FIELD, THE OPPOSITE MAPPING. The advance therefore subtracted the step
// the law was about to add. Census over the whole corpus, 240,271 cube ticks,
// each level cut at its own first divergence so both sides stay on one
// world-line; median |error| against GD's own `rot` column at tick t, mod 90:
//     s.rot, no advance          1.731    (corpus 1.720 .. 2.247)
//     s.rot - step (as shipped)  3.462    (corpus 3.450 .. 4.473)
//     s.rot + step (law's sign)  0.023    (corpus 0.002 .. 0.664)
//     the model's own rot[t]     0.000    (corpus 0.000 .. 0.028)
// The shipped arm is EXACTLY TWICE the no-advance arm on every level -- 3.462
// against 1.731 full size, 4.473 against 2.247 for lv11's mini -- which is the
// arithmetic signature of a sign inversion and of nothing else. Independently,
// the column's own step carries `rotNeg` with the law's sign on 180,093 of
// 180,248 airborne cube ticks.
//
// AND THE REMEDY IS NOT TO FLIP THE SIGN. The census scores against GD's
// END-OF-TICK column; the SAT is fed an angle INSIDE the tick. Asked at GD's
// own positions for lv20's uid 7030 (Q1 -- the rule, not a trajectory), margins
// in px at the two ticks that decide it:
//     arm                        7,298            7,299        fires
//     GD's own rot[t]            31.220 -> -0.585 32.950 -> +0.642  7,299
//     s.rot, no advance          32.950 -> -0.157 31.220 -> +0.214  7,299
//     s.rot + step (law's sign)  31.220 -> -0.585 29.489 -> -0.228  never
//     s.rot - step (as shipped)  34.681 -> +0.257 32.950 -> +0.642  7,298
// The law-signed arm fires NOWHERE, because GD's rotation reverses on the
// activation tick -- the pad's own runNormalRotation turns the column from
// -1.7308 to +1.7308 at 7,299 -- so extrapolating from the pre-pad sign lands
// at 29.489, inside the only refusal notch at that position (swept at 0.001 deg
// over the whole circle: refuses on 27.051 .. 30.375 and accepts everywhere
// else). Choosing on the census median would have deleted lv20's pad.
//
// `s.rot` unmodified is the only candidate the model can compute before the pad
// loop that reproduces GD's answer, and it has margin on both sides: it refuses
// 7,298 by 0.157 px and takes 7,299 by 0.214 px, against a column drift of
// 0.023 deg which is 0.006 px at the local slope (0.243 px/deg) -- a factor of
// 25 of slack. All 28 cases of that table were re-evaluated through the SHIPPED
// predicate (`leveldp --eval-padgate`): 0 disagreements, 5 hits and 23 misses,
// so the agreement is not the trivial agreement of two functions that say no.
//
// REACH, measured at GD's own positions over the whole corpus: 7,765 oriented
// objects, 46 of them in the families that reach these four sites (lv16 2,
// lv18 4, lv19 4, lv20 23, lv21 13, lv22 none at all), of which a cube's AABB
// window touches 13. The two arms differ on 5 objects and change a FIRST FIRE
// on exactly one -- lv20's pad uid 7030, 7,298 -> 7,299, which is GD's tick.
// The other four are lv16 uid 3450's contact tail (8,072 vs 8,073) and three
// that agree. It does NOT fix lv20: the model still dies at 15,125.
inline bool g_noSatRotRaw = false;      // --no-satrotraw

// --no-padplayerrot: a rotated PAD's activation is judged with the player square
// forced AXIS-ALIGNED (the 2026-09-06 behaviour, commit a874728), instead of
// turned by the player's own rotation.
//
// GD's gate, read out of `GJBaseGameLayer::collisionCheckObjects` (2.2081 win
// 0x214960; measured 2026-09-06), reaches
// `activatedByPlayer` (vt +0x558, via bumpPlayer 0x2179d0, its only call site)
// for a type-8 pad iff BOTH of
//    inclusive AABB overlap of player->getObjectRect() and obj->getObjectRect()
//                                                            0x214b09-0x214b56
//    and, when obj->m_isOriented [+0x2e8],
//        overlaps1Way(objOBB, plOBB) && overlaps1Way(plOBB, objOBB)
//                                          0x214b5e / 0x214bb0 / 0x214bbf
// where the player's OBB is a 30 x 30 square TURNED BY `player->getRotation()`
// (PlayerObject::getObjectRotation 0x3a0670 = CCNode vt+0x158), rebuilt from the
// live position inside the object loop (0x214b95/0x214ba1; setPosition dirties
// it every tick at 0x39c676). There is no radius, tolerance, depth or velocity
// term in the gate: the player's angle is the only input to it that moves.
//
// a874728 forced that angle to 0 because the mod's `ccl:` hook prints an
// axis-aligned 30 x 30 `prect`. That rect is real, but it is the AABB conjunct
// (`player->getObjectRect()`, taken once at 0x2149b8) -- NOT the shape the SAT
// uses. `PlayerObject::getOrientedBox` (0x3a0650) never sets the player's own
// m_isOriented, which is exactly why `prect` reads 30 x 30 while the cube spins
// and why every instrument that prints only the rect is blind to the rotation.
//
// The witness is lv20's uid 7030 (id 35, (10835.9,133.5), rot 29, ohw 18.1253 /
// ohh 2.9000). Re-scoring the two-way SAT at GD's OWN x,y and rotation column
// for dump ticks 7,295..7,301 (margin, px, > 0 = contact):
//     t       7295    7296    7297    7298    7299    7300    7301
//   pRot=0   -1.176  +0.672  +2.570  +4.237  +5.036  +0.984  -3.025
//   GD rot   -1.486  -1.213  -0.913  -0.585  +0.642  -2.996  -6.606
// The turned square first passes at **7,299, which is GD's own activation
// tick**; the axis-aligned one passes at 7,296, three ticks early, and that is
// lv20's whole-run first divergence before this change. After it, at the
// MODEL's own angle rather than GD's, the firing landed on 7,298 -- one tick
// still early, and that last tick was the ROTATION-SIGN leaf and not this
// shape. [2026-09-06] That leaf is taken too: with g_noSatRotRaw's advance
// deleted the model's angle is GD's to 0.023 deg and this pad fires on 7,299.
//
// SCOPED TO THE MODES WHOSE ANGLE THE MODEL TRACKS -- see padPlayerRotMode.
// Kept as the A/B arm: with it the whole 22-level replay suite has to be
// byte-identical to the build before the change.
inline bool g_noPadPlayerRot = false;   // --no-padplayerrot

// Which modes may hand the pad gate the player's real rotation.
//
// GD turns the player's square in EVERY mode; this list is a hedge against the
// MODEL's own rotation, not against GD's. Handing the SAT an angle the model
// does not track would make the test worse than the axis-aligned square it
// replaces, so the list is derived, not chosen:
//
//   * `State::rot` is written in exactly four places -- the wave's easing
//     (step.hpp, `kWaveRotK`), the mode-portal edge table, the cube's spin and
//     the ship's bank, and the ball's `rotStep`. The rotation block itself is
//     `if (c.mode == 0) ... else if (c.mode == 1) ... else if (c.mode == 2)`,
//     plus the wave inside its own branch. So modes 3 (ufo), 5 (robot), 6
//     (spider) and 7 (swing) carry rot == 0 for their whole life (the edge
//     table writes 0 on entry for every mode but cube and ball, and nothing
//     advances it), and for them this list cannot change a single digit
//     whichever way it is written.
//   * Of the four modes that DO carry an angle, the rate census over the whole
//     corpus (415,581 ticks, |d rot/tick| error > 0.01 deg) reads cube 3.7% /
//     ship 13.8% / wave 21.1% / **ball 70.3%** -- and the ball's number is
//     categorical rather than a coefficient error: its air rate is a stake only
//     `flipGravity` and `ringJump` write and the model does not carry one, so a
//     ball that left the ground off a step or a pad is turning at a rate that
//     is simply not GD's. Sweeping the threshold from 0.001 to 1.0 leaves the
//     ball at 70.3% (categorical) while ship and wave collapse (boundary).
//
// So the only load-bearing exclusion is the BALL. UNWITNESSED EITHER WAY: all
// 21 corpus contact rows against a rotated pad are mode 0, full size (census
// below), so nothing here is measured -- it is the shape of the hedge.
inline bool padPlayerRotMode(uint8_t mode) {
    return mode == 0 || mode == 1 || mode == 4;   // cube / ship / wave
}

// (--no-dualflip, a dual's flip not reaching the partner, is gone since the flag clean-up.)
// GD's gate for firing the partner: the two bodies' SIX mode bytes must match.
// Written as GD writes it rather than as  == b, because the difference is
// real -- wave is not among the six, so a cube and a wave both read false on
// every compared flag and therefore COUNT AS MATCHING. Collapsing this to
// equality would refuse a pair GD accepts.
inline bool sameModeFlags(uint8_t a, uint8_t b) {
    static const uint8_t kCmp[6] = {1, 2, 3, 5, 6, 7};  // ship ball ufo robot spider swing
    for (int i = 0; i < 6; ++i)
        if ((a == kCmp[i]) != (b == kCmp[i])) return false;
    return true;
}
// --ringtaplink (on by default since 2026-10): a ring that turns a body's gravity reports it to the partner
// (--dualorbflip, step.hpp) even when the press had already made a grounded ball tap on the same
// tick. pushButton gives the press to the ring (ringJump, then m_jumpBuffered = 0), so the tap
// never happens in GD, and the gravity ring's turn goes through GJBaseGameLayer::flipGravity
// (ringJump 0x39967c), which sets the partner to the inverse. The model taps first and undoes the
// tap in the ring, so the body ends where the tap had put it, and comparing with that hid the
// ring's turn. A custom level, a dual pair of mini balls: p1 on the ceiling presses inside a gravity
// ring that a lock-x Move carries with the player; GD turns the airborne p2 0 -> 1 with vy x 0.5
// (-0.0585), the model left it at -0.117. The caller was read off the raw stack in the mod's
// flipGravity hook.
inline bool g_ringTapLink = true;
// --rtlp2ring (on by default since 2026-10; with --ringtaplink): when the second body fires a yellow, pink, red or
// drop ring on the same fresh press, the late partner call does not halve it. The buttons phase takes
// p1's button first: p1's ring turns p2 (the partner call, halving p2's moved vy), and only then does
// p2's pushButton fire its own ring. ringJump sets those four from the gravity it finds and nothing
// else: the drop ring's 0x20 branch (0x1403991f8..) writes +-15 (+-14 flying) by the upside-down byte,
// and the jump branch writes sign x strength x the type's factor (yellow 1, pink 0x0c, red 0x23), the
// sign read after any flip of its own. The green ring (0x1d) flips before that write and the blue one
// (0x0d) after it, so they turn p2 again from where the partner call left it -- not this rule. The
// model fires p2's ring inside p2's step, under the old gravity, and then halved the result. A custom
// level, dual mini balls, t=13,162: p1 presses inside a gravity ring that moves with the player, p2
// inside a drop ring (uid 88839); GD has p2 up 1 -> 0 with vy -15.000, the model +7.5 (+15 halved).
inline bool g_rtlP2Ring = true;
// r52 (a gravity portal right after a rotation-frame change does not fire if the
// player was already inside it) is not subsumed by the portal latch: it also
// covers type 3. (--no-r52gravhold is gone since the flag clean-up.)
// The gravity-frame speed at or below which a ramp CONTACT becomes a LANDING.
// GD's own: hitGround sets m_isOnGround only when s*v <= this (comisd against
// the double @0x622E98); above it the caller restores the old vy (0x3907dd).
// Not fitted -- read from the binary.
constexpr double kSlopeLandV = 5.0;
// (--no-slopeland5, a ramp contact that always lands, is gone since the flag clean-up.)
// (--old-slope, the A/B switch that restored the pre-2026-08-04 slope exit -- ball = tap-anchored line,
// mini x0.625, no ride-time ramp -- is gone; see slopeExitVy / slopeRampFactor for why the current form
// is the disassembled truth.)
// A jump taken while still ON a ramp adds this FRACTION of the ramp's own exit
// velocity (slopeExitVy's cube row), on top of the mode's jump. Measured on the
// `rampjump` calibration map -- see the jump branch for the 12-unit table.
constexpr double kSlopeJumpBonus = 0.25;
// The bonus is capped at 0.4 x that mode's bare jump value (rampjump rig 12/12,
// measured in the note at the jump branch).
constexpr double kSlopeJumpBonusCap = 0.4;
constexpr double kSpiderGScale = 0.6;
constexpr double kSpiderHalf = 13.5;
constexpr double kSpiderHalfMini = 8.1;   // 13.5 * 0.6, UNVERIFIED
// ...and the teleport SEARCH is 1.0 px wider in x than that. The lv22 note above
// could only say "at least as wide as the player's own box"; lv21 pins it.
// Measured 2026-08-10 on lv21's flipped spider resting at y=406, tapping next to
// the block (11791,375) whose top is 390 (so the near target is y=403.5 and the
// far one is y=223.5, 180 px apart -- the level's whole route turns on which).
// Seven injections of the player's x through the MCP, reading GD's landing:
//   |x-cx| = 28.799  29.299  29.499 -> 403.5 (near)
//   |x-cx| = 29.510  29.650  29.799  29.999 -> 223.5 (far)
// so the boundary is 29.5 to within 0.011 px, i.e. hw + 14.5 for a 30-wide
// block. The model was using hw + kSpiderHalf = 28.5 and took the far target
// on every tick in the 28.5..29.5 band.
// NOT SEPARABLE from this one site: "the spider's x half is 14.5" and "the box
// is 15 wide and GD wants more than 0.5 px of overlap" are the same formula
// (hw + 14.5) for every object. Only the vertical half is independently pinned
// at 13.5, by the rest positions (403.5 = 390 + 13.5, 223.5 = 210 + 13.5).
// Applied to the teleport search ONLY, which is where it was measured. The
// support test a few lines up has the same shape and may well want the same
// number, but that is a second variable and lv1-20 cannot referee it (nothing
// before lv21 has a spider).
constexpr double kSpiderSearchHalfX = 14.5;
constexpr double kSpiderSearchHalfXMini = 8.7;   // 14.5 * 0.6, UNVERIFIED
// The wave has TWO different sizes, and they are not the same number.
//
// The BAND clamp keeps its centre 10 off the surface: measured on lv17 t=3565,
// it comes down the diagonal and stops dead at y=100.000 (vy=0, onGround=1)
// on a band whose floor is 90, then slides for 30+ ticks. That 10 is the
// clamp's own constant (the same slot that holds 15 for everyone else), not a
// hitbox.
//
// Its HITBOX is far smaller -- about 5. Two independent brackets on lv17:
//   * the ball portal at (11055,197) w=34 fires at x=11033.47, and the tick
//     before is 11032.17, so half is in [4.53, 5.83)
//   * the ship portal at (8865,195) h=86 spans y=[152,238] and the wave rides
//     past it at y=140.25/141.54 WITHOUT switching, so half < 10.46
// 5.0 is the only round value in both. This half is what portal, pad and orb
// contact use as well -- with 15 the model entered that ship portal four ticks
// early and came out in the wrong mode while GD was still a wave.
// The mini values carry the established 0.600 ratio and are UNVERIFIED.
// CONFIRMED against GD 2026-08-06: injecting the player at y=600 in lv20's wave
// section and reading where GD puts it back gives y=380.000 at three x well
// apart (4,550 / 5,121 / 5,199). The band there is the x=867 wave portal's
// (cy=239 -> floor 90, ceil 390) -- the second wave portal at x=4,445 is INERT
// because the player is already a wave, so it never rewrites the band. 390-10
// = 380 exactly. 10 it is; pHalf (5) would give 385 and 15*vsize would give 375.
constexpr double kWaveClamp = 10.0;
constexpr double kWaveClampMini = 6.0;
// The side/crush kill's grace after a gravity flip: GD stamps the flip time in
// flipGravity (player+0x800) and collidedWithObjectInternal (:1180-1226) takes
// the kill arm only when `now - stamp >= 0.1` s, otherwise re-seating the
// player on the face and calling hitGround.
//
// MEASURED IN THE GAME, lv18's blue pad at (24375,273) and the two 30x30 blocks
// under and beside it (2026-09-01, worker 98). The player is injected 2.5 px
// into a block's top face -- the inner box crosses it, the outer box does not
// resolve it -- and the only thing varied is how long ago gravity flipped:
//     no flip at all                         DEATH
//     flip on the same tick                  LIVES, re-seated to top+15, og=1
//     flip 12 ticks earlier                  LIVES, og=1
//     flip 24 ticks earlier                  LIVES
//     flip 25 ticks earlier                  DEATH
//     flip 36 ticks earlier                  DEATH
// so the window is 25 ticks counted from the tick the dump first shows the flip
// -- 0.1 s x 240 Hz = 24, plus one because GD stamps the time after that tick's
// row is written. Counted the model's way (flipT = 0 on the tick gdUpOf
// changes), the grace holds while flipT < 25.
//
// SIMPLIFICATION: GD measures TIME and this counts TICKS. They agree everywhere
// the corpus goes except inside a timewarp zone (id 1935), where the substep
// density changes; lv22's is the only one.
constexpr int kFlipGraceTicks = 25;
// Ages (State::spiderJumpT) for which a spider is spared a solid's side kill: now - jump < 0.04 s.
constexpr int kSpiderJumpGraceTicks = 10;
// --spidergraceage: ...and age 10 is spared too, ages 0..10. Measured in GD (worker 99), SubZero
// 4002: a flipped spider teleports onto a ceiling at t=23,988 (age 0 on that row) and walks left
// through the 3x30 wall uid 17176. Unshifted it lives at 23,998 (age 10) and dies at 23,999, where
// the wall's right edge also leaves the inner box; with x moved right by 1, 2 and 3 ticks of travel
// at 23,995 the edge test holds through 23,999 and later, and GD still dies at 23,999 (age 11) every
// time. The model killed at 23,998. On by default; --no-spidergraceage is the off arm.
inline bool g_spiderGraceAge = true;
inline int spiderJumpGraceTicks() {
    return g_spiderGraceAge ? kSpiderJumpGraceTicks + 1 : kSpiderJumpGraceTicks;
}
// How long an id-1859 touch keeps the ceiling resolution armed. GD sets its
// counter (player+0xb7c) to 2 on the touch and decays it once per tick, so the
// arm covers the touch tick and the one after it. See modifiers.hpp's
// armBoxTouch for the three measurements that pin the object as the real
// discriminant.
constexpr int kArmTicks = 2;
constexpr double kWaveHalf = 5.0;
// ...and the MINI one is NOT 0.600 of it. That carry-over was flagged
// UNVERIFIED above and this session refuted the same 0.600 twice already (the
// kill box does not scale at all, the ramp box scales by ~0.70). GD brackets it
// on lv20's 1.1x speed portal uid4303 (rot 41, real box 51 x 56 -- obb and
// w0,h0 agree, so the oriented shape is not in question). Writing the fire test
// on the portal's own long axis, the threshold is `25.5 + 1.41077 * half`:
//   x=6128.008 y=298.316  |lx| = 28.979  GD does NOT fire  -> half <  2.466
//   x=6131.903 y=300.912  |lx| = 27.743  GD DOES fire      -> half >= 1.590
// (both read off a plain replay's dump, the ticks GD's `speed` column steps
// from 0.9 to 1.1; the model and GD are bit-identical up to the first of them).
// 3.0 fires three ticks early, and after that the run carries 0.316 px of extra
// x per tick for the rest of the level -- the `dy is 0 and only dx grows`
// signature. 2.0 sits in the middle of the bracket; it is BRACKETED, not
// measured, so tighten it before deriving anything from it.
constexpr double kWaveHalfMini = 2.0;
// ...and the HAZARD stage-1 box is a third one again: GD tests the AABB of
// `player->getObjectRect()`, which cfg `hitboxtrace=1` reads as 10x10 full and
// **6x6 mini** -- half 5.0 and 3.0. The 2.0 above is a bracket off a speed
// portal's firing, a different code path, and it is 1 px too small here.
// Bisected live in GD on both faces, two levels, two speeds; see the table at
// `wHazHalf` in stepOne. 3.000 exactly, touching counts, not speed-dependent.
constexpr double kWaveHazHalfMini = 3.0;
// ...and the PORTAL contact box is that same 6x6 rect, so it is 3.0 too.
// Kept as its own name because the two are measured separately.
// [2026-09-01] **The speed portal uses this one as well.** It was the last
// portal path still on kWaveHalfMini, on the strength of the lv20 bracket
// above -- and that bracket does not constrain it: its site (uid4303) is a
// FULL-SIZE wave in the reference, and it was computed with an axis-aligned
// player square (`25.5 + 1.41077*half`) where GD tests the sprite-rotated box.
// The model fires that same site on GD's exact tick with the full-size 5.0.
// What the 2.0 did cost: lv21's 0.9 portal at (19587.1,239.18) fired THIRTY
// TICKS and 48 px late, running that stretch 24% fast (census family
// `m4/mini1/g0/gdg0/sp1.1/air/in1`, which the fix removes).
constexpr double kWaveContactHalfMini = 3.0;
// [2026-08-19 night 3] **The mini's portal contact half-width is smaller than the
// collision's 9.** Lining up the ramps calibration rig tick by tick between GD and
// the model (py/calib_diff.py), **only the mini->normal size portal has the model
// 1 tick early, 8/8** (norm->mini is the same tick 8/8, i.e. the normal size's 15
// is right). Bracket: against portal id99 (cx=4470, w=31 -> hw 15.5)
//   GD does not fire at |x-cx| = 23.342  -> half < 7.842
//   GD fires at        |x-cx| = 22.044  -> half >= 6.544
// Same family as the wave's mini being 3.0 (6x6 box). 7.5 is 12.5x0.6, the middle
// of the bracket. **All 8 units share the same geometry, so it cannot be narrowed
// further** -- tighten it with a rig that varies the phase (shifts the portal x).
constexpr double kMiniContactHalf = 7.5;
// ...and SQUARE. The `portwave` rig (2026-08-18) read b ~= 11.5 on the y side
// at portal rots 20/43 and the tall box was implemented and REVERTED the next
// day: the rig put every portal centre at GROUND_TOP+15 while the zero-input
// wave slides at ~+6, and that constant dy of ~-9 enters the SAT's u-axis
// inequality with exactly the |sin| weight the y half has -- (a,b)=(3,11.5)
// at dy=0 and (3,3) at dy=-9 predict the SAME brackets at both angles, so the
// rig cannot tell them apart. A rig for b must VARY the portal heights.
// The square box is the one with in-level evidence: lv20's rot-53 stack
// (uid527, census t0=600) rejects at |ly|=50.89 (t=849) and fires at 49.07
// (t=850) against ohh 45 -- a y half of 2.77..5.80, which is 3.0 and rules
// out 11.5 (that fires at t=846, and the census following shrank 400->247).
// It also agrees with GD's own getObjectRect (6x6 mini).
// ...and the box that KILLS the wave on a solid is a different, much smaller
// one. The 5.0/3.0 above are the CONTACT half (portal, pad, orb) and match
// GD's own getObjectRect for the wave exactly -- 10x10 full, 6x6 mini
// (cfg `hitboxtrace=1`, lv20 t=645/t=654). The rect is not what a solid is
// tested against.
//
// Bisected live (gd_run + inject, boundary to 0.01 px), two levels, two object
// ids, both sizes, and BOTH faces:
//   lv17 t=7040 FULL, block id173 (9135,165) top 180: y=181.510 lives,
//                                                     y=181.500 dies
//   lv17 t=7040 FULL, same wall's left face x=9090:   x=9088.4  lives,
//                                                     x=9088.5  dies
//   lv20 t=839  MINI, block id94  (1093,125) top 140: y=141.510 lives,
//                                                     y=141.500 dies
// Every one of those brackets closes on 1.5, and the exactly-1.5 cases die, so
// touching counts as a hit. The value does NOT scale with vsize -- the mini and
// the full wave share it -- which is why deriving it from the rect (5.0, and
// 0.6x of that) was wrong in both sizes.
//
// This was costing whole routes, not pixels: replaying lv20's own best plan,
// the model tracked GD to 0.3 px for 786 ticks and then killed the player at
// t=786 on id94 (1005,195) at a corner overlap of 1.1 x 3.4 px that GD flies
// straight through.
// SOLIDS ONLY. The hazard test keeps the contact half -- measured, see the
// note at the wave's hazard branch.
constexpr double kWaveKillHalf = 1.5;
// [2026-09-15] kWaveRampKillHalf, the 9.54 bracketed between a corpus over-kill
// and a corpus under-kill, IS GONE: calib_slopespike measured the rule, and it is
// not a box half at any value. A spiked ramp's lethal region is bounded on the
// sloped side by a PERPENDICULAR distance against the contact half (playerHalf,
// so this 1.5 is not it either) and on the flat side by the inner kill box. The
// outline, its two registered predictions and what is still unmeasured are at the
// spiked-ramp kill test in step.hpp.
// The dart's sprite rotation eases toward its travel angle by this fraction of
// the remaining gap per tick. Measured, see the wave branch in stepOne.
constexpr double kWaveRotK = 0.0625;
constexpr double kWaveRotKMini = 0.10;
constexpr double kRadToDeg = 57.29577951308232;
// ...but the RAMP test uses its own box, and it is NOT centred on the player.
// Point-probed on lv20's zig-zag wave corridor (t=4042, one tick, x and y both
// injected, boundary bisected to 0.004 px) against the two ceiling ramps
// (5625,295) 310->280 and (5655,295) 280->310 and the two floor ramps under
// them. Every one of the 26 boundaries is reproduced by ONE rectangle:
//   corner sampled at x + 3.072 when the wedge thickens to the RIGHT
//                     x - 4.068 when it thickens to the LEFT
//   surface offset by 4.0 vertically, and the sample x clamped to the ramp box
// e.g. ceiling line - 7.072 in the far field, flat at corner - 4.0 across the
// 3 px left / 4 px right of the two ceilings' shared corner (that flat is the
// clamp), floor line + 8.072 on the mirrored side -- and the floor pair has NO
// flat, because there the sample points away from their shared corner.
// The old single 8.0 is that rectangle read at 45 degrees from
// one side only (lv18's downhill ramp at (19125,195): kill at line + 8.0 +-
// 0.4, and this box says 8.070 there).
// Only |m| = 1 is measured; the split into "sample x" and "offset y" is what
// separates it from a plain half extent, and it is what makes the corner flat.
// The mini values carry the established 0.600 ratio and are UNVERIFIED.
constexpr double kWaveSlopeDxR = 3.072;
constexpr double kWaveSlopeDxL = 4.068;
constexpr double kWaveSlopeDy = 4.0;
// ...and the mini ratio is BRACKETED, not carried over. The old 0.600 was
// flagged UNVERIFIED and lv20 refutes it; 1.000 is refuted the other way. Two
// GD verdicts in the same corridor pin it, writing the box as `s` x the
// measured full-size one (3.072 / 4.068 / 4.0):
//   LIVES  lv20 t=3691, mini wave (4790.70, 236.210), ramp uid3321 (4785,225)
//          sy0=210 sy1=240 (m=+1, floor half, sample to the RIGHT)
//          survival needs 236.210 >= 230.70 + s*(3.072 + 4.0)  ->  s <= 0.779
//   DIES   lv20 t=3712, mini wave (4819.28, 225.825), ramp uid3383 (4815,225)
//          sy0=240 sy1=210 (m=-1, floor half, sample to the LEFT)
//          the kill needs 225.825 <  220.72 + s*(4.068 + 4.0)  ->  s >  0.633
// so s is in (0.633, 0.779] and 0.700 sits in the middle of it. 0.600 misses
// GD's kill by 0.26 px -- and that one corridor cost the cold run ~40
// iterations of 5 px each, because every tick GD killed and the model did not
// came back as a `kill` fixup instead of as a rule.
// Two brackets, one constant: TIGHTEN IT with py/hitbox_sweep.py before
// deriving anything from the value.
constexpr double kWaveSlopeMiniScale = 0.700;
constexpr double kWaveSlopeDxRMini = kWaveSlopeDxR * kWaveSlopeMiniScale;
constexpr double kWaveSlopeDxLMini = kWaveSlopeDxL * kWaveSlopeMiniScale;
constexpr double kWaveSlopeDyMini = kWaveSlopeDy * kWaveSlopeMiniScale;
constexpr double kShipHalf = 15.0;  // ship outer box (any contact excludes)
// player-vs-hazard half: the OUTER 30x30 box. Measured on lv1: spike (525,105)
// hitbox 6x12 kills at |dx|=17.4 -> 3+15. (rectcorridor's 10 was a safe
// under-estimate for blocking, not the true kill range.)
constexpr double kHazHalf = 15.0;
// Planning margin: GD checks collisions at sub-tick positions and sampling only
// the tick endpoints can miss a corner graze, so plans are kept this far off
// hazard edges.
//
// 1.5 with 4 samples was too blunt for tight corridors. Measured on lv10's ball
// zone: GD carried the ball ALIVE past a spike at a gap of 15.107, while the
// model kills at kHazHalf + margin = 16.5 -- so every branch through that
// corridor died in the model and the frontier collapsed to a single state by
// x=3244. The margin exists to cover the gap BETWEEN samples, so pay for it
// with samples instead: 8 sub-steps put consecutive samples ~0.4 px apart at
// cube speed, and 0.5 covers that with room to spare.
// Even 0.5 was too much. The lv10 measurement pins it: GD ran the ball past a
// spike of half-width 2.79 at a horizontal distance of 17.90 and it LIVED --
// 17.90 is barely outside the exact box contact at 2.79 + 15 = 17.79, so any
// margin at all kills a case GD survives. With 8 sub-steps the sampling gap is
// ~0.4 px and the endpoints are exact, so the margin has nothing left to buy.
// A plan that grazes now shows up as a death in the driver's GD replay, which
// re-anchors -- that is the right place to catch it, not a blanket inflation of
// every hazard in the level.
// Hazards are sampled at this many points between the previous and the new
// position. A tick IS one GD physics step, so GD only ever tests the END of the
// tick -- every intermediate sample is a position the player never occupied,
// and killing on one would be killing on a fiction, which is what makes the
// model miss frame-perfect gaps like lv11's three spikes at x=24,883.
// TRIED AND REVERTED (2026-07-31): kSubSteps = 1, endpoints only. lv1-10 all
// still cleared, but lv11 went the WRONG way -- the GD replay fell from 24,874
// back to 11,914, i.e. the model started emitting plans GD kills. So the
// intermediate samples are catching something real; GD's collision pass is
// evidently finer than the per-tick state dump exposes. The under-kill is the
// dangerous direction, so this stays at 8 until it can be measured directly
// instead of argued from the tick rate.
constexpr int kSubSteps = 8;
constexpr double kHazMargin = 0.0;
// --hazaftersolid (cube): test the hazards AFTER the solid loop, from where the solids left
// the player, instead of inside it in object order. GD's checkCollisions (0x2137f0) runs its
// hazard stage after the solid loop: at 0x214752 it re-reads player->getObjectRect(), i.e. the
// position after the push-out and the landing snap (lab: hazard-kill-rect-2026-09-04.md Q4).
// The model's cube loop tested a hazard the moment it came up in K.near, so a spike sorted
// before the block the cube lands on was tested against the fall's integrated y, below the
// block's top. Measured on lv2 t=16,179 (a route to the third coin): the cube lands on a low
// block at y=114; the model's sweep reached 111.894, 0.506 px into the floor spike beside it
// (id 9 at 21,015,92, top 97.4), and killed on its last two samples -- GD lands and lives, and
// clears the level from there.
// Default since 2026-09-20, always on since the flag clean-up: the replay suite is unchanged
// either way (1,116/1,116 -- no verified solution lands this shape), the 47 reference deaths
// stay at 41/47 with no reference lost, and a cold run clears 22/22 with six levels taking a
// different route. What it buys is the order GD actually uses.
// --flyhazaftersolid: the same change on the FLYING branch (ship/UFO/wave/swing), which was
// left alone when the cube side landed. GD's hazard stage is after the solid loop for every
// mode, so the same hole should be there -- but nothing has measured it, so this one is off
// by default until it is.
// On by default since 2026-10 (was opt-in).
// (The switch is gone since the 0.4.0 clean-up; its on behaviour is fixed.)
// --hazafterportal: ...and with the box a mode portal left the body on this tick. The mode
// branches test the hazards with the box the body came in with, before the portal pass; GD's hazard
// stage re-reads the rect after the object pass, portals included (step.hpp, after the portal pass).
// A custom level's wave takes a UFO portal beside a saw (radius 18.54): GD kills it on the portal's
// row with the UFO's 30 x 30 (closest point 17.8 from the centre), where the wave's 10 x 10 a tick
// earlier was 30 px clear; the model, the same x, y, vy and rotation on that row, died a row later,
// and the kill record was refused next to moving geometry three iterations running. A size portal
// is not one: a custom level's mini cube turns full size beside a small spike, the full box overlaps
// it by 1.8 px on the portal's row and GD lives, then GD kills it a row later at a 1.4 px overlap.
// On by default since 2026-10; --no-hazafterportal turns it off.
inline bool g_hazAfterPortal = true;
// --hazportalonce (default on since 2026-10): ...and that test is the only one. A body taking a mode portal that changes
// its box on this tick is not hazard-tested by its mode branch with the box it came in with; the test
// after the portal pass is the hazard stage GD runs, once (step.hpp, hazOnceSkip). Measured with a
// probe on lv18: the upside-down mini UFO takes wave portal uid 13660 on t=19,624 with y injected at
// 419 on the tick before, so the UFO's box (half 9) overlaps spike uid 13627 by 6.9 px in x and 2.8
// in y and the mini wave's (half 2) misses it. GD flies on as a wave; the same overlap one tick short
// of the portal is killed by GD on the next tick. The model killed on the portal's tick with or
// without --hazafterportal (the mode branch's fly/hazard). A size portal stays out (see above).
inline bool g_hazPortalOnce = true;
// --padafterportal: ...and the pads. GD's collision pass takes one section's objects in uid order
// (gdCollidesAfter), so a pad the pass reaches after a mode portal that fired on this tick meets
// the new mode's box; the model judged every pad with the box the body came in with. A custom
// level puts a yellow pad (uid 121971, rot -45) inside a ship portal (uid 121969): GD's
// wave turns ship there on t=8,507 and takes the pad the same tick (vy 16, the pad on GD's record
// on that tick's row); the wave's box missed it and the model took it two ticks later. A pad before
// the portal in that order keeps the old box. And a pad the pass reaches BEFORE this tick's teleport
// is tested where the teleport found the body, not skipped (a spider pad still is): the second pad
// of that pair (uid 121972, below the 747 uid 121973 beside it) is on GD's record one row later
// (t=8,508's row is 8,507 -- vy stays 16 through the teleport tick), while the model, skipping it
// there, fired it on 8,509 at the landing site. Frame 0 only. On by default since 2026-10;
// --no-padafterportal turns it off.
inline bool g_padAfterPortal = true;
// --portreread: an axis-aligned portal the collision pass reaches after a mode portal that fired on this
// tick meets the new mode's box only where GD reads the player's rect again in between, and the box the
// body came in with everywhere else (step.hpp, gdRectReReadBetween). collisionCheckObjects (0x214960)
// is called once per section cell and copies player->getObjectRect() into four registers at its head; in
// the call it refreshes them only at 0x215e78, which a ramp (type 0x19), a teleport (0x1c) or a type
// 0x2e object branches to. A ramp's own test there is getObjectRect(2,2), its rect doubled. So the new
// box reaches a later portal in a later cell, or in the same cell past a ramp of a uid in between whose
// doubled rect the entry box touches; a portal in the same cell with no such ramp keeps the old box.
// Measured on a custom level (dynamicLevelHeight), t=6,534: a cube takes the wave portal uid 46045 in
// cell (74, 3) with the mini portal uid 46067 on the same spot, 0.517 px inside the cube's entry box.
// Two routes reach that tick at the same x. GD goes mini on 6,534 in one (y 283) and on 6,538 in the
// other (y 327, when the full wave's 10 x 10 box reaches the portal). Injected, the outcome follows the
// state, not the route; bisected on y (vy -9.424), GD fires on 6,534 from y 297.4722 at 6,533 and not
// from 297.4774, entry box tops 310.303 / 310.308. Only the second run touches the moving ramp
// uid 46049 (GD rect (7440, 340.31, 30, 60), doubled bottom 310.305) between the two portals in that
// cell. Four official scenes keep the old box with the second portal in the mode portal's cell and no
// ramp between: lv22 t=19,681, lv18 t=19,624 and t=13,954, lv20 t=19,236. Frame 0 only; a size portal
// earlier in the pass leaves the rect at the old scale (togglePlayerScale's CCScaleTo, --portorder).
// On by default since 2026-10; --no-portreread turns it off.
inline bool g_portReRead = true;
// --waveportalfree: a cube on the ground that takes a wave portal stays where this tick's update
// left it. GD's update takes the cube's gravity step first; the portal is collisionCheckObjects',
// before the solid pass; PlayerObject::toggleDartMode (0x39af90) halves vy (+0x9a0) and makes the
// box 10 x 10; and the solid pass then has nothing to push the smaller box out of. Every grounded
// cube -> wave row in the corpus (6 levels, on blocks and on the floor, either gravity) reads the
// one-tick fall (0.0486 px at 0.9, 0.0483 at 1.1) with vy halved (-0.108 / -0.1075); the model
// kept the seat. On a custom level (t=90) that 0.049 px carried to t=713, where GD's wave died on a
// block the model's cleared by 0.035 px. Cube only, not dual, no teleport on the tick. On by default since 2026-10; --no-waveportalfree turns it off.
inline bool g_wavePortalFree = true;
// --movingwallseat: a ramp's thin wall / strip (preslopegate's thinSolid) that the slope map vetoes still
// seats the body when the ramp MOVED this tick, and gives it the ramp's own velocity (dy / dt, dt 0.25).
// PlayerObject::collidedWithObjectInternal (0x391a70) reads a moved partner's position delta first
// (vfunc +0x4a8 and getScalePosDelta), stores its velocity at +0x630 and takes a separate branch for it
// (the veto's top flag together with the moved flag reaches setPosition(top + h/2)); a partner that did not
// move stays vetoed. Measured with hitboxtrace on a custom level, t=2,001: a mini ball climbing the static
// ramp 15560 to its top corner has two downhill ramps ahead of the same shape, static 16047 (top 270.075)
// and moving 16260 (rising 0.06 a tick, top 271.174). GD's slope calls: 15560 seats it at 276.417, 16047
// leaves it, 16260 lifts it to 280.174 = 271.174 + 9 with vy 0.240 = 0.06 / 0.25. The model vetoed both
// (15560's line within 2 px of the wall's top) and the loop died five times at t=2,035. Upright top face,
// and a flipped body's bottom face (step.hpp, the thin solid's flipped arm): on another custom level, t=475,
// a flipped ball riding ceiling ramp 809 down into a V meets the wall of 808, dropping 0.046 a tick in an
// eased Move, and GD puts it at the wall's bottom - 15 with vy -0.185; the same V standing still (ramps
// 12267 into 12400, t=5,377) leaves it on its ride. On by default since 2026-10; --no-movingwallseat turns it off.
inline bool g_movingWallSeat = true;
// --movingwallhead: ...and a FLIPPED body rising (vy > 0) whose head side meets the top of a ramp's thin
// wall / strip that moved UP this tick is put on that top too, keeping its vy. collidedWithObjectInternal
// (0x391a70) marks a partner that moved with the body's gravity (0x391e24: flipped and the y grew), and
// with vy > 0 (`0 < m_yVelocity`, the velocity as stored, not along gravity), the bottom within reach
// (bottom + tol >= top now or last tick) and the top under the body's top - tol (the head contact) it
// takes setPosition(top + h/2), hitGroundNoJump, vy = max(vy, +0x630 + 1.0) (the max at 0x392815) and
// didHitHead. +0x630 is not written on that path. The same site as --movingwallseat, a custom level t=2,001,
// on the solution's own run: the mini ball taps off the top of ramp 15560 on t=2,000 (flipped, vy 2.730)
// and on 2,001 its right edge reaches the wall of 16260 (rising 0.06 a tick, top 271.174): GD's row reads
// y 280.174 = 271.174 + 9 and vy 2.859, airborne; the model's thin solid has no arm for vy > 0 there and
// left it at 276.537 (the whole-run replay then died at t=2,721). UFO left out (its branch also reads a
// byte the model does not carry). On by default since 2026-10; --no-movingwallhead turns it off.
inline bool g_movingWallHead = true;
// --movingwallrise: an upright body seated on the top of a ramp's thin wall / strip that MOVED this tick
// takes collidedWithObjectInternal's moved-partner arm (0x391a70, the writes before 0x392884): setPosition(top
// + h/2), hitGround, +0x630 = the partner's velocity and vy = max(vy, +0x630) -- so the body keeps a vy that
// beats the wall's, where --movingwallseat wrote the wall's. (GD's onGround is set, but GD still moves the body
// by that vy; the model's `grounded` would hold it still, so it stays clear, as --movingwallseat has it.) The
// arm also takes a body RISING into a wall that moves up (`0 < m_yVelocity`), which the thin solid's arms left
// alone. And the ride the wall lifts the body off ends: the ramp's window still holds it, but its target is
// under the body, so the next tick's uphill exit launch fires. A custom level t=7,497..7,502: a mini cube
// climbs ramp 7884 (m 1) toward the apex of 7885, both rising about 0.95 px a tick. GD puts it on 7885's
// wall at 800.492 = top + 9 with vy 3.743 = 0.935 / 0.25; next tick the launch 4.449 = slopeExitVy(1,
// cube, 1.95) 11.12 x 0.4 off 7884; then 4.233 = max(4.233, 3.932) and 4.026 = max(4.017, 4.026) seated
// on the wall again; the model, which kept the ride three ticks longer and then stood on the apex at vy 0,
// now follows GD's rows to the spike it dies on at 7,581. Includes --movingwallseat's upright case. On by
// default since 2026-10; --no-movingwallrise turns it off.
inline bool g_movingWallRise = true;
// --dualbornportal: a second body born at a dual portal that stands on a gravity portal takes it on its
// first step, and the partner it turns is judged by the two bodies' modes as they stand then. Measured
// with hitboxtrace's pfg: line for both bodies on a custom level, t=12,905: a dual portal stacked with a
// size, a yellow gravity, a speed and a ball portal; p2 is born on 12,904 (normal gravity, the mirror of
// the upside-down p1). On 12,905 p1 takes the ball portal first, then p2 is flipped 0 -> 1 by the
// gravity portal (vy -3.477 -> -1.7385), back 1 -> 0 by the ball portal's re-mirror (playerWillSwitchMode,
// -> -0.869) and halved by the mode change: GD's -0.4346, the model's -1.7385. Two model rules held it:
// the dual "entry tick only" skip read p2's birth position as "was already inside" -- GD's portal
// activation is the player's own and the new body has fired nothing (the --teleoldpos mark) -- and
// --dualcouple compared the modes at the start of the tick, where GJBaseGameLayer::flipGravity (0x212b00)
// compares them when p2's portal fires: p1 is a ball by then, p2 still a ship, so p1 is left alone.
// On by default since 2026-10; --no-dualbornportal turns it off.
inline bool g_dualBornPortal = true;
// --movelockx: a Move does not move a speed portal (or a trigger) in x. GJBaseGameLayer::moveObjects
// (0x22dd50), which every Move goes through, adds the x offset only when the object's byte +0x2c8
// is clear. GameObject::customSetup sets that byte from GameObject::shouldLockX (0x196e40), a switch
// over the object id (objLocksX below), and PlayLayer::addObject clears it again in a platformer
// level (layer +0x309e). The y offset always applies. A custom level moves group 25 by +240 from
// behind the start (Move uid 49, dur 0): GD's recording moves 334 of the group's 335 recorded
// objects and leaves the one 4x speed portal (uid 22825, id 1334) where it was authored; the model
// moved it too, so the body met that portal 240 px late (t=3,625) and died at the end of the level.
// On by default since 2026-10; --no-movelockx turns it off.
inline bool g_moveLockX = true;
inline bool objLocksX(int id) {
    switch (id) {
    case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31:
    case 32: case 33: case 34: case 55: case 56: case 57: case 58: case 59: case 105:
    case 200: case 201: case 202: case 203: case 744: case 899: case 900: case 901: case 915:
    case 1006: case 1007: case 1049: case 1268: case 1334: case 1346: case 1347: case 1616:
    case 1812: case 1814: case 1815: case 1912: case 1913: case 1914: case 1915: case 1916:
    case 1917: case 1934: case 1935: case 2015: case 2062: case 2067: case 2068: case 2900:
    case 2901: case 2925: case 2999: case 3023: case 3024: case 3029: case 3030: case 3031:
    case 3602: case 3603: case 3604: case 3605: case 3606: case 3607: case 3608: case 3609:
    case 3612: case 3613: case 3640: case 3660: case 3661: case 3662:
        return true;
    default:
        return id >= 3006 && id <= 3021;
    }
}
// Cube: how far below a surface the foot may already be and still be pushed up.
// 3.0 was a guess and it was too small: lv6 t=13606 has GD landing the cube on
// the ledge at x=17700 (top y=180) with the foot 3.29 px below it, and the model
// missed by 0.29 px and fell past.
// Swept at that ledge (inject y in 0.5 steps, read land/die):
//   penetration <= 10.50 -> lands,  >= 11.00 -> dies
// so the true limit is ~10.5-11. 6.0 is used instead: it is the ship's exactly
// measured boundary (kShipLandTol) and it covers everything observed in play,
// while a 10 px upward teleport would be a large licence to hand the planner on
// the strength of one sweep whose contact had already begun at injection time.
// Redo as a contact-onset sweep (like the ship's) before raising it further.
// TRIED AND REVERTED (2026-08-03): 10.5 (the swept value quoted above), to see
// whether lv18's frontier could then reach the ledge 269 (24,675,255) top 270
// that it tops out 8.35 px below. It changed NOTHING -- the frontier's y range,
// alive counts and death tick were bit-identical. kLandTol only forgives a foot
// that is ALREADY within N px BELOW the face; at y=262 the foot is at 253, i.e.
// 17 px below 270, which no tolerance in this range reaches. lv18's wall is not
// the landing tolerance.
// [2026-08-18] 6.0 -> 10.5 (the measured value the sweep above produced, as is).
// The landing at lv19 t=9,451 has a 6.05px penetration and fell **0.05px outside
// the threshold**. Once the robot's g was corrected to the measured -0.194 the
// player's y matched GD, and as a result the penetration went from 5.958 to 6.047
// and the landing was dropped (GD does land). Restoring the constant to the
// measured value passes both. --landtol A/Bs it.
// [2026-08-18, revised the same day] 10.5 -> 10.0. 10.5 made lv9 STUCK in the same
// day's cold run: the gravity-flipped ball at x=11,265 "landed" from **10.2125px**
// past the underside (180) of the slab (11285,187) and burrowed in, whereas GD does
// not land it and it dies on the wall with an x penetration of 10.9
// (gd_death_context names it). So the top face of the same slab was re-swept finely
// with worker-98 injections (x centred / entering sideways from non-overlapping x,
// vy -2 / 0):
//   d=10.0 (centred)       lands
//   d~10.05 (sideways)     refused -> floats 7 ticks, then side death at xpen~11
//                          (same as the ball)
//   d=10.2125 (real run)   refused (lv9's ball itself)
//   d=10.5 (centred)       dies on the injection tick (the crush side fires first)
//   d=6.047 (real run)     lands (lv19 t=9,451 -- the reason for raising to 10.5;
//                          holds at 10.0 too)
// The boundary is a single one at [10.0, 10.05]; no sideways/centred distinction is
// needed. Yesterday's 10.5 was the value of lv6's 0.5px-step sweep, whose own note
// warned of contamination with "contact had already begun at injection time"
// ([[gd-injection-resolution]]). The side where the penetration keeps deepening
// while refused is killed by cube/solid-side at the same xpen ~11 as GD (lv9's
// replay is confirmed to match on all 8,672 ticks).
inline double kLandTol = 10.0;
// --cubempush: the ground modes' push-out onto a solid face that climbs toward a rising player
// (step.hpp, the ground solid loop; the flight loop's own is mpush). On by default since 2026-10-07;
// --no-cubempush is the off arm.
inline bool g_cubeMpush = true;
// --cubempushnoground (default on since 2026-10): cubempush does not set onGround on the push tick (step.hpp).
inline bool g_cubeMpushNoGround = true;
// --rampslablift (on by default since 2026-10): cubempush's other side -- a solid face climbing into a body that
// rides a ramp. GD resolves the ramps in the candidate loop and the solids after it, and a top face
// that rose within the landing reach of the seated foot (kLandTol + |dcy| / 0.25, cubempush's) puts
// the body on it with the face's speed, dcy / 0.25, which ends the ride; the ramp's exit launch then
// fires on the next tick. The model's mid-ride landing test is the strict 0.001 (the lv19 slab), so it
// left the body on the ramp. A custom level t=4,981: a mini cube on a 0.5 ramp, its right edge 0.85 px
// over a slab climbing 0.073 a tick (group 2, with the moving ramp beside it); GD's hbox line has the
// box on the slab's top 282.48 (y 291.478, vy 0.293), the exit launch (vy 1.343) a tick later. One
// witness, a mini cube.
inline bool g_rampSlabLift = true;
// --headpushhold (default on since 2026-10): cubempush's head side keeps pushing a body whose vy is 0 (a robot's hover
// after the bonk), with GD's pin rule for the velocity -- min(vy, -1) when the updated vy points with
// gravity, 0 otherwise -- and a robot's hover ends one tick after the push, as --bonkouter's plain bonk.
inline bool g_headPushHold = true;
// --mpushupright (default on since 2026-10): cubempush's head side is for an upright body only. Its evidence is all
// upright (a slab coming down onto a rising cube, a robot under a block); the flipped mirror was never
// measured. A custom level's upside-down cube falls toward its head onto a thin moving plate (30 x 1.5,
// rising 0.035 a tick) 6.3 px inside it: GD neither pushes it nor kills it there -- y runs on unchanged
// with the cube moved 3 px left or 1 to 12 px right in x (an inject sweep), and a pink pad beside the
// plate launches it a tick later where it dies. The model put it on the plate and flew on.
inline bool g_mPushUpright = true;
// --armpost (default on since 2026-10): the id-1859 ceiling arm is renewed by the player's POST-move box against the
// box's row for this tick, not by the start-of-tick box. GD writes the arm in collisionCheckObjects
// (0x215ac6), after the move, and resolves the solids after that in the same pass (collidedWithObject
// 0x214687), so the arm already gates the ceiling on the tick of the first overlap; update's decrement
// (0x389f47) keeps it for the next tick. With the flag the flipped cube's bonk takes the same window as the
// upright one (step.hpp, the bonk gate).
inline bool g_armPost = true;
// --fgpost (default on since 2026-10): the same for the id-2866 flip arm (State::fgArm, GD's 0xb80 written at 0x215ba1
// and decremented at 0x389f40): renewed by the post-move box against this tick's row, and the head-hit
// gates (the armed flip, and the plain bonk's "not armed") read the value after this tick's touch instead
// of the parent's. The model read it from the start-of-tick box and then a tick later again, so a first
// contact on the head-hit tick itself armed it two ticks after GD.
inline bool g_fgPost = true;
// --rampendclick (on by default since 2026-10-07 with --groupxsplit, which opens a lv21 route that needs
// it; --no-rampendclick is the off arm): a cube hanging under a ceiling ramp whose ride ends by itself on a
// tick (the contact point has run off the end) and that also has a FRESH press that tick takes the
// slope-exit launch and no jump, as GD does (step.hpp, uphillLaunch). GD's order is update ->
// collisions -> buttons: the collision pass ends the ride (the launch clears onGround), and the press,
// handled after it, finds no ground. A press while the ride still holds jumps (with the ramp bonus),
// which is what the impulsedThisTick gate there already says.
inline bool g_rampEndClick = true;
// --exitjump (default on since 2026-10): an upright cube that rode a floor ramp last tick (on it and grounded) and
// presses on the tick the model ends the ride jumps, with the ramp bonus, and the exit launch loses to
// that jump. pushButton (0x397f40) runs updateJump(0) at the head of the tick when the body can jump
// (+0x9c1), before the movers; the jump raises +0xa1c, and postCollision's exit block (0x38d7f5)
// skips the launch for a cube whose +0xa1c is up -- and the launch only beats a smaller vy anyway
// (0x38d779). A custom level t=4,708: a cube rides a 2:1 floor ramp up to y 634.955 (top seat 635),
// presses, and GD comes out at vy 11.58 = 11.18 + 1.599/4, y 635.000; the model ended the ride on that
// tick and wrote the launch, 1.599.
inline bool g_exitJump = true;
// --seatspeed: a ride whose speed fell since its last seat ends, and launches at the old speed.
// PlayerObject::collidedWithSlopeInternal (0x38f810) returns before touching the ramp when the body is
// already on a slope going up (+0x9b1 set, +0x9b4 positive on its gravity's side) and the ramp's rate
// at the current speed, h/w x the player's speed (+0x9f4 x +0x7b8), is under the rate it stored at
// the last seat (+0x938, written at the end of the same function with +0x9b4). Nothing holds the body,
// and the exit launch reads +0x9b4 -- the old speed's. The model's convex seam (step.hpp, the ramp
// loop) is the same test at one speed; this adds the speed (State::seatSpd) and ramps the launch from
// the seat's dx. The ramp is seated before a speed portal of a larger uid in the same pass, which is
// what leaves the old speed stored; the other order (portal first) is not modelled.
// A custom level, t=4,065..4,067: a ship riding a 45-degree ramp turns ball and passes a 0.5x
// portal (1.95 -> 1.0465 px a tick); GD seats the ball at 4,066 and leaves at 4,067 with vy 8.342,
// the ball's m=1 launch at dx 1.95; the model rode on to the ramp's top and left at 4,069 with 4.477.
// Upright riders on a ramp's top only, as the convex seam. On by default since 2026-10; --no-seatspeed turns it off.
inline bool g_seatSpeed = true;
// --dropground: a drop ring (GameObjectType 0x20) leaves the player's grounded flag as it was
// (step.hpp, the ring pass; ringJump skips its onGround clear for that type). On by default since
// 2026-10-07; --no-dropground is the off arm.
inline bool g_dropGround = true;
// --seatc6 (default on since 2026-10): a FLYING body held on, or caught by, a moved top face writes c6 (the face's
// dcy/0.25) as GD's collidedWithObjectInternal does on both arms of the top-face contact (0x39269a /
// 0x393b0b), takes the seat's max only under GD's own gate (climbing, not off a slope, |c6| <= 5;
// else the hitGround arm's vy 0), and the postCollision launch from last tick's c6 (step.hpp,
// --mpushlaunch) also raises the velocity-limit exemption as boostPlayer does. Before, c6 came only
// from the flight push-out, so a resting body on a fast lift was never launched. GD's write has no
// mode test; the ground modes are left out until a divergence is measured on them.
inline bool g_seatC6 = true;
// --ridec6 (default on since 2026-10): ...and a ground-mode rider carried by a moved face (the cube branch's ride, step.hpp
// rideOn) writes c6 too, so the postCollision launch from last tick's c6 reaches it. A custom level t=4,455: a
// cube stands on a block that an eased Move lifts 3.970 px on one tick and 3.414 on the next; GD leaves at
// vy 15.880 = 3.970 / 0.25 on the slowing tick, and the model stayed seated on the block.
inline bool g_rideC6 = true;
// --orbswing (default on since 2026-10): the yellow and pink orbs give a swing the x0.600 every other ring already
// does (step.hpp, the yellow/pink branch; ringJump's common tail 0x3995be..0x3995f9 has no type test).
// Yellow: measured (a mini swing, GD 5.390 against the model's 8.984). Pink: binary only, unmeasured.
inline bool g_orbSwing = true;
// --flyclampreach (default on since 2026-10): GD clamps a flyer to the band (checkCollisions 0x213c38) BEFORE it
// resolves the objects (collisionCheckObjects, 0x214412), so a body the band ceiling just pulled down is
// offered to a rising solid's top face again, with the face's speed in the reach (step.hpp, the moved-
// face push-out). The model's push-out skipped a grounded body and one whose centre was under the face,
// which is exactly the body the clamp leaves on a fast lift. Upright only (the witness).
inline bool g_flyClampReach = true;
// The "gap between the extrapolated seat and the player's y" that ride continuation
// (stickHere) allows. **A constant in px** -- measured on the seam rig at 3 speeds
// (see the stickHere note). --stickgap A/Bs it.
// The value is the midpoint of the range [4.0307, 4.0371) that satisfies **130** of
// the 132 units (seam/seam07/seam11 x cube/ball x |m|{1,0.5} x 12 phases). The 2
// outliers are one-offs (seam unit8 wants >= 4.087, seam11 unit43 wants < 4.026),
// 1 tick at the edge of the phase -- the remaining systematic error is under 0.05px.
inline double kStickGap = 4.034;
// How far off a rotation trigger (id 2900) the player may be on the PERPENDICULAR axis and still
// fire it. Bracketed by GD's own gframe changes across two full lv22 runs -- fires at |dv| up to
// 116.7, skips at 274.4 and 657.8 -- so anything in that gap fits every measurement; 150 sits in
// it with margin either side. The reasoning, and why the old 30 was falsified, is at the gate in
// step.hpp's applyRotation. --rotperp A/Bs it.
inline double kRotPerpWin = 150.0;
// How deep a top face may sit (how far below the line) and still be grabbed as a
// "step" while riding. Midpoint of the measured boundary (3.100, 3.200] on the
// ridestep calibration rig. --stepdepth A/Bs it.
// (--nohangladder, which cut the hanging ride's ship ladder r88, is gone since the flag clean-up.)
// [2026-08-22 r106] Inner-box half-size for the crush (squeeze). Measured on the
// calib_crush calibration rig: "instant death when the inner box (centre +-4.5)
// touches the interior of a solid". The boundaries are
// cube (19.5,19.6] / robot (19.5,19.75] / ball (19.5,20.0] /
// mini cube (13.5,13.6] -- all consistent with "dies when ceiling underside -
// centre <= 4.5", and **still 4.5 for mini** (the same size-independent family as
// the wave's solid=1.5). Injecting into the interior of a corridor also dies on
// the injection tick = an interior intersection, not an entry condition.
// Independent corroboration: the kLandTol sweep's "d=10.5 (centred) dies on the
// injection tick (the crush side fires first)" = the centre is the same 4.5 from the
// slab's top face. The existing cube/solid-side killing at xpen ~11 (= 15 - 4.5 +
// alpha) is the same box seen from the side. (--nocrush is gone since the flag clean-up.)
inline double kCrushHalf = 4.5;
inline double kStepDepth = 3.15;
// Ship: MEASURED, not guessed. State-injection sweep at lv4's ledge x=17250
// (top y=150), stepping the injected y by 0.1 and reading the outcome:
//   penetration <= 5.95  -> lands (GD pushes the ship up onto the ledge)
//   6.05 .. 9.15         -> passes straight through, no landing and no death
//   >= 10.15             -> dies
// so the landing limit is exactly 6.0, twice the cube's 3.0. Using the cube's
// value here made the model fall past ledges GD had landed on (lv4 t=13257,
// penetration 5.67 -- it missed by 0.02 px and cost the whole level).
// The 6..10 "passes through" band is GD having a smaller inner box for the
// kill test than the outer box it rides on; the model kills there instead,
// which is conservative (it never plans a route that GD would end).
constexpr double kShipLandTol = 6.0;
// --ceilrideslope: the flight ceiling ride (fly/ceilride) does not fire while a
// ceiling ramp is pressing the player (see the branch in step.hpp). Always on since 2026-10 (the
// slope clean-up; was opt-in). The 6.0 above was re-measured on 2026-09-18 on calib_slabside (a
// UFO on the floor entering a 30x1.5 slab sideways): head 6.0 px into it is pushed out, 6.1 is not.
// --lawseatonslope: a ride's slopeT starts from the --slopelaw seat, not from the
// model's landing (State::seatT). Always on since 2026-10 (the slope clean-up; was opt-in).
// --ceilpinmin: a flight pin under a ceiling that moved toward gravity this tick
// sets vy to min(vy, -1) when the updated vy points with gravity, and 0 otherwise,
// instead of the face's dcy/0.25 (see the ceiling-ride branch). On since
// 2026-09-19; always on since the flag clean-up.
// GD's min (0x392e35) has no mode gate, so --ceilpinmin's two arms (the entry at any depth, the min
// clamp) apply to the swing too; the A/B switch that left the swing out (--pinminnoswing) is gone.
// --mpushsign: a flight push-out onto a rising face keeps max(vy, face) only when
// the updated vy already rises, the player was not on a ramp and the face is not
// faster than 5.0; otherwise vy = 0 (see the mpush branch). Always on since the
// flag clean-up.
// --mpushlaunch: postCollision's launch from last tick's c6 when contact with a
// face rising faster than 5.0 ends (State::prevC6q). Always on since the flag
// clean-up.
// --upceilv3: the ceiling ramp underside's min(vy, 0) also on a push-out, not
// only on a contact reached from below. On since 2026-09-19; always on since the flag clean-up.
// --ceillimrelease: a `slope/ceillim` contact starts the ceiling ramp's ride
// count, so the tick it ends gets the slope-exit launch (`ceil/release`), as
// `slope/upceil` already does. Always on since the flag clean-up.
// --releaseceilpress: no downhill release on a tick a ceiling ramp is pressing
// the body (GD's one isOnSlope bit stays set). On since 2026-09-19; always on
// since the flag clean-up.
// --cubeceilgrace: the cube family's ceiling-ramp seat inside 0.1 s of a mode
// switch or gravity flip (State::modeT / flipT). Always on since the flag
// clean-up.
// --ceilreleasemode: the ceiling release keeps the ride's clock across a
// floor->underside hand-over and uses the mode of the press (State::ceilMode).
// Always on since the flag clean-up.
// --ceilveto: the flight loop's slope-adjacency veto is asked per face, the
// ceiling ride reading the head's (step.hpp, the veto above fly/land).
// --ceilcont: a flight body seated on a ceiling ramp last tick keeps that ramp
// past its span, on the line extrapolated to the centre x (step.hpp, before the
// slope passes). The two are one unit: --ceilveto alone moves lv16's 19,104 to
// 19,106 and --ceilcont closes that.
// --dualcouple: a dual's gravity flip reaches the partner in both directions as
// GD's partner call does (set to the inverse, halve only on a change), in place
// of r101's same-box skip (fixup.hpp stepBoth).
// All three on since 2026-09-19 (they read only this tick's state, geometry
// and load-time tables, so an anchor loses nothing they use), and always on
// since the flag clean-up.
// --stickground: the ride's stick-to-the-line holds only a body that is
// grounded, not one whose ramp contact came in above the landing gate
// (step.hpp, stickToSlope). Always on since the flag clean-up.
// --hangland: a fresh contact of a flipped flight body with a ceiling ramp's
// gravity-facing side zeroes vy when the gravity-frame speed is within the 5.0
// landing gate (step.hpp, the flipped ceiling branch). Always on since the flag
// clean-up.
// --tpgroundvy: a cube or robot standing on an object keeps the tick's gravity
// step through a teleport (747 and 2902), as GD's solid clamp runs after it
// (step.hpp, the teleport branch). On since 2026-09-19 (it reads `grounded`,
// which --start carries); always on since the flag clean-up.
// --tpbandskip: the tick after a teleport skips the ball/spider band clamps
// (State::tpSkip, GD's player+0x560). --ballceilpos: an upright ball/spider is
// clamped under the BAND's ceiling by position, keeping a downward vy, where
// the clamp used to need vy > 0. Measured together (lv20 t=9,233..9,235) and
// meant to be used together: either alone clamps a tick early or not at all.
// ON BY DEFAULT since 2026-09-20, AS ONE UNIT. GD's y
// at t=9,235 is exactly `pmax - pHalf` = 510 - 9 -- it pins an upright ball
// under the band's ceiling BY POSITION while vy is still downward, and the tick
// right after a teleport skips that clamp. Accepted as a pair and not as two
// improvements: --ballceilpos alone was measured to make the census WORSE
// (18 -> 19, 2026-09-19 16:57). Both halves always on since the flag clean-up.
// --stickrelease: the downhill release is undone when a solid moving away from
// the player is within 5*dt of its foot -- GD's postCollision stick re-land
// runs after the release stamp (step.hpp, the release branch).
// ON BY DEFAULT since 2026-09-20, PAIRED WITH
// --recinterp. Measured on the game at lv22 t=6,680: GD seats the ball on the
// moving solid uid6067 exactly 15.000 above its top and an injected y=365 falls
// back to the same face, and the pair reproduces that to 0.003 px. The pair is
// the unit because the recorder wrote no row for uid6067 on that tick, so
// without --recinterp `dcy` is exactly 0 and this branch's gate never opens --
// the flag alone fixes the sectioned instruments and does nothing at all in a
// run from t=0. Both always on since the flag clean-up.
// SCOPE: the acceptance rests on the local t=6,680
// measurement and the corpus/census/deathref/cold gates. It does NOT rest on
// "lv22's first whole-run divergence moved to 11,342" -- that instrument does
// not load 120 of the level's 152 touch triggers past x=2,283.
// --stickseam: in a frame whose vertical axis is world x (1 and 3), a rider
// carried by a receding floor loses support for one tick when it leaves the
// solid its ride is tied to (State::groundUid). GD's stick re-land follows one
// ground object and its fallback (+0x600, pre-registered by checkCollisions)
// compares world y only, so it never fires there (measured with cfg
// `sticktrace`, lv22 t=6,122; frame 0 rescues at the same kind of step).
// Always on since the flag clean-up.
// How deep the horizontal overlap must be before GD resolves a solid it did not
// see the ship fly into, as a multiple of the player's half width. Bracketed by
// the two 0->1 transitions of the hooked collidedWithObject return on lv11:
// mini (half 9) 14.670 -> 15.970, normal (half 15) 25.930 -> 27.230, i.e.
// 1.630..1.774 and 1.729..1.815. 1.75 is the only clean value in both.
constexpr double kSolidResolveX = 1.75;
// The plain (unarmed) cube CEILING STOP is OFF. [2026-08-16]
// It was fitted to two samples: lv1's false stop (xPen 6.67, full size) and
// lv22 t=2,749 (xPen 19.78, mini) -- but t=2,749 is the 2866 FLIPHEAD event,
// which `acquireFlip` handles on its own. A third sample, measured 2026-08-16
// by sweeping GD directly (gd_run inject at lv22 t=11,385, mini), has GD NOT
// pinning at xPen 15.957 -- and not at the block's dead centre either
// (xPen 24.0, the deepest possible), so no threshold on xPen can separate it
// from t=2,749. GD's unarmed didHitHead does nothing: the player keeps moving
// into the block and dies of the overlap, which is exactly what it did.
// (The A/B switch that restored the pin, --ceilpin, is gone; `held` read it and went with it.)
// --coins: make the level's coins part of the goal. Off by default, and when it
// is off State::coins stays 0, so the dedupe key, the cap families and the goal
// test are all bit-identical to a run without the feature.
inline bool g_coinRoute = false;
// --worldgates (default off, a measurement): model the level's item gates -- the Count (1611 /
// 1811), Tap (1595) and Item Compare (3620) triggers and the pickups that feed them -- whether or
// not --coins asks for the coins. GD runs them either way; without this the model only has them
// under --coins, and then only the ones whose chain reaches a coin. Only the world: the goal and
// the coin prunes stay --coins's.
inline bool g_worldGates = false;
// Are the item gates part of the world this call? (--coins or --worldgates)
inline bool gatesModelled() { return g_coinRoute || g_worldGates; }
// --coinmargin: how far INSIDE a coin's (or a pickup's) box the model has to be
// before it counts the thing as taken. **0 by default, which is GD's own rule.**
//
// It was briefly 3. The reasoning was that a plan allowed to graze the boundary
// has a verdict that flips on a fraction of a pixel -- lv21's route passed a
// coin at |dy| = 25.5 against a bound of 25 and GD credited nothing. But the
// 0.5 px was a TRAJECTORY difference, not a plan that aimed at the edge, and
// the fix for a trajectory difference is the fixup recorder (which now runs on
// a missed coin too). A margin instead shrinks the target: the wave's coin
// bound is only 25 px, so 3 takes an eighth of the window away, and every coin
// route through a wave section is the one that needs it most.
// Kept as a knob because "how tight is too tight to plan" is a real question,
// but it is not answered by a number nobody measured.
inline double kCoinMargin = 0.0;
// --coindbg: per touch bit, (the highest count already printed for that Count
// trigger) + 1, so the `countgate:` line appears once per distinct n. Global
// and reset, not a function-local static: a second solve in the same process
// would otherwise print nothing for any count the first one had reached.
inline std::atomic<int> g_countSaid[kTouchBits] = {};
// --coinmask: the collected set an anchored (--start) search begins with.
inline int g_coinMaskSeed = 0;
// --coinskip: coins that are not a goal this run, seeded as collected on every
// call so neither the goal test nor the miss prunes ask for them.
inline int g_coinSkip = 0;
// --itembase "<item>:<n>,...": what GD's item counters already held at that
// anchor. A Count trigger compares the COUNT, and the pickups behind an anchor
// are not in the window State::items numbers, so the history arrives as a
// number and the bits only ever add to it.
inline std::vector<std::pair<int, int>> g_itemBase;
// Ship zones have an invisible ceiling. It was modelled as "ship portal cy +
// 120" from a single lv1 measurement (portal cy 255, ceiling 375.000) -- but
// that was a coincidence. lv4's ship portal sits at cy = 233, so the rule
// predicts 353, and GD's measured ceiling in that zone is again 375.00 -- the
// model clamped the ship 22 px low and it could never reach the route above.
// Two zones with different portal heights, same ceiling: it is ABSOLUTE.
// ...and 375 is not universal either. lv10 t=17836 has GD flying the ship at
// y=427 and still climbing, so a hard clamp at 375 blocked that route outright.
// maxY does not explain it (lv1 maxY=540 -> 375, lv2 maxY=390 -> 375,
// lv10 maxY=630 -> above 427), so there is no level-derived rule in hand.
// TRIED AND REVERTED: disabling the clamp entirely (relying on g_yBound alone).
// It let lv10 through to t=18,743 but **lv2 went from cleared to stuck** at
// t=10,569 and lv1 needed 5 iterations instead of 2 (2.7 min vs 0.3). The clamp
// is load-bearing where it is right, so it stays at the value that is right for
// lv1/lv2/lv4.
// What it actually is: a SEARCH BOUND, not physics. GD exceeds it (lv10 y=427)
// and no level-derived rule reproduces it -- neither maxY nor the ship zone's
// own highest surface (lv1 zone 450 -> 375, lv2 zone 390 -> 375). But lv1/lv2
// need it, because without it the DP spends its frontier on high routes that
// GD then refuses.
//
// [2026-08-31] ALL OF THE ABOVE IS HISTORY, AND THIS VALUE IS INERT. It is read
// in exactly two places (step.hpp's two band sites and cli.hpp's --start seed)
// and every one of them is gated on `g_shipCeilSet`, which only the --shipceil
// flag ever set -- and that flag is now gone, because nothing in the pipeline
// passed it. What the model actually uses is bandFor(the firing portal's cy, H),
// and that is not a fallback but the better answer: GD reports [90,390] in lv1's
// first ship zone and bandFor(239, 300) gives exactly that, while the branch
// portal at cy=405 gives [240,540] -- a route this hard 375 would have forbidden
// outright. The reverted experiment recorded above removed the clamp WITHOUT
// putting the derived band in its place, which is why it read as a regression.
inline double g_shipCeil = 375.0;
// ...and the flying modes have a hard FLOOR too, which is not the world's.
// Measured on lv12's UFO section through the gd MCP: injecting the UFO at y=60
// at t=5400 / 5500 / 5640 (x = 7010 .. 7321) has GD put it back at y=195.000
// with onGround=1 every time, and injecting at y=700 clamps it down to 465.
// So the band is [180, 480] for the player's surfaces -- exactly 300 tall --
// while there is NOT ONE object under it (checked: zero type-0 in y[140,220]
// across x[7000,7160]). A cube injected the same way at t=5300 just dies, so
// the band belongs to the flying modes, not to the level's geometry.
// The model had the ground pinned at 90 and simply fell through, which is why
// the DP planned a route GD never lets it take.
// Same status as g_shipCeil: measured per level, not derived. 0 = no floor.
// The band is per SECTION, not per level: forcing lv12's UFO band (180/465)
// onto its ship sections killed the search inside the first one immediately.
// So these two apply to the UFO portal (type 19) only, and a cube or ball
// portal clears them again.
// ...and for the UFO the band is DERIVED from the portal, not a knob at all.
// lv12 has two UFO sections at wildly different heights and one number cannot
// serve both, so the portal's own cy was checked against the measurements:
//   portal (6945, 345)   -> floor 180 measured, 345 - 165 = 180   ceiling 480
//                           measured, 345 + 135 = 480
//   portal (16665, 1095) -> floor 930 measured, 1095 - 165 = 930
// Two sections, three bounds, exact on all of them. The same arithmetic also
// reproduces the ship bound that was tuned by hand long ago: lv1's ship portal
// sits at cy = 255, and 255 + 135 - 15 = 375 -- exactly g_shipCeil's default.
// THE DERIVATION IS THE RULE; the two below were its overrides and are now
// inert, for the same reason as g_shipCeil (their flags are gone, and nothing
// had been passing them). Left in place, read where they always were, so that
// deleting them is a separate and obvious change.
constexpr double kFlyBandBelow = 165.0;
constexpr double kFlyBandAbove = 135.0;
inline double g_flyFloor = 0.0;
// 0 = no override. (It never was a "fall back to g_shipCeil": no such fallback
// exists in the code, and that comment had been wrong for as long as it stood.)
inline double g_ufoCeil = 0.0;

}  // namespace dp
