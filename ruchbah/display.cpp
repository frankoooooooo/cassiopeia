#include "display.h"

// Ruchbah's screen: the cell terrace of masses, one cell per stretch of the ring the scanner reads. The
// screen strikes and steps its own copy of the voice's ring at the voice's rate (once per audio block,
// 689 a second), so the cells move as the sound does, and each swells with how fast its masses are moving:
// at rest a field of small, still cells; a strike sends a swell through it: a pluck's bump travels and spreads, Stiff
// speeds it, Damping settles it, Excite chooses what each note throws in (turning it strikes once, to
// show the shape). Rows going back are 35 ms
// apart, so the motion shows as a wake. Heights carry up to 4× gain as the strike disperses (grey keeps the
// true level). While nothing sounds and the ring is still, the terrace eases into a faint pose of the
// Excite shape (display only), then nothing redraws.
namespace ruchbah {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const i32 left_px = 14, span_px = 360;
const i32 row_dx = 11, row_dy = 15, base_y = 182, height_px = 28;
const u32 redraw_budget = 56;
const i32 history_step = 36;                  // Q10 seconds between rows (35 ms)
const i32 steps_per_second = 689;             // 44100 / 64, the voice's block rate
}

inline void display_reset(DisplayState& state) {
    state.notes.reset();
    state.started = 0;
    state.cursor = 0;
    state.env = 0;
    state.clock = 0;
    state.due = 0;
    state.ring.lfsr = 0xace1u;
    state.rest_mix = 0;
    state.gain = 256;
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
    const i32 want_gain = clamp((26000 * 256) / peak, 256, 1024);
    state.gain += ((want_gain - state.gain) * dt) >> 8;
    const bool held = input.held_notes != 0 && input.held_notes <= 128;
    const i32 want_mix = !held && peak < 1600 ? 32767 : 0;
    state.rest_mix = input.reduced_motion ? want_mix : state.rest_mix + (((want_mix - state.rest_mix) >> 6) * dt >> 4);
    Ring pose;
    for (u32 i = 0; i < masses; ++i) pose.x[i] = state.pose[i];
    for (u32 i = 0; i < cols; ++i) {
        const u32 at = (2 * i + 1) * (0x80000000u / cols);  // each column's centre round the ring
        const i32 v = ((scan(state.ring, at) * state.gain) >> 8) + ((((scan(pose, at) * 14746) >> 15) * state.rest_mix) >> 15);
        state.cv[0][i] = static_cast<opxy::i16>(clamp(v, -32767, 32767));
        // Speed: the ring's velocity there (the same interpolation), with the display gain: 0.55× still .. 1.25×.
        const u32 m = at >> 27, m2 = (m + 1) & (masses - 1);
        const i32 f = static_cast<i32>((at >> 12) & 0x7fffu);
        i32 speed = state.ring.v[m] + (((state.ring.v[m2] - state.ring.v[m]) * f) >> 15);
        speed = (speed < 0 ? -speed : speed) * state.gain >> 5;  // a full-size swell near 1/8 of full-scale per step
        state.cs[0][i] = static_cast<opxy::u16>(563 + ((clamp(speed, 0, 32767) * 717) >> 15));
    }
    if (!state.started || input.reduced_motion) {
        for (u32 j = 1; j < rows; ++j)
            for (u32 i = 0; i < cols; ++i) { state.cv[j][i] = state.cv[0][i]; state.cs[j][i] = state.cs[0][i]; }
    } else if ((state.clock += dt) >= history_step) {
        state.clock = state.clock >= 4 * history_step ? 0 : state.clock - history_step;
        for (u32 j = rows - 1; j > 0; --j)
            for (u32 i = 0; i < cols; ++i) { state.cv[j][i] = state.cv[j - 1][i]; state.cs[j][i] = state.cs[j - 1][i]; }
    }

    u32 left = state.started ? redraw_budget : cells;
    for (u32 k = 0; k < cells && left; ++k) {
        const u32 n = (state.cursor + k) % cells, j = rows - 1 - n / cols, i = n % cols;
        const i32 v = state.cv[j][i], scale = state.cs[j][i];
        const i32 x_q2 = left_px * 4 + static_cast<i32>(((2 * i + 1) * span_px * 2) / cols) + static_cast<i32>(j) * row_dx * 4;
        const i32 y_q2 = (base_y - static_cast<i32>(j) * row_dy) * 4 - ((v * height_px * 4) >> 15);
        // Grey follows height, and moving cells are brighter: a swell reads as a passing light.
        const i32 lum = 120 + ((70 * v) >> 15) + ((scale - 563) * 60) / 717 - 6 * static_cast<i32>(j);
        const u32 shade = static_cast<u32>(clamp(((lum - 30) * 2) / 27, 0, 15));
        const u32 want = static_cast<u32>(y_q2 & 0xffff) | static_cast<u32>(scale >> 5) << 16 | shade << 24;
        if (state.started && want == state.drawn[n]) continue;
        opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), scale, shade, opxy::draw::fill_and_stroke);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % cells;
    }
    state.started = 1;
}
}
