#pragma once
#include "dp/frames.hpp"

namespace dp {

// `rot` is GD's own getRotation() for that tick (grouptrace's 8th column, added
// 2026-08-13). hw/hh are HALF the AXIS-ALIGNED BOUND, so for a turned object
// they are not the shape -- see Obj::oriented and the note on the portal test.
// Recordings made before the column existed load with rot = 0, i.e. exactly the
// old behaviour.
// `env`: the row is not the object's rect but the box GD's own random numbers can put it in
// (the MOD's areaenv.hpp, --groups' ninth column). The level loader turns such rows into a
// hazard twin of the object (see envTwinUid).
struct DynSample { int t; float cx, cy, hw, hh; uint8_t on; float rot = 0.f; uint8_t env = 0; };
// [2026-08-22 r104] Interpolate linearly between the recording's rows (always;
// --no-dyninterp is gone since the flag clean-up): restores smooth motion to
// moving geometry that grouptrace's 0.05px threshold turned into a staircase.
// (defined up here, before Dynamics, because applyTriggers reads g_autoTrig;
// the touch-trigger story lives with TouchTrig below)
// --movetarget: a Move in target mode and a Stop (1616) act on the
// touch chains' closed form. Without it both are inert there: a target-mode Move
// carries ox=oy=0, and a Stop's target group holds only triggers the walk skips.
// Measured on lv22 (2026-09-21, attempt 30 of a flags-on cold): touching a red
// block spawns group 540 = Stop uid18096 (halts group 543, the two Moves that
// lower the ceiling group 265 over 4.19 s) + Move uid18093 (target mode, y only,
// 0.5 s, ease 9) that takes the ceiling to a total of -122 px. GD's rect of the
// killing spike uid18105 at t=2,599 is 96 px down; the model had it 33 px down
// and never killed, so the loop fenced the spot off with deadbands.
// ON since 2026-09-21, its own unit; always on since the
// flag clean-up.
// Per touch box (the State::trig bit): the Move triggers a Stop in its chain
// halts. Filled with the boxes (buildTouchMoveTicks); read by applyTriggers.
inline std::vector<std::vector<int>> g_touchStops;

struct TrigCtl {          // one object a trigger ends up moving
    int uid;
    float dx, dy;         // total offset once the move has finished
    // How long the move takes, from the firing tick. NOT rounded: GD's duration
    // is in seconds and the physics step is 1/240 s, so the tick count is
    // dur*240 with its fraction. Rounding it cost 4.5 px on lv20's 240 px doors
    // (uid 17399, dur 0.435002 -> 104.4 ticks, rounded to 104).
    double durTicks;
    int ease;             // GD's EasingType, raw (see gdEase)
    double erate;         // its rate (period, for the elastic family)
    // lockToPlayerX anywhere in the chain, in TICKS (0 = not locked). While it
    // lasts the object's x is the player's x, offset by wherever both were when
    // it fired -- so it has a closed form too, just one that needs the SIM's own
    // player x rather than only the level data:
    //
    //     x(t) = base + (moves) + (playerX(min(t, t0+lockTicks)) - playerX(t0))
    //
    // Measured on lv20 (uid 13276 and its group, 2,400 samples): residual
    // 0.002 px, with t0 = the crossing tick itself. No +1 here -- a lock is a
    // per-tick copy of a position, not an eased move, so it has neither the
    // easing's slow start nor the recorder's threshold lag.
    //
    // This is the one class where a RECORDING is provably wrong for anyone
    // else: it followed the recorded run's player, so replaying it against a
    // plan that reaches the trigger at a different x is off by the whole
    // difference. lv20's structure at x=24,900 is exactly this.
    double lockTicks;
    // lockToPlayerY: counted and reported, never used. No level in the suite
    // has one, and guessing at a y lock would be the same mistake made twice.
    double lockYTicks;
    // --movetarget (note at the top of this file). `mover` is the Move trigger whose
    // hop set this entry's motion, so a Stop naming that Move can freeze it.
    // `tmode` marks a target-mode Move (0 = not one; 1 both axes, 2 x only,
    // 3 y only): (tdx, tdy) is where the group ends up relative to its
    // placement, and the offset it adds is decided when it fires (destination
    // minus where the group is by then).
    int mover = 0;
    uint8_t tmode = 0;
    float tdx = 0.f, tdy = 0.f;
    // --spawnlag (g_spawnLag): how many ticks later than an immediate chain this entry's Move
    // fires, because a Spawn on the way queued it (its delay, or its place in an ordered spawn).
    // 0 without the flag.
    int spawnLag = 0;
    // --anchormove: a Rotate entry with a centre group (its position changes at the crossing, also when
    // the centre is the target group itself); a Rotate without one turns it about its own position.
    uint8_t orbit = 0;
};
// --spawnlag (default off): a touch chain whose Spawn QUEUES the next hop starts its motion later
// than the box's entry + 1. Two ways to be queued, both in the disassembly:
//   - the Spawn's own delay (key 63; SpawnTriggerGameObject::triggerObject 0x4b91f0 hands a
//     positive delay to GJEffectManager::spawnGroup instead of spawning on the spot);
//   - an ORDERED Spawn (GJBaseGameLayer::spawnObjectsInOrder 0x21ae00): the first spawnable
//     trigger of the group fires on the spot and each later one is queued (x - x_first) / 311.58 s
//     behind it.
// A queued hop is fired by GJEffectManager::updateSpawnTriggers, which runs BEFORE the tick's
// move step, so a delay d queued on the entry tick E moves things from E + ceil(d*240) where an
// immediate chain moves them from E + 1: ceil(d*240) - 1 ticks later. lv22's yellow boxes
// (uid 2195/2303/2459/18179) each reach Move uid18144 (x=3,257, the +30 of the spike row) through
// an ordered Spawn whose group also holds a spawnable Pickup at x=3,255: d = 2/311.58 s, one tick.
// Measured on worker 98 (kit 1006_171918_lv22 it4): the spike uid18123 rises at 2763 after the
// entry at 2760, where the model raised it at 2762; y/x injections at 2759 move nothing until the
// player is pushed out of the box. The x-sorted reading of "first" is what that measurement needs
// (uid order would put the Move first and fire it on the spot); it is not read from the binary.
// Read only by the formula path's per-box parts: the recording path already carries each object's
// own delay behind the box (--touchretimebox), and the first-iteration fallback keeps its measured
// 5 ticks.
// On by default since 2026-10-07; --no-spawnlag is the off arm. How long a queued Spawn waits and which
// box of an ordered group goes first are measured on one level only (not disassembled).
inline bool g_spawnLag = true;
// --anchormove (off): see level_loader.hpp, the pass before the anchor's own move.
// On by default since 2026-10-07; --no-anchormove is the off arm.
inline bool g_anchorMove = true;
// uid -> x of every object the dump marks spawn-triggered (objrects' `spawn` column), triggers
// with no row in triggers.txt included -- lv22's Pickup uid18173 is the first of its ordered
// group and has none. Filled by the CLI under --spawnlag.
inline std::unordered_map<int, double> g_spawnableX;
inline int spawnLagTicks(double seconds) {
    if (seconds <= 0.0) return 0;
    return std::max(0, (int)std::ceil(seconds * 240.0 - 1e-9) - 1);
}
// Touch boxes a re-anchor caught with their move ALREADY RUNNING (see the
// `init.trig` block at the --start handling). The "has it moved past half its
// offset" test there can only answer "open" or "shut"; an anchor dropped in the
// middle of the slide is neither, and answering "shut" puts the door back where
// it started for the whole tail.
//
// Measured on lv22 (2026-08-13): the wall that forms the FLOOR of the first
// rotated section (uids 1212/1213/1214/1221, one column of id 1 blocks at
// cx=2115) slides cy 345 -> 225 between t=1,697 and t=1,814. Every anchor the
// driver takes in that window -- t=1,730 is the one the loop settles on -- has
// it 20.4 px into its 120, i.e. 17%, so the bit stayed clear, the tail ran
// against the wall at REST, and the spider's tap in the rotated frame found no
// surface for 700 px and teleported to world x=1,459.5 where GD lands at
// 2,143.5. From the head the same plan is exact, which is what says the
// geometry was never the problem.
//
// For these boxes the recording IS the timeline (the live overlay comes from
// this plan's own replays), so the object is re-dated to its own first-motion
// row instead of to the state's trigT -- shift 0, phase preserved.
inline TouchMask g_recPhase{};
// GD's easing, which is cocos2d-x's CCEase* family reached through
// EffectGameObject::m_easingType / m_easingRate. In = t^rate and Out = t^(1/rate)
// are cocos's own convention, not a guess.
//
// Measured against grouptrace on the lv19 and lv20 bootstrap runs (655 objects
// over four families -- None, EaseInOut, ElasticOut, BounceOut): with this
// curve, the exact duration and the fire tick below, the closed form
//     pos(t) = base + offset * gdEase(type, rate, (t - fireT) / durTicks)
// reproduces every recorded sample to within **0.004 px**. See
// py/trigger_curve_fit.py, which is the harness that says so.
//
// The families with no such measurement yet (Exp / Sine / Back, and the In/Out
// halves of Elastic and Bounce) are cocos's formulas placed here unverified;
// they are still far closer than the 0.5-0.5cos(pi*u) stand-in they replace.
inline double gdEase(int kind, double rate, double u) {
    u = u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u);
    const double p = (rate > 0.0) ? rate : 2.0;
    const double kPi = 3.14159265358979;
    auto bounceOut = [](double x) {
        if (x < 1.0 / 2.75) return 7.5625 * x * x;
        if (x < 2.0 / 2.75) { x -= 1.5 / 2.75;   return 7.5625 * x * x + 0.75; }
        if (x < 2.5 / 2.75) { x -= 2.25 / 2.75;  return 7.5625 * x * x + 0.9375; }
        x -= 2.625 / 2.75;
        return 7.5625 * x * x + 0.984375;
    };
    auto elasticOut = [&](double x, double period) {
        if (x <= 0.0 || x >= 1.0) return x;
        return std::pow(2.0, -10.0 * x)
             * std::sin((x - period / 4.0) * kPi * 2.0 / period) + 1.0;
    };
    auto elasticIn = [&](double x, double period) {
        if (x <= 0.0 || x >= 1.0) return x;
        const double y = x - 1.0;
        return -std::pow(2.0, 10.0 * y)
             * std::sin((y - period / 4.0) * kPi * 2.0 / period);
    };
    switch (kind) {
        case 0: return u;                                     // None (linear)
        case 1: {                                             // EaseInOut
            const double t = 2.0 * u;
            return t < 1.0 ? 0.5 * std::pow(t, p)
                           : 1.0 - 0.5 * std::pow(2.0 - t, p);
        }
        case 2: return std::pow(u, p);                        // EaseIn
        case 3: return std::pow(u, 1.0 / p);                  // EaseOut
        case 4: return u < 0.5 ? elasticIn(2.0 * u, p) * 0.5
                               : elasticOut(2.0 * u - 1.0, p) * 0.5 + 0.5;
        case 5: return elasticIn(u, p);
        case 6: return elasticOut(u, p);
        case 7: return u < 0.5 ? (1.0 - bounceOut(1.0 - 2.0 * u)) * 0.5
                               : bounceOut(2.0 * u - 1.0) * 0.5 + 0.5;
        case 8: return 1.0 - bounceOut(1.0 - u);
        case 9: return bounceOut(u);
        case 10: return u < 0.5
                     ? 0.5 * std::pow(2.0, 10.0 * (2.0 * u - 1.0))
                     : 0.5 * (2.0 - std::pow(2.0, -10.0 * (2.0 * u - 1.0)));
        // [2026-09-01] The two Exponential ENDPOINTS, read out of the binary
        // (getEasedValue, the 0.001f at rodata 3A83126F is real). Both were
        // wrong, in opposite directions, and both are permanent: an object
        // whose move ends on one of these curves stops in the wrong place and
        // stays there, so the hitbox is off for the rest of the level.
        //
        // ExpIn: GD subtracts 0.001 and therefore stops at 0.999 -- one tenth
        // of a percent SHORT of the offset it was given, forever. The model
        // reached 1.0, i.e. it overshot. cocos has no u==0 guard either (it
        // returns -0.0000234 there), so the guard goes too: match the binary,
        // not the tidier curve. (Wrong for 2.2081, see below.)
        // --expinzero (on by default since 2026-10): ...but the binary has the
        // guard. PR #2 read 2.2081's GameToolbox::getEasedValue (0x68b70) as
        // returning 0 at u == 0 before the exponential, and so does the decompile
        // (case 0xb: 0.0 unless u != 0.0, then powf(2, (u - 1) * 10) - 0.001). The
        // difference is 0.0000234 of the move's offset, on the move's first tick only.
        case 11: return (g_expInZero && u == 0.0) ? 0.0 : std::pow(2.0, 10.0 * (u - 1.0)) - 0.001;
        // ExpOut: GD special-cases t==1 to land exactly. Without it the curve
        // stops at 1 - 2^-10 = 0.9990234, 0.0977% short. Six moves in lv22 use
        // this, the largest 51 px, so the model was parking them ~0.05 px low.
        case 12: return u >= 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * u);
        case 13: return -0.5 * (std::cos(kPi * u) - 1.0);
        case 14: return 1.0 - std::cos(u * kPi * 0.5);
        case 15: return std::sin(u * kPi * 0.5);
        case 16: {
            const double o = 1.70158 * 1.525;
            double x = 2.0 * u;
            if (x < 1.0) return 0.5 * (x * x * ((o + 1.0) * x - o));
            x -= 2.0;
            return 0.5 * (x * x * ((o + 1.0) * x + o) + 2.0);
        }
        case 17: { const double o = 1.70158; return u * u * ((o + 1.0) * u - o); }
        case 18: {
            const double o = 1.70158, x = u - 1.0;
            return x * x * ((o + 1.0) * x + o) + 1.0;
        }
        default: return u;
    }
}
// --instantlag (default on since 2026-10): a Move of duration 0 fired by an x crossing lands its whole offset on the
// tick AFTER its effect tick, as an eased Move takes its first step there (gdEase(0) = 0 on the effect
// tick); the closed form put it on the effect tick itself. Measured on a custom level with grouptrace:
// Moves 7174 / 7175 (duration 0, x=17,700, dy -300 / +300) are crossed between rows 9,973 and 9,974,
// the model's effect tick is 9,975, and GD's recorded block (group 99) jumps 1275 -> 1575 on row 9,976
// -- where the cube standing on group 96 also starts to fall. The box-started Moves already count from
// the tick after (te - (f + 1)), so only the x-crossing paths change.
inline bool g_instantLag = true;
inline double instantE(int te, int f) { return (g_instantLag && te <= f) ? 0.0 : 1.0; }
// How many ticks after the move really starts does grouptrace FIRST write a row
// for it? The recorder drops changes under 0.05 px (grouptrace.hpp kEps), and an
// eased move spends its first ticks in the thousandths, so the recording's
// apparent first motion is 1-2 ticks late -- 1 for a linear move, 2 for
// EaseInOut. That lag is not an unknown: with the offset, the duration and the
// curve in hand it is computable, which is what this does. Everything
// downstream then works in TRUE first-motion ticks and the artefact cancels.
// See the ZoomTrig block for the measurements. Progress is mapped through x
// because the state cannot carry the crossing tick.
// --zoomchain: rows since zoom k's crossing row, for the row at (x, t). The first state to see
// the zoom crossed fixes the crossing for the invocation (modifiers.hpp).
inline long long zoomRowsSince(size_t k, double x, double dx, long long t) {
    const long long est = (long long)std::floor((x - g_zoomTrigs[k].cx) / dx + 1e-6);
    if (t < 0 || k >= g_zoomCrossN) return est;
    long long tc = g_zoomCrossT[k].load(std::memory_order_relaxed);
    if (tc == kZoomCrossUnknown) {
        long long want = t - est;
        // the earliest wins if two threads see it on different layers
        while (!g_zoomCrossT[k].compare_exchange_weak(tc, want, std::memory_order_relaxed)) {
            if (tc != kZoomCrossUnknown && tc <= want) break;
        }
        tc = g_zoomCrossT[k].load(std::memory_order_relaxed);
    }
    return t - tc;
}
// --zoomchain: zoom z's value d rows after its crossing row, starting from `from`. A tween starts
// on the row before (d + 1 ticks in); an instant zoom lands on the row itself.
inline double zoomValueAfter(const ZoomTrig& z, double from, long long d) {
    if (z.durTicks <= 0.0) return d >= 0 ? z.target : from;
    const double e = (double)(d + 1);
    if (e <= 0.0) return from;
    if (e >= z.durTicks) return z.target;
    return from + (z.target - from) * gdEase(z.ease, z.rate, e / z.durTicks);
}
inline double camScaleChain(double x, double dx, long long t) {
    size_t m = 0;   // the zooms crossed by x
    while (m < g_zoomTrigs.size() && g_zoomTrigs[m].cx <= x) ++m;
    if (m == 0) return 1.0;
    // rows since each crossing, newest first, back to the first zoom that had settled before the
    // next one started (everything earlier is then irrelevant)
    thread_local std::vector<long long> n;
    n.clear();
    size_t k = m;
    while (k > 0) {
        --k;
        n.push_back(zoomRowsSince(k, x, dx, t));
        if (n.size() >= 2) {
            const ZoomTrig& z = g_zoomTrigs[k];
            const long long dStart = n[n.size() - 1] - n[n.size() - 2] - 1;   // rows into z when the next started
            if (z.durTicks <= 0.0 ? dStart >= 0 : (double)(dStart + 1) >= z.durTicks) break;
        }
    }
    // forward from zoom k: its start value is irrelevant only if it settled, so a chain that ran
    // back to the first zoom starts from 1
    double from = 1.0;
    for (size_t j = 0; j < n.size(); ++j) {
        const size_t idx = k + j;
        const long long nj = n[n.size() - 1 - j];
        if (j + 1 < n.size()) {
            const long long nNext = n[n.size() - 2 - j];
            from = zoomValueAfter(g_zoomTrigs[idx], from, nj - nNext - 1);
        } else {
            from = zoomValueAfter(g_zoomTrigs[idx], from, nj);
        }
    }
    return from;
}
inline double camScaleAt(double x, double dx, long long t) {
    double cur = 1.0;
    if (dx <= 0.0) dx = 1.0;
    if (g_zoomChain) return camScaleChain(x, dx, t);
    for (const auto& z : g_zoomTrigs) {
        if (z.cx > x) break;
        if (z.durTicks <= 0.0) { cur = z.target; continue; }
        const double u = (x - z.cx) / (dx * z.durTicks);
        cur = (u >= 1.0) ? z.target
                         : cur + (z.target - cur) * gdEase(z.ease, z.rate, u);
    }
    return cur;
}

