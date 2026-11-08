#include "display.h"

// Ruchbah's screen: the cell terrace of masses, one cell per stretch of the ring the scanner reads. The
// screen strikes and steps its own copy of the voice's ring at the voice's rate (once per audio block,
// 689 a second), so the cells move as the sound does: a pluck's bump travels and spreads, Stiff speeds
// it, Damping settles it, Excite chooses what each note throws in (turning it strikes once, to show the
// shape). Each cell swells with how fast its masses move: at rest a field of small, still cells; a
// strike sends a swell through it. Calm by design (the first version was frantic): heights are the ring
// averaged over neighbouring masses and eased over ~0.1 s, the swell follows a motion envelope that
// falls over ~0.4 s, the gain stops at 2× (grey keeps the true level), and each row behind eases toward
// the one in front (~140 ms), so motion shows as a wake. While nothing sounds and the ring is still,
// the terrace eases into a faint pose of the Excite shape (display only), then nothing redraws.
namespace ruchbah {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const i32 left_px = 14, span_px = 360;
const i32 row_dx = 11, row_dy = 15, base_y = 182, height_px = 28;
const u32 redraw_budget = cells;               // every node may update each frame (a small frame)
const i32 tau_row = 143;                      // Q10 seconds: each row eases toward the one in front over ~140 ms
const i32 steps_per_second = 689;             // 44100 / 64, the voice's block rate
}

