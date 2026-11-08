#include "display.h"
#include "core.h"

// Caph's screen: the cell terrace split down every cell. The left half is oscillator A at that phase,
// the right half its detuned twin B, read a drifting phase lead later. Each strike starts them together;
// while notes sound they slide apart and back at a beat rate that follows Detune, and at rest they ease
// back into whole cells. Shape morphs the
// profile from triangle to pulse to saw, Width moves the pulse edge and the triangle's apex. Going back,
// each row eases toward the one in front (~140 ms), so Sweep's strike rolls away. At rest nothing redraws.
namespace caph {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const i32 left_px = 14, span_px = 360;
const i32 row_dx = 11, row_dy = 15, base_y = 180, height_px = 30;
const i32 half_dx = 11;                         // B's centre sits 11 px right of A's
const u32 redraw_budget = 64;
const i32 tau_front = 82;                       // Q10 seconds: the front row eases toward the sound over ~80 ms
const i32 tau_row = 143;                        // Q10 seconds: each row eases toward the one in front over ~140 ms
}

inline void display_reset(DisplayState& state) {
    state.notes.reset();
    state.started = 0;
    state.cursor = 0;
    state.offset = 0;
    state.env = 0;
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace screen;
    using opxy::draw::clamp;
    const i32 dt = clamp(input.elapsed, 0, 102);
    state.notes.update(input);
    if (!state.started) state.serial = input.note_serial;
    if (input.note_serial != state.serial) {
        state.serial = input.note_serial;
        if (!input.reduced_motion) { state.env = opxy::q31::maximum; state.offset = 0; }
    }
    state.env -= (state.env >> 9) * dt;
    if (state.env < 0) state.env = 0;

    const i32 detune = input.slots[c_detune] < 0 ? 0 : input.slots[c_detune] >> 16;  // Q15
    const bool sounding = input.held_notes != 0 && input.held_notes <= 128;
    // The beat: up to 2 cycles a second at 35 cents (half a 220 Hz note's, to keep the screen calm), only
    // while notes sound.
    // At rest the halves ease back together (the shorter way round), so the split means beating now.
    if (sounding && !input.reduced_motion) state.offset += static_cast<u32>(detune) * 2u * static_cast<u32>(dt) * 128u;
    else {
        // Back together over ~0.25 s, then exactly together (an exponential alone takes seconds to stop).
        const i32 lead = static_cast<i32>(state.offset);
        const i32 next = lead - ((lead >> 8) * dt);
        state.offset = static_cast<u32>(next > -(1 << 22) && next < (1 << 22) ? 0 : next);
    }

    // Detune's caption in cents (no printf on the device): "0ct" .. "35ct".
    const u32 ct = static_cast<u32>((detune * 35 + 16384) >> 15);
    char cents[opxy::draw::header_capacity] = {};
    u32 w = 0;
    if (ct >= 10) cents[w++] = static_cast<char>('0' + ct / 10);
    cents[w++] = static_cast<char>('0' + ct % 10);
    cents[w++] = 'c';
    cents[w] = 't';
    opxy::draw::header(frame, c_detune, cents);

    // The front row now.
    const i32 shape = cz::swept(input.slots[c_shape] < 0 ? 0 : input.slots[c_shape] >> 16, input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    const u32 d = duty16(input.slots[c_width] < 0 ? 0 : input.slots[c_width] >> 16);
    const bool snap = !state.started || input.reduced_motion;
    const i32 kf = cz::ease_rate(dt, tau_front), kr = cz::ease_rate(dt, tau_row);
    if (!snap)  // each row eases toward the one in front, back row first
        for (u32 j = rows - 1; j > 0; --j)
            for (u32 h = 0; h < 2; ++h)
                for (u32 i = 0; i < cols; ++i) state.cv[j][h][i] = static_cast<opxy::i16>(cz::ease(state.cv[j][h][i], state.cv[j - 1][h][i], kr));
    for (u32 i = 0; i < cols; ++i) {  // the front row eases toward the sound
        const u32 x = ((2 * i + 1) << 16) / (2 * cols);
        const i32 a = naive_shape(x, shape, d), b = naive_shape((x + (state.offset >> 16)) & 0xffffu, shape, d);
        state.cv[0][0][i] = static_cast<opxy::i16>(snap ? a : cz::ease(state.cv[0][0][i], a, kf));
        state.cv[0][1][i] = static_cast<opxy::i16>(snap ? b : cz::ease(state.cv[0][1][i], b, kf));
    }
    if (snap)
        for (u32 j = 1; j < rows; ++j)
            for (u32 h = 0; h < 2; ++h)
                for (u32 i = 0; i < cols; ++i) state.cv[j][h][i] = state.cv[0][h][i];

    u32 left = state.started ? redraw_budget : halves;
    for (u32 k = 0; k < halves && left; ++k) {
        const u32 n = (state.cursor + k) % halves, c = n >> 1, h = n & 1, j = rows - 1 - c / cols, i = c % cols;
        const i32 v = state.cv[j][h][i];
        const i32 x_q2 = left_px * 4 + static_cast<i32>(((2 * i + 1) * span_px * 2) / cols) + static_cast<i32>(j) * row_dx * 4 + (h ? half_dx * 2 : -half_dx * 2);
        const i32 y_q2 = (base_y - static_cast<i32>(j) * row_dy) * 4 - ((v * height_px * 4) >> 15);
        // A lighter than B, both brighter with height and dimmer going back.
        const i32 lum = (h ? 125 : 160) + (((h ? 70 : 80) * v) >> 15) - 6 * static_cast<i32>(j);
        const u32 shade = static_cast<u32>(clamp(((lum - 30) * 2) / 27, 0, 15));
        const u32 want = static_cast<u32>(y_q2 >> 2) | shade << 16;
        if (state.started && want == state.drawn[n]) continue;
        opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), unit, shade, opxy::draw::fill_and_stroke);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % halves;
    }
    state.started = 1;
}
}