inline int recordLag(double dx, double dy, double durTicks, int ease,
                     double erate) {
    // Nothing moves -- a toggle-only group. A boolean flip has no threshold to
    // climb, so its first row IS its first tick and there is no lag to remove.
    if (durTicks <= 0.0 || (std::fabs(dx) < 0.5 && std::fabs(dy) < 0.5))
        return 0;
    for (int n = 1; n <= 240; ++n) {
        const double e = gdEase(ease, erate, (double)n / durTicks);
        if (std::fabs(dx * e) >= 0.05 || std::fabs(dy * e) >= 0.05) return n;
    }
    return 1;
}
// --reclagup (default off): the level loader inverts the first row's displacement for the closest
// n <= lag, on the ground that a row cannot come later than the count above. It can, by one tick:
// the recorder compares FLOAT positions (grouptrace kEps = 0.05f), and where dy*e(n) sits on 0.05
// itself the double test above counts it while the float difference falls short. Custom level, Move
// uid 9344 (EaseInOut rate 2, 1,680 ticks, -90 px) on uid 4498 at y=705: 90*e(28) is
// 0.050000000000000003 in double but |float(705 - 90 e) - 705| = 0.04998779, so the first row is
// k=29 (0.054 recorded) where the count says 28, and the closed form ran one tick behind GD's rows
// (the start fitted to all of them is 14,944, worst 0.0007 px; 14,945 is out by 0.079). The flipped
// mini cube landing under that block sat 0.065 px high, inside the ramp beside it, took the ramp
// and launched off the block's end where GD fell. With the flag the inversion also takes lag+1,
// only when it is clearly the closer one: its error under half of lag's, and lag's over the rows'
// own 3-decimal rounding.
// On by default since 2026-10-07; --no-reclagup is the off arm.
inline bool g_recLagUp = true;
// --reclagabs (default on since 2026-10): the inversions of the first row's displacement (the level loader and
// recLagUpN) compare the curve's DISTANCE |full * e(n)| with it, as the recorded `moved` is a distance
// (a hypot). Back (16, 17) and Elastic (4, 5) start by moving the other way, e(n) < 0, and against the
// signed curve the closest n is always 1, the tick where e is nearest 0. A custom level, block uid 20987
// moved 60 px up by a Move with Back In Out over 1,282 ticks: GD's rows fit the curve from the model's own
// fire tick 1,795 to 0.0008 px, the first row is 1,812 (0.053 px back, 17 ticks, which the threshold count
// gives), and the inversion chose 1, so the model read the recording 16 ticks ahead -- the block 3.7 px
// high at the worst, and a ship GD stops under it at t=2,604 flew on. The rotation's inversion already
// takes the magnitude.
inline bool g_recLagAbs = true;
inline double recLagCurve(double full, double e) { return g_recLagAbs ? std::fabs(full * e) : full * e; }
// --offafter (default on since 2026-10): an object whose recorded row turns it OFF on tick t is still there for tick t's collision
// pass. The recorder writes a row after the tick's trigger pass, where an x-crossing Toggle switches its group, and that
// pass comes after the collisions -- the same reason a recorded ON lands the tick after its row (the PORT / SPEED rule
// below). A custom level t=15,447: blocks 12500 / 12501 (group 217, carried along with the player) read off on row 15,447;
// GD's cube stands on 12501's top at that row's position (736.074 = 706.074 + 30) with vy 0 and falls from 15,448, where
// the model let it fall a tick early. The rows' positions are the tick's own (the moves come before the collisions).
inline bool g_offAfter = true;
// The on/off to use at tick `tt` for row `k` of a recording (the row in force at tt).
inline uint8_t recOnAt(const std::vector<DynSample>& sm, size_t k, int tt) {
    if (g_offAfter && sm[k].on == 0 && sm[k].t == tt && k > 0 && sm[k - 1].on) return 1;
    return sm[k].on;
}
inline int recLagUpN(int bestN, int lag, double full, double moved, double dur, int ease,
                     double erate) {
    if (!g_recLagUp || bestN != lag || dur <= 0.0) return bestN;
    const double eL = std::fabs(recLagCurve(full, gdEase(ease, erate, (double)lag / dur)) - moved);
    const double eU = std::fabs(recLagCurve(full, gdEase(ease, erate, (double)(lag + 1) / dur)) - moved);
    return (eL > 0.001 && eU < 0.5 * eL) ? lag + 1 : bestN;
}
// --rotlag (default on since 2026-10): an object whose anchor is a ROTATE gets the recorder's threshold lag of
// the rotation, where recordLag (translation only) gives it a toggle's lag, 0. A Rotate is not a
// toggle. GD builds both commands as a GroupCommandObject2 (GJEffectManager::createMoveCommand and
// createRotateCommand), steps both in GJEffectManager::prepareMoveActions, and the first step does
// not advance the elapsed time (GroupCommandObject2::step clears the +0x1b0 flag that reset set,
// instead), so a rotation, like a move, is still at rest on its fire tick and changes from the next
// one. The recorder writes a turned object's row once its rot has moved 0.05 degrees (grouptrace's
// kEps covers rot too), so the first row is fireT plus the first n at which the angle clears that.
// With lag 0 an object a Rotate turns read its recording one tick ahead whenever the Rotate was
// crossed inside the run (fireT = crossing + 1, first row = crossing + 2). Behind the start, fireT
// is taken from the recording itself minus the same lag, so the object's own shift was 0 there
// either way; the flag moves that fireT one tick earlier, to the true effect tick.
// Measured on the recorder replays of two custom levels (the plan GD flew, so the recording is the
// run's own): all 48 objects turned by a Rotate crossed inside the run read shift -1, the 30 behind
// the start shift 0. On the other level a ship died a tick later than GD under --recendextend: the
// saw it extended past the recording's end was already a tick ahead. Only objects whose facing
// turns: one that keeps it (lock rotation) leaves its rows on the orbit's displacement instead,
// which needs the radius, and keeps lag 0.
inline bool g_rotLag = true;
inline int g_rotLagN = 0;   // objects the flag gave a lag (the CLI prints it)
inline int rotRecordLag(double totalDeg, double durTicks, int ease, double erate) {
    if (durTicks <= 0.0 || std::fabs(totalDeg) < 0.05) return 0;
    for (int n = 1; n <= 240; ++n)
        if (std::fabs(totalDeg * gdEase(ease, erate, (double)n / durTicks)) >= 0.05) return n;
    return 1;
}
// Autonomous triggers: touch=0 / spawn=0 move rows. GD activates one when the
// PLAYER's x crosses the trigger's own x (EffectGameObject::spawnXPosition()
// returns getPosition().x for exactly this class), and the controlled group's
// first motion lands 2 ticks after the crossing -- measured live on lv19
// uid 15242 (crossing x=30,375 -> first motion t = crossing + 2; reaching it
// on SCREEN does not fire it).
//
// Unlike a touch trigger this is not a per-state fact: every worldline of a
// solve crosses a given x at (nearly) the same tick, so the firing tick is a
// LEVEL property. It is resolved once -- from the frontier's leading x during
// the search, from the single trajectory in --replay, or from the recording
// itself for triggers behind a re-anchor -- and stored in fireT. -1 = not
// crossed yet; controlled objects sit at their FIRST sample until then.
//
// Why this exists when grouptrace already records the motion: the recording
// plays on the RECORDED run's timeline. A bootstrap corpse crossed x=30,375
// at t=23,397 where the real run crosses at t=21,690 -- 1,707 ticks of wall
// phase error, and the DP planned around a wall that was not there (a whole
// session went to diagnosing it). Deriving the fire tick from the solve's own
// x removes the dependence on what the recording happened to do; the recording
// only contributes the SHAPE of the motion, re-timed to this run's crossing.
struct AutoTrig {
    int uid = 0;              // reporting only
    double cx = 0;            // player x that fires it
    std::vector<TrigCtl> ctl;
    // Resolved TRUE effect tick: the last tick the object is still at rest is
    // fireT-1, and gdEase((t-fireT)/dur) is its position from fireT on.
    int fireT = -1;
    // The player's x when it fired, for the lockToPlayer term (see TrigCtl).
    // Resolved with fireT; behind a re-anchor only `cx` itself is known, which
    // is the crossing x to within one tick of travel.
    double fireX = 0;
    // --autoown: the most ticks any state of the frontier that fired it could cross it after the leader, each at
    // its own speed then (model.hpp fireAutoTriggers). 0 for a trigger fired before the search or by a single
    // state: every group of the call shares that crossing.
    int fireSpreadT = 0;
    // Where the player was on the last tick this trigger's lock was still open.
    // Trails px while the window runs so the object can hold that x afterwards.
    double lockLastX = 0;
    // Ticks between the crossing and the move actually starting.
    // MOVE (901) = 1. Measured 2026-08-09 over the lv19 and lv20 bootstrap runs
    // (nodeath=1 grouptrace=1, so the dump and the recording share one
    // timeline): fitting the closed form to all 655 single-owner objects puts
    // the origin at crossing+1 for EVERY one of them, min = med = max, with a
    // worst-case residual of 0.004 px. py/trigger_curve_fit.py reproduces it.
    //
    // It used to say 2, from lv19 uid 15242 (crossing t=21,688 against first
    // RECORDED motion t=21,690). That measurement was right and its reading was
    // wrong: grouptrace does not write sub-0.05 px changes, and an EaseInOut
    // move is under that for its first two ticks (see recordLag). The extra
    // tick was the recorder's threshold, not GD's.
    // TOGGLE (1049) = 0: a boolean has no such threshold, so its measurement
    // (lv20, the three groups whose toggle is the first thing to touch them --
    // uid 4528 x=6,465, uid 10316 x=17,715, the trio at x=22,455 -- all flip on
    // the very tick the player crosses) stands as read. The later toggles in
    // that level show a large negative offset only because their group had
    // already been flipped by an earlier trigger; they are not independent
    // measurements and are not evidence of anything.
    int delay = 1;
    // --anchormove: the root is a Toggle, whose on/off the recording reads at the crossing.
    uint8_t toggleRoot = 0;
    // The objects a Toggle in this chain switches, which way, and how many ticks after fireT
    // (the chain's spawn delays) -- the root's own switch when the root is a Toggle, and every
    // spawned one below it. Read by the on/off events (autoOnEvents); nothing else.
    struct Tog {
        int uid;
        uint8_t on;
        double delay;
        int grp = 0;   // the group the Toggle switches (read by --tapdisarm, Dynamics::disarmEv)
    };
    std::vector<Tog> tog;
    // --autostop: this entry is an x-crossing STOP (id 1616), and these are the Moves in its
    // target group that it halts (empty for every other entry). Its delay is 0: fireT is the
    // crossing tick itself, the last tick a halted Move still advances.
    std::vector<int> stopMoves;
    // A ROTATE root (id 1346): its whole angle (times360 * 360 + degrees), its duration in ticks,
    // its easing and whether the objects it turns keep their facing. rotAngleOk is false when the
    // dump predates the times360 column (the angle is then not known). Read by --rotlag.
    double rotTotal = 0.0, rotDurT = 0.0, rotErate = 2.0;
    int rotEase = 0;
    uint8_t rotLock = 0, rotAngleOk = 0;
    // --boxarm: a Move that is switched off at its crossing unless a box has switched its group back on first
    // (triggers.hpp loadAutoTriggers). Its fire tick is read through autoFireT. Only ever set under the flag.
    uint8_t offInit = 0;
};
inline std::vector<AutoTrig> g_autoTrig;
// --autostop (on by default since 2026-10-06; --no-autostop is the off arm): an x-crossing Stop
// halts the Moves in its target group where the player crosses it. The model read a Stop only
// inside a touch chain (g_touchStops), so an autonomous one reached nothing and its Moves ran to
// the end. Custom level, x=17,655 (Stop
// uid 24193, target group 88): group 88 holds four Moves, one of them the lockToPlayerX Move
// uid 23832 (x=17,415, 1 s); GD's recording of the locked block uid 23983 follows the player
// up to t=10,014 -- the tick the player crosses x=17,655 -- and holds from 10,015, while the
// model kept it under the player for the lock's whole 240 ticks.
inline bool g_autoStop = true;
// --autospread (print only, model.hpp fireAutoTriggers): the triggers fired by the frontier's leader
// while states were still behind them, open until none is.
inline bool g_autoSpread = false;
struct AutoSpreadOpen { size_t i; long long leadT; size_t behind, of; };
inline std::vector<AutoSpreadOpen> g_autoSpreadOpen;
// --autoown (default on since 2026-10): the layered search fires an autonomous trigger on the frontier leader's
// crossing (model.hpp fireAutoTriggers) and places every group by that tick. With the flag a group is
// placed by its OWN crossing while the trigger's movers can still be running: the tick its x passed
// the trigger's, t-1 - floor((x - cx) / dx) at its present speed (+ the trigger's delay), never
// earlier than the leader's, and unfired while it has not reached it. The leader's values are put
// back when the group is done (AutoOwnGuard). A single-speed frontier gets the leader's tick back
// exactly. A custom level: part of the frontier took a speed portal and crossed x 12,795 fourteen
// ticks before the rest; the slow states met a descending spike row 7.9 px low and flew past where
// their own replay died at GD's tick.
inline bool g_autoOwn = true;
// Per g_autoTrig entry: how many ticks after its fire its movers can still be moving.
inline std::vector<double> g_autoRun;
struct AutoOwnGuard {
    std::vector<std::pair<size_t, int>> saved;
    AutoOwnGuard() = default;
    AutoOwnGuard(const AutoOwnGuard&) = delete;
    AutoOwnGuard& operator=(const AutoOwnGuard&) = delete;
    ~AutoOwnGuard() {
        for (const auto& p : saved) g_autoTrig[p.first].fireT = p.second;
    }
};
// The own crossing, worked back at the group's present speed, is capped at the trigger's fireSpreadT past the
// leader's: no state of the frontier that fired it was further behind than that. The present speed is the speed
// since the crossing only if it has not changed: a custom level's group sped up from 1.05 to 1.30 px a tick after
// its crossings of triggers at x 1,065..2,355 (fired before the search, a spread of 0), the uncapped reading put
// them up to 305 ticks late, its movers sat where they had been, and that half of the frontier died where the
// leader's ticks kept it alive.
inline void autoOwnApply(AutoOwnGuard& G, double gx, double gdx, long long t) {
    if (!(gdx > 0.0)) return;
    if (g_autoRun.size() != g_autoTrig.size()) {
        g_autoRun.assign(g_autoTrig.size(), 0.0);
        for (size_t i = 0; i < g_autoTrig.size(); ++i)
            for (const TrigCtl& c : g_autoTrig[i].ctl)
                g_autoRun[i] = std::max(g_autoRun[i],
                                        c.durTicks + (double)c.spawnLag + c.lockTicks);
    }
    for (size_t i = 0; i < g_autoTrig.size(); ++i) {
        AutoTrig& A = g_autoTrig[i];
        if (A.fireT < 0) continue;
        int own = -1;
        if (gx >= A.cx) {
            const long long k = (long long)std::floor((gx - A.cx) / gdx);
            own = std::min((int)(t - 1 - k) + A.delay, A.fireT + A.fireSpreadT);
            if (own <= A.fireT) continue;                          // not behind the leader
            if ((double)(t - own) > g_autoRun[i] + 2.0) continue;  // its movers are done for it too
        }
        if (g_trigDbgT >= 0 && t == g_trigDbgT)   // --trigdbg <tick>: what this group's placement reads
            std::printf("autoown t=%lld gx=%.1f gdx=%.3f trig=%zu cx=%.1f fireT %d -> %d spread=%d run=%.0f\n",
                        t, gx, gdx, i, A.cx, A.fireT, own, A.fireSpreadT, g_autoRun[i]);
        G.saved.push_back({i, A.fireT});
        A.fireT = own;
    }
}
// --rotnospawn (default on since 2026-10): an autonomous Rotate's walk does not follow the triggers in its target
// group into their targets. A Rotate turns those triggers as objects and fires none of them -- the rule
// the Move walk already has (--movenospawn, triggers.hpp). A custom level: the Rotate at x=1 spins a
// press orb (id 1594) in place about its own decoration, and the orb's target group holds a row of
// moving spikes; the walk took the spikes for orbiting objects, anchored them on the Rotate's crossing
// (t=3) against a recording that starts at t=3,748, read the recording 3,745 ticks ahead and placed
// them 60 px above GD's, so a ship GD kills on a spike's peak flew past it in the model.
inline bool g_rotNoSpawn = true;
// ...and, per g_autoTrig entry, the Stop entries that name its root (built with g_autoTrig,
// cli.hpp). Empty under --no-autostop.
inline std::vector<std::vector<int>> g_autoStopBy;
// --dynobbrot (on by default since 2026-10-06; --no-dynobbrot is the off arm): a moving hazard that
// a trigger also turns takes its turned box (the --obb corners, bhw/bhh along bc/bs) at the
// recording's angle for the tick, not at the angle it was exported with. It reads the recording's
// angle, so a closed-form hazard with no recorded row is not covered. The box was only ever loaded once, so a hazard whose rotation changed kept
// testing the player against its placed orientation. Custom level, spike uid 23082 (id 8, scaled
// to a 24 x 311 box, placed at rot 69, turned to 82.4 by t=16,121 while its group carries it):
// GD kills the wave at t=16,121, 4.84 px past the long face at the recorded angle (the wave's
// half 5 turned by 33.8 deg reaches 7.0); at the export angle the same face is 27.69 px away,
// and the model died 8 ticks later, on the static box.
inline bool g_dynObbRot = true;
// --rotobb (default on since 2026-10): a hazard a Rotate turns gets its own box as an oriented box, and --dynobbrot turns
// it with the angle, when the --obb export does not hold it. The export holds the objects turned at load (any
// angle that is not a right one); one that a Rotate turns later was tested as its bounding box only. GD latches
// an object a rotate action turns (GameObject+0x2e8, set by the rotation step) and from then on tests it
// rect-then-oriented-box like any turned hazard. Unturned, the box is the rect, so nothing changes until the turn.
// A custom level: a 4 x 7.6 spike orbiting under three Rotates, at -47.2 degrees when a wave passed it. Placed
// where GD had it to 0.002 px (--rottail), its 8.29 x 8.10 bounding box killed the wave a tick before GD did;
// its turned box kills on GD's tick, at GD's x.
inline bool g_rotObb = true;
// --rotobblatch (default on since 2026-10; with --rotobb): ...and that box is the hazard's only from the tick a rotation has turned
// it, as GD's latch is: GameObject+0x2e8 is set by the rotation step (or at load for an object placed at an angle that
// is not a right one), and the collision pass takes a hazard's oriented box only once it is set. Before that GD tests
// the plain rects. --rotobb's "unturned, the box is the rect" holds for every mode but the wave, whose second stage
// turns the PLAYER's box by its sprite angle: against a still-unturned spike that test misses where the rects meet.
// A custom level: a mini wave (sprite -18.26) passes a 6 x 12 spike that a group Move carries and three Rotates turn
// later (rot 0 in every recorded row to t=10,353). GD kills it at the rects (|dx| 5.70 against 6, |dy| 8.68 against 9);
// the model's turned-player test missed, three plans the search took died in GD there (t=10,169 / 10,185 / 10,274),
// and the model died on GD's tick in all three with --rotobb off.
inline bool g_rotObbLatch = true;
// The tick a halted Move last advances (the earliest resolved Stop naming entry `k`), or
// 1 << 30 when nothing stops it.
inline int autoStopT(int k) {
    int st = 1 << 30;
    if (k < 0 || (size_t)k >= g_autoStopBy.size()) return st;
    for (const int s : g_autoStopBy[(size_t)k]) {
        const int f = g_autoTrig[(size_t)s].fireT;
        if (f >= 0) st = std::min(st, f);
    }
    return st;
}
// --tapdisarm (g_tapDisarm): per g_autoTrig entry, the boxes whose chains switch its trigger (TouchTrig::trigTog,
// tied by the CLI): the box's bit, which way, and how many ticks after the box fires. Empty without the flag.
struct AutoDisarm {
    int box;
    uint8_t on;
    double delay;
};
inline std::vector<std::vector<AutoDisarm>> g_autoDisarm;
// Is entry k's trigger switched off where the player crosses it (fireT less its delay), in a state with this
// mask and these fire ticks? The last switch to land by the crossing decides; none = on, as placed. A switch
// landing on the crossing's own tick counts (not measured). GD counts the switches (toggleGroup's +0x4c0, off
// while negative), so two offs need two ons; the last one is enough for the chains seen so far.
// --boxarm: an entry marked offInit starts OFF instead, so with no box tied to it, or in a state that has entered
// none (fireB null), it is switched off -- a box outside the touch window (the CLI ties only the first kTouchBits)
// must not let it fire.
inline bool autoDisarmed(int k, TouchMask mask, const uint16_t* fireB) {
    const bool offInit = g_boxArm && k >= 0 && (size_t)k < g_autoTrig.size() && g_autoTrig[(size_t)k].offInit;
    if (!fireB || k < 0 || (size_t)k >= g_autoDisarm.size() || g_autoDisarm[(size_t)k].empty()) return offInit;
    const AutoTrig& A = g_autoTrig[(size_t)k];
    if (A.fireT < 0) return false;
    const double cross = (double)(A.fireT - A.delay);
    double best = -1e300;
    uint8_t on = offInit ? 0 : 1;
    for (const AutoDisarm& d : g_autoDisarm[(size_t)k]) {
        if (d.box < 0 || d.box >= kTouchBits || !(mask & touchBit(d.box))) continue;
        const double te = (double)fireB[d.box] + d.delay;
        if (te > cross || te < best) continue;
        best = te;
        on = d.on;
    }
    return on == 0;
}
// --boxarm: entry k's fire tick as THIS state sees it -- its fireT, or -1 for an offInit entry no box of the state
// has switched back on by its crossing. Every placement that clocks a Move off fireT reads it through here; an
// entry that is not offInit (every entry without the flag) reads fireT itself.
inline int autoFireT(int k, TouchMask mask, const uint16_t* fireB) {
    const AutoTrig& A = g_autoTrig[(size_t)k];
    if (A.fireT < 0 || !A.offInit || !g_boxArm) return A.fireT;
    return autoDisarmed(k, mask, fireB) ? -1 : A.fireT;
}

// ---- an object's on/off as a sequence of switches ----------
// One switch of an object's on-state: a Toggle (or an activate) that some trigger's chain
// reaches, taking effect `delay` ticks after that trigger fires. A box (`box` >= 0, the
// State::trig bit) fires at the state's own fireB[box], and only in a state that entered it. An
// x-crossing trigger (`autoIdx` >= 0, its g_autoTrig index) fires at that trigger's fireT, a tick
// the whole level shares and that the search resolves as it goes -- so it is read when the event
// is resolved, not copied in. Otherwise `at` is the fire tick (-1 = never).
//
// A touch mask alone cannot say this. It is monotone -- a bit that is set stays set -- so it has
// no way to express a switch-off later in the same chain, two chains fighting over one object,
// or a delay before either takes effect. Nothing reads these yet.
struct OnEvent {
    int box = -1;
    int at = -1;
    uint8_t on = 1;       // 1 = the object comes on, 0 = it goes off
    double delay = 0.0;   // ticks after the source fires (the chain's spawn delays)
    int autoIdx = -1;
    int grp = -1;         // the group the switch acts on (--tapdisarm's resolveOnGroups; -1 = not known)
};

// --offafter: an x-crossing Toggle's OFF switches the object out of the tick AFTER its trigger pass, as its recorded row
// does (recOnAt)
inline double offAfterLag(const OnEvent& e) {
    return (g_offAfter && e.autoIdx >= 0 && e.on == 0) ? 1.0 : 0.0;
}
// The object's on-state at tick t: `init` (its on/off right after the game's reset) until
// something switches it, then the event with the LATEST effective tick (fire + delay) among those
// that have taken effect by t. On a tie the one later in the list wins, which is chain order --
// NOT known to be the game's order, so a tie between two sources is a place to
// measure, not to trust.
inline bool resolveOn(bool init, const OnEvent* ev, size_t n, TouchMask mask,
                      const uint16_t* fireB, int t) {
    bool on = init;
    double best = -1e300;
    for (size_t k = 0; k < n; ++k) {
        const OnEvent& e = ev[k];
        double f;
        if (e.box >= 0) {
            if (e.box >= kTouchBits || !fireB || !(mask & touchBit(e.box))) continue;
            f = fireB[e.box];
        } else if (e.autoIdx >= 0) {
            if ((size_t)e.autoIdx >= g_autoTrig.size()) continue;
            const int ft = g_autoTrig[(size_t)e.autoIdx].fireT;
            if (ft < 0) continue;
            if (autoDisarmed(e.autoIdx, mask, fireB)) continue;   // --tapdisarm
            f = ft;
        } else {
            if (e.at < 0) continue;
            f = e.at;
        }
        const double te = f + e.delay + offAfterLag(e);   // --offafter
        if (te > t || te < best) continue;
        best = te;
        on = e.on != 0;
    }
    return on;
}
// --recendtail: the same, from `init` as it stood at tick `after` -- only the switches that land after it.
inline bool resolveOnAfter(bool init, int after, const OnEvent* ev, size_t n, TouchMask mask,
                           const uint16_t* fireB, int t) {
    bool on = init;
    double best = -1e300;
    for (size_t k = 0; k < n; ++k) {
        const OnEvent& e = ev[k];
        double f;
        if (e.box >= 0) {
            if (e.box >= kTouchBits || !fireB || !(mask & touchBit(e.box))) continue;
            f = fireB[e.box];
        } else if (e.autoIdx >= 0) {
            if ((size_t)e.autoIdx >= g_autoTrig.size()) continue;
            const int ft = g_autoTrig[(size_t)e.autoIdx].fireT;
            if (ft < 0) continue;
            if (autoDisarmed(e.autoIdx, mask, fireB)) continue;   // --tapdisarm
            f = ft;
        } else {
            if (e.at < 0) continue;
            f = e.at;
        }
        const double te = f + e.delay + offAfterLag(e);   // --offafter
        if (te <= (double)after || te > t || te < best) continue;
        best = te;
        on = e.on != 0;
    }
    return on;
}