inline void display_reset(DisplayState& state) {
    state.notes.reset();
    state.started = 0;
    state.cursor = 0;
    state.env = 0;
    state.due = 0;
    state.ring.lfsr = 0xace1u;
    state.rest_mix = 0;
    state.gain = 256;
    for (u32 i = 0; i < cols; ++i) { state.calm[i] = 0; state.energy[i] = 0; }
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace screen;
    using opxy::draw::clamp;
    const i32 dt = clamp(input.elapsed, 0, 102);
    const auto slot = [&](u32 c) { return input.slots[c] < 0 ? 0 : input.slots[c] >> 16; };
    state.notes.update(input);
    if (!state.started) state.serial = input.note_serial;
    // A note, the first frame, or a new Excite choice strikes the ring, so the knob shows its shape.
    const u32 kind = cz::choice(input.slots[c_excite], excite_count);
    if (!state.started || kind != state.shown) {
        excite(state.ring, kind);
        Ring pose;
        pose.lfsr = 0x5eedu;  // a fixed noise pose, so the rest frame doesn't shimmer
        excite(pose, kind);
        for (u32 i = 0; i < masses; ++i) state.pose[i] = pose.x[i];
    }
    state.shown = kind;
    if (input.note_serial != state.serial) {
        state.serial = input.note_serial;
        excite(state.ring, kind);
        if (!input.reduced_motion) state.env = opxy::q31::maximum;
    }
    // Step the ring as the voice does: the same stiffness (swept), damping and rate.
    if (input.reduced_motion) state.due = 0;
    else state.due += dt * steps_per_second;
    for (u32 guard = 0; state.due >= 1024 && guard < 80; ++guard) {
        state.due -= 1024;
        const i32 stiff = cz::swept(slot(c_stiff), input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
        step(state.ring, stiffness(stiff), keep(slot(c_damping)));
        state.env = opxy::q31::multiply(state.env, cz::sweep_decay);
    }
    if (state.due >= 1024) state.due = 0;  // a late frame: drop the backlog rather than stall

    // Display gain from the ring's peak (1..4×, eased), and the rest pose's weight.
    i32 peak = 1;
    for (u32 i = 0; i < masses; ++i) { const i32 a = state.ring.x[i] < 0 ? -state.ring.x[i] : state.ring.x[i]; peak = a > peak ? a : peak; }
    const i32 want_gain = clamp((22000 * 256) / peak, 256, 512);  // 1..2x
    state.gain += ((want_gain - state.gain) * dt) >> 8;
    const bool held = input.held_notes != 0 && input.held_notes <= 128;
    const i32 want_mix = !held && peak < 1600 ? 32767 : 0;
    state.rest_mix = input.reduced_motion ? want_mix : state.rest_mix + (((want_mix - state.rest_mix) >> 4) * dt >> 4);  // ~0.25 s, so it rests within ~3 s of release
    Ring pose;
    for (u32 i = 0; i < masses; ++i) pose.x[i] = state.pose[i];
    const u32 mass = 0x08000000u;  // one mass along the ring, as a scan phase
    for (u32 i = 0; i < cols; ++i) {
        const u32 at = (2 * i + 1) * (0x80000000u / cols);  // each column's centre round the ring
        // The ring here averaged with its neighbours either side, so single-mass ripples don't jitter.
        const i32 ring = (scan(state.ring, at - mass) + 2 * scan(state.ring, at) + scan(state.ring, at + mass)) >> 2;
        const i32 target = clamp(((ring * state.gain) >> 8) + ((((scan(pose, at) * 14746) >> 15) * state.rest_mix) >> 15), -32767, 32767);
        // Ease toward it over about 0.1 s (a frame's step is dt / 104 of the way).
        state.calm[i] = static_cast<opxy::i16>(!state.started || input.reduced_motion ? target : state.calm[i] + (((target - state.calm[i]) * clamp(dt * 10, 0, 1024)) >> 10));
        state.cv[0][i] = state.calm[i];
        // Motion: the ring's speed here (velocities, averaged the same way), into a follower that rises at
        // once and falls over about 0.4 s; it swells the cell 0.7x .. 1.15x.
        const u32 m = at >> 27, m2 = (m + 1) & (masses - 1), m0 = (m - 1) & (masses - 1);
        i32 speed = (state.ring.v[m0] + 2 * state.ring.v[m] + state.ring.v[m2]) >> 2;
        speed = clamp(((speed < 0 ? -speed : speed) * state.gain) >> 5, 0, 32767);
        i32 e = state.energy[i];
        e = speed > e ? speed : e - ((e * dt) >> 9);
        state.energy[i] = static_cast<opxy::i16>(e < 0 ? 0 : e);
        state.cs[0][i] = static_cast<opxy::u16>(717 + ((state.energy[i] * 461) >> 15));
    }
    if (!state.started || input.reduced_motion) {
        for (u32 j = 1; j < rows; ++j)
            for (u32 i = 0; i < cols; ++i) { state.cv[j][i] = state.cv[0][i]; state.cs[j][i] = state.cs[0][i]; }
    } else {  // each row eases toward the one in front (the front row already eases, through calm[])
        const i32 kr = cz::ease_rate(dt, tau_row);
        for (u32 j = rows - 1; j > 0; --j)
            for (u32 i = 0; i < cols; ++i) {
                state.cv[j][i] = static_cast<opxy::i16>(cz::ease(state.cv[j][i], state.cv[j - 1][i], kr));
                state.cs[j][i] = static_cast<opxy::u16>(cz::ease(state.cs[j][i], state.cs[j - 1][i], kr));
            }
    }

    u32 left = state.started ? redraw_budget : cells;
    for (u32 k = 0; k < cells && left; ++k) {
        const u32 n = (state.cursor + k) % cells, j = rows - 1 - n / cols, i = n % cols;
        const i32 v = state.cv[j][i], scale = state.cs[j][i];
        const i32 x_q2 = left_px * 4 + static_cast<i32>(((2 * i + 1) * span_px * 2) / cols) + static_cast<i32>(j) * row_dx * 4;
        const i32 y_q2 = (base_y - static_cast<i32>(j) * row_dy) * 4 - ((v * height_px * 4) >> 15);
        // Grey follows height, and moving cells are brighter: a swell reads as a passing light.
        const i32 lum = 120 + ((70 * v) >> 15) + ((scale - 717) * 60) / 461 - 6 * static_cast<i32>(j);
        const u32 shade = static_cast<u32>(clamp(((lum - 30) * 2) / 27, 0, 15));
        const u32 want = static_cast<u32>((y_q2 >> 2) & 0xffff) | static_cast<u32>(scale >> 5) << 16 | shade << 24;  // whole pixels
        if (state.started && want == state.drawn[n]) continue;
        opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), scale, shade, opxy::draw::fill_and_stroke);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % cells;
    }
    state.started = 1;
}
}