// --tapdisarm: the object's on/off the way GD keeps it, by GROUP. toggleGroup (0x223bc0) acts only when the
// group's bit (the effect manager's +0x4f8) differs from the request, and then moves a count on every object in
// the group (+0x4c0: +1 on, -1 off), the object being off while the count is negative. So the object is off
// while any group it sits in is off, and a group is as its last switch left it (on until one lands). An event
// with no group (-1) stands for a group of its own. resolveOn's one-list form cannot say this: a custom level's
// spike sits in two switched groups, and switching one back on left it out in GD.
inline bool resolveOnGroups(const OnEvent* ev, size_t n, TouchMask mask, const uint16_t* fireB, int t) {
    struct G { int grp; double best; uint8_t on; };
    G gs[16];
    size_t ng = 0;
    for (size_t k = 0; k < n; ++k) {
        const OnEvent& e = ev[k];
        double f;
        if (e.box >= 0) {
            if (e.box >= kTouchBits || !fireB || !(mask & touchBit(e.box))) continue;
            f = fireB[e.box];
        } else if (e.autoIdx >= 0) {
            if ((size_t)e.autoIdx >= g_autoTrig.size()) continue;
            const int ft = g_autoTrig[(size_t)e.autoIdx].fireT;
            if (ft < 0) continue;
            if (autoDisarmed(e.autoIdx, mask, fireB)) continue;
            f = ft;
        } else {
            if (e.at < 0) continue;
            f = e.at;
        }
        const double te = f + e.delay + offAfterLag(e);   // --offafter
        if (te > t) continue;
        const int key = e.grp >= 0 ? e.grp : -2 - (int)k;
        size_t j = 0;
        while (j < ng && gs[j].grp != key) ++j;
        if (j == ng) {
            if (ng == 16) continue;   // more groups than any object seen; the rest are left out
            gs[ng++] = {key, -1e300, 1};
        }
        if (te < gs[j].best) continue;   // a tie goes to the later in the list, as in resolveOn
        gs[j].best = te;
        gs[j].on = e.on;
    }
    for (size_t j = 0; j < ng; ++j)
        if (!gs[j].on) return false;
    return true;
}
// The switches the x-crossing triggers give each object (AutoTrig::tog), by object uid, each
// tied to its trigger's index in `autos` -- which has to be the final g_autoTrig, sorted, since
// the index is how the event finds its fire tick.
inline std::unordered_map<int, std::vector<OnEvent>> autoOnEvents(
        const std::vector<AutoTrig>& autos) {
    std::unordered_map<int, std::vector<OnEvent>> out;
    for (size_t a = 0; a < autos.size(); ++a)
        for (const AutoTrig::Tog& g : autos[a].tog) {
            OnEvent ev;
            ev.autoIdx = (int)a;
            ev.on = g.on;
            ev.delay = g.delay;
            ev.grp = g.grp;
            out[g.uid].push_back(ev);
        }
    return out;
}
// Travel coordinate of each touch box, by its bit in State::trig (filled by
// loadTouchTriggers). ownTouch's proximity gate reads it: the one measured
// punch pair (box ~3,255..3,300 lifting uid18119 at 3,486) is 230px apart,
// while the pre-band grazes that phantomed the whole switch band sat 500+px
// from the ceiling they were credited with delaying.
inline float g_touchBoxU[kTouchBits] = {};
// --keycensus: how many times each box divided the dedupe key (search_key.hpp).
// The window's whole cost lands there -- a box in the window splits the key for
// as long as it is moving -- so this says WHICH boxes are worth their bit.
// Relaxed atomics: the layer expansion is parallel and nobody reads the tally
// until the run ends, so the interleaving does not matter, only the per-box
// order of magnitude. Off by default; the counting is one increment.
inline bool g_keyCensus = false;
inline std::array<std::atomic<long long>, kTouchBits> g_keyCount{};
// The tick box b was FIRST entered by any state of THIS solve (-1 = not yet).
// A level property like AutoTrig::fireT, and the same approximation ("the
// first group to reach it wins"). Deliberately not carried by the state: the
// state's trigT only remembers its LAST box, and the per-box delay of the
// switch band needs every punch's own tick. Bits the ANCHOR arrived with stay
// -1 -- their effect is already inside the live recording (it replayed this
// very plan's prefix), so crediting them again would double-count the delay.
// FILLED RATHER THAN LISTED. This used to be 32 literal -1s, which is a trap
// the moment kTouchBits moves: a shorter list leaves the tail at 0, and 0 here
// means "entered at tick 0" rather than "not yet". The fill cannot go out of
// step with the width.
inline std::array<int, kTouchBits> g_touchFireT = [] {
    std::array<int, kTouchBits> a{};
    a.fill(-1);
    return a;
}();
// --formularec (default on since 2026-10): a formula-driven object reads its recording, unshifted, while every
// box that moves it stands as the recorded attempt left it -- entered on the same tick by the state
// and by the attempt (--touchentered uid:tick, filled per call into g_touchRecEntry by bit; -1 = the
// attempt had not entered it by t0), or entered by neither. The formula reconstructs a world the
// recording only holds one sample of; when the state's boxes ARE that sample, the recording is the
// game's own answer and the formula is not. A custom level's five platform blocks (one touch box,
// entered at t=7,022 in both) sat 126.6 px below the recording at t=7,066 on the formula path.
inline bool g_formulaRec = true;
// ...and only when the recording IS that attempt's (--recprov rec,anchor,reach from the mod): the
// recording behind the last --groups and the anchor's rows must be one attempt, and the recording is read
// only up to the tick it reached. The mod keeps the deepest recording, so the two can be different
// attempts, and another attempt's recording holds that attempt's switches at the same ticks -- the same
// box entered on the same tick by a different run says nothing about this run's world. Without --recprov
// (an older mod, or a recording of unknown attempt) the recording is not read.
inline bool g_recProvGiven = false;
inline long long g_recProvRec = -1, g_recProvAnchor = -1, g_recProvReach = -1;
// --formularectouch (default on since 2026-10): ...and the attempt's entries after t0 are known too -- --rectouch
// dates each box the attempt fired with the same test over its whole recorded rows (repair.hpp
// recTouchArg) -- up to the tick --recprov lets the recording be read. --touchentered stops at t0, so
// a box the state entered after t0 read as one the attempt had not, and the placement went back to
// the formula on the tick the state entered it even when the attempt had entered it on the same tick.
// The other way round it held the recording although the attempt had entered a box the state had not.
// A custom level: a swing carried up by a platform block from t=6,994; the box entered at 6,996 by the
// state and the attempt alike put the block back 118.6 px down at 6,997 and the swing fell a tick early.
inline bool g_formulaRecTouch = true;
inline bool formulaRecProvOk(long long t) {
    return g_recProvGiven && g_recProvRec >= 0 && g_recProvRec == g_recProvAnchor && t <= g_recProvReach;
}
inline std::array<int, kTouchBits> g_touchRecEntry = [] {
    std::array<int, kTouchBits> a{};
    a.fill(-1);
    return a;
}();
// How many formula-driven placements --formularec served from the recording, and how many it left
// on the formula (a box the state and the attempt disagree on). Tallied on the replay (single
// thread) for the print; relaxed atomics so a threaded search may count too.
inline std::atomic<long long> g_formulaRecHit{0}, g_formulaRecMiss{0};
// uids some Rotate (1346) trigger turns. A rotation carries its group around a
// centre object, so no offset describes it and the closed form must not claim
// it -- lv21 has 200 objects that look like plain moves until you notice a
// rotate on the same group (their recorded path missed the move-only formula by
// 120 px). Filled by loadAutoTriggers, which walks the same chains.
inline std::unordered_set<int> g_rotated;
// What a Rotate does to one object, from the dump alone -- the input side of
// the computed timeline that will replace the recording for these objects.
//
// MEASURED 2026-09-01 against grouptrace, lv21 uid15367 (in the turned group,
// orbiting uid15390, both carried down 150 by the same move) over the full 960
// ticks of a -720 degree turn:
//
//   the model today, best single shift of the recording   1.65-2.18 px
//   closed form, absolute angle                           0.0142 px
//   per-tick recurrence in double                         0.0142 px
//   per-tick recurrence, position stored through float    0.0060 px
//
// The first two of those agree EXACTLY, which is what says the composition is
// not where the last hundredth of a pixel lives: it is rounding. After a full
// -720 the object should return to its entry point and GD leaves it at
// (-0.005, +0.013) -- 960 ticks of a float store (the cvtpd2ps in GD's move
// path). Hence the recurrence below, and hence `float` and not `double` for
// the carried position.
//
// lv22 uid254 (7 degrees, BounceOut, over 120 ticks) reaches 0.0940 px by the
// same route, and there the float store makes no difference at all -- 120
// ticks of 7 degrees has nothing to accumulate. Its residual is the shape of
// gdEase case 9 at rate 2, not the composition: the endpoint is exact
// (R(-7 deg) on (0,162) predicts dx=+19.75/dy=-1.21, GD gives +19.743/-1.208)
// and the object's move alone reproduces its cy to 1.208 px, which IS the
// rotation's y-component. That easing is being re-read from the binary; until
// it is, 0.0940 px is the expected value for a Bounce-eased rotate and not a
// defect to chase.
struct RotSpec {
    int trigUid = 0;       // the Rotate, for reporting and for its fire tick
    int centreUid = 0;     // the single object of the centre group
    double total = 0.0;    // t360*360 + deg -- the WHOLE angle
    double durT = 0.0;     // ticks
    int ease = 0;
    double erate = 2.0;
    int lockrot = 0;       // does the object's own facing follow the orbit?
    // Resolved at load, once the objects are placed:
    int anchor = -1;       // g_autoTrig index of the Rotate (-1 = not autonomous)
    int centreIdx = -1;    // index into dyn of the centre object
    float relX = 0.f, relY = 0.f;   // entry position MINUS the centre's entry
};
inline std::unordered_map<int, RotSpec> g_rotSpec;   // uid -> how it turns
// --rottail (default on since 2026-10): past the end of a recording that stopped at the attempt's death, an object an
// autonomous Rotate turns goes on turning by the Rotate's closed form, about its centre object, from where the
// last row left it. A recording ends where its attempt died, and a mover held at its last row is wherever GD had
// it then: on a custom level a spike orbiting a fixed centre (+90 degrees over 1.7 s, EaseInOut) was held at
// the row of the death at t=13,605, and the next plan flew past it at 13,625 where GD's spike, 6.4 px further
// along and 8.6 degrees further round, killed the wave. The closed form, theta = total * ease((t - fireT) /
// dur) about the centre group's single object, reproduces GD's 66 rows from 13,560 to 13,625 to 0.011 px, the
// angle to the recorded digit, and the bounding box from the object's own box turned by that angle.
// It applies only while the placement still holds the last row, and turns from the recording's own last tick:
// the recorder writes a row whenever anything changes, so an object whose last row is older stood as that row
// until then, one that had not moved at all yet included (the spike that killed the same wave 20 ticks earlier
// had one row, t=1, in the recording its plan was searched against). Only an object nothing else moves (no
// Move parts) whose centre object is not itself recorded as moving. Each Rotate turns about its own centre, in
// crossing order. It takes the place of --recendextend's two steady steps for such an object.
inline bool g_rotTail = true;
struct RotTailPart {
    int trigUid = 0;      // the autonomous Rotate (its g_autoTrig entry gives the fire tick)
    int centreUid = 0;    // the single object of its centre group; 0 = each object turns about itself
    double total = 0.0, durT = 0.0, erate = 2.0;
    int ease = 0;
    uint8_t lock = 0;     // lock rotation: the object orbits but keeps its facing
    double trigCx = 0.0;  // the Rotate's x, for the crossing order
};
inline std::unordered_map<int, std::vector<RotTailPart>> g_rotTailParts;   // turned uid -> its Rotates
inline int g_rotTailN = 0;   // objects the loader armed (the CLI prints it)
// --recendtail (default on since 2026-10): past the recording's last row (the attempt's death), a moving object of a rig --
// one an x-crossing lockToPlayerX Move or a Rotate about a centre object reaches -- goes on as the trigger
// schedule moves it, and an object's on/off goes on by its switches.
//   - The rig is simulated at load the way GD steps it (level_loader.hpp, the --recendtail stage): from the
//     reset positions, each tick the Moves' eased increments, the lock's copy of the player's x step, then each
//     Rotate's increment about its centre object's position of that tick, in crossing order. The centres are
//     simulated with it, so a centre that moves -- carried by the same lock, or turned by another Rotate --
//     carries its orbit along. The crossings are read off --xtrack (GD's x on the anchor's attempt), straight
//     on past the track's last tick; without --xtrack nothing is armed.
//   - An object is armed only where the simulation reproduces its own recording: over its last rows the
//     simulated position, less one constant offset taken at the last row, within 0.5 px and 0.2 degrees, and
//     not moving where the recording stood still between its last row and the recording's end. A touch chain,
//     a spawn chain the walk does not see or a target Move leaves it unarmed, on the tail it had.
//   - Past the last row the object is the simulation plus that offset, and while the lock is open, plus the
//     player's x now less the track's x then (a state that reaches a speed portal at a different tick).
//   - The on/off past the last row starts from the last row's and takes every Toggle that lands after it: an
//     x-crossing one at its trigger's fire tick, a touch one at its box's fire tick in a state that entered it.
// A custom level's spike uid 23082 (id 8, a 24 x 311 box): it rides a 60 s lockToPlayerX Move and turns about
// three centres (Rotates on groups 194, 163 and 207, each centre carried by the same lock), and a Toggle at
// x=29,625 switches it on. Every recording the search read ended at the attempt before's death; past it the
// model held it at x 29,613, switched off, where GD carries it 580 px on and turns it 8.7 degrees -- 22
// iterations died on it one after another, each a few ticks further on. The simulation from the reset puts it
// within 0.005 px in y and the recorded angle to 0.001 degree at t=18,214, 360 ticks past its last row.
// Precedence among the tails past the last row: --rotextend extends the rows at load, so this starts from its
// last row; where an object is armed, this replaces --moverextend, the parts tail, --ctlautotail, the slide
// completion, --lockx's recorded-x replacement, --rottail, --recendextend and --recendresid (all of them read
// the last row or a part of the schedule this already holds); a touch's own step (the punch hold, the slam)
// is still added after it. An object it does not arm keeps those, in their own order. The on/off rule runs
// before --activators and --boxoff, which still decide the objects they own.
inline bool g_recEndTail = true;
// --recendauto (default on since 2026-10; with --recendtail): a recAuto object -- one whose recording a touch chain does not
// explain, so it plays for every state -- is a rig candidate as well, armed by the same check against its own
// last rows. It was left out, and stood at its last row: a custom level's ramp uid 14699 (id 1744, m 0.084)
// rides the rig's 60 s lockToPlayerX Move and the group-199 bob, and the two touch Moves of its group (+60 then
// -60, 0.7 s, long done) made it recAuto. Every recording ended at the attempt's death (t=19,448); two ticks
// later the model's ramp stopped at x 37,415.8 where GD carries it on (37,871 at t=19,640), and a cube GD
// rides on it fell through to the rig's floor spike.
inline bool g_recEndAuto = true;
// --areamove (default on since 2026-10; with --areas <file> and --xtrack): an object an Area Move (3006) moves stands where GD's
// area pass puts it for THIS state's player, not where the recorded run's player had it. GD
// (processAreaMoveGroupAction 0x22a4d0 and its leaves, read in the mod's src/solver/areaenv.hpp): every frame, for
// an object of the target group, s0 = obj.x - (centre.x + offset) (direction type 1), s = s0 * (s0 >= 0 ? modBack
// : modFront), u = clamp((s / length - deadzone) / (1 - deadzone), 0, 1), 1 - u when inbound; below u = 1 the
// object is moved by (1 - ease(u)) * (moveX, moveY) in xy mode, the actions of a frame adding up; the centre is
// P1's position when the trigger's special target is -1 (processAreaEffects 0x2283e0), read before this tick's
// step (the previous tick's x). Taken only for that shape (xy mode, direction type 1, centre P1, no variance,
// not relative, no ease-out), and an object only where the formula over its recorded rows, from the recorded
// run's x (--xtrack) and its first row as the base, reproduces them within kAreaArmTol px.
// A custom level's three saws of group 140 (two Area Moves, length 450, +300 inbound / -300 at offset -450):
// all 6,132 recorded rows within 0.147 px; the recording ended at the attempt's death (t=9,458) and the model held
// them there, 562 px off GD at t=10,038, where a branch that lives in GD died.
inline bool g_areaMove = true;
inline constexpr double kAreaArmTol = 1.0;
struct AreaAct {
    int uid = -1, id = 0, target = 0, special = 0, dirType = 0, eInType = 0, eInBuf = -3;
    double cx = 0, len = 0, off = 0, modFront = 1, modBack = 1, deadzone = 0, moveX = 0, moveY = 0, eInRate = 2;
    bool xy = false, inbound = false;
    // a move that is not xy (its distance along ccpForAngle(angle - 90)), an Area Scale's (3008) factors and an Area
    // Rotate's (3007) angle
    double moveDist = 0, angle = 0, scaleX = 1, scaleY = 1, rot = 0;
};
inline std::vector<AreaAct> g_areas;
// getEasedAreaValue: -1 the value itself, -2 the curve, a buffer index the 101-point table of the curve (linear
// between; unknown (-3) is taken as a table for a curve, the value for none). GameToolbox::getEasedValue's curves are
// the Move trigger's (gdEase).
inline double areaEase(const AreaAct& a, double u) {
    if (a.eInBuf == -1 || (a.eInBuf == -3 && a.eInType == 0)) return u;
    if (a.eInBuf == -2) return gdEase(a.eInType, a.eInRate, u);
    const float uf = (float)u;
    const int k = (int)(uf * 100.0f);
    if (k >= 100) return gdEase(a.eInType, a.eInRate, 1.0);
    const double lo = gdEase(a.eInType, a.eInRate, k / 100.0), hi = gdEase(a.eInType, a.eInRate, (k + 1) / 100.0);
    return (double)(uf - (float)k * 0.01f) * (hi - lo) * 100.0 + lo;
}
// The displacement of one action on an object whose x without area offset is `bx`, the centre P1 at `px`;
// false when it makes no call (u >= 1).
// getAreaObjectValue (direction type 1) for an object at x `bx`, the centre P1 at `px`.
inline float areaValue(const AreaAct& a, double bx, double px) {
    const float s0 = (float)(bx - (px + a.off));
    const float s = s0 * (0.f <= s0 ? (float)a.modBack : (float)a.modFront);
    const float x = s / (float)(int)a.len;
    const float dz = (float)a.deadzone;
    float r = dz == 0.f ? x : (x - dz) / (1.f - dz);
    r = r > 1.f ? 1.f : (r < 0.f ? 0.f : r);
    return a.inbound ? 1.f - r : r;
}
inline bool areaDisp(const AreaAct& a, double bx, double px, double& dx, double& dy) {
    if (a.id != 3006) return false;
    const float u = areaValue(a, bx, px);
    if (!(u < 1.f)) return false;
    const double k = 1.0 - areaEase(a, (double)u);
    if (a.xy) {
        dx = k * a.moveX;
        dy = k * a.moveY;
    } else {   // processAreaMoveGroupAction: kk * m along ccpForAngle((angle - 90) deg)
        const float ang = ((float)a.angle - 90.f) * 0.017453292f;
        const float t = (float)k * (float)a.moveDist;
        dx = (double)(t * std::cos(ang));
        dy = (double)(t * std::sin(ang));
    }
    return true;
}
// processAreaTransformGroupAction (3008): f = (S - 1)(1 - e) + 1, multiplied into the object's scale; an Area
// Rotate (3007): (1 - e) * R added to its angle. False when the action makes no call.
inline bool areaScaleRot(const AreaAct& a, double bx, double px, double& fx, double& fy, double& dr) {
    if (a.id != 3008 && a.id != 3007) return false;
    const float u = areaValue(a, bx, px);
    if (!(u < 1.f)) return false;
    const double k = 1.0 - areaEase(a, (double)u);
    if (a.id == 3008) {
        fx = (a.scaleX - 1.0) * k + 1.0;
        fy = (a.scaleY - 1.0) * k + 1.0;
    } else {
        dr = k * a.rot;
    }
    return true;
}
// Where an object with base (bx, by), unturned half sizes (ha, hb) at angle r0 stands under `acts` (g_areas
// indices) for P1 at `px`: its centre, and -- when a scale or rotate acts on it (`sized`) -- the half sizes of its
// turned, scaled box's bounding rect (getObjectRect) and its angle.
struct AreaPose {
    double cx = 0, cy = 0, hw = 0, hh = 0, rot = 0;
    bool sized = false;
};
inline AreaPose areaPoseOf(const std::vector<int>& acts, double bx, double by, double ha, double hb, double r0,
                           double px) {
    AreaPose p;
    p.cx = bx;
    p.cy = by;
    double sx = 1.0, sy = 1.0, dr = 0.0;
    for (const int k : acts) {
        const AreaAct& a = g_areas[(size_t)k];
        if (a.id == 3007 || a.id == 3008) p.sized = true;
        if (px < a.cx) continue;   // not crossed yet
        double dx = 0.0, dy = 0.0, fx = 1.0, fy = 1.0, r = 0.0;
        if (areaDisp(a, bx, px, dx, dy)) { p.cx += dx; p.cy += dy; }
        if (areaScaleRot(a, bx, px, fx, fy, r)) { sx *= fx; sy *= fy; dr += r; }
    }
    p.rot = r0 + dr;
    if (p.sized) {
        const double th = p.rot * (3.14159265358979 / 180.0);
        const double c = std::fabs(std::cos(th)), s = std::fabs(std::sin(th));
        const double w = ha * sx, h = hb * sy;
        p.hw = w * c + h * s;
        p.hh = w * s + h * c;
    }
    return p;
}
inline int g_recEndTailN = 0, g_recEndTailTried = 0, g_recEndTailOnN = 0;   // printed only
inline constexpr int kRecEndTailTicks = 4000;     // simulated past an object's last row; held after
inline constexpr int kRecEndTailCheck = 120;      // the rows before the last one that must agree
inline constexpr double kRecEndTailTol = 0.5, kRecEndTailRotTol = 0.2, kRecEndTailOff = 4.0;
// OFF, and the reason is not the physics -- it is that ADDING a rotation in the
// placement path double-applies it. Every collidable turned object in the
// corpus HAS a recording, and a recording is GD's own answer with the orbit
// already in it; turning it again on top is simply wrong. deathref caught it
// in one run: lv21_t13304 PASS -> FAIL, the model killed at t=12,980 against
// GD's 13,416, a rotated hazard swung twice as far as it should.
//
// The population makes this decisive rather than a tuning matter. Of lv21's
// 872 computable orbits, 806 are type 7 -- DECORATION, which the loader never
// admits and which therefore costs nothing standing still; the collidable ones
// are 62 hazards + 4 (type 47), all recorded. lv22 is 264 decoration and 9
// solids, all recorded. So there is no population of "turned objects the model
// holds still": the whole set is 75 objects that already have GD's answer.
//
// What 008 is actually worth is therefore RE-TIMING, not motion: a recording
// is one run's timeline, and one shift cannot carry a rotate and a move that
// cross at different x (1.65-2.18 px). Capturing that means REPLACING those
// objects' samples with rows computed from the definitions, each trigger on
// its own fire tick -- not adding a rotation to the placement path. That is a
// different change, and it is the one to make.
inline bool g_rotCompute = false;
// Load-time self-check: replay the computed timeline against every recorded
// rotated object and report the residual. The corpus-wide version of the
// harness that measured the two objects above.
inline bool g_rotCheck = false;
// Load-time counters, so "the orbit did not arm" can be told apart from "the
// object never reached the router".
inline size_t g_rotSeen = 0, g_rotRouted = 0;
// stage 1': put the ORBIT into samples[] and leave the translation analytic.
// --no-rotsplit for the A/B. See the block in level_loader.hpp for why this is
// the insertion point and adding a rotation to placement is not.
inline bool g_rotSplit = true;
// --rotextend (default off): an autonomously turned object's samples[] go on past the last row its
// recordings hold, computed from its Rotate, to the end of the turn (level_loader.hpp, stage 1'').
// A recording ends where its attempt died, and past its last row an object holds that row, so a
// blade still sweeping stands still in the model. Measured on lv21 (coins off, the loop's recorder
// replay of iteration 2, integ f04b889): the bar of group 98 -- eight id 1582 blades turning -720
// degrees about uid 14886 over 960 ticks -- killed every attempt at t=11,271 and the recording
// stopped there; the bootstrap that would have carried it on is on another clock and left out. The
// model killed on the held row a tick late (11,273 against GD's dead row 11,272), and with the turn
// carried on it kills on 11,272, on the same blade (uid 15024).
// (The switch is gone since the 0.4.0 clean-up; its on behaviour is fixed.)
// --slideparts (default off): past the last row of its recording, an object with two or more
// autonomous moves goes on by each move's own eased increment from that move's crossing, instead
// of the recording arm's completion, which reads the moves as one (their summed offset over one
// duration, from the recording's first motion) and so finishes a sequence as a single slide.
// Measured on lv21 (coins off, rot-extend d8d3c83 with --rotextend, the recorder's replay of
// iteration 5): uid 16678 (id 1583, groups 215/216: Move +90 over 144 ticks from x=15,960, then
// Move -210 over 168 from x=16,138) is recorded to t=12,454 at y=241.614, 21 ticks into the
// second move; the completion took the -120 as 95% done and put it at 216.406 on the next tick
// and at 35.550 eight ticks later. GD killed on it at 12,454 (dead row 12,455) and the model flew
// on to 13,935.
// (The switch is gone since the 0.4.0 clean-up; its on behaviour is fixed.)

// GD's rotation, one tick of it. `sign` is the mapping from the dump's angle to
// a y-up rotation matrix, measured as -1 on both subjects (GD's positive turn
// is clockwise on screen).
//
// The position is carried as float on purpose -- see the note above; it is the
// difference between 0.0142 px and 0.0060 px on a 960-tick turn.
inline void rotStep(float& x, float& y, double cxPrev, double cyPrev,
                    double cxNow, double cyNow, double dTheta) {
    const double c = std::cos(dTheta), s = std::sin(dTheta);
    const double rx = (double)x - cxPrev, ry = (double)y - cyPrev;
    x = (float)(cxPrev + c * rx - s * ry + (cxNow - cxPrev));
    y = (float)(cyPrev + s * rx + c * ry + (cyNow - cyPrev));
}
// Always on (--no-trigclosed is gone since the flag clean-up). For an
// autonomously moved object the FORMULA places it, not the recording -- but
// only where the object passes autoClosed (a per-object check at load).
//
// Why a formula beats a measurement here: a recording ends where its attempt
// died, it is one run's timeline, and it only exists for objects some plan
// actually reached. The formula has none of those limits and, where both exist,
// they agree to 0.002 px. For a lockToPlayer object the recording is not merely
// limited but WRONG for anyone else -- it followed the recorded run's player.
// Objects the formula cannot describe (move-to-target with no offset, anything
// a rotation carries, two locks, a y lock) fail autoClosed and keep theirs.
// --trigraw: autonomous triggers behind the anchor (before it) use **the
// recording's tick as is** when a recording exists (no switching over to the x
// estimate through the 60 tick gate). est's "how many ticks ago, by
// distance/speed" assumes x advanced monotonically, which breaks behind the
// rotation maze: at lv22's t=14,250 anchor, uid10628 (x=15,075, real firing
// t=10,520) was re-dated to est=13,845, group 331's whole timeline slid by
// +3,325 ticks, the ON return of "off@10,520 -> on@14,198" flew into the future
// at t~17.5k, and the floor GD was riding was missing in the model alone. The
// driver takes the anchor from the same dump as the snap (the recording of this
// attempt's replay), so for that pair the recording's time axis = this run's
// time axis and a shift of 0 is correct. Without the flag the behaviour is
// exactly as before.
inline bool g_trigRaw = false;
// --anchornoop (on by default since 2026-10): an autonomous object whose ANCHOR is a Toggle that
// moves it nowhere, and whose recording already shows that Toggle's state on its first row, is read
// on the recording's own clock (shift 0) and casts no vote for the anchor's fire tick. The shift
// pairs the anchor's fire tick with the object's first recorded change, which is only the anchor's
// effect when the anchor's effect is IN the recording. A custom level's 17 pads (group 171) are
// anchored on a Toggle-OFF at x=-51.5 whose effect precedes the first row (`init,...,0`); the
// first change on file is the ring at x=5,914 switching them ON at t=4,771, so the pads were read
// 4,771 ticks ahead and stood at their post-x=12,800 position (+750/-75) from the anchor on.
// Dynamics::anchorUnrec.
// (The switch is gone since the 0.4.0 clean-up; its on behaviour is fixed.)
// --xtrack <file> (default none): GD's own x on every tick of the attempt the anchor was taken
// from, `tick,x` rows (a header naming `tick` and `x`, as gdref's csv has, is honoured). An
// autonomous trigger behind the anchor is then dated by the first tick at which GD's x reached
// it -- the definition the x estimate approximates with one dx. That estimate divides the whole
// distance by the speed the static portals give at x0, which a level whose portals sit in groups
// (routed to dyn, never in L.speeds) or off the path the player took gets wrong by hundreds of
// ticks, and no list of portals says which ones GD actually went through.
inline std::vector<std::pair<int, double>> g_xTrack;
// GD's x at tick t on the track's attempt (--recendtail): the track's own row, linear between rows, and past
// either end straight on by the end's last step. 0 without a track.
inline double xRefAt(int t) {
    const auto& v = g_xTrack;
    if (v.empty()) return 0.0;
    const size_t n = v.size();
    if (t <= v.front().first) {
        if (n < 2 || v[1].first <= v[0].first) return v[0].second;
        return v[0].second - (v[1].second - v[0].second) / (v[1].first - v[0].first) * (v[0].first - t);
    }
    if (t >= v.back().first) {
        if (n < 2 || v[n - 1].first <= v[n - 2].first) return v[n - 1].second;
        return v[n - 1].second
               + (v[n - 1].second - v[n - 2].second) / (v[n - 1].first - v[n - 2].first) * (t - v[n - 1].first);
    }
    const auto it = std::lower_bound(v.begin(), v.end(), t,
                                     [](const std::pair<int, double>& a, int b) { return a.first < b; });
    if (it->first == t) return it->second;
    const auto& a = *(it - 1);
    return a.second + (it->second - a.second) * (double)(t - a.first) / (double)(it->first - a.first);
}
// ...and the first tick its x reaches `cx` (-1 = never, the track standing still at its end).
inline int xRefCross(double cx) {
    const auto& v = g_xTrack;
    if (v.empty()) return -1;
    for (size_t k = 0; k < v.size(); ++k)
        if (v[k].second >= cx) {
            if (k == 0) return v[0].first;
            // between two rows: the first whole tick at or past cx
            const auto& a = v[k - 1];
            for (int t = a.first + 1; t <= v[k].first; ++t)
                if (xRefAt(t) >= cx) return t;
            return v[k].first;
        }
    const size_t n = v.size();
    if (n < 2) return -1;
    const double step = (v[n - 1].second - v[n - 2].second) / (double)(v[n - 1].first - v[n - 2].first);
    if (!(step > 0.0)) return -1;
    return v[n - 1].first + (int)std::ceil((cx - v[n - 1].second) / step - 1e-9);
}
// --recleads (default on since 2026-10): an autonomous object whose recording starts moving before the player
// has reached any of the autonomous controllers the walk found for it is read as its recording
// (level_loader.hpp, emit). Until its anchor fires such an object is held at its first row, and the
// motion that came first is not the walk's to describe. A custom level's blue orb uid 630 bobs +/-90
// on a spawn loop that an x-crossing Spawn at x=6,225 starts (Spawn 579 -> Move 577 and Spawn 581
// -> Move 578 and Spawn 580 -> ...); the walk knows only the three Moves of another group from
// x=10,815 on, so the orb sat 76 px under GD's and the model alone fired it on a press at t=4,843.
// Needs --xtrack (the recording run's x at its first change); undecided without it.
inline bool g_recLeads = true;
inline int g_recLeadsN = 0;   // objects it re-routed in the last load (printed only)
// --recanchorx (default on since 2026-10): an autonomously anchored object is re-timed against the recording's first change,
// read as the moment its anchor fired in the recording run. When that change comes BEFORE the recording run
// crossed the anchor (--xtrack), something else moved it first, and the pairing reads the recording at the
// wrong age: the object is paired with the recording run's own fire of the anchor (its crossing plus the delay)
// instead. A custom level's saw (id 679) turns in place from t=1,158 under a Rotate and only starts its Move at
// 5,285; its anchor is crossed at 2,827, the pairing shifted it by 1,669 ticks, and at 5,712 the model held it
// 54 px behind the saw GD killed the ship on. Needs --xtrack; undecided without it.
inline bool g_recAnchorX = true;
inline int g_recAnchorXN = 0;   // objects it re-paired in the last load (printed only)
// Autonomous lag: an object moved by a touch box AND an autonomous
// trigger (Level::formula) dates that trigger from its recording with the lag of
// the AUTONOMOUS move (Dynamics::autoLag), not the touch move's (recLag). The
// recorder writes nothing until 0.05 px, and a slow ease-in takes a long time to
// get there: lv22's descending ceiling (group 265, uid18098: -480 px over 1,005.6
// ticks, ease-in rate 2.5) first shows at t=2,256 at 0.052 px, while the curve
// through that row and the rows at 2,298 (0.571) and 2,411 (6.597) starts at
// 2,230 to 0.01 px. With recLag (1, the touch move's) the trigger fired at 2,255
// and the ceiling ran 2 px high at t=2,411 and 13.6 px high at t=2,842 -- enough
// for the model to fly past two of GD's deaths.
// ON by default since 2026-09-22, as ONE unit with the mod's anchor opening from
// the recorded positions (cfg dptouchentered, --touchentered below): the old
// opening put that ceiling at its final height, far from the player, which is
// what let the kill records at GD's deaths through the nearDynObject gate; the
// recorded opening puts it where GD has it, and only with this lag is "where GD
// has it" right. The pair: cold 22/22, per-level iterations and [fp] identical
// on 21 levels, lv22 the same 26 iterations and deaths by another route.
// Always on since the 0.2.0 flag clean-up (it was --automixlag).
// --lagfit (print only): for each object the lag above re-dates, the residual of
// its first recorded rows against the autonomous and the touch curve (level_loader).
inline bool g_lagFitDbg = false;
// --lockanchor: an anchor behind an auto lockToPlayerX trigger restores the lock's
// displacement from the recording (cli.hpp, the behind-the-anchor trigger loop).
// On since 2026-09-21; always on since the flag clean-up.
// --touchentered <uid,...|->: the touch boxes (by trigger uid) the attempt's
// recorded positions overlapped by the anchor. Given, the anchor scan opens
// only these from the recording (cli.hpp, `mayOpen`). "-" = given, none.
inline bool g_touchEnteredGiven = false;
inline std::unordered_set<int> g_touchEntered;
// ...and, when the caller writes `uid:tick`, the first tick the recorded positions
// overlapped that box. The anchor scan then takes the box's fire tick from it
// instead of from the recording's first motion, which for an object that an
// autonomous Move also drives is that Move's, not the box's: lv22's trap 18092
// (entered at 2,614 in the recorded rows) was dated 2,255 -- the start of the
// ceiling's slow descent -- so its target-mode slam had long finished at a t0=2,651
// anchor where GD's ceiling is still coming down 3 px a tick.
inline std::unordered_map<int, int> g_touchEnteredT;
// --rectouch uid:tick,... (read under --slambox, constants.hpp): the touch triggers GD itself fired in the
// attempt the call's recording (--groups) comes from -- the mod's touchfire map, written by
// playerTouchedTrigger and banked with that attempt -- each dated by the attempt's whole recorded rows
// with markTouched's own test, the date --touchentered gives up to t0 (-1 = the rows never enter it), so
// that the date and the state's entry tick come from one test. Filled per call by bit into g_recTouchBit.
inline bool g_recTouchGiven = false;
inline std::unordered_map<int, int> g_recTouch;
// --boxoffown (on by default since 2026-10): an object an off box takes out (Dynamics::offBox, --boxoff) does
// not read "gone" off the recording while no such box has fired in the group's world. The recorded
// attempt may have pressed inside the box where this state does not, and that press is not this
// state's. A custom level: a 1594 (actgrp 0) switches off group 2, a spike among it; the recording
// says off from t=5,956, from an attempt that pressed in the box. The replayed plan does not press
// there (its button is up from 5,883), the model took the spike out at 5,956 anyway, and GD killed
// the player on it at t=6,005, three times in one cold run. Since the box's own fire now decides,
// the flag also fires a pressed off box from the parent's overlap in a ship or UFO (step.hpp,
// markTouched's offFresh), the button pass GD takes that press in.
inline bool g_boxOffOwn = true;
inline std::array<int, kTouchBits> g_recTouchBit = [] {
    std::array<int, kTouchBits> a{};
    a.fill(-1);
    return a;
}();
// --slambox: is the step box b would add already in the recording? Only when the recorded attempt fired
// that same box and the state entered it on the recorded entry's own tick -- the same activation, delta
// 0. Another box, or the same box entered on another tick, keeps its step. The old
// rule (--slamdedup) had only a tick to go on, so it took the state's box for the recording's one.
inline bool slamBoxRecorded(const std::array<int, kTouchBits>& rec, int b, int F) {
    return b >= 0 && b < kTouchBits && F >= 0 && rec[(size_t)b] >= 0 && rec[(size_t)b] == F;
}
// --anchorentry (on; --no-anchorentry is the off arm): for a box with an entry tick, the anchor scan measures its objects' travel
// and first motion from where the recording had them AT THE ENTRY, not from their first sample
// (cli.hpp, the anchor scan). Two things the first sample cannot see: an earlier autonomous Move of
// the same object (SubZero 4003: group 10 goes +150 at t=3 under an autonomous Move and -150 under
// the touch Move uid 1233, so the net travel at an anchor past both is 0 and the box read as never
// entered, leaving the spikes and slabs of group 10 150 px up), and an entry that did not fire the
// box (--touchoff: entered at 1,484 while its group was off, fired at 1,823). An opened box keeps the
// fire tick measured after the entry instead of being re-dated to the entry.
inline bool g_anchorEntryBase = true;
// --dyndbg <uid>: dump one moving object's placement inputs at load (see the
// print near the fireT resolution).
inline int g_dynDbg = -1;
// The search's thread pool, lent to the per-object loops of Dynamics::seek and
// Dynamics::applyTriggers: runs task(k) for every k in [0,n) and returns when
// all have run. Set by the search for exactly as long as its pool exists, on
// the thread that owns the pool, and only called there, between its parallel
// phases (seek and applyTriggers are serial-caller-only already). Empty
// everywhere else, which is the plain loop.
inline thread_local std::function<void(size_t, const std::function<void(size_t)>&)>
    g_dynParTasks;
// How many objects the loader put on the formula path, and how many of those
// never saw either controller fire in this call. The second is the guard's
// readout: an object that is formula-driven but whose triggers never fire sits
// at its base position, which is right, but if the count is the whole set the
// classification found objects the plan cannot reach and the formula is
// carrying nothing.
inline int g_formulaDriven = 0;
// (--noformula, which kept every object on the recording path, is gone since the flag clean-up.)
// --touchretime: re-time a touch-controlled object's recording against THIS
// state's entry into the box that reaches it, instead of playing it on the
// recording run's clock (the touch branch below sets anchor = recAnchor, so
// shift is 0 by construction). Measured on lv22's block row, group 508 under
// box uid17771 (worker 98, three arms of the same plan): the row's first moving
// row is two ticks after the ball's rect first overlaps the box -- t=6534 after
// an overlap from 6532, t=6508 after an injected overlap from 6506 without a
// landing, and no motion at all through t=6546 when the ball is held clear.
// Only boxes entered at or after the solve's own start are re-timed
// (g_touchRetimeFrom): a box the anchor arrived with has its fireB seeded from
// the recording's first motion, not from an entry, and its recording is this
// run's own. Also changes how recAuto is classified (level_loader.hpp). On
// since v0.1.4, always on since the flag clean-up; lv19's doors measured a
// 5-tick latency with a spawn delay in the chain, which this does not model.
inline long long g_touchRetimeFrom = 0;
// --touchretimelag: a touch-only object carries the recorder's threshold lag
// (level_loader.hpp) and the re-timed anchor adds it, so the box entry +
// latency (the motion's start) lines up with the recording's first ROW rather
// than being taken for it. Measured against the recording on lv22 uid1214 it
// moves the playback from two ticks ahead of the recorded row to one, and
// against the reference dumps lv22 tracks 19,100 -> 19,343 ticks with the
// standing "tracking shrank at t=2,600" gone. On since 2026-09-20, always on
// since the flag clean-up. It was held off for a while because a
// cold run then routed lv22 into a hole and did not clear inside the budget --
// that was a property of the base it was measured on (c935132 plus all five
// bundle units), and it does not reproduce once the other units are in: lv22
// clears in the same 32 iterations as without it, and the other 21 levels are
// fingerprint-identical.
// --recinterp: a recording's position between two close rows is interpolated,
// not held (Dynamics::seek).
// ON BY DEFAULT since 2026-09-20, PAIRED WITH
// --stickrelease. On its own it moves 9 of the corpus's 1,116 sections, all in
// lv22, and changes no verdict; its value is that it supplies the `dcy` the
// stick re-land needs on a tick the recorder skipped. Always on since the flag
// clean-up, like --stickrelease.
inline constexpr int kRecInterpGap = 4;
// The recorder's per-channel write threshold (grouptrace.hpp kEps): a row is
// written once any of x, y, w, h, rot has moved kRecEps or more from the last
// row written.
inline constexpr double kRecEps = 0.05;
// --recstall (default on since 2026-10): ...but a gap in a FAST mover's rows is a stop, not a slow stretch. An
// object that writes a row on the tick before the gap and on the tick after it moves kRecEps or
// more a tick there, so a tick with no row is a tick it moved less than that -- and when the change
// across a gap of g ticks is under g - 0.5 of those steps, the missing ticks are ones it did not move
// on: it holds the earlier row until the later one. A custom level's spike uid 9432 (group 51,
// turned by a Rotate already running when a second Rotate of the same group and centre fired on
// it): rows every tick at -0.075 px in x, none at 1,049, and 1,048 -> 1,050 is -0.076. GD's
// inverted mini ship touched it at 1,049 in its 1,048 place (0.0015 px of overlap), where the
// interpolated place left 0.014 px clear.
inline bool g_recStall = true;
// Whether the gap between sm[c] and sm[c+1] is such a stop (see g_recStall).
inline bool recStallGap(const std::vector<DynSample>& sm, size_t c) {
    if (!g_recStall || c == 0 || c + 2 >= sm.size()) return false;
    const DynSample &p = sm[c - 1], &s = sm[c], &n = sm[c + 1], &q = sm[c + 2];
    const int g = n.t - s.t;
    if (g < 2 || s.t - p.t != 1 || q.t - n.t != 1) return false;
    const double ch[3][4] = {{p.cx, s.cx, n.cx, q.cx}, {p.cy, s.cy, n.cy, q.cy},
                             {p.rot, s.rot, n.rot, q.rot}};
    for (const auto& v : ch) {
        const double before = std::fabs(v[1] - v[0]), after = std::fabs(v[3] - v[2]);
        if (before < kRecEps || after < kRecEps) continue;
        return std::fabs(v[2] - v[1]) < (g - 0.5) * 0.5 * (before + after);
    }
    return false;
}
inline constexpr int kTouchRetimeLat = 2;
// --touchretimebox: re-time a touch box's objects against the BOX's entry, not each
// object's own first motion. Without it every object is taken to start moving
// kTouchRetimeLat (+ its lag) after the entry, which holds for an object the box moves
// directly and not for one a spawn chain reaches later. lv22's spike row under box uid 1417
// (id 392, uid 17571 and 18201..18206): the recording has them first moving at 1,755 /
// 1,766 / 1,774 -- they drop one after another -- and the re-timing pinned all of them to
// the entry + 3, so five of the seven sank 10-18 ticks early. GD kills the spider on uid
// 17571 at t=1,813 in its unmoved place; the model had already lowered it and walked on.
// With this, the recording's entry into a box is estimated ONCE per box, as the earliest
// first motion (less latency and lag) among the objects only that box moves, and each
// object keeps its own delay behind it.
// ON BY DEFAULT since 2026-09-24; --no-touchretimebox restores the per-object
// latency. kDefTouchRetimeBox is what reset.hpp writes back.
// (The switch is gone since the 0.4.0 clean-up; its on behaviour is fixed.)
// The one touch box whose chain locks something to the player's x, and how long
// the lock lasts in ticks. A per-BOX quantity rather than a per-object one
// because what a state has to remember is "how far have I travelled since I
// punched THAT box", and the corpus needs exactly one of them: the census over
// all 22 levels found locks down a touch chain on lv19 only, through a single
// box (26 objects, 7 of them collidable). A second box would need a second
// accumulator, so it is reported rather than silently averaged -- the same
// treatment `alockN` already gives two locks on one object.
inline int g_lockBox = -1;
inline double g_lockTicks = 0.0;
// --lockx (off): see the recording arm in applyTriggers; the lock replaces a locked object's recorded x.
// On by default since 2026-10-07, as a pair with --anchormove; --no-lockx is the off arm.
inline bool g_lockX = true;
// --locknodx (default on since 2026-10): a lockToPlayerX Move's own x offset is left out of the object's autonomous
// moves (level_loader.hpp); the lock is its x movement.
inline bool g_lockNoDx = true;
// --lockstop (default on since 2026-10): a Stop in a touch chain this world has fired closes an autonomous
// lockToPlayerX window when it names the lock's Move. The window closed only at its own length or at
// an x-crossing Stop (--autostop); the touch chains' Stops (g_touchStops) were read for a formula
// object's moves, never for a lock. A custom level: a hazard is locked to the player's x for 20 s from
// the start, and a press block at x 5,914.5 spawns a Stop for the lock's group. GD's recording of the
// hazard stops moving on the tick the player presses there and holds; the model kept it on the player
// for 32 ticks more, 51.7 px ahead, and killed a cube 70 ticks later where GD jumped on.
// The window closes on the box's fire tick, the tick a touch Stop freezes a formula move on.
inline bool g_lockStop = true;
inline int touchStopTick(const TouchMask& mask, const uint16_t* fireB, int moverUid) {
    int st = 1 << 30;
    if (moverUid == 0 || !fireB) return st;
    const int nb = std::min((int)g_touchStops.size(), kTouchBits);
    for (int b = 0; b < nb; ++b) {
        if (!(mask & touchBit(b))) continue;
        for (const int s : g_touchStops[(size_t)b])
            if (s == moverUid) st = std::min(st, (int)fireB[b] + (g_spawnLag ? 0 : 1));
    }
    return st;
}
// --shiftstat: one `shiftstat:` line per moving object, the first time it is
// placed, saying which recorded row the model ends up reading for it.
//
// The re-timing (`tt = t - shift`) is PER OBJECT, so "the model is N rows behind
// GD" has no single N -- which is exactly what stopped a blanket phase fix on
// 2026-09-03 (uid 434 came out at shift=1, but shift=0 objects are a row nearer).
// This counts the distribution over the objects a collision actually consumes,
// so the size of the re-anchoring family is a number rather than an impression.
// Diagnostic only: prints and changes nothing.
inline bool g_shiftStat = false;
struct Dynamics {
    // Stable storage: `snapObj` / `usedOrb` / `usedPad` hold raw Obj* across
    // ticks, so these must never reallocate after load. The rects are MUTATED
    // in place once per tick by seek().
    std::vector<Obj> objs;
    std::vector<std::vector<DynSample>> samples;  // parallel to objs
    // [2026-08-25] Does a trigger ROTATE this object anywhere in the recording?
    // GD's collision test branches on the object's own "is turned" flag, and a
    // rotate-target carries it for the whole run -- not only once the angle has
    // grown away from zero. See the note at turnedBox's axis-aligned branch for
    // the measurement (lv22's spider portal uid 1156). Parallel to objs; empty
    // in an old Dynamics, which reads as "never", i.e. the previous behaviour.
    std::vector<uint8_t> everRot;
    // --dynobbrot: each object's turned-box axis as loaded (bc/bs at its placed rot), so seek can
    // turn it by the recording's angle minus that. Filled on the first seek with the flag; empty
    // otherwise.
    std::vector<double> obbBc0, obbBs0;
    // --rotobblatch: per object, -1 when the latch does not apply (an exported box, or not a --rotobb object), else
    // the first recorded tick its angle differs from the placed one (INT_MAX when no recorded row turns it). Its
    // obbOk is on from that tick, or while a placement turns it (obbLatch). Filled by the level loader.
    std::vector<int> obbLatchT;
    void obbLatch(size_t i, int t, double rotDeg) {
        if (i >= obbLatchT.size() || obbLatchT[i] < 0) return;
        double d = std::fmod(std::fabs(rotDeg - objs[i].rot), 360.0);
        if (d > 180.0) d = 360.0 - d;
        objs[i].obbOk = (t >= obbLatchT[i] || d > 0.001) ? 1 : 0;
    }
    // --shiftstat only: whether this object has already printed its line.
    // Sized lazily so an old Dynamics (or a run without the flag) carries
    // nothing.
    std::vector<uint8_t> shiftSaid;
    std::vector<size_t> cur;                      // per-object cursor
    // which window each one belongs in, mirroring the static split
    // COIN is last so the existing numbering is untouched. A coin only ever
    // reaches the dynamic set when a trigger controls it (--coins).
    enum Bucket : uint8_t { NEAR, PORT, PAD, ORB, SPEED, SLOPE, COIN };
    std::vector<uint8_t> bucket;
    // FORMULA-DRIVEN: this object's position is computed from its triggers
    // rather than replayed from the recording. Set by the loader for objects a
    // touch controller AND an autonomous one both reach, where BOTH actually
    // move something. GD applies both; a recording only ever holds one
    // attempt's answer to "was this touched" -- lv19's doors are recorded open,
    // lv22's switch band recorded untouched -- so for these objects neither
    // recording is the state's world, and everything the re-timing machinery
    // does below is an approximation of a world it cannot see. The two eased
    // moves ARE that world, to 0.0007 px against all 286 recorded rows of each
    // of lv19's seven (oneoff/py/fit96.py), so an object on this path takes
    // none of it: not the dual special case, not the ownTouch hold, not the
    // autonomous re-timing.
    std::vector<uint8_t> formula;
    // --formulanolock (speed.hpp): a formula-driven object whose autonomous chain locks it to the player
    std::vector<uint8_t> formulaLock;
    // Last tick's FINAL cy (after seek AND applyTriggers). dcy for a
    // trigger-CONTROLLED object must be the delta of its analytic position,
    // not of the raw samples: the recorder SKIPS rows when the move since the
    // last row is under its epsilon, so an eased tail records every 2nd-3rd
    // tick and the raw-sample dcy reads 0 on the skipped ticks -- while the
    // analytic position keeps moving. The ride gates on dcy != 0, so the
    // rider dropped grounded for exactly those ticks (lv22 t=849/851, the
    // hanging spider on the bobbing intro blocks: one +0.129 gravity blip per
    // skipped row).
    std::vector<float> prevCy;
    bool lastStep1 = false;
    // A moving SLOPE carries its surface line with it. sy0/sy1 are absolute
    // heights, so they are re-derived from the load-time values plus however
    // far the object has travelled in y. (Pure translation only -- a ROTATING
    // slope would need its line recomputed, and nothing rides one yet.)
    std::vector<float> baseCy, baseSy0, baseSy1;
    std::vector<uint8_t> on;   // toggled in? (0 = the object is not there)
    // --activators: for an object an activator switches (a pickup or a toggle block), its
    // on/off is not the recording's -- a recording holds one run's presses -- but its t=0 value
    // (onInit, the recording's reset snapshot) with every switch that reaches it applied in time
    // order for THIS group (resolveOn: the boxes by the group's own fire ticks, the x-crossing
    // Toggles by their fireT). Empty for every other object, for one whose recording has no
    // snapshot, and for all of them without the flag. Filled by the level loader.
    std::vector<std::vector<OnEvent>> onEv;
    std::vector<uint8_t> onInit;
    // --tapdisarm: the same for an object an x-crossing trigger switches when a box can switch that trigger
    // off (g_autoDisarm) -- read only in a state where it has (the object's on/off is its recording's
    // otherwise). Filled by the level loader; empty for every other object and without the flag.
    std::vector<std::vector<OnEvent>> disarmEv;
    std::vector<uint8_t> disarmInit;
    // ...and per object, whether anything that places it asks autoDisarmed for a trigger a box can switch off: its
    // disarmEv, or a --recendtail switch (tailOnEv) from such a trigger. Such an object depends on the group's
    // boxes, so it is not one of the pure objects placed once per tick (isPure). Filled by the level loader.
    std::vector<uint8_t> disarmDep;
    // --areamove (g_areaMove): per object the g_areas actions that move it (empty for every other object and without
    // the flag), its position without area offset (its first recorded row, before any action runs), and whether its
    // recorded rows passed the formula (only then is it placed by it). areaPxPrev is the group's x the tick before,
    // set by prepareGroup just before applyTriggers: the area pass runs before the player's step.
    std::vector<std::vector<int>> areaOf;
    std::vector<double> areaBaseX, areaBaseY;
    // ...and its unturned half sizes and angle at the base (for an Area Scale / Rotate)
    std::vector<double> areaBaseHa, areaBaseHb, areaBaseRot;
    std::vector<uint8_t> areaArmed;
    double areaPxPrev = 0.0;
    // An armed object's place for this group: its base plus every crossed action's displacement, and with a scale or
    // rotate its box's half sizes and angle (written only then).
    template <class T, class H, class R>
    bool areaPlace(size_t i, T& cx, T& cy, H* hw = nullptr, H* hh = nullptr, R* rotv = nullptr) const {
        if (!g_areaMove || i >= areaArmed.size() || !areaArmed[i]) return false;
        const AreaPose p = areaPoseOf(areaOf[i], areaBaseX[i], areaBaseY[i], areaBaseHa[i], areaBaseHb[i],
                                      areaBaseRot[i], areaPxPrev);
        cx = (T)p.cx;
        cy = (T)p.cy;
        if (p.sized) {
            if (hw) *hw = (H)p.hw;
            if (hh) *hh = (H)p.hh;
            if (rotv) *rotv = (R)p.rot;
        }
        if (g_dynDbg == objs[i].uid)
            std::printf("areamove: uid=%d px(prev)=%.3f -> (%.3f,%.3f) half (%.3f,%.3f) rot %.2f\n", objs[i].uid,
                        areaPxPrev, p.cx, p.cy, p.hw, p.hh, p.rot);
        return true;
    }
    // --boxoff: the boxes (State::trig bits) whose firing takes this object out. The object keeps
    // its recording otherwise: an off box says nothing about what else switches it on or off, and
    // moving its objects onto onEv's reset snapshot dropped whatever the recording held -- kitref
    // on the custom survey turned 11 replays that agreed with the game into divergences, on
    // levels where the box was never entered.
    std::vector<std::vector<uint8_t>> offBox;
    // ---- touch-trigger control (see TouchTrig, below) ----
    // Which touch triggers move this object and by how much once they have.
    // Zero for every object in lv1-18, so all of this is dead weight there.
    std::vector<TouchMask> trigMask;
    // The recording's motion is NOT this object's touch chain (wrong direction
    // or far beyond its offset), so it is an autonomous fact of the recorded
    // worldline and plays for every state -- the switch band's ceiling: chain
    // says nudge +10, the recording descends -100. A door whose recording IS
    // its chain (lv19) stays touch-gated. Filled by the loader.
    std::vector<uint8_t> recAuto;
    // --moverextend / --moverextendvel (split). A mover whose last three rows are consecutive ticks of one
    // constant velocity AND end at the recording's own last tick (the attempt's death) is a candidate.
    //  * --moverextend (Move data): it goes on only if one of the object's own Move triggers (partsAny, linear
    //    ease 0) has exactly that per-tick step (dx/dur, dy/dur within 0.02), and only for the Move's REMAINING
    //    duration: dur ticks minus the length of the constant-velocity run that ends at the last row (the run
    //    began where the Move began). Past that it holds, as before.
    //  * --moverextendvel (fallback hypothesis, NOT backed by trigger data): terminal velocity without a cap.
    // extVx/extVy are per tick, extCap the number of ticks past the last row it may go on (0 = none).
    // Built once (prepareExtend).
    std::vector<uint8_t> extOk;
    std::vector<float> extVx, extVy;
    std::vector<int> extCap;
    void prepareExtend() {
        extOk.assign(objs.size(), 0);
        extVx.assign(objs.size(), 0.f);
        extVy.assign(objs.size(), 0.f);
        extCap.assign(objs.size(), 0);
        int maxT = -1;
        for (const auto& sm : samples)
            if (!sm.empty()) maxT = std::max(maxT, sm.back().t);
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto& sm = samples[i];
            if (sm.size() < 3 || sm.back().t != maxT) continue;
            // An object with its own autonomous Move parts has the completion arm of applyTriggers (and the
            // per-part increments past its last row); it tracked the truth to 0.1 px at +1 tick where this tail ran
            // 2-7 px fast (a custom level's uid 28010, measured). Only objects without autoParts are extended.
            if (i < autoParts.size() && !autoParts[i].empty()) continue;
            const DynSample& a = sm[sm.size() - 3];
            const DynSample& b = sm[sm.size() - 2];
            const DynSample& c = sm.back();
            if (b.t != c.t - 1 || a.t != b.t - 1) continue;
            const float v1x = c.cx - b.cx, v1y = c.cy - b.cy, v2x = b.cx - a.cx, v2y = b.cy - a.cy;
            if (std::fabs(v1x - v2x) > 0.02f || std::fabs(v1y - v2y) > 0.02f) continue;
            if (std::fabs(v1x) < 0.05f && std::fabs(v1y) < 0.05f) continue;
            // the run of consecutive ticks at this velocity that ends at the last row
            int run = 2;
            bool reachedFirstRow = true;   // the run goes back to the series' first row
            for (size_t k = sm.size() - 3; k > 0; --k) {
                if (sm[k].t != sm[k - 1].t + 1
                    || std::fabs((sm[k].cx - sm[k - 1].cx) - v1x) > 0.02f
                    || std::fabs((sm[k].cy - sm[k - 1].cy) - v1y) > 0.02f) { reachedFirstRow = false; break; }
                ++run;
            }
            // A run that goes back to THIS object's own first row has nothing before it that shows where the Move began
            // (a gap -- the object standing still -- or a different velocity would): the rows do not date the Move, so the
            // data form is not taken. Judged on the object's own first row, not the recording's first tick: an
            // object that first appears mid-Move at t=100 is just as unknown as one present from t=0.
            const bool startUnknown = reachedFirstRow;
            int cap = -1;   // -1 = no Move found
            if (g_moverExtend && i < partsAny.size()) {
                for (const AutoPart& p : partsAny[i]) {
                    if (p.ease != 0 || p.dur <= 0.0) continue;
                    if (std::fabs(p.dx / p.dur - v1x) > 0.02 || std::fabs(p.dy / p.dur - v1y) > 0.02) continue;
                    cap = std::max(cap, std::max(0, (int)std::lround(p.dur) - run));
                }
            }
            if (startUnknown && cap >= 0) cap = -1;   // the data form needs the Move's start
            if (cap < 0 && g_moverExtendVel) cap = 1 << 28;
            if (cap <= 0) continue;
            extOk[i] = 1; extVx[i] = v1x; extVy[i] = v1y; extCap[i] = cap;
        }
    }
    // --recendextend: the last step of a mover whose recording ends at the recording's own last tick, when its last
    // three rows are consecutive ticks of one steady step in position (0.02 px) and angle (0.01 deg). recV* are per
    // tick; hw/hh go on by their last step too (a turning box's rect changes with its angle). Built once.
    std::vector<uint8_t> recOk;
    std::vector<float> recVx, recVy, recVr, recVhw, recVhh;
    void prepareRecEnd() {
        const size_t n = objs.size();
        recOk.assign(n, 0);
        recVx.assign(n, 0.f); recVy.assign(n, 0.f); recVr.assign(n, 0.f);
        recVhw.assign(n, 0.f); recVhh.assign(n, 0.f);
        int maxT = -1;
        for (const auto& sm : samples)
            if (!sm.empty()) maxT = std::max(maxT, sm.back().t);
        for (size_t i = 0; i < samples.size() && i < n; ++i) {
            const auto& sm = samples[i];
            if (sm.size() < 3 || sm.back().t != maxT) continue;
            const DynSample& a = sm[sm.size() - 3];
            const DynSample& b = sm[sm.size() - 2];
            const DynSample& c = sm.back();
            if (b.t != c.t - 1 || a.t != b.t - 1) continue;
            const float vx = c.cx - b.cx, vy = c.cy - b.cy, vr = c.rot - b.rot;
            if (std::fabs(vx - (b.cx - a.cx)) > 0.02f || std::fabs(vy - (b.cy - a.cy)) > 0.02f
                || std::fabs(vr - (b.rot - a.rot)) > 0.01f)
                continue;
            if (std::fabs(vx) < 0.05f && std::fabs(vy) < 0.05f && std::fabs(vr) < 0.005f) continue;
            recOk[i] = 1; recVx[i] = vx; recVy[i] = vy; recVr[i] = vr;
            recVhw[i] = c.hw - b.hw; recVhh[i] = c.hh - b.hh;
        }
    }
    // The ticks (1..kRecEndExtendTicks) object i goes on past its last row at recording tick tr, 0 when it holds.
    int recEndTicks(size_t i, int tr) const {
        if (!g_recEndExtend || i >= recOk.size() || !recOk[i]) return 0;
        const auto& sm = samples[i];
        if (sm.empty() || tr <= sm.back().t) return 0;
        return std::min(tr - sm.back().t, kRecEndExtendTicks);
    }
    // --rottail (see g_rotTail): per object, its Rotates with the g_autoTrig entry and the centre's
    // position resolved, the object's own box (w, h at angle 0) for its bounding box at a new angle (0 when the
    // rows cannot give it), and the recording's last tick. Filled by the level loader.
    struct RotTail { int a; double cx, cy, total, durT, erate; int ease; uint8_t lock, own; };   // own: no centre
    std::vector<std::vector<RotTail>> rotTail;
    std::vector<float> rotBoxW, rotBoxH;
    int rotTailMaxT = -1;
    // --recendtail (g_recEndTail): an armed object's simulated placement from its last row on, the offset to the
    // last row already in it (x/y/r[k] is the recording's tick t0 + k), and the ticks its lock is open on the
    // track's clock. Empty x = not armed. Filled by the level loader.
    struct RigTail { int t0 = -1; int lockA = 1 << 30, lockB = -1; std::vector<float> x, y, r; };
    std::vector<RigTail> rigTail;
    // ...and each object's switches past its last row (OnEvent, x-crossing and touch alike), and whether any is a
    // touch one (the placement then depends on the group: not a pure object).
    std::vector<std::vector<OnEvent>> tailOnEv;
    std::vector<uint8_t> tailOnBox;
    int tailRecEnd = -1;   // the recording's last tick over every object (the switches start after it)
    // Turn object i's placement (cx, cy, rot) on from the last row (model tick tl) to model tick t; false when no
    // Rotate of its turned between the two.
    bool rotTailFrom(size_t i, int tl, int t, double& cx, double& cy, float& rot) const {
        bool moved = false;
        for (const RotTail& r : rotTail[i]) {
            const int F = g_autoTrig[(size_t)r.a].fireT;
            if (F < 0) continue;
            auto theta = [&](int tau) -> double {
                if (tau <= F) return 0.0;
                const double u = r.durT > 0.0 ? std::min(1.0, (double)(tau - F) / r.durT) : 1.0;
                return r.total * gdEase(r.ease, r.erate, u);
            };
            const double d = theta(t) - theta(tl);
            if (d == 0.0) continue;
            // GD's angle runs clockwise; in the y-up frame that is a turn by -d.
            if (!r.own) {
                const double a = -d * (3.14159265358979 / 180.0);
                const double c = std::cos(a), s = std::sin(a);
                const double rx = cx - r.cx, ry = cy - r.cy;
                cx = r.cx + c * rx - s * ry;
                cy = r.cy + s * rx + c * ry;
            }
            if (!r.lock) rot += (float)d;
            moved = true;
        }
        return moved;
    }
    // The turned-box axis at angle rotDeg (the --dynobbrot rule, for a placement that moved the angle).
    void recEndAxis(size_t i, double rotDeg) {
        // --rotobblatch: a placement that turns it past the recording latches it (and the latch stays)
        if (i < obbLatchT.size() && obbLatchT[i] >= 0 && !objs[i].obbOk) {
            double d = std::fmod(std::fabs(rotDeg - objs[i].rot), 360.0);
            if (d > 180.0) d = 360.0 - d;
            if (d > 0.001) objs[i].obbOk = 1;
        }
        if (!g_dynObbRot || !objs[i].obbOk || i >= everRot.size() || !everRot[i] || i >= obbBc0.size()) return;
        const double a = -(rotDeg - objs[i].rot) * (3.14159265358979 / 180.0);
        const double ca = std::cos(a), sa = std::sin(a);
        objs[i].bc = obbBc0[i] * ca - obbBs0[i] * sa;
        objs[i].bs = obbBc0[i] * sa + obbBs0[i] * ca;
    }    std::vector<float> trigDx, trigDy;
    std::vector<double> trigDur;
    std::vector<int> trigEase;
    std::vector<double> trigErate;
    // The tick this object FIRST moved in the recording (-1 = it never did).
    // That is the anchor the recorded trajectory is re-timed against, so a
    // frontier group that fired the trigger at a different tick than the run
    // that recorded it still sees the right shape at the right offset.
    // Shared by the touch and the autonomous machinery.
    std::vector<int> trigRecFire;
    // --touchretimebox: per touch bit, the tick the RECORDING entered that box, estimated from
    // the earliest first motion among the objects only it moves (-1 = none; empty = flag off).
    // Filled by the loader.
    std::vector<int> boxRecEntry;
    // ...and how many ticks late that FIRST ROW is (see recordLag). The true
    // start is trigRecFire - recLag, which is the tick the analytic curve and
    // the re-timing shift are both written against.
    std::vector<int> recLag;
    // ...and the lag of the AUTONOMOUS move alone, for an object that also has a
    // touch move (formula-driven): its recLag is the touch move's, which is the
    // wrong curve for dating the autonomous trigger. Equal to recLag for every
    // other object. Read unless --no-automixlag.
    std::vector<int> autoLag;
    // Does the closed form reproduce this object's recording? Decided per
    // object at load by replaying the formula against every recorded sample
    // (see the check in loadLevel). Only consulted under --trigclosed.
    std::vector<uint8_t> autoClosed;
    // ---- autonomous-trigger control (see AutoTrig, above) ----
    // Which g_autoTrig entry anchors this object's re-timing (-1 = none).
    // An index, not a mask: the fire tick is a level property so no state
    // carries it, and lv21 has 174 autonomous moves (a 32-bit mask cannot).
    // An object moved by several autonomous triggers is anchored on the
    // EARLIEST-crossed one -- its recFire is that controller's first effect,
    // and the later controllers ride the same shift (their relative timing is
    // embedded in the recording).
    std::vector<int> autoAnchor;
    // --anchornoop: 1 = autoAnchor's Toggle switches this object and moves it nowhere, and the
    // recording's first row already holds the state it switches to, so no recorded change is the
    // anchor's (see g_anchorNoop). Filled by the loader for every object; read only under the flag.
    std::vector<uint8_t> anchorUnrec;
    // --recanchorx: the recording run's own fire tick of autoAnchor (its crossing on --xtrack plus the
    // delay) where the recording's first change came BEFORE that crossing, else -1. Filled by the loader.
    std::vector<int> recAnchorX;
    // "This object's touch entry moves it nowhere (dx=dy=0) but it ALSO has a
    // real autonomous mover." Deliberately a SEPARATE flag rather than widening
    // autoAnchor: autoAnchor is read by :632, :710, :853 and cli.hpp:1652/:1981,
    // and widening it would drag the ownTouch punch-freeze (:853) and the
    // anchor-seeding rf scan (cli.hpp:1652, both the ahead branch at :1670 and
    // the behind branch at :1718) along with it -- both were measured as
    // unintended effects of the wider forms. Read at the fire gate below and
    // NOWHERE else.
    std::vector<uint8_t> recSelfFire;
    // Also reached by an autonomous controller (level_loader.hpp). Read only by
    // --touchretime, which leaves such objects on the recording's clock.
    std::vector<uint8_t> autoReach;
    std::vector<float> autoDx, autoDy;   // summed final offset (ease fallback)
    std::vector<double> autoDur;
    std::vector<int> autoEase;
    std::vector<double> autoErate;
    // Every controller's move kept SEPARATELY, because they superpose: each one
    // starts at its own crossing and runs its own curve, and the object is the
    // sum. Adding the offsets up under one anchor is only right when there is
    // one controller -- lv20 has objects under five, and the sum-under-one-
    // anchor form was off by hundreds of px on those. Measured on the lv20
    // bootstrap: the superposition matches all 384 multi-controller objects to
    // 0.002 px (py/trigger_curve_fit.py's sibling check).
    struct AutoPart {
        int trig; float dx, dy; double dur; int ease; double erate;
        // --movetarget only: see TrigCtl's fields of the same names.
        int mover = 0;
        uint8_t tmode = 0;
        float tdx = 0.f, tdy = 0.f;
        // --spawnlag: TrigCtl::spawnLag, the smallest over the paths merged into this part.
        int lag = 0;
    };
    std::vector<std::vector<AutoPart>> autoParts;
    std::vector<std::vector<AutoPart>> partsAny;   // --moverextend: every Move the object's groups listen to (loader)
    // Was this object's recording REPLACED by the computed orbit (g_rotSplit)?
    // The split stores `entry_C + R(theta)*rel` and drops `C(t) - entry_C`
    // -- level_loader's own words, "recorded = orbit + the centre's own
    // translation", with the translation subtracted out at the fit check. The
    // only path that ever added it back sits behind g_rotCompute, which is
    // false and which no flag can set, so for these objects the translation
    // was silently lost. They share their move controllers with their centre
    // (the split refuses them otherwise), so their own autoParts IS that
    // translation; this marks them so the recording arm can add it.
    std::vector<uint8_t> rotSplit;
    // The TOUCH chains kept per box (trig = the box's State::trig bit), where
    // trigDx/trigDy above are their sum. The sum is enough for a door -- one
    // box, one move -- and undecidable for the switch band, where YELLOW boxes
    // (+30 up) and RED boxes (-840 down) drive the same spikes: every per-box
    // question (which way does THIS punch push? is the recorded descent any
    // single box's chain?) has no answer in the summed form. Filled only for
    // touch-controlled objects; empty everywhere lv1-18.
    std::vector<std::vector<AutoPart>> touchParts;
    // lockToPlayerX window in ticks (0 = none) and the g_autoTrig entry that
    // opened it -- NOT necessarily the object's anchor. See TrigCtl::lockTicks.
    std::vector<double> autoLock;
    std::vector<int> autoLockTrig;
    // --lockstop: the Move that locks the object (its ctl entry's mover, else the entry's own
    // uid), the one a touch chain's Stop has to name. Filled at the first placement under the flag.
    std::vector<int> lockMover;
    void prepareLockMover() {
        lockMover.assign(objs.size(), 0);
        for (size_t i = 0; i < objs.size() && i < autoLockTrig.size(); ++i) {
            const int k = autoLockTrig[i];
            if (k < 0 || (size_t)k >= g_autoTrig.size()) continue;
            const AutoTrig& A = g_autoTrig[(size_t)k];
            int mv = A.uid;
            for (const TrigCtl& c : A.ctl)
                if (c.uid == objs[i].uid && c.lockTicks > 0.0 && c.mover != 0) { mv = c.mover; break; }
            lockMover[i] = mv;
        }
    }
    // ...and the touch side. Non-zero means "while this object's box has been
    // punched and the window is open, this object's x IS the player's x,
    // offset by wherever both were when it fired" -- so its x is a property of
    // the STATE, not of the level, and a recording of it is one attempt's
    // player path. dynamics' own note on TrigCtl::lockTicks calls this "the one
    // class where a RECORDING is provably wrong for anyone else".
    std::vector<double> touchLock;
    // --lockx: no autonomous Move moves this object in x but the lock, so its x
    // is the lock's alone and the recording's x can be replaced, not corrected.
    bool autoLockXOnly(size_t i) const {
        if (i >= autoParts.size()) return true;
        for (const AutoPart& p : autoParts[i])
            if (std::fabs(p.dx) > 0.001f) return false;
        return true;
    }
    bool anyTrig = false;
    bool anyAuto = false;
    int lastT = -1;

    size_t size() const { return objs.size(); }

    // ---- caches that change nothing but the time taken ----------------------
    // applyTriggers runs once per frontier GROUP, and a tick can have many
    // groups. For most moving objects the placement it computes does not depend
    // on the group at all: an object no touch box reaches, that is not
    // formula-driven and not locked to the player, is placed from seek()'s state
    // for this tick, its own static data and the level-wide autonomous clocks
    // (AutoTrig::fireT) -- nothing the group carries. Those "pure" objects are
    // placed once and left alone until one of the three changes. Profiled on a
    // level with 868 moving objects: applyTriggers was 9.5% of the main thread,
    // which is the critical path (the workers are idle ~70% of the time).
    uint64_t seekEpoch = 0;              // bumped by every seek() that rewrites
    uint64_t pureEpoch = ~0ull;          // what the pure objects were placed for:
    int pureT = -1;                      //   seek epoch, tick,
    std::vector<int> pureFire;           //   and every AutoTrig::fireT
    bool isPure(size_t i) const {
        return !trigMask[i]
            && !(i < onEv.size() && !onEv[i].empty())   // --activators: depends on the group
            && !(i < formula.size() && formula[i])
            && !(i < autoLockTrig.size() && autoLockTrig[i] >= 0)
            // --recendtail: the lock reads the group's x, a touch switch its boxes
            && !(i < rigTail.size() && rigTail[i].lockB >= 0)
            && !(i < tailOnBox.size() && tailOnBox[i])
            // --areamove: placed from the group's x
            && !(i < areaArmed.size() && areaArmed[i])
            // --tapdisarm: the on/off reads autoDisarmed, which asks the group's boxes
            && !(i < disarmDep.size() && disarmDep[i])
            // --boxarm: a Move of it fires only where the group's boxes switched it back on
            && !(i < armDep.size() && armDep[i]);
    }
    // --boxarm: a part, anchor or lock of the object is an offInit entry (AutoTrig::offInit), so where it is
    // placed depends on the state's boxes. Filled by the CLI once the level is loaded, under the flag only.
    std::vector<uint8_t> armDep;
    // collect() walks one bucket's objects instead of all of them. Built in
    // seek(), which runs on the main thread between the parallel phases:
    // collect() is also called from step() on the worker threads, so it must
    // never build this itself. Until it exists collect() takes the full walk.
    std::vector<std::vector<uint32_t>> byBucket;
    size_t byBucketN = (size_t)-1;
    bool anyUidM1 = false;   // an object --dyndbg's default of -1 would name
    size_t nNonPure = 0;     // objects a same-tick applyTriggers still places
    void rebuildBuckets() {
        byBucket.assign(8, {});
        for (size_t i = 0; i < objs.size() && i < bucket.size(); ++i)
            if (bucket[i] < byBucket.size())
                byBucket[bucket[i]].push_back((uint32_t)i);
        anyUidM1 = false;
        for (const Obj& o : objs) anyUidM1 |= (o.uid == -1);
        nNonPure = 0;
        for (size_t i = 0; i < objs.size(); ++i) nNonPure += !isPure(i);
        byBucketN = objs.size();
    }
    // The per-object loops of seek() and applyTriggers() split across the
    // search's pool (g_dynParTasks) in runs of kParRun objects. body(b, e)
    // handles objects [b, e). Only for a loop whose body writes nothing but its
    // own object's slots -- then every object is placed by the same arithmetic
    // whichever thread runs it, and the result is the serial loop's. `work` is
    // how many objects the call will actually place -- a second group of the
    // same tick places only the non-pure ones, usually a handful, and the pool
    // is not woken for a handful. Below kParMin the loop stays on this thread.
    // 512 against 2,048: no slower on levels with 868 and 1,063 moving objects,
    // and faster on one with 4,350 (77.5 s against 80.7).
    static constexpr size_t kParMin = 512, kParRun = 64;
    template <class F> void forRuns(bool par, size_t work, F&& body) {
        const size_t n = objs.size();
        if (!par || !g_dynParTasks || work < kParMin) { body((size_t)0, n); return; }
        g_dynParTasks((n + kParRun - 1) / kParRun, [&](size_t k) {
            body(k * kParRun, std::min(n, (k + 1) * kParRun));
        });
    }

    // Place the trigger-controlled objects for a frontier group whose states
    // have touched the triggers in `mask`, `fireHi` being the latest tick any of
    // them fired (-1 = none).
    //
    // Called right after seek(), and it has to be IDEMPOTENT: seek() early-
    // returns when the tick has not changed, so the second group of the same
    // layer would otherwise stack its offset on top of the first group's. It
    // rebuilds each rect from the object's OWN first sample instead of from
    // whatever is currently in objs[i].
    //
    // The first sample and not the current one, deliberately. Once the driver
    // replays a plan that DOES touch the trigger, grouptrace records the door
    // sliding open -- and that recording would then be applied to every state,
    // including the ones that never touched anything. The whole point is that
    // the two worlds differ, so for these objects the recording is ignored and
    // the mask is the only thing that moves them.
    //
    // When the bit IS set the recording is replayed, re-timed so that its first
    // motion lands on this group's firing tick. That is the whole point of not
    // implementing trigger semantics: lv19's third touch trigger is a "move to
    // target" lift, not a simple offset, and m_moveOffset for it is (0,0) --
    // GD's own trace has the ball riding it grounded from x=28,783 to x=29,001
    // across what is otherwise a spike floor. No closed form was going to
    // produce that; the replay already contains it.
    //
    // Only when there is no recording yet (the first iteration, before any plan
    // has touched the box) does it fall back to the static offset, eased. That
    // fallback is no longer an approximation: the curve is GD's own easing by
    // type and rate (gdEase), the duration is the dump's dur*240 unrounded, and
    // the origin is the true first-motion tick -- 0.004 px against 655 recorded
    // objects. It used to be 0.5-0.5*cos(pi*u) over a rounded duration, which
    // was within a few percent of EaseInOut and up to 4.5 px off on lv20's
    // 240 px doors. A union of start and end rect was tried before that and is
    // wrong in BOTH directions -- conservative for a door that opens a gap,
    // optimistic for a platform that rises under your feet.
    // `px` is where the player is THIS tick (the group's leading x during the
    // search, the single trajectory's x in a replay). Only the lockToPlayer
    // term reads it.
    // `fireB` is the group's per-box firing ticks (State::fireB, aggregated the
    // same conservative way `fireHi` is: the LATEST tick in the group). Only
    // the formula path reads it; it may be null, and then no object is
    // formula-driven for this call.
    // `lockOff` is how far the state has travelled since it punched the locked
    // box (State::lockOff). Applied only on the formula path: a recording of a
    // locked object already contains the recorded run's own lock, so adding
    // this to it would count the ride twice.
    void applyTriggers(TouchMask mask, int fireHi, const uint16_t* fireB,
                       float lockOff, int t, double px = 0.0) {
        if (!anyTrig && !anyAuto) return;
        // (serial caller: the per-object loops below are the parallel part)
        if (g_lockStop && lockMover.size() != objs.size()) prepareLockMover();
        // Skip the pure objects when they are already placed for exactly this
        // (seek epoch, tick, autonomous clocks). Never with a print flag on: those
        // print per call, and skipping would change what they say.
        const bool quiet = g_dynDbg == -1 && g_shiftDbgUid < 0 && !g_shiftStat
                           && !(g_rotCompute && !g_rotSpec.empty());
        bool samePure = quiet && pureEpoch == seekEpoch && pureT == t
                        && pureFire.size() == g_autoTrig.size();
        for (size_t k = 0; samePure && k < g_autoTrig.size(); ++k)
            samePure = (pureFire[k] == g_autoTrig[k].fireT);
        if (!samePure) {
            pureEpoch = seekEpoch;
            pureT = t;
            pureFire.resize(g_autoTrig.size());
            for (size_t k = 0; k < g_autoTrig.size(); ++k)
                pureFire[k] = g_autoTrig[k].fireT;
        }
        // With no print flag on, the only thing an object's placement writes
        // outside its own slots is AutoTrig::lockLastX (the lockToPlayerX
        // branch), and nothing but that branch reads it. So the objects that
        // can take it (autoLockTrig >= 0) are placed afterwards, in index order
        // on this thread, and every other object is placed in runs across the
        // pool (forRuns). The rotation branch, which reads another object, runs
        // only under g_rotCompute, and `quiet` excludes that too.
        const bool par = quiet && !anyUidM1 && byBucketN == objs.size();
        auto place = [&](size_t b, size_t e, bool lockPass) {
        for (size_t i = b; i < e; ++i) {
            if (samePure && isPure(i)) continue;
            if (par && lockPass != (i < autoLockTrig.size() && autoLockTrig[i] >= 0))
                continue;
            const TouchMask m = trigMask[i];
            // FORMULA-DRIVEN OBJECTS LEAVE HERE, before anything reads a
            // sample. `fireHi` cannot serve them: it is the latest box THE
            // STATE entered, which need not be a box that reaches THIS object,
            // so two doors opened by two different boxes would be handed the
            // same tick. The fit wants 20,504 for one and 20,591 for the
            // other. What this object needs is the latest tick among the boxes
            // that both reach it and the state has entered -- which is exactly
            // what the per-box array gives, and is why this path could not be
            // written before State carried one.
            // Deliberately NOT conditioned on `fireB` being non-null: which
            // path an object takes is a property of the object, and making it
            // depend on a pointer would let a caller that forgot the array
            // silently put a formula-driven object back on the recording path
            // -- with the autonomous fields the loader filled in for the
            // formula still set, which is a combination that exists nowhere
            // else. A null array simply means no box has been entered.
            // --boxparts (formula 2) only while the recording holds no motion of this object: where it
            // does, the recording path's re-timing is the measured one (lv22 uid2748, +9 px over 48 ticks
            // from t=2,745, is k^2/128 there and 16% short in the part formula).
            if (i < formula.size() && formula[i]
                && !(formula[i] == 2 && trigRecFire[i] >= 0)
                && !samples[i].empty()) {
                // --formularec: every box that moves it as the attempt left it -> seek's position
                // (the recording at t, unshifted) stands. Delta 0 only: a box entered a tick apart
                // takes the formula.
                // --formulanolock: the formula has no lock term, so such an object reads the recording
                // whichever attempt left it (as far as it reaches) -- the same read as an own attempt's
                const long long lockReach = (g_recProvGiven && g_recProvRec >= 0) ? (long long)g_recProvReach
                                                                                  : (long long)samples[i].back().t;
                const bool lockObj = g_formulaNoLock && i < formulaLock.size() && formulaLock[i];
                const bool lockRead = lockObj && t <= lockReach;
                if ((g_formulaRec && formulaRecProvOk(t)) || lockRead) {
                    bool agree = true;
                    for (int b = 0; b < kTouchBits && agree; ++b) {
                        if (!(m & touchBit(b))) continue;
                        const int rec = g_touchRecEntry[(size_t)b];
                        const bool st = fireB && (mask & touchBit(b));
                        if (st) agree = rec >= 0 && (int)fireB[b] == rec;
                        // --formularectouch: an entry dated t shows in the state's mask from t + 1 (the
                        // placement for t runs before t's collision pass), so the attempt's counts from
                        // then too. Without the flag the test is the old one.
                        else agree = !(rec >= 0 && (g_formulaRecTouch ? rec < t : rec <= t));
                    }
                    if (agree) {
                        // Written here, not left to seek: another group placed this object on
                        // the same tick before (pureEpoch), and its formula value would stand.
                        // seek's read: the last row at or before tt, linear across a recorder gap
                        // of at most kRecInterpGap (--recinterp).
                        const auto& sm = samples[i];
                        auto recAt = [&](int tt, double& rx, double& ry) {
                            size_t lo = 0, hi = sm.size();
                            while (lo + 1 < hi) {
                                const size_t md = (lo + hi) / 2;
                                if (sm[md].t <= tt) lo = md; else hi = md;
                            }
                            const DynSample& s = sm[lo];
                            rx = s.cx; ry = s.cy;
                            if (s.t < tt && lo + 1 < sm.size() && sm[lo + 1].t - s.t <= kRecInterpGap
                                && !recStallGap(sm, lo)) {
                                const DynSample& n = sm[lo + 1];
                                const double u = (double)(tt - s.t) / (double)(n.t - s.t);
                                rx = (double)s.cx + u * ((double)n.cx - (double)s.cx);
                                ry = (double)s.cy + u * ((double)n.cy - (double)s.cy);
                            }
                        };
                        double rx = 0.0, ry = 0.0, px = 0.0, py = 0.0;
                        // ...a locked one a row behind: its lock moves it after the tick's collision pass, so
                        // the pass meets it where the row before left it (a mini swing riding the block row of
                        // --formulanolock's note sits 1 row of the block's descent above the unshifted read)
                        const int lag = lockObj ? 1 : 0;
                        recAt(t - lag, rx, ry);
                        recAt(t - 1 - lag, px, py);
                        objs[i].cx = (float)rx;
                        objs[i].cy = (float)ry;
                        objs[i].dcy = ry - py;
                        if (g_dynDbg == objs[i].uid || g_dynDbg == -3)
                            std::printf("frec: uid=%d t=%d cx=%.3f cy=%.4f dcy=%+.4f\n",
                                        objs[i].uid, t, rx, ry, ry - py);
                        g_formulaRecHit.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }
                    g_formulaRecMiss.fetch_add(1, std::memory_order_relaxed);
                }
                const DynSample& s0 = samples[i][0];
                int fb = -1;
                if (fireB)
                    for (int b = 0; b < kTouchBits; ++b)
                        if ((m & mask) & touchBit(b))
                            fb = std::max(fb, (int)fireB[b]);
                const int aa0 = autoAnchor[i];
                const int ft0 = (aa0 >= 0 && (size_t)aa0 < g_autoTrig.size())
                                ? autoFireT(aa0, mask, fireB) : -1;   // --boxarm
                // The offset at an arbitrary tick. Being a closed form is the
                // point: it answers for t-1 as readily as for t, which is what
                // makes dcy exact here even across an anchor jump, where the
                // recording-driven branch has to fall back to two consecutive
                // recorded rows and reads 0 when the recorder skipped one.
                // --movetarget: the touch side PER BOX, each part at its own
                // box's firing tick (the sum above answers one tick for all of
                // them, which is undecidable once a Stop or a target-mode Move
                // is in play); a Stop freezes the Moves it names at the tick
                // its box fires; a target-mode part's distance is decided on
                // the tick it starts. The group's position is read off this
                // object's own offset, which is right when the object and the
                // target's reference point share their controllers (lv22's
                // ceiling group 265 does: uid18094 is one of its members).
                auto stopTick = [&](int mover) -> int {
                    int st = 1 << 30;
                    if (mover == 0 || !fireB) return st;
                    const int nb = std::min((int)g_touchStops.size(), kTouchBits);
                    for (int b = 0; b < nb; ++b) {
                        if (!(mask & touchBit(b))) continue;
                        // --spawnlag: the Move's last step is the fire tick's own (a touch
                        // Stop fires in checkCollisions, after that tick's move step), so it
                        // freezes at f, not f + 1. lv22 it8: box uid18091 (entered at 2730)
                        // stops the spike row's -480 descent, and one step too many put the row
                        // 0.4 px below the game until the replay died at 2775.
                        for (const int s : g_touchStops[(size_t)b])
                            if (s == mover)
                                st = std::min(st, (int)fireB[b] + (g_spawnLag ? 0 : 1));
                    }
                    return st;
                };
                auto plainAt = [&](int tt, double& ox, double& oy) {
                    ox = 0.0; oy = 0.0;
                    // A MOVE OF DURATION 0 IS DONE ON ITS OWN TICK: its eased
                    // fraction is 1, and the `/dur` guard that kept it out of
                    // the sum dropped the move altogether. SubZero 4002's group
                    // 87 falls 600 px that way (trigger uid18255 at x=31,899,
                    // dur 0), so every formula-driven member of it stayed where
                    // it started: the saw uid16730 was tested at cy=2,303 while
                    // the recording -- and GD -- have it at 1,703, and the model
                    // walked through the kill GD makes at t=22,893 (ledger
                    // 2026-09-23 06:5x). The recording-driven members were right
                    // all along, which is why this showed up as one object
                    // disagreeing with its own group.
                    if (fireB)
                        for (const AutoPart& p : touchParts[i]) {
                            if (p.tmode) continue;
                            if (!((m & mask) & touchBit(p.trig))) continue;
                            // --spawnlag: a queued Spawn on the way starts it later
                            const int f = (int)fireB[p.trig] + (g_spawnLag ? p.lag : 0);
                            const int te = std::min(tt, stopTick(p.mover));
                            if (te <= f) continue;
                            const double e = p.dur > 0.0
                                ? gdEase(p.ease, p.erate,
                                         (double)(te - (f + 1)) / p.dur)
                                : 1.0;
                            ox += p.dx * e; oy += p.dy * e;
                        }
                    if (ft0 >= 0) {
                        int te = std::min(tt, stopTick(g_autoTrig[(size_t)aa0].uid));
                        if (g_autoStop) te = std::min(te, autoStopT(aa0));   // --autostop
                        if (te >= ft0) {
                            const double e = autoDur[i] > 0.0
                                ? gdEase(autoEase[i], autoErate[i],
                                         (double)(te - ft0) / autoDur[i])
                                : instantE(te, ft0);   // --instantlag
                            ox += autoDx[i] * e; oy += autoDy[i] * e;
                        }
                    }
                };
                auto targetAt = [&](int tt, double& ox, double& oy) {
                    plainAt(tt, ox, oy);
                    if (!fireB) return;
                    struct TP { int f; const AutoPart* p; double ddx, ddy; };
                    TP tp[8];
                    int n = 0;
                    for (const AutoPart& p : touchParts[i])
                        if (p.tmode && ((m & mask) & touchBit(p.trig)) && n < 8)
                            tp[n++] = {(int)fireB[p.trig] + (g_spawnLag ? p.lag : 0), &p,
                                       0.0, 0.0};
                    std::sort(tp, tp + n, [](const TP& a, const TP& b) { return a.f < b.f; });
                    auto contrib = [&](const TP& q, int at, double& cx, double& cy) {
                        cx = 0.0; cy = 0.0;
                        if (at <= q.f || q.p->dur <= 0.0) return;
                        const double e = gdEase(q.p->ease, q.p->erate,
                                                (double)(at - (q.f + 1)) / q.p->dur);
                        cx = q.ddx * e; cy = q.ddy * e;
                    };
                    for (int k = 0; k < n; ++k) {
                        // The tick whose position it reads. Without --spawnlag the tick it
                        // starts (f + 1). With it, the fire tick f itself: GD samples the group
                        // when the Move-To fires -- after that tick's move step for a touch
                        // (checkCollisions), before the next one for a queued spawn
                        // (updateSpawnTriggers) -- which is the position at the END of tick f.
                        // lv22 it8 (box uid18091 entered at 2730, the spike row still rising
                        // under a yellow box from 2686): read at f + 1 the row lands 0.399 px
                        // below the game for the rest of the move; the old reading matched
                        // only because the unlagged yellow part ran one tick early.
                        const int s = tp[k].f + (g_spawnLag ? 0 : 1);
                        double bx = 0.0, by = 0.0;
                        plainAt(s, bx, by);
                        for (int j = 0; j < k; ++j) {
                            double cx, cy;
                            contrib(tp[j], s, cx, cy);
                            bx += cx; by += cy;
                        }
                        const int md = tp[k].p->tmode;   // 1 both, 2 x only, 3 y only
                        tp[k].ddx = (md == 3) ? 0.0 : (double)tp[k].p->tdx - bx;
                        tp[k].ddy = (md == 2) ? 0.0 : (double)tp[k].p->tdy - by;
                    }
                    for (int k = 0; k < n; ++k) {
                        double cx, cy;
                        contrib(tp[k], tt, cx, cy);
                        ox += cx; oy += cy;
                    }
                };
                const bool perBox = !touchParts[i].empty();
                auto offsetAt = [&](int tt, double& ox, double& oy) {
                    if (perBox) { targetAt(tt, ox, oy); return; }
                    ox = 0.0; oy = 0.0;
                    // dur 0 = done on its own tick (the superposition arm below
                    // already reads it that way); see the note in plainAt
                    if (fb >= 0 && tt > fb) {
                        const double e = trigDur[i] > 0.0
                            ? gdEase(trigEase[i], trigErate[i],
                                     (double)(tt - (fb + 1)) / trigDur[i])
                            : 1.0;
                        ox += trigDx[i] * e; oy += trigDy[i] * e;
                    }
                    // NO +1 on this side. The two ticks mean different things:
                    // `fireB` is the tick the box was ENTERED and the move
                    // starts the tick after, while `AutoTrig::fireT` is already
                    // the EFFECT tick (`fireT = crossing + delay`, cli.hpp), so
                    // the move starts on it. The recording-driven branch says
                    // the same by setting `lat = 0` for the autonomous case and
                    // 5 for the touch case. With a spurious +1 lv19's door sits
                    // 0.0615 px above the recording at t=20,601.
                    if (ft0 >= 0 && tt >= ft0) {
                        const int te = g_autoStop ? std::min(tt, autoStopT(aa0)) : tt;
                        const double e = autoDur[i] > 0.0
                            ? gdEase(autoEase[i], autoErate[i],
                                     (double)(te - ft0) / autoDur[i])
                            : instantE(te, ft0);   // --instantlag
                        if (te >= ft0) { ox += autoDx[i] * e; oy += autoDy[i] * e; }
                    }
                };
                double fx = 0.0, fy = 0.0, px1 = 0.0, py1 = 0.0;
                offsetAt(t, fx, fy);
                offsetAt(t - 1, px1, py1);
                if (g_dynDbg == objs[i].uid)
                    std::printf("formdbg t=%d uid=%d perBox=%d fb=%d ft0=%d "
                                "autoDur=%.2f autoDy=%.1f trigDur=%.2f fy=%.3f "
                                "base=%.3f -> %.3f\n",
                                t, objs[i].uid, perBox ? 1 : 0, fb, ft0,
                                autoDur[i], autoDy[i], trigDur[i], fy,
                                (double)s0.cy, (double)s0.cy + fy);
                const double ncy = (double)s0.cy + fy;
                // ...and the face's PER-TICK motion, from the same closed form.
                // Without it the object teleports as far as anything standing
                // on it is concerned: the support tolerance is
                // `kSupportTol + |dcy|`, so a rider on a descending face keeps
                // its footing only while dcy says the face moved. This IS lv19
                // t=20,601 -- a ball riding uid14011 down at 0.062 px/tick --
                // and with dcy at zero the model drops the support on the first
                // tick, falls, and dies 45 ticks later on a hazard the ride
                // would have carried it over.
                //
                // NOT gated on `lastStep1`, unlike the branch below. That gate
                // exists because a position difference across an anchor jump is
                // the platform's whole journey rather than one tick; a formula
                // evaluated at t-1 is one tick by construction, so the case the
                // gate protects against cannot arise -- and the first tick
                // after an anchor, which is exactly where this object is
                // ridden, is not a 1-tick advance.
                objs[i].dcy = fy - py1;
                // x: the moves, plus the lock. The 158 px that `s0.cx + fx`
                // could not account for (uid14011 is recorded at cx=28,789 at
                // t=20,601 against a base of 28,631, with dx=0 and adx=0 on both
                // controllers) is a lockToPlayerX -- the recording shows its x
                // advancing by the player's own dx, from the tick the box fires.
                // With the lock the closed form covers x as well, so this writes
                // it; without one it stays on the recording, which is where the
                // objects the formula cannot fully describe belong.
                const bool locked = (g_lockBox >= 0 && i < touchLock.size()
                                     && touchLock[i] > 0.0
                                     && mask.test(g_lockBox));
                if (locked) objs[i].cx = (double)s0.cx + fx + (double)lockOff;
                // --boxparts (formula 2): the parts ARE this object's whole motion, so x as well.
                // In a turned frame (lv22's rotated section) a world-y dip is a frame-x move, and
                // writing y alone left the copy in frames 1 and 3 where it started.
                else if (formula[i] == 2) objs[i].cx = (double)s0.cx + fx;
                objs[i].cy = ncy;
                // ...and a ramp's ends with it, as the recording path does below: the surface is
                // sy0/sy1, and moving cy alone left lv22's slope uid 6949 (group 510) where it started,
                // 3.3 px above GD's at t=6,652, so the ball landed a tick early (quick lv22 t=6,600).
                if (formula[i] == 2 && objs[i].slope) {
                    const double dy = ncy - (double)baseCy[i];
                    objs[i].sy0 = (double)baseSy0[i] + dy;
                    objs[i].sy1 = (double)baseSy1[i] + dy;
                }
                // --dyndbg <uid> follows one object; --dyndbg -3 prints EVERY
                // formula-driven object, which is what you want when the set
                // shares a group and a position (lv19's seven doors all sit at
                // the same cy, so following one says nothing about which of
                // them the player is actually standing on).
                if (objs[i].uid == g_dynDbg || g_dynDbg == -3)
                    std::printf("fdrv: uid=%d t=%d box=%d autoFireT=%d "
                                "base=%.3f lock=%d off=%.3f cx=%.3f "
                                "fy=%+.4f dcy=%+.4f -> cy=%.4f\n",
                                objs[i].uid, t, fb, ft0, (double)s0.cy,
                                locked ? 1 : 0, (double)lockOff, objs[i].cx,
                                fy, fy - py1, ncy);
                continue;
            }
            const int aA = m ? -1 : autoAnchor[i];   // touch control wins
            // --areamove: an armed object no trigger controls keeps what seek() read (size, angle, on) and is moved
            // by the area pass from this group's x.
            if (!m && aA < 0) {
                double ahw = objs[i].hw, ahh = objs[i].hh, arot = std::nan("");
                if (areaPlace(i, objs[i].cx, objs[i].cy, &ahw, &ahh, &arot)) {
                    if (!std::isnan(arot)) {   // a scale or rotate acts on it
                        objs[i].hw = ahw;
                        objs[i].hh = ahh;
                        recEndAxis(i, arot);
                    }
                    continue;
                }
            }
            if ((!m && aA < 0) || samples[i].empty()) continue;
            const auto& sm = samples[i];
            const DynSample& s0 = sm[0];
            double cx = s0.cx, cy = s0.cy, hw = s0.hw, hh = s0.hh;
            uint8_t onv = s0.on;
            float rotv = s0.rot;   // GD's own getRotation (see turnedBox)
            // --recendresid: the placement the parts tail left from the last row (tailCx, tailCy), and what its
            // parts took of the recording's own last step (tailLastDx, tailLastDy)
            bool partsTail = false;
            double tailCx = 0.0, tailCy = 0.0, tailLastDx = 0.0, tailLastDy = 0.0;
            bool rigTailed = false;   // --recendtail placed it past the last row (the tails below stand down)
            // Who controls this object this tick, where its first motion is
            // anchored, and the analytic fallback's offset/duration/curve.
            // Touch: fired = this GROUP's mask bit, anchor = the group's entry
            // tick (fireHi), 5 ticks of box->motion latency (measured, lv19
            // door), and the recording is re-timed against the entry tick as it
            // always was. Auto: fired = the level-wide fireT is resolved AND
            // already reached, anchor = fireT, which IS the true first-motion
            // tick -- so the recording is re-timed against its OWN true start,
            // recFire minus the recorder's threshold lag.
            bool fired; int anchor, recAnchor; float fdx, fdy;
            double fdur; int lat, fease; double ferate;
            // The re-timing this object ends up with, for --shiftstat. 0 means
            // the re-timing branch was not taken at all, which is the same thing
            // as "reads the recording on its own clock".
            int shiftUsed = 0;
            if (m) {
                fired = (mask & m).any();
                anchor = fireHi; recAnchor = trigRecFire[i];
                fdx = trigDx[i]; fdy = trigDy[i];
                fdur = trigDur[i]; lat = 5;
                fease = trigEase[i]; ferate = trigErate[i];
                // ...but `fireHi` is ONE scalar for the whole state (trigT is
                // the tick of the LAST box that state entered), while this
                // object may belong to a different door entirely. Every extra
                // box a branch touches re-dated every door it had already
                // opened. When this object HAS a recording, its own first-motion
                // row is the tick it opens -- the live overlay is this plan's
                // own GD replay -- so anchor on that and let shift be 0.
                //
                // Measured on lv22 (2026-08-13): with the touch boxes turned
                // into the rotated frame (see touchFor) the player finally
                // enters box 30 mid-section, and with the old shared anchor
                // that re-dated the WALL the section stands on -- the replay's
                // exit moved from world x=2,143.5 (GD's value) to 1,425.4.
                //
                // g_recPhase additionally says "the anchor was taken while this
                // one was still sliding", which the state's mask cannot express.
                if (trigRecFire[i] >= 0) {
                    if (g_recPhase & m) fired = true;
                    // A DUAL-controlled object (autonomous descent + touch
                    // nudge, the switch band's ceiling): the recording's
                    // motion is the AUTONOMOUS part and plays for EVERY
                    // state. Gated on the touch mask alone, an untouched
                    // state saw the ceiling at its entry position forever --
                    // dynpos printed two beliefs per tick, 367.75 (rest) and
                    // 296.8 (the recorded descent), and the t=2,164-anchored
                    // solve (before the descent's first motion, so no
                    // recPhase either) crossed the sealed band with zero
                    // punches and died in GD at the same t=2,764 as every
                    // other unpunched worldline. (autoAnchor alone missed it:
                    // the ceiling is classified touch-only; recAuto is the
                    // discriminator that caught it.)
                    // The recSelfFire disjunct is the middle one. The first is
                    // dead here (no object reaching this line has autoAnchor >= 0:
                    // autoCtl implies m == 0, and formulaDriven leaves at :708);
                    // it served --noformula, which is gone since the flag
                    // clean-up, and removing it is left to a change of its own.
                    // The recAuto disjunct is untouched.
                    //
                    // Gated on the RECORDING's own first motion: without the tick
                    // test this forces `fired` from t=1 and the fired branch then
                    // interpolates across the leading gap (:931-942) -- measured
                    // on lv22 uid254, where sample[0] (1, 57.000) and sample[1]
                    // (315, 65.940) gave cy=62.552 at t=196.
                    // `trigRecFire[i] >= 0` is guaranteed by the enclosing :754.
                    //
                    // MEASURED AGAINST GD, and that measurement is the scope of
                    // the claim (2026-09-13). The class is nine objects, all in
                    // lv22 (2,094 triggered objects corpus-wide -> 157 controlled
                    // -> 15 whose touch moves nothing -> 9 that also move by
                    // themselves). A fresh recording of the same plan was taken
                    // from the game and held against both arms at the 20-tick
                    // print points around each object's first use: 33 comparison
                    // points, of which this branch puts 33 within 0.02 px of the
                    // game and the branchless form 27. The two differ on 4 of the
                    // 9 objects and 6 of the 33 points, by up to ~148 px, in the
                    // window between the recording's first motion and the late
                    // fire the touch mask alone produces.
                    // WHAT IT DOES NOT SHOW: the three objects the player
                    // actually rides in this corpus (uid 195, 254, 320) come out
                    // the same either way, so the corpus trajectory does not move
                    // -- the 22 replays are byte-identical with and without this.
                    if (autoAnchor[i] >= 0
                        || (i < recSelfFire.size() && recSelfFire[i]
                            && t >= trigRecFire[i])
                        || (i < recAuto.size() && recAuto[i]))
                        fired = true;
                    anchor = recAnchor; lat = 0;
                    // --touchretime (see the flag): this state's own entry.
                    if (fireB && autoAnchor[i] < 0
                        && !(i < recAuto.size() && recAuto[i])
                        && !(i < autoReach.size() && autoReach[i])
                        && !(g_recPhase & m)) {
                        int fb = -1, fbBox = -1;
                        for (int b = 0; b < kTouchBits; ++b)
                            if (((m & mask) & touchBit(b)) && (int)fireB[b] > fb) {
                                fb = (int)fireB[b];
                                fbBox = b;
                            }
                        // --touchretimebox: the object's own delay behind the box's
                        // recorded entry, carried over to this state's entry.
                        const int boxEntry =
                            (fbBox >= 0 && (size_t)fbBox < boxRecEntry.size())
                                ? boxRecEntry[(size_t)fbBox] : -1;
                        // The `- 1` is not a fudge, and the whole lag is worse.
                        // TRIED AND REVERTED (2026-09-20): adding the whole
                        // recLag here, on the argument that the autonomous
                        // branch below subtracts the whole lag from recAnchor
                        // and that lv22 uid1214 still reads the recording one
                        // tick ahead with the flag on. It does land uid1214 on
                        // the row of the same tick (9 of its 12 clamp rows
                        // match at shift 0, against 8 at +1 with the -1), and
                        // lv22's tracking against the reference then FALLS from
                        // 19,343 to 18,596 ticks with 6 -> 10 diverged sections
                        // and the t=2,600 section back to 168 of 400. So the
                        // one tick that is left is the trace's own convention
                        // -- the recorder samples after the whole tick, and the
                        // model's clamp for tick t wants the row labelled t+1
                        // -- not a residual to be removed.
                        if (fb >= 0 && (long long)fb >= g_touchRetimeFrom)
                            anchor = (boxEntry >= 0)
                                ? fb + (trigRecFire[i] - boxEntry)
                                : fb + kTouchRetimeLat
                                      + ((i < recLag.size())
                                             ? std::max(0, recLag[i] - 1) : 0);
                    }
                }
            } else {
                const int f = autoFireT(aA, mask, fireB);   // --boxarm
                fired = (f >= 0 && f <= t);
                anchor = f;
                recAnchor = trigRecFire[i]
                    - (i < autoLag.size() ? autoLag[i] : recLag[i]);
                // --anchornoop: no recorded change is the anchor's, so nothing to pair its
                // fire tick with -- the recording on its own clock (shift 0).
                if (f >= 0 && i < anchorUnrec.size() && anchorUnrec[i])
                    recAnchor = f;
                // --recanchorx: the recording's first change was not the anchor's.
                else if (g_recAnchorX && i < recAnchorX.size() && recAnchorX[i] >= 0)
                    recAnchor = recAnchorX[i];
                fdx = autoDx[i]; fdy = autoDy[i];
                fdur = autoDur[i]; lat = 0;
                fease = autoEase[i]; ferate = autoErate[i];
            }
            // --shiftstat, third line: the object's re-timing INPUTS, once, at
            // the tick its controller first counts as fired. This is where the
            // shift is actually decided, and it is the only place that can say
            // WHY -- the touch branch above sets anchor = recAnchor outright, so
            // a touch-controlled object with a recording is 0 by construction and
            // every non-zero shift comes from the autonomous branch, where the
            // model's own computed fireT is held against the recording's first
            // motion (trigRecFire - recLag).
            if (g_shiftStat && fired) {
                if (shiftSaid.size() != objs.size())
                    shiftSaid.assign(objs.size(), 0);
                if (!(shiftSaid[i] & 4)) {
                    shiftSaid[i] |= 4;
                    std::printf("shiftres: uid=%d bucket=%d branch=%s anchor=%d "
                                "recFire=%d recLag=%d shift=%d ease=%d "
                                "rate=%.3f dur=%.3f dx=%.3f dy=%.3f t=%d\n",
                                objs[i].uid, (int)bucket[i],
                                m ? "touch" : "auto", anchor, trigRecFire[i],
                                (i < recLag.size() ? recLag[i] : -1),
                                (anchor >= 0 ? anchor - recAnchor : 0),
                                fease, ferate, fdur, (double)fdx, (double)fdy,
                                t);
                }
            }
            if (fired) {
                // [2026-08-26] A touch the STATE plans that the RECORDING never
                // made. With a recording present the branch below replays it
                // wholesale, so a planned punch changed NOTHING in the model's
                // world -- the switch band's ceiling (lv22 x=3,255..3,675,
                // hanging blocks) kept descending on the recorded (unpunched)
                // curve and the frontier died under it, which is the measured
                // "frontier dies four ticks after touching" of the 32-cap note.
                // GD's real response to the punch, measured on uid18119 with a
                // jump added at t=2,560: the ceiling RE-BASES at its position
                // of the touch tick, nudges up ~10px, and the descent easing
                // restarts from rest (335 at the punch, 338 at t=2,582 where
                // the unpunched curve reads 328). So when the state's own fire
                // tick is far from the recording's motion anchor -- this touch
                // is NOT what the recording shows -- FREEZE the recording at
                // the touch tick and apply the box's analytic chain offset
                // from there. The re-descent stays unmodeled (frozen): the
                // tail is replayed in GD either way, and that replay's own
                // recording carries the true post-punch world into the next
                // iteration. Recordings that ARE the state's touch (lv19/20
                // doors: recAnchor within a beat of the fire) are untouched.
                // Gated on the object ALSO having an auto controller: the
                // switch-band ceiling is auto-descent + touch-nudge, so its
                // recorded motion is the descent and never this punch; a
                // touch-only door's recording IS its own touch, and freezing
                // it at fireHi (the state's LAST box, one scalar for all
                // doors) re-dated lv19's earlier doors off a later box
                // (quick_regress lv19 t=19,800: 400 -> 3 before this gate).
                bool nearBox = false;
                if (m && (mask & m)) {
                    // TouchMask, not uint32_t: both operands are TouchMask and
                    // the loop below runs to kTouchBits, so a narrow local
                    // truncated the high boxes away and then shifted past its
                    // own width for b >= 32. Boxes 32.. always read as "not
                    // touched" -- and at a width of 64 those are exactly the
                    // boxes the wider mask exists to carry.
                    const TouchMask touched = mask & m;
                    for (int b = 0; b < kTouchBits; ++b)
                        if (touched.test(b)
                            && std::fabs((double)g_touchBoxU[b]
                                         - (double)objs[i].cx) < 400.0) {
                            nearBox = true;
                            break;
                        }
                }
                // An object whose recording is a WORLDLINE fact rather than the
                // state's own touch (recAuto, decided at load from the per-box
                // chains) takes the per-box path below; the single-scalar hold
                // here keeps serving the dual-controlled (autoAnchor) class it
                // was measured on.
                const bool recAutoObj = m && i < recAuto.size()
                                        && recAuto[i] != 0;
                const bool ownTouch = m && (mask & m) && nearBox
                    && trigRecFire[i] >= 0
                    && !recAutoObj && autoAnchor[i] >= 0
                    && fireHi > recAnchor + 8;
                if (trigRecFire[i] >= 0 && !autoClosed[i]) {
                    const int shift = (anchor >= 0) ? (anchor - recAnchor) : 0;
                    shiftUsed = shift;
                    int tt = t - shift;
                    // NOT frozen forever: an eternal freeze let ANY grazed box
                    // hold the whole descent open and the anchored solve
                    // crossed the sealed band with zero punches (a phantom
                    // SOLVED at x=24,145 while GD dies at 2,763). The measured
                    // response is "the recording, DELAYED": hold ~60 ticks at
                    // the punch position, then the descent resumes from rest
                    // -- recorded(t-60) reads 285 at t=2,764 where the punched
                    // GD run measures 285.7. Consecutive punches slide the
                    // window (fireHi is the LAST box), which undercounts the
                    // cumulative delay of a punch chain -- the safe side: the
                    // model plans extra punches, and extra yellow hits are
                    // exactly what the mechanic accepts.
                    if (ownTouch && tt > fireHi) {
                        const int hold = 60;
                        tt = (t - shift > fireHi + hold) ? (tt - hold) : fireHi;
                    } else if (recAutoObj && (mask & m)) {
                        // Cumulative per-box delay. GD's measured response to a
                        // punch is "the recording, held ~60 ticks at the punch
                        // position" (rec(t-60) reads 285 at t=2,764 where the
                        // punched run measures 285.7) -- and the +30 the chain
                        // promises does NOT persist (285.7 measured, 315 if it
                        // did), so the punch's whole effect is the delay. Each
                        // touched box that pushes AWAY (dy > 0, the yellow
                        // punches) contributes its own min(60, t - F) window
                        // from its own fire tick -- for a single box this is
                        // the exact hold above. Boxes the anchor arrived with
                        // (F = -1) are already inside the live recording and
                        // contribute nothing.
                        int delay = 0;
                        for (const AutoPart& p : touchParts[i]) {
                            if (p.dy <= 0.f) continue;
                            if (!mask.test(p.trig)) continue;
                            const int F = g_touchFireT[p.trig & (kTouchBits - 1)];
                            if (F < 0 || t <= F) continue;
                            if (std::fabs((double)g_touchBoxU[p.trig & (kTouchBits - 1)]
                                          - (double)objs[i].cx) >= 400.0)
                                continue;
                            if (g_dynDbg == objs[i].uid)
                                std::printf("holddbg t=%d uid=%d box=%d F=%d recAnchor=%d recFire=%d dy=%.3f add=%d\n",
                                            t, objs[i].uid, p.trig, F, recAnchor, trigRecFire[i], (double)p.dy,
                                            std::min(60, t - F));
                            // --holddedup: a touch within 16 ticks of the recording's own motion anchor is a
                            // touch the recording holds (a custom level, t=5133: the plates' recording starts at 5121 from
                            // the box before, and box 23 fires 11 ticks later in the same pass of the player); the
                            // measured punch hold is for a touch far from it.
                            if (g_holdDedup && trigRecFire[i] >= 0 && std::abs(F - recAnchor) <= 16)
                                continue;
                            delay += std::min(60, t - F);
                        }
                        tt -= delay;
                    }
                    // --shiftdbg <uid>: print this uid's re-timing once. An
                    // instrument for the suspicion that the descent phase of
                    // the t=2776 spike (uid18118) is off from live by tens of
                    // ticks.
                    if (g_shiftDbgUid >= 0 && objs[i].uid == g_shiftDbgUid
                        && !g_shiftDbgDone) {
                        g_shiftDbgDone = true;
                        std::printf("shiftdbg: uid=%d anchor=%d recAnchor=%d "
                                    "shift=%d t=%d tt=%d touch=%d\n",
                                    objs[i].uid, anchor, recAnchor, shift, t,
                                    tt, m ? 1 : 0);
                    }
                    // binary search: these lists are ~100 samples and this runs
                    // once per object per group per tick
                    size_t lo = 0, hi = sm.size();
                    while (lo + 1 < hi) {
                        const size_t mid = (lo + hi) / 2;
                        if (sm[mid].t <= tt) lo = mid; else hi = mid;
                    }
                    const DynSample& s = sm[lo];
                    cx = s.cx; cy = s.cy; hw = s.hw; hh = s.hh; onv = recOnAt(sm, lo, tt);   // --offafter
                    rotv = s.rot;
                    // --moverextend (see seek): past the last row of a constant-velocity mover the
                    // retimed tick goes on at that velocity (this placement overwrites seek()'s).
                    if ((g_moverExtend || g_moverExtendVel) && i < extOk.size() && extOk[i] && lo + 1 == sm.size() && tt > s.t) {
                        const float k = (float)std::min(tt - s.t, extCap[i]);
                        cx = s.cx + extVx[i] * k;
                        cy = s.cy + extVy[i] * k;
                    }
                    // [2026-08-22 r104] **Interpolate linearly between rows.**
                    // grouptrace does not write changes under 0.05px, so an
                    // object moving at 0.045px/tick is recorded as "jumps
                    // 0.09px once every 2 ticks". GD moves smoothly, so holding
                    // (the staircase) is a systematic error of up to 0.09px and
                    // **cannot reproduce the tick at which contact breaks, not
                    // even to within 1 tick** (the lv22@6,129 family).
                    // Position and rot only. on (a boolean) and hw/hh stay
                    // stepped.
                    // --touchretime: not across the LEADING gap. Before the
                    // recording's first motion the object is at rest, and the
                    // row after the gap is already the first moved position --
                    // interpolating toward it from t=1 put lv22's block row at
                    // 344.70 one tick before GD moves it (t=6510, shift 5), which
                    // is exactly the landing tick.
                    const bool leadingGap = m
                                            && trigRecFire[i] >= 0
                                            && tt < trigRecFire[i];
                    // Only a step a uniform mover can make, per channel. Until
                    // the newer row's tick every channel stayed within kRecEps
                    // of the older row (else a row would have been written),
                    // so an object moving under kRecEps a tick arrives less
                    // than 2*kRecEps away. A larger step across a gap is an
                    // object that sat still and then jumped or set off fast,
                    // and holding the older row stays within kRecEps of it
                    // the whole time. 4002 platform uid 18238: rows at 20,066
                    // (y 2,068.5) and 20,711 (1,468.5) are a 600 px instant
                    // Move, which this spread over 645 ticks -- the model's
                    // platform hung 81 px above GD's and crushed the player.
                    // --recstall: ...nor across a fast mover's stop (recStallGap)
                    if (lo + 1 < sm.size() && !leadingGap && !recStallGap(sm, lo)) {
                        const DynSample& n = sm[lo + 1];
                        const int span = n.t - s.t;
                        if (span > 0 && tt > s.t) {
                            const double u = (double)(tt - s.t) / (double)span;
                            const auto lerp = [u](float a, float b) {
                                const double d = (double)b - (double)a;
                                return std::fabs(d) < 2.0 * kRecEps
                                           ? (float)((double)a + d * u) : a;
                            };
                            cx = lerp(s.cx, n.cx);
                            cy = lerp(s.cy, n.cy);
                            rotv = lerp(s.rot, n.rot);
                        }
                    }
                    // A recording ends where its attempt died, and holding the
                    // last sample freezes a door mid-slide. Measured on lv19
                    // (2026-08-05): the corridor recording stops at t=20,012
                    // with the slide 32 px into its 45, the model rides the
                    // 67%-open slat 12.94 px above where GD rides the OPEN
                    // door, the 1.1x speed portal at (28,163,303) fires 13
                    // ticks apart, and the whole endgame runs 6.8 px of x
                    // adrift (the 27,686 / 29,024 pincer of the 60-iteration
                    // run). When the map knows the full offset, finish the
                    // slide: add the analytic eased INCREMENT from where the
                    // recording ends -- continuous at the seam, and the small
                    // map-vs-real mismatch (the ease fit is within a few
                    // percent) stays exactly as large as it already was.
                    // Move-to-target pieces (offset 0,0 -- the lift) cannot be
                    // completed analytically; those need the driver's nodeath
                    // re-record instead.
                    // For a recAuto object the summed fdx/fdy is opposing
                    // chains added together, but the slide still has to be
                    // finished: the band's recording is a DEAD attempt's
                    // (grouptrace stops ~25 ticks after the seal kills the
                    // recorder at t=2,764), and holding its last row left the
                    // ceiling at 260.59 forever -- a 13 px slit over the lane
                    // that GD closes, which the anchored solve threaded for
                    // yet another no-punch phantom. The autonomous descent's
                    // own chain is on file per box (the red -840/1005.6t tail
                    // is the same movers), so complete with the largest part
                    // whose dy agrees with the recorded motion's direction.
                    double bdx = fdx, bdy = fdy, bdur = fdur;
                    int bease = fease; double berate = ferate;
                    if (recAutoObj) {
                        bdur = 0.0;
                        const double dirY = (double)s.cy - (double)s0.cy;
                        float best = 0.f;
                        for (const AutoPart& p : touchParts[i])
                            if ((double)p.dy * dirY > 0.0
                                && std::fabs(p.dy) > best) {
                                best = std::fabs(p.dy);
                                bdx = p.dx; bdy = p.dy; bdur = p.dur;
                                bease = p.ease; berate = p.erate;
                            }
                    }
                    // (Unconditional since the 0.4.0 clean-up; it was --slideparts, on in every
                    // loop call that read a recording since d008ee2.) An object with two or more
                    // autonomous moves goes on from its last row by each move's own eased increment,
                    // every move from its own crossing -- the form stage 1' objects already
                    // take (below). The completion after this reads the moves as ONE, the
                    // summed offset over one duration from the recording's first motion,
                    // which is right for a single slide and not for a sequence.
                    bool partsDone = false;
                    // --ctlautotail: the same tail for a touch-controlled object whose chain places
                    // nothing (offset 0) and whose boxes this state has not entered, from the Moves
                    // the loader keeps for every object (partsAny; autoParts is empty when a touch
                    // controls it). A custom level's spike uid 139269: a zero-offset touch Rotate on
                    // its group, out of the player's reach, and four autonomous Moves -- (+300,0) and
                    // (+81,0) overlapping (2.12 px a tick), then two eased ones. Its rows end at the
                    // attempt's death, it held there, and GD killed on the next tick at 12,567.10
                    // where the held row reads 12,564.98. It starts from the last row, so a
                    // --moverextend tail placed above is replaced, not added to.
                    const bool ctlTail = g_ctlAutoTail && m && !(mask & m).any() && fdx == 0.f
                                         && fdy == 0.f && autoParts[i].empty() && i < partsAny.size()
                                         && !partsAny[i].empty();
                    if (lo + 1 == sm.size() && tt > s.t && !recAutoObj
                        && (autoParts[i].size() >= 2 || ctlTail)
                        && !(i < rotSplit.size() && rotSplit[i])) {
                        const int tEnd = s.t + (t - tt);   // the last row on this state's clock
                        if (ctlTail) { cx = s.cx; cy = s.cy; }
                        const bool fromLast = (double)s.cx == cx && (double)s.cy == cy;   // --recendresid
                        for (const AutoPart& p : (ctlTail ? partsAny[i] : autoParts[i])) {
                            const int f = autoFireT(p.trig, mask, fireB);   // --boxarm
                            if (f < 0) continue;
                            auto eAt = [&](int tk) {
                                if (tk < f) return 0.0;
                                return p.dur > 0.0
                                    ? gdEase(p.ease, p.erate, (double)(tk - f) / p.dur)
                                    : instantE(tk, f);   // --instantlag
                            };
                            const double de = eAt(t) - eAt(tEnd);
                            cx += p.dx * de;
                            cy += p.dy * de;
                            if (g_recEndResid) {
                                const double dl = eAt(tEnd) - eAt(tEnd - 1);
                                tailLastDx += p.dx * dl;
                                tailLastDy += p.dy * dl;
                            }
                        }
                        partsDone = true;
                        if (g_recEndResid && fromLast) { partsTail = true; tailCx = cx; tailCy = cy; }
                    }
                    if (!partsDone && lo + 1 == sm.size() && tt > s.t && bdur > 0) {
                        const double full = std::hypot((double)bdx, (double)bdy);
                        const double done = std::hypot((double)s.cx - (double)s0.cx,
                                                       (double)s.cy - (double)s0.cy);
                        if (full > 0.5 && done < full - 0.5) {
                            auto ease = [&](double u) {
                                return gdEase(bease, berate, u);
                            };
                            const double uEnd =
                                (double)(s.t - recAnchor) / bdur;
                            const double uNow =
                                (double)(tt - recAnchor) / bdur;
                            // normalized so the terminal position is EXACTLY
                            // rest + offset (the measured open position),
                            // whatever small phase error the recording ended
                            // with; the increment form overshot by that error
                            const double eEnd = ease(uEnd);
                            if (eEnd < 0.999) {
                                const double de =
                                    (ease(uNow) - eEnd) / (1.0 - eEnd);
                                cx += ((double)s0.cx + (double)bdx - (double)s.cx) * de;
                                cy += ((double)s0.cy + (double)bdy - (double)s.cy) * de;
                            }
                        }
                    }
                    // --recendtail: past the last row an armed object is its simulated rig (the offset to the last
                    // row is in the table), whatever the tails above made of it, and its lock follows this
                    // group's x rather than the track's. See g_recEndTail for what stands down.
                    if (g_recEndTail && i < rigTail.size() && !rigTail[i].x.empty()
                        && lo + 1 == sm.size() && tt > s.t && rigTail[i].t0 == s.t) {
                        const RigTail& R = rigTail[i];
                        const size_t k = std::min((size_t)(tt - R.t0), R.x.size() - 1);
                        cx = R.x[k]; cy = R.y[k]; rotv = R.r[k];
                        if (tt >= R.lockA && tt <= R.lockB) cx += px - xRefAt(tt);
                        rigTailed = true;
                        partsTail = false;
                        recEndAxis(i, (double)rotv);
                        if (objs[i].obbOk) {
                            hw = objs[i].bhw * std::fabs(objs[i].bc) + objs[i].bhh * std::fabs(objs[i].bs);
                            hh = objs[i].bhw * std::fabs(objs[i].bs) + objs[i].bhh * std::fabs(objs[i].bc);
                        }
                    }
                    // ...and the planned touch's own chain, applied FROM the
                    // frozen base (the measured re-base + nudge; the note at
                    // ownTouch above). 5 = the measured box->motion latency.
                    if (ownTouch) {
                        double u = 1.0;
                        if (fdur > 0) u = (double)(t - fireHi - 5) / fdur;
                        if (u > 0.0) {
                            const double e = gdEase(fease, ferate, u);
                            cx += fdx * e;
                            cy += fdy * e;
                        }
                    } else if (recAutoObj && (mask & m)) {
                        // A touched box that pushes TOWARD the player (dy < 0,
                        // the red skulls) is applied as a step: the user-stated
                        // and measured mechanic is an immediate slam, and the
                        // chain's 1005.6-tick easing belongs to the autonomous
                        // descent, not to this. A state that punches red sees
                        // the seal at once and dies in the model exactly where
                        // GD kills it.
                        // --slamdedup: the box the recording's own motion anchors on is the object's first touch (the
                        // earliest fire among the boxes it was touched by); a later, independent touch near it is
                        // not in the recording and keeps its step.
                        int fireMin = 1 << 30;
                        int nHit = 0;   // boxes the state touched that move this object (two on the same tick are both fireMin)
                        for (const AutoPart& q : touchParts[i]) {
                            if (!mask.test(q.trig)) continue;
                            const int Fq = g_touchFireT[q.trig & (kTouchBits - 1)];
                            if (Fq >= 0) { fireMin = std::min(fireMin, Fq); ++nHit; }
                        }
                        for (const AutoPart& p : touchParts[i]) {
                            if (p.dy >= 0.f) continue;
                            if (!mask.test(p.trig)) continue;
                            const int F = g_touchFireT[p.trig & (kTouchBits - 1)];
                            if (F < 0 || t < F + 5) continue;
                            if (std::fabs((double)g_touchBoxU[p.trig & (kTouchBits - 1)]
                                          - (double)objs[i].cx) >= 400.0)
                                continue;
                            // --slamdedup: a touch the recording already holds (its motion anchor within
                            // 8 ticks of the state's fire tick) is in the recording's own rows; the step
                            // would add the same move a second time (a custom level, t=5124, plate -9 / 24 ticks).
                            // Only with ONE touching box (nHit == 1): the recording holds one touch and the state has no
                            // other candidate to mistake it for.
                            // --slambox: by the box itself, from the recorded attempt's own record of what
                            // GD fired (slamBoxRecorded), in place of the tick-only rule above. A call the
                            // mod gives no --rectouch (the first, unanchored one) keeps the rule above.
                            const int pb = p.trig & (kTouchBits - 1);
                            const bool dup = (g_slamBox && g_recTouchGiven)
                                ? slamBoxRecorded(g_recTouchBit, pb, F)
                                : (g_slamDedup && trigRecFire[i] >= 0 && nHit == 1 && F == fireMin && std::abs(F - recAnchor) <= 8);
                            if (g_dynDbg == objs[i].uid)
                                std::printf("slamdbg t=%d uid=%d box=%d F=%d recAnchor=%d recFire=%d rec=%d dy=%.3f dedup=%d\n",
                                            t, objs[i].uid, p.trig, F, recAnchor, trigRecFire[i],
                                            g_recTouchBit[(size_t)pb], (double)p.dy, dup ? 1 : 0);
                            if (dup) continue;
                            cx += p.dx;
                            cy += p.dy;
                        }
                    }
                    // The other half of the g_rotSplit decomposition. The
                    // samples carry `entry_C + R(theta)*rel` -- the orbit about
                    // a STATIONARY centre -- and the loader subtracted the
                    // centre's own translation out to get there ("recorded =
                    // orbit + the centre's own translation", at the fit check).
                    // Nothing added it back: the one path that would is behind
                    // g_rotCompute, which is false and which no flag sets, so
                    // every split object has been running without its
                    // translation since 7c6daa1. The split refuses any object
                    // that does not share its move controllers with its centre,
                    // so this object's own autoParts IS `C(t) - entry_C`, each
                    // controller on its own crossing.
                    if (i < rotSplit.size() && rotSplit[i]) {
                        for (const AutoPart& p : autoParts[i]) {
                            const int f = autoFireT(p.trig, mask, fireB);   // --boxarm
                            if (f < 0 || t < f) continue;
                            // --autostop: a halted Move holds where its Stop left it
                            const int te = g_autoStop ? std::min(t, autoStopT(p.trig)) : t;
                            if (te < f) continue;
                            const double e = (p.dur > 0.0)
                                ? gdEase(p.ease, p.erate, (double)(te - f) / p.dur)
                                : instantE(te, f);   // --instantlag
                            cx += p.dx * e;
                            cy += p.dy * e;
                        }
                    }
                    // --lockx: an object whose only x mover is a lockToPlayerX Move takes its x
                    // from the lock, as the closed form below does, instead of from the
                    // recording. The recording is re-timed by the object's anchor, and when the
                    // anchor and recAnchor come from different controls (a custom level's uid 20017:
                    // anchor from the y Moves, recAnchor from another group's crossing, shift
                    // -80) the recorded x is the player's x 80 ticks ahead, then frozen at the
                    // recording's last row. y still comes from the recording.
                    if (g_lockX && !rigTailed && aA >= 0 && autoLockTrig[i] >= 0 && autoLockXOnly(i)
                        && autoFireT(autoLockTrig[i], mask, fireB) >= 0) {   // --boxarm
                        AutoTrig& A = g_autoTrig[(size_t)autoLockTrig[i]];
                        const int lockT0 = A.fireT - A.delay;
                        int lockEnd = lockT0 + (int)autoLock[i];
                        if (g_autoStop) lockEnd = std::min(lockEnd, autoStopT(autoLockTrig[i]));
                        if (g_lockStop) lockEnd = std::min(lockEnd, touchStopTick(mask, fireB, i < lockMover.size() ? lockMover[i] : A.uid));
                        cx = s0.cx;
                        if (t >= lockT0 && t <= lockEnd) {
                            A.lockLastX = px;
                            cx += px - A.fireX;
                        } else if (t > lockEnd) {
                            cx += A.lockLastX - A.fireX;
                        }
                        if (g_dynDbg == objs[i].uid && (t % 40 == 0))
                            std::printf("lockxdbg t=%d uid=%d lockT0=%d lockEnd=%d fireX=%.3f "
                                        "px=%.3f cx=%.3f\n", t, objs[i].uid, lockT0, lockEnd,
                                        A.fireX, px, cx);
                    }
                    if (g_dynDbg == objs[i].uid && (t % 40 == 0))
                        std::printf("armdbg t=%d uid=%d arm=recording split=%d "
                                    "parts=%zu cx=%.3f cy=%.3f\n",
                                    t, objs[i].uid,
                                    (int)(i < rotSplit.size() ? rotSplit[i] : 0),
                                    autoParts[i].size(), cx, cy);
                } else {
                    // The closed form. Either there is no recording at all (the
                    // door nobody has opened yet) or --trigclosed is on and this
                    // object's recording AGREES with the formula, in which case
                    // the formula is the better source: it does not end where
                    // some attempt died and it does not carry another run's
                    // phase. `on` and the size still come from the recording --
                    // a toggle is not a move and has no closed form here.
                    // ...and if the object is locked to the player, its x is
                    // THIS run's player x, which is the whole reason a recording
                    // cannot serve it (see TrigCtl::lockTicks).
                    if (aA >= 0 && autoLockTrig[i] >= 0
                        && autoFireT(autoLockTrig[i], mask, fireB) >= 0) {   // --boxarm
                        AutoTrig& A = g_autoTrig[(size_t)autoLockTrig[i]];
                        // The lock starts on the CROSSING, one tick before an
                        // eased move would (measured, 0.002 px over 2,400
                        // samples), and runs for its own duration. `lockLastX`
                        // trails the player while the window is open so that
                        // once it closes the object holds where the player left
                        // it -- the same running-assignment trick fireT uses,
                        // and like fireT it is level-wide: the last group of a
                        // tick wins. States within a tick differ in x by a
                        // speed group at most, which is the same approximation
                        // gFire already makes for doors.
                        const int lockT0 = A.fireT - A.delay;
                        int lockEnd = lockT0 + (int)autoLock[i];
                        // --autostop: a Stop naming the lock's Move closes the window on
                        // its crossing tick (the lock still copies x on that tick).
                        if (g_autoStop) lockEnd = std::min(lockEnd, autoStopT(autoLockTrig[i]));
                        // --lockstop: ...and so does one in a touch chain this world fired.
                        if (g_lockStop) lockEnd = std::min(lockEnd, touchStopTick(mask, fireB, i < lockMover.size() ? lockMover[i] : A.uid));
                        if (t >= lockT0 && t <= lockEnd) {
                            A.lockLastX = px;
                            cx += px - A.fireX;
                        } else if (t > lockEnd) {
                            cx += A.lockLastX - A.fireX;
                        }
                    }
                    if (trigRecFire[i] >= 0 && sm.size() > 1) {
                        const int shift = (anchor >= 0) ? (anchor - recAnchor) : 0;
                        const int tt = t - shift;
                        size_t lo = 0, hi = sm.size();
                        while (lo + 1 < hi) {
                            const size_t mid = (lo + hi) / 2;
                            if (sm[mid].t <= tt) lo = mid; else hi = mid;
                        }
                        hw = sm[lo].hw; hh = sm[lo].hh; onv = recOnAt(sm, lo, tt);   // --offafter
                        rotv = sm[lo].rot;
                    }
                    if (recAutoObj) {
                        // No recording yet (first iteration). The summed
                        // offset is opposing chains added together, so play
                        // each TOUCHED box's own part instead. The autonomous
                        // descent is unknowable here (its starter is a
                        // collision trigger the dump cannot time); the next
                        // iteration's recording carries it.
                        for (const AutoPart& p : touchParts[i]) {
                            if (!mask.test(p.trig)) continue;
                            const int F = g_touchFireT[p.trig & (kTouchBits - 1)];
                            if (F < 0) continue;
                            double u = 1.0;
                            if (p.dur > 0) u = (double)(t - F - 5) / p.dur;
                            if (u <= 0.0) continue;
                            const double e = gdEase(p.ease, p.erate, u);
                            cx += p.dx * e;
                            cy += p.dy * e;
                        }
                    } else if (m || autoParts[i].empty()) {
                        // touch trigger (one box, one summed offset), or an
                        // autonomous object with no move at all (toggle only)
                        double u = 1.0;
                        if (anchor >= 0 && fdur > 0)
                            u = (double)(t - anchor - lat) / fdur;
                        const double e = gdEase(fease, ferate, u);
                        cx += fdx * e;
                        cy += fdy * e;
                    } else {
                        // Superposition: every controller on its own clock.
                        if (g_dynDbg == objs[i].uid && (t % 40 == 0))
                            for (const AutoPart& p : autoParts[i])
                                std::printf("armdbg   part t=%d uid=%d trig=%d "
                                            "fireT=%d d=(%.1f,%.1f) dur=%.2f\n",
                                            t, objs[i].uid, p.trig,
                                            g_autoTrig[(size_t)p.trig].fireT,
                                            (double)p.dx, (double)p.dy, p.dur);
                        for (const AutoPart& p : autoParts[i]) {
                            const int f = autoFireT(p.trig, mask, fireB);   // --boxarm
                            if (f < 0 || t < f) continue;
                            // --autostop: a halted Move holds where its Stop left it
                            const int te = g_autoStop ? std::min(t, autoStopT(p.trig)) : t;
                            if (te < f) continue;
                            const double e = (p.dur > 0.0)
                                ? gdEase(p.ease, p.erate, (double)(te - f) / p.dur)
                                : instantE(te, f);   // --instantlag
                            cx += p.dx * e;
                            cy += p.dy * e;
                        }
                    }
                    // DIAGNOSTIC (not for landing): which branch served this
                    // object, and what its parts were clocked by.
                    if (g_dynDbg == objs[i].uid && (t % 40 == 0))
                        std::printf("armdbg t=%d uid=%d branch=%s m=%u "
                                    "parts=%zu recAutoObj=%d anchor=%d "
                                    "cy=%.3f\n", t, objs[i].uid,
                                    recAutoObj ? "recAuto"
                                        : (m || autoParts[i].empty())
                                            ? "collapsed" : "superpos",
                                    (unsigned)m.word(0), autoParts[i].size(),
                                    (int)recAutoObj, anchor, cy);
                    if (g_dynDbg == objs[i].uid && t == 11720)
                        for (const AutoPart& p : autoParts[i])
                            std::printf("armdbg   part trig=%d uid=%d cx=%.1f "
                                        "fireT=%d d=(%.2f,%.2f) dur=%.1f\n",
                                        p.trig, g_autoTrig[(size_t)p.trig].uid,
                                        g_autoTrig[(size_t)p.trig].cx,
                                        g_autoTrig[(size_t)p.trig].fireT,
                                        p.dx, p.dy, p.dur);
                }
            }
            // --shiftdbg <uid> also samples the object's EVALUATED position
            // every 20 ticks -- the belief the kill tests actually see, which
            // no amount of reasoning about the composition rules substitutes
            // for (the switch-band phantom survived three composition fixes
            // that were all aimed at the wrong mechanism).
            // ---- the ROTATION, after whatever produced the translation ------
            // A Rotate carries its group around a centre object, which no
            // offset describes: 806 of lv21's turned objects and 264 of lv22's
            // have no recording at all, so until this ran the model held them
            // PERFECTLY STILL while GD swung them. Applied here, after the
            // branches above, so it composes with any of them rather than
            // being duplicated into each.
            //
            // The centre is evaluated from its own entry plus its own
            // controllers rather than read out of objs[centreIdx]: this loop
            // has no order guarantee, so the centre may not have been placed
            // yet on this tick.
            //
            // Closed form, not the per-tick recurrence. The recurrence with a
            // float store is 0.0060 px against GD and this is 0.0142 px (see
            // RotSpec), but the recurrence needs a sequential carry and this
            // path is idempotent and may be entered with a jumped tick. 0.0142
            // against the 1.65-2.18 px a re-timed recording leaves is the
            // trade; the remaining eight thousandths wait for a table.
            if (g_rotCompute && !g_rotSpec.empty() && !rigTailed) {
                const auto rs = g_rotSpec.find(objs[i].uid);
                if (rs != g_rotSpec.end() && rs->second.anchor >= 0
                    && rs->second.centreIdx >= 0) {
                    const RotSpec& R = rs->second;
                    const int f = g_autoTrig[(size_t)R.anchor].fireT;
                    if (f >= 0 && t >= f) {
                        const size_t ci = (size_t)R.centreIdx;
                        double ccx, ccy;
                        if (samples[ci].size() > 1) {
                            // The centre has a recording of its own, and seek()
                            // has already placed it for this tick. Use it.
                            //
                            // Deriving it analytically instead was WRONG and
                            // deathref caught it: a centre whose motion is
                            // recorded rather than trigger-controlled has no
                            // autoParts, so the analytic form pinned it at its
                            // entry and the whole orbit hung off the wrong
                            // point. lv21_t13304 went PASS -> FAIL (died -436)
                            // -- an over-kill, a rotated hazard swung through
                            // the player because its centre never moved.
                            ccx = objs[ci].cx;
                            ccy = objs[ci].cy;
                        } else {
                            ccx = samples[ci][0].cx;
                            ccy = samples[ci][0].cy;
                            for (const AutoPart& p : autoParts[ci]) {
                                const int cf = autoFireT(p.trig, mask, fireB);   // --boxarm
                                if (cf < 0 || t < cf) continue;
                                const double e = (p.dur > 0.0)
                                    ? gdEase(p.ease, p.erate,
                                             (double)(t - cf) / p.dur)
                                    : instantE(t, cf);   // --instantlag
                                ccx += p.dx * e;
                                ccy += p.dy * e;
                            }
                        }
                        // sign: GD's positive turn is clockwise on screen, the
                        // opposite of a y-up matrix. Measured -1 on both
                        // subjects (lv21 uid15367, lv22 uid254).
                        const double th = -R.total
                            * gdEase(R.ease, R.erate, (double)(t - f) / R.durT)
                            * 3.14159265358979 / 180.0;
                        const double c = std::cos(th), s = std::sin(th);
                        const double rx = cx - ccx, ry = cy - ccy;
                        cx = ccx + c * rx - s * ry;
                        cy = ccy + s * rx + c * ry;
                    }
                }
            }
            if (g_shiftDbgUid >= 0 && objs[i].uid == g_shiftDbgUid
                && t % 20 == 0)
                std::printf("dynpos: uid=%d t=%d cx=%.2f cy=%.2f on=%d\n",
                            objs[i].uid, t, cx, cy, (int)onv);
            // --rottail: a placement that still holds the last row goes on turning with its Rotates from the
            // recording's last tick (rotTailMaxT + shiftUsed on the model's clock). The recorder writes a row
            // whenever anything changes, so an object whose last row is older stood as that row until then --
            // one that had not moved at all when the attempt died included: its turn starts past the end.
            bool rotTailed = rigTailed;   // --recendtail: already turned on, nothing below adds to it
            if (g_rotTail && !rigTailed && i < rotTail.size() && !rotTail[i].empty()) {
                const DynSample& L = sm.back();
                if ((double)L.cx == cx && (double)L.cy == cy && t - shiftUsed > rotTailMaxT
                    && rotTailFrom(i, rotTailMaxT + shiftUsed, t, cx, cy, rotv)) {
                    rotTailed = true;
                    if (rotBoxW[i] > 0.f) {
                        const double th = (double)rotv * (3.14159265358979 / 180.0);
                        const double c = std::fabs(std::cos(th)), s = std::fabs(std::sin(th));
                        hw = 0.5 * ((double)rotBoxW[i] * c + (double)rotBoxH[i] * s);
                        hh = 0.5 * ((double)rotBoxW[i] * s + (double)rotBoxH[i] * c);
                    }
                    recEndAxis(i, (double)rotv);
                }
            }
            // --recendextend: a placement that still holds the last row goes on by the last steady step, on the
            // recording's own clock (t - shiftUsed). One that another tail moved keeps that tail's answer.
            if (g_recEndExtend && !rotTailed) {
                const DynSample& L = sm.back();
                // --recendresid: ...or the parts tail alone moved it, and then the step minus the parts' share --
                // only on the ticks the extension covers, not held past them: the tail is the long-range answer
                // and the residual a straight line (a lock to the player, a turn) that a held offset would bend
                // for the rest of the level. A search reads a recording that ended at the last attempt's death
                // thousands of ticks earlier.
                const bool resid = partsTail && cx == tailCx && cy == tailCy
                                   && (t - shiftUsed) - L.t <= kRecEndExtendTicks;
                if (((double)L.cx == cx && (double)L.cy == cy) || resid)
                    if (const int k = recEndTicks(i, t - shiftUsed)) {
                        const double rdx = resid ? tailLastDx : 0.0, rdy = resid ? tailLastDy : 0.0;
                        cx += ((double)recVx[i] - rdx) * k; cy += ((double)recVy[i] - rdy) * k;
                        hw += (double)recVhw[i] * k; hh += (double)recVhh[i] * k;
                        rotv += recVr[i] * (float)k;
                        recEndAxis(i, (double)rotv);
                    }
            }
            // --areamove: an armed object's x and y are its base plus this state's area displacement, whatever the
            // recording or a tail made of them (the recording holds the recorded run's player's area pass).
            {
                double ax = 0.0, ay = 0.0, ahw = hw, ahh = hh, arot = std::nan("");
                if (areaPlace(i, ax, ay, &ahw, &ahh, &arot)) {
                    cx = ax;
                    cy = ay;
                    if (!std::isnan(arot)) {
                        hw = ahw;
                        hh = ahh;
                        rotv = (float)arot;
                        recEndAxis(i, arot);
                    }
                }
            }
            objs[i].cx = cx; objs[i].cy = cy; objs[i].hw = hw; objs[i].hh = hh;
            // dcy from the FINAL analytic position, not the raw samples (see
            // prevCy's declaration: the recorder skips sub-epsilon rows and
            // the raw dcy reads 0 on those ticks while the face still moves).
            // Only on a 1-tick advance -- an anchor jump keeps seek's
            // row-based value, same reasoning as seek's step1 gate.
            if (lastStep1 && i < prevCy.size())
                objs[i].dcy = (double)cy - (double)prevCy[i];
            // If the recording says "gone by this tick", the analytic side must
            // not bring it back. The live recording is this very run's own GD
            // replay, so `on` is the truth there. Returning sm[0] as it was here
            // left the block GD switched off at t=10,512 (lv22 uid11470) standing
            // in the model, and the player sliding down the corridor died of a
            // "side collision" at t=11,359.
            // --boxoffown: ...unless an off box takes this object out and none has fired here yet.
            bool offBoxHeld = false;
            if (g_boxOffOwn && i < offBox.size() && !offBox[i].empty()) {
                offBoxHeld = true;
                if (fireB)
                    for (const uint8_t b : offBox[i])
                        if ((mask & touchBit((int)b)) && (int)fireB[b] <= t) {
                            offBoxHeld = false;
                            break;
                        }
            }
            if (!offBoxHeld && cur[i] < sm.size() && sm[cur[i]].t <= t
                && recOnAt(sm, cur[i], t - shiftUsed) == 0)   // --offafter: the row's own clock
                onv = 0;
            // ...and the mirror for the ON direction, for objects nothing
            // MOVES (a pure toggle). The shifted search above re-times the
            // recording against the controller's predicted firing tick -- the
            // right thing for a door's motion, but an autonomous TOGGLE has no
            // re-timing basis: its "anchor" is the FIRST x-crossing while the
            // crossing that actually fires is the same unsolved gate 2899 has
            // (lv22's uid 6442 fires on its THIRD crossing of x=15,765,
            // together with the 2899 pair). The live recording is this plan's
            // own GD replay, so for a no-move object its unshifted clock is
            // the only truth. Without this, lv22's ship portal uid 6440
            // (group 309, toggled OFF at t=10,116 / ON at 14,225) read the
            // pre-toggle sample through a +1,2xx shift and NEVER fired -- the
            // model kept the cube falling past x=15,785 where GD flies a ship.
            {
                bool noMove = fdx == 0.f && fdy == 0.f;
                for (const AutoPart& p : autoParts[i])
                    if (p.dx != 0.f || p.dy != 0.f) { noMove = false; break; }
                // PORT/SPEED only. The first cut applied this to every no-move
                // object and lv22's frame-3 corridor (t=11,000 segment, the
                // uid 11470 neighbourhood) broke: the regress follow shrank
                // 400 -> 351 and two solid-support families appeared. The
                // solids' interplay with the shifted read is validated the
                // other way (the standing force-off above), so only the
                // interactive classes with a measured case move to the
                // unshifted clock.
                if (!m && noMove
                    && (bucket[i] == PORT || bucket[i] == SPEED)
                    && cur[i] < sm.size() && sm[cur[i]].t <= t) {
                    uint8_t v = sm[cur[i]].on;
                    // Re-ENABLING takes effect the tick AFTER the recorded row
                    // (measured: uid 6440's ON row is t=14,225 -- the toggle
                    // itself ran during 14,224 -- and GD's portal fires during
                    // 14,226, one tick later; the model firing at 14,225 came
                    // out -1.404 where GD has -1.512). The OFF direction stays
                    // on the row's own tick, same as the standing rule above
                    // (validated on uid 11470).
                    if (v && sm[cur[i]].t == t && cur[i] > 0
                        && !sm[cur[i] - 1].on)
                        v = 0;
                    onv = v;
                }
            }
            // --recendtail: past the recording's end, the last row's on/off and every switch that lands after
            // it. The recording's end, not the object's last row: the recorder writes a row whenever anything
            // changes, so an object whose last row is older stood as that row until the end, switched or not.
            if (g_recEndTail && i < tailOnEv.size() && !tailOnEv[i].empty()) {
                const int after = std::max(sm.back().t, tailRecEnd) + shiftUsed;
                if (t > after)
                    onv = resolveOnAfter(sm.back().on != 0, after, tailOnEv[i].data(), tailOnEv[i].size(),
                                         mask, fireB, t) ? 1 : 0;
            }
            // --activators: this object's on/off is its switches', not the recording's.
            if (i < onEv.size() && !onEv[i].empty())
                onv = resolveOn(onInit[i] != 0, onEv[i].data(), onEv[i].size(), mask, fireB, t)
                          ? 1 : 0;
            // --tapdisarm: ...and, once a switch of this object's x-crossing triggers would have landed in a state
            // where a box had switched that trigger off, its switches' on/off without it (resolveOn skips it).
            if (i < disarmEv.size() && !disarmEv[i].empty() && fireB) {
                bool any = false;
                for (const OnEvent& e : disarmEv[i])
                    if (e.autoIdx >= 0 && (size_t)e.autoIdx < g_autoTrig.size()
                        && g_autoTrig[(size_t)e.autoIdx].fireT >= 0
                        && (double)g_autoTrig[(size_t)e.autoIdx].fireT + e.delay <= (double)t
                        && autoDisarmed(e.autoIdx, mask, fireB)) {
                        any = true;
                        break;
                    }
                // By group (resolveOnGroups), from every group on -- or, for an object the reset left off for a
                // reason its switches do not hold, the one-list form from that.
                if (any)
                    onv = (disarmInit[i] != 0
                               ? resolveOnGroups(disarmEv[i].data(), disarmEv[i].size(), mask, fireB, t)
                               : resolveOn(false, disarmEv[i].data(), disarmEv[i].size(), mask, fireB, t))
                              ? 1 : 0;
                if (any && g_dynDbg == objs[i].uid) {   // --dyndbg: which switches it took, which were stopped
                    std::printf("tapdisarm t=%d uid=%d init=%d ->%d:", t, objs[i].uid, (int)disarmInit[i], (int)onv);
                    for (const OnEvent& e : disarmEv[i]) {
                        if (e.autoIdx >= 0 && (size_t)e.autoIdx < g_autoTrig.size()) {
                            const AutoTrig& A = g_autoTrig[(size_t)e.autoIdx];
                            if (A.fireT < 0 || A.fireT > t) continue;
                            std::printf(" u%d@%d:%d%s", A.uid, A.fireT, (int)e.on,
                                        autoDisarmed(e.autoIdx, mask, fireB) ? "X" : "");
                            for (const AutoDisarm& d : g_autoDisarm[(size_t)e.autoIdx])
                                if (d.box >= 0 && d.box < kTouchBits && (mask & touchBit(d.box)))
                                    std::printf("(b%d@%d:%d)", d.box, (int)fireB[d.box], (int)d.on);
                        } else if (e.box >= 0 && (mask & touchBit(e.box)))
                            std::printf(" box%d@%d:%d", e.box, (int)fireB[e.box], (int)e.on);
                    }
                    std::printf("\n");
                }
            }
            // --boxoff: out once a box that switches it off has fired in this group's world.
            if (i < offBox.size() && fireB)
                for (const uint8_t b : offBox[i])
                    if ((mask & touchBit((int)b)) && (int)fireB[b] <= t) { onv = 0; break; }
            on[i] = onv;
            objs[i].recOn = onv;   // --holdtap (object.hpp)
            if (objs[i].pendBox >= 0)   // --pressfresh (Obj::pendOff)
                objs[i].pendOff = !onv && !(mask & touchBit(objs[i].pendBox));
            if (g_dynDbg == objs[i].uid)
                std::printf("dyntrig t=%d uid=%d fired=%d m=%08x cx=%.3f "
                            "cy=%.3f hw=%.1f hh=%.1f on=%d\n",
                            t, objs[i].uid, (int)fired, (unsigned)m.word(0), cx, cy,
                            hw, hh, (int)onv);
            // --shiftstat: `shiftstat:` once per object the first time it is
            // placed (this is the DENOMINATOR -- the objects a collision
            // consumes; decoration never reaches Dynamics at all), and
            // `shiftnz:` the first time its re-timing is non-zero.
            //
            // TWO LINES ON PURPOSE. The first placement happens long before the
            // controlling trigger fires, and until it does `anchor` is -1 and
            // the shift reads 0 -- sampling only there says "every object is at
            // shift 0", which is how the first version of this counter reported
            // 2,094 of 2,094 while --shiftdbg had already shown uid 434 at
            // shift=1. An object that never prints a `shiftnz:` line is the
            // real shift-0 case.
            if (g_shiftStat) {
                if (shiftSaid.size() != objs.size())
                    shiftSaid.assign(objs.size(), 0);
                if (!(shiftSaid[i] & 1)) {
                    shiftSaid[i] |= 1;
                    std::printf("shiftstat: uid=%d bucket=%d t=%d\n",
                                objs[i].uid, (int)bucket[i], t);
                }
                if (shiftUsed != 0 && !(shiftSaid[i] & 2)) {
                    shiftSaid[i] |= 2;
                    std::printf("shiftnz: uid=%d bucket=%d shift=%d t=%d "
                                "anchor=%d recAnchor=%d\n",
                                objs[i].uid, (int)bucket[i], shiftUsed, t,
                                anchor, recAnchor);
                }
            }
            turnedBox(objs[i], bucket[i], (double)rotv,
                      i < everRot.size() && everRot[i] != 0);
            // a moving SLOPE carries its surface line (same rule as seek())
            if (objs[i].slope) {
                const double dy = cy - (double)baseCy[i];
                objs[i].sy0 = (double)baseSy0[i] + dy;
                objs[i].sy1 = (double)baseSy1[i] + dy;
            }
        }
        };
        forRuns(par, samePure ? nNonPure : objs.size(),
                [&](size_t b, size_t e) { place(b, e, false); });
        if (par) place(0, objs.size(), true);
    }

    // Put every object where it was at tick t. Cursors move forward with the
    // layer loop, so this is O(1) amortised; a backwards jump (--start, or the
    // witness resim starting over) rewinds and re-scans, which happens once.
    //
    // ROW t IS THE RIGHT ROW, and this is the note that stops the next person
    // re-deriving otherwise. GJBaseGameLayer::update runs processCommands
    // (0x238090) -> processMoveActionsStep (0x238388) -> checkCollisions
    // (0x23846e), and the mod's recorder writes its row inside the FIRST of
    // those, so the row written as T holds the objects from before that update's
    // move. It is tempting to conclude that a collision therefore saw row T+1.
    // It did not: the player state that collision produces does not appear in
    // the dump until row T+2 either, so the two offsets cancel and the pairing
    // is (player row T, object row T).
    //
    // Measured twice on 2026-09-03, both against the pairing being anything else
    // (a --dynphase flag that shifted the row by one was built, run and reverted):
    //   * lv19 t=20,601. A ball rides a surface descending 0.062 px/tick
    //     (uid 14011, cy 210.178, h 15): row t gives 210.178 + 7.5 + 15 =
    //     232.678, which is GD's dump to the last digit, for eleven ticks. The
    //     shifted row drops the support on the first tick and falls for five.
    //   * lv22 uid 434's firing edge at t=777, swept in GD at 0.05 px (the
    //     earlier 0.5 px sweep could not tell the two apart, which is how the
    //     shifted row briefly looked better):
    //         dy=0     GD (35.502, 35.552)   row t: 35.541 INSIDE   row t+1: 35.859
    //         dy=43.09 GD (32.052, 32.102)   row t: 32.130 (0.028)  row t+1: 31.616 (0.44)
    // The 0.028 px that is left at dy=43.09 is a residual of the portal
    // predicate, not of the row: see the two-gate contact shape.
    // A search that steps several lineages tick by tick through this one object (searchMontecarlo).
    // What one tick carries into the next is each object's final cy (seek's 1-tick advance reads it
    // for dcy and prevCy); everything else seek rewrites from the recording. So a lineage's world is
    // put back by restoring those and naming the tick they were placed for. forgetPlacement: the
    // next seek is a jump, as after an anchor, whatever was placed last.
    void forgetPlacement() { lastT = 1 << 30; }   // later than any tick: the next seek goes back
    void saveCarry(std::vector<double>& cy) const {
        cy.resize(objs.size());
        for (size_t i = 0; i < objs.size(); ++i) cy[i] = objs[i].cy;
    }
    void loadCarry(const std::vector<double>& cy, int tPrev) {
        for (size_t i = 0; i < objs.size() && i < cy.size(); ++i) objs[i].cy = cy[i];
        lastT = tPrev;
    }
    // --walkresume (cli.hpp): the per-object cursors back to the first row. A forward seek only
    // ever advances a cursor, so one left where a later tick put it (the search ran past the tick
    // a walk is resumed at) would read a row too late; from 0 the seek finds the same row an
    // unbroken walk would have.
    void resetCursors() { std::fill(cur.begin(), cur.end(), (size_t)0); }
    // --walkresume: what seek and applyTriggers leave on an object that the next ones do not
    // rewrite -- turnedBox's latch (`oriented` and the box it fixed) -- folded into `h`, so a
    // walk is resumed only into a world that carries the same.
    void foldSticky(uint64_t& h, bool withLastT = true) const {
        auto mix = [&h](const void* p, size_t n) {
            const unsigned char* b = static_cast<const unsigned char*>(p);
            for (size_t k = 0; k < n; ++k) { h ^= b[k]; h *= 1099511628211ull; }
        };
        for (const Obj& o : objs) {
            mix(&o.oriented, sizeof o.oriented);
            mix(&o.ohw, sizeof o.ohw); mix(&o.ohh, sizeof o.ohh);
            mix(&o.rc, sizeof o.rc); mix(&o.rs, sizeof o.rs);
        }
        if (withLastT) mix(&lastT, sizeof lastT);
    }
    void seek(int t) {
        if (objs.empty() || t == lastT) return;
        if ((g_moverExtend || g_moverExtendVel) && extOk.size() != objs.size()) prepareExtend();
        if (g_recEndExtend && recOk.size() != objs.size()) prepareRecEnd();
        // Every object is about to be rewritten, so whatever applyTriggers placed
        // is stale from here on (see pureEpoch).
        ++seekEpoch;
        if (byBucketN != objs.size()) rebuildBuckets();
        const bool back = (t < lastT);
        // dcy is PER-TICK surface motion, so the position-difference form is
        // only meaningful when this seek advanced exactly one tick. The FIRST
        // seek of an anchored replay jumps from the load position (lastT=-1)
        // straight to t0, and the difference was the platform's WHOLE journey:
        // lv19 t=21,800 anchored with dcy=136, which put every mover inside
        // the `0.6 + |dcy|` support tolerance and seated the ship on a lift
        // 91 px overhead. A jump (either direction) falls back to the
        // recording's own consecutive rows, same as the backward branch.
        const bool step1 = !back && (t == lastT + 1);
        lastStep1 = step1;
        lastT = t;
        // --dynobbrot: keep the loaded axes before the first turn overwrites them (outside the
        // split loop below, which would race on the resize).
        if (g_dynObbRot && obbBc0.size() != objs.size()) {
            obbBc0.resize(objs.size()); obbBs0.resize(objs.size());
            for (size_t i = 0; i < objs.size(); ++i) { obbBc0[i] = objs[i].bc; obbBs0[i] = objs[i].bs; }
        }
        // Each object reads its own recording and writes its own slots, so the
        // loop splits (forRuns) -- unless --dyndbg could print from it.
        forRuns(g_dynDbg == -1 && !anyUidM1, objs.size(), [&](size_t b, size_t e) {
        for (size_t i = b; i < e; ++i) {
            const auto& sm = samples[i];
            if (sm.empty()) continue;
            size_t c = back ? 0 : cur[i];
            while (c + 1 < sm.size() && sm[c + 1].t <= t) ++c;
            cur[i] = c;
            const DynSample& s = sm[c];
            // per-tick y motion of the SURFACE itself (see Obj::dcy).
            // On a BACKWARDS seek (a --start anchor, or the witness resim
            // starting over) there is no previous position to subtract, and
            // zeroing it costs the rider its first step: the ride only fires
            // when `dcy != 0`, so a player anchored mid-ride skips one surface
            // step and stays that much behind for the whole ride. Measured on
            // lv22's ball corridor (2026-08-14): the anchored resim followed
            // 166 ticks and then reported dy=-1.53 growing 0.17/tick with
            // dvy=+0.0000 -- one missed step of a floor that sinks at exactly
            // that rate. The recording knows the answer: when the sample landed
            // on THIS tick and the one before it is the tick before, their
            // difference IS the per-tick motion.
            // last FINAL position before this tick's placement overwrites it
            // (applyTriggers reads it to rebuild dcy for controlled objects)
            if (i < prevCy.size())
                prevCy[i] = step1 ? objs[i].cy : (float)s.cy;
            objs[i].dcy =
                step1 ? ((double)s.cy - (double)objs[i].cy)
                      : (c > 0 && sm[c].t == t && sm[c - 1].t == t - 1)
                            ? ((double)sm[c].cy - (double)sm[c - 1].cy)
                            : 0.0;
            objs[i].cx = s.cx; objs[i].cy = s.cy;
            // --recinterp: between two rows at most kRecInterpGap ticks apart,
            // the object was moving the whole time -- the recorder simply did
            // not write the ticks whose step was under its 0.05 px threshold
            // (grouptrace kEps). Holding the older row made a slow mover step
            // every other tick: lv22 uid5825 at t=6,118..6,124 has rows at
            // 6,118 / 6,119 / 6,121 / 6,123 while GD's rider moves every tick.
            // A longer gap is a real stop and keeps the hold.
            if (s.t < t && c + 1 < sm.size()
                && sm[c + 1].t - s.t <= kRecInterpGap
                && !recStallGap(sm, c)) {   // --recstall
                const DynSample& n = sm[c + 1];
                const double u = (double)(t - s.t) / (double)(n.t - s.t);
                objs[i].cx = (float)((double)s.cx + u * ((double)n.cx - (double)s.cx));
                objs[i].cy = (float)((double)s.cy + u * ((double)n.cy - (double)s.cy));
                if (step1 && i < prevCy.size())
                    objs[i].dcy = (double)objs[i].cy - (double)prevCy[i];
            }
            // --moverextend: past the recording's last row (the attempt's death) a constant-velocity mover
            // goes on (a custom level's uid 139041: rows end at t=10,272, +1.4707 px/tick; GD's spike is at
            // 12,339.088 on 10,273 and kills a mini wave the held row misses by 1.46 px).
            const bool mvExt = (g_moverExtend || g_moverExtendVel) && i < extOk.size() && extOk[i]
                               && c + 1 == sm.size() && t > s.t;
            if (mvExt) {
                const float k = (float)std::min(t - s.t, extCap[i]);
                objs[i].cx = s.cx + extVx[i] * k;
                objs[i].cy = s.cy + extVy[i] * k;
                if (step1 && i < prevCy.size())
                    objs[i].dcy = (double)objs[i].cy - (double)prevCy[i];
            }
            float hwNow = s.hw, hhNow = s.hh;
            double rotNow = (double)s.rot;
            // --recendextend: past the last row by the last steady step, where --moverextend did not place it.
            if (!mvExt && c + 1 == sm.size())
                if (const int k = recEndTicks(i, t)) {
                    objs[i].cx = s.cx + recVx[i] * (float)k;
                    objs[i].cy = s.cy + recVy[i] * (float)k;
                    hwNow += recVhw[i] * (float)k; hhNow += recVhh[i] * (float)k;
                    rotNow += (double)recVr[i] * k;
                    if (step1 && i < prevCy.size())
                        objs[i].dcy = (double)objs[i].cy - (double)prevCy[i];
                }
            objs[i].hw = hwNow; objs[i].hh = hhNow;
            on[i] = recOnAt(sm, c, t);   // --offafter
            objs[i].recOn = s.on;   // --holdtap (object.hpp)
            objs[i].pendOff = 0;   // --pressfresh: applyTriggers decides it per group
            turnedBox(objs[i], bucket[i], rotNow,
                      i < everRot.size() && everRot[i] != 0);
            obbLatch(i, t, rotNow);   // --rotobblatch
            // --dynobbrot: the hazard's turned box at this row's angle. objs[i].rot is still the
            // placed angle (turnedBox writes it for portals and speed portals only), and GD's
            // rotation is clockwise, so the axis turns by minus the difference.
            if (g_dynObbRot && objs[i].obbOk && i < everRot.size() && everRot[i]
                && i < obbBc0.size()) {
                const double a = -(rotNow - objs[i].rot) * (3.14159265358979 / 180.0);
                const double ca = std::cos(a), sa = std::sin(a);
                objs[i].bc = obbBc0[i] * ca - obbBs0[i] * sa;
                objs[i].bs = obbBc0[i] * sa + obbBs0[i] * ca;
            }
            if (objs[i].slope) {
                const double dy = (double)objs[i].cy - (double)baseCy[i];
                objs[i].sy0 = (double)baseSy0[i] + dy;
                objs[i].sy1 = (double)baseSy1[i] + dy;
            }
            if (g_dynDbg == objs[i].uid)
                std::printf("dynseek t=%d uid=%d cur=%zu/%zu cy=%.3f "
                            "(sample t=%d) at=%p\n",
                            t, objs[i].uid, c, sm.size(), (double)objs[i].cy,
                            s.t, (const void*)&objs[i]);
        }
        });
    }
    // A MOVING object that is also being TURNED. The recording's hw/hh are half
    // the axis-aligned BOUND, which for a turned box is not the shape at all --
    // and the model was using it as the hitbox.
    //
    // Measured on lv22 (2026-08-13): the spider portal uid 434 (really 34x86) is
    // turned by a trigger from -2.45 deg at t=768 to -39.2 deg at t=810, so its
    // recorded bound grows 37.6x87.4 -> 80.7x88.1. The model fired it THREE
    // TICKS early, which put the whole rest of the run 66 px off in y and was the
    // first divergence on both the seeded clear and every runR3/runR4 plan.
    // A 2-D injection sweep (py/portal_sweep2.py) pinned the real region: at
    // -26 deg only 1 of 168 grid points inside the bound actually fires.
    //
    // The nominal size comes back out of the bound and the angle (the same
    // inversion Obj::oriented already documents for static rotations):
    //   a = W|c| + H|s| , b = W|s| + H|c|  ->  W = (a c - b s)/cos2t , etc.
    // Only PORTALS get this: `oriented` is wired to the portal test and tuned
    // there, and a 90-degree multiple leaves the bound correct anyway.
    // --rotspeed: ...and the SPEED portals, whose pass has the same oriented test.
    static void turnedBox(Obj& o, uint8_t bucketOf, double rotDeg,
                          bool everRot = false) {
        if (bucketOf != PORT && !(g_rotSpeed && bucketOf == SPEED)) return;
        o.rot = rotDeg;
        // [2026-08-18] **Do not throw away the real box built from w0/h0 at
        // load.** This used to drop oriented to 0 on every seek and rebuild it
        // by inverting the bounding box, and since the inversion does not work
        // near 45 degrees (|cos2θ| < 0.1), grouped portals alone were **firing
        // on the bounding box**. Measured on lv21 uid24194 (id111 UFO portal,
        // rot=43, w0/h0 = 34x86, bound 167x172): GD switches at t=18,087
        // (x=23,929), but the model did so at the bound's left edge at t=18,075
        // (x=23,910) -- 12 ticks / 19px early. Turning **does not change the
        // box's size**, so only the angle needs to be redrawn.
        if (o.oriented) {
            const double th2 = rotDeg * 3.14159265358979 / 180.0;
            o.rc = std::cos(-th2); o.rs = std::sin(-th2);
            return;
        }
        const double om = std::fabs(std::fmod(rotDeg, 90.0));
        // [2026-08-25] A portal a trigger ROTATES is "turned" in GD's sense for
        // the whole run, even while its angle still reads 0 -- and GD's turned
        // branch tests the PLAYER's oriented box too, which an axis-aligned
        // AABB test throws away. That is a real difference: a cube is always
        // spinning, so the window it presents to the portal is not a square.
        //
        // Measured on lv22's spider portal uid 1156 (2085,255, 34x86, static
        // rot 0, rotated by its group from t=1,612). The model fired it at
        // t=1,582 and GD at t=1,583, leaving a CONSTANT 0.087/tick vy error
        // (0.216 cube gravity vs 0.129 spider) that never washes out -- lv22
        // cold has been stuck behind it. Injection sweep at t=1,582, x fixed at
        // 2,053.519, varying only y (GD fires / does not):
        //     y=213.874 FIRES    y=209.000 FIRES    y=208.000 FIRES
        //     y=207.500 no       y=201.138 no       y=199.472 no
        // A plain AABB cannot produce that boundary at ANY half-width (the same
        // y=201.138 fires one tick later at x=2,054.817), but the two-box SAT
        // with the player's box turned by its rotation TWO TICKS BACK -- which
        // is exactly the `pRotHere` the portal loop already computes -- puts the
        // edge at y=208.00 against an observed bracket of (207.500, 208.000].
        // Four more points (including the t=1,583 fire at margin 0.075) fit the
        // same line.
        //
        // Only the near-0 case is taken: at ~90 degrees the bound's halves are
        // the box's SWAPPED, and no measurement covers that.
        // [2026-08-25] OFF BY DEFAULT (--rotport). The measurement above stands,
        // but switching it on cost lv22's cold run 41% -> 15% (x=9,827 ->
        // x=3,689, and the DP calls went 8s -> 94s): with 133 of lv22's moving
        // objects rotate-targeted, every one of them starts testing against the
        // player's turned box, and the route landscape that comes out is worse
        // even though each individual verdict is closer to GD. The measured
        // boundary is also contaminated by something this note cannot yet
        // separate -- GD fires this portal at t=1,583 on one attempt and
        // t=1,582 on another with the SAME plan, so the object's phase carries
        // across attempts. Re-open it with a per-object gate, not a global one.
        if (om <= 0.5 && everRot && o.hw > 0.5 && o.hh > 0.5) {
            o.oriented = 1;
            o.ohw = o.hw; o.ohh = o.hh;   // axis aligned: the bound IS the box
            // ...but ohw/ohh are the UNTURNED box's halves, and at an odd quarter turn (`om`, the
            // angle mod 90, is near 0 at 90 and 270 degrees too) the bound's halves are that box's
            // swapped. Kept unswapped, the box was turned twice -- here and on every redraw (the
            // oriented branch above turns it by the whole angle). On custom level C the
            // gravity portal uid 28482 (id 11, 25x75 at scale 0.5, rot 90, group 1) is 37.5 x 12.5
            // in GD's getObjectRect, and GD flips both bodies on a 0.7-1.1 px corner contact at
            // x~5,075, y~905 (t=3,910/3,912); the model tested 12.5 x 37.5, missed it, and every
            // plan through there died at t~4,024 while the recorder patched vy tick by tick.
            // --no-rotport tests the bound as an AABB and matches GD there; so does this swap.
            if (((long long)std::llround(rotDeg / 90.0)) & 1)
                std::swap(o.ohw, o.ohh);
            const double th0 = rotDeg * 3.14159265358979 / 180.0;
            o.rc = std::cos(-th0); o.rs = std::sin(-th0);
            return;
        }
        if (om <= 0.5 || om >= 89.5) return;   // axis aligned: bound == shape
        const double th = rotDeg * 3.14159265358979 / 180.0;
        const double c = std::fabs(std::cos(th)), s = std::fabs(std::sin(th));
        const double det = c * c - s * s;      // cos 2t
        if (std::fabs(det) < 0.1) return;      // near 45 deg: not invertible
        const double W = ((double)o.hw * c - (double)o.hh * s) / det;
        const double H = ((double)o.hh * c - (double)o.hw * s) / det;
        if (W <= 0.5 || H <= 0.5) return;
        o.oriented = 1;
        o.ohw = W; o.ohh = H;
        o.rc = std::cos(-th); o.rs = std::sin(-th);
    }
    // Dynamic objects cannot live in the sorted-by-cx index, so their window is
    // a linear scan. It is bounded by the number of moving objects in the level
    // (1,731 at worst) and runs once per layer, not once per state.
    // Turn every moving object into a gameplay frame (see turnObj / frameLevel).
    // `collect` is a linear scan and the parallel arrays are indexed by position,
    // so nothing has to be re-sorted -- only the geometry and the OFFSETS turn.
    // Offsets are vectors, so they take the same linear map without the
    // translation part, which toFrame already is (a pure rotation about 0).
    // Not handled: lockToPlayerX (autoLock). A lock copies the player's WORLD x
    // onto the object, which in a turned frame is a v, not a u -- lv22's two
    // locks are both in the un-rotated switch band, so this stays a documented
    // hole rather than a guess.
    void turn(int f) {
        if ((f & 3) == 0) return;
        auto rotVec = [&](float& dx, float& dy) {
            double u, v;
            toFrame(f, (double)dx, (double)dy, u, v);
            dx = (float)u; dy = (float)v;
        };
        for (size_t i = 0; i < objs.size(); ++i) {
            for (DynSample& s : samples[i]) {
                double u, v;
                toFrame(f, (double)s.cx, (double)s.cy, u, v);
                s.cx = (float)u; s.cy = (float)v;
                if (f & 1) std::swap(s.hw, s.hh);
            }
            turnObj(objs[i], f);
            baseCy[i] = objs[i].cy;
            baseSy0[i] = (float)objs[i].sy0;
            baseSy1[i] = (float)objs[i].sy1;
            rotVec(trigDx[i], trigDy[i]);
            rotVec(autoDx[i], autoDy[i]);
            for (AutoPart& p : autoParts[i]) rotVec(p.dx, p.dy);
            for (AutoPart& p : touchParts[i]) {
                rotVec(p.dx, p.dy);
                // --movetarget: the destination turns with the frame, and in an
                // odd frame a one-axis move is on the other axis.
                rotVec(p.tdx, p.tdy);
                if ((f & 1) && (p.tmode == 2 || p.tmode == 3))
                    p.tmode = (uint8_t)(5 - p.tmode);
            }
        }
        std::fill(cur.begin(), cur.end(), (size_t)0);
        lastT = -1;
        ++seekEpoch;
    }

    void collect(uint8_t b, double x0, double x1,
                 std::vector<const Obj*>& out) const {
        // Same objects, same order, fewer visits (see byBucket). --dyndbg prints
        // one line per object of every bucket, so it keeps the full walk.
        if (g_dynDbg < 0 && byBucketN == objs.size() && b < byBucket.size()) {
            for (const uint32_t i : byBucket[b]) {
                // --pressfresh (Obj::pendOff); --holdtap: the state decides (object.hpp holdTapGone)
                if (!on[i] && !objs[i].pendOff && !(g_holdTap && objs[i].holdTapX > 0.f)) continue;
                const Obj& o = objs[i];
                if (o.cx + o.hw < x0 || o.cx - o.hw > x1) continue;
                out.push_back(&o);
            }
            return;
        }
        for (size_t i = 0; i < objs.size(); ++i) {
            const Obj& o = objs[i];
            if (g_dynDbg >= 0 && o.uid == g_dynDbg)
                std::printf("dyncollect uid=%d b=%d/%d on=%d cx=%.3f hw=%.3f "
                            "win=[%.1f,%.1f] -> %s\n",
                            o.uid, (int)bucket[i], (int)b, (int)on[i], o.cx,
                            o.hw, x0, x1,
                            (bucket[i] != b || !on[i])          ? "SKIP(bucket/on)"
                            : (o.cx + o.hw < x0 || o.cx - o.hw > x1)
                                ? "SKIP(window)" : "in");
            if (bucket[i] != b || (!on[i] && !o.pendOff && !(g_holdTap && o.holdTapX > 0.f))) continue;
            if (o.cx + o.hw < x0 || o.cx - o.hw > x1) continue;
            out.push_back(&o);
        }
    }
};

}  // namespace dp


