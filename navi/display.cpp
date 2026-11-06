#include "display.h"
#include "core.h"

// Navi's screen: the cell terrace as tilted slats. Each cell is a thin slat turned to the wave's slope
// across its column and stretched so its ends sit at the wave's height either side, while its width
// across the screen stays inside the column: separate slats that never overlap their neighbours. The
// formant's fast cosines read as runs of steep, alternating slats. The column holding the inflection point draws brightest: a seam across
// the terrace that Inflect moves. Formant and Range add teeth before the seam. Going back, each row is
// the terrace 70 ms earlier, so Sweep's strike rolls away. At rest nothing redraws.
namespace navi {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const i32 left_px = 14, span_px = 360;
const i32 row_dx = 11, row_dy = 15, base_y = 180, height_px = 30;
const i32 plate_px = 16;                         // the authored slat's length at scale 1 (column pitch 22.5)
const u32 redraw_budget = 48;
const i32 history_step = 72;                     // Q10 seconds between rows (70 ms)

inline u32 isqrt(u32 v) {
    u32 r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return r;
}
}

inline void display_reset(DisplayState& state) {
    state.notes.reset();
    state.started = 0;
    state.cursor = 0;
    state.env = 0;
    state.clock = 0;
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace screen;
    using opxy::draw::clamp;
    const i32 dt = clamp(input.elapsed, 0, 102);
    state.notes.update(input);
    if (!state.started) state.serial = input.note_serial;
    if (input.note_serial != state.serial) {
        state.serial = input.note_serial;
        if (!input.reduced_motion) state.env = opxy::q31::maximum;
    }
    state.env -= (state.env >> 9) * dt;
    if (state.env < 0) state.env = 0;

    const auto slot = [&](u32 c) { return input.slots[c] < 0 ? 0 : input.slots[c] >> 16; };
    const i32 formant = cz::swept(slot(c_formant), input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    const u32 d = inflect16(slot(c_inflect));
    const Bend b = bend(d, formant16(formant, slot(c_range)));
    for (u32 e = 0; e <= cols; ++e) state.ev[0][e] = static_cast<opxy::i16>(sample(e == cols ? 65535u : (e << 16) / cols, b));
    state.seam[0] = static_cast<opxy::u16>((d * cols) >> 16);

    if (!state.started || input.reduced_motion) {
        for (u32 j = 1; j < rows; ++j) {
            state.seam[j] = state.seam[0];
            for (u32 e = 0; e <= cols; ++e) state.ev[j][e] = state.ev[0][e];
        }
    } else if ((state.clock += dt) >= history_step) {
        state.clock = state.clock >= 4 * history_step ? 0 : state.clock - history_step;
        for (u32 j = rows - 1; j > 0; --j) {
            state.seam[j] = state.seam[j - 1];
            for (u32 e = 0; e <= cols; ++e) state.ev[j][e] = state.ev[j - 1][e];
        }
    }

    u32 left = state.started ? redraw_budget : cells;
    for (u32 k = 0; k < cells && left; ++k) {
        const u32 n = (state.cursor + k) % cells, j = rows - 1 - n / cols, i = n % cols;
        const i32 vl = state.ev[j][i], vr = state.ev[j][i + 1];
        // Ends at the edge heights, in px Q4: run across the plate, rise between its ends.
        // The wave falls (vl − vr)·height across the 22.5 px column; over the slat's 16 px that is 32/45 of it.
        const i32 run = plate_px * 16, rise = ((((vl - vr) * height_px * 16) >> 15) * 32) / 45;  // vl above vr: the right end sits lower
        const i32 len = static_cast<i32>(isqrt(static_cast<u32>(run * run + rise * rise)));
        const i32 cosv = (run * unit) / len, sinv = (rise * unit) / len;  // clockwise when the right end is lower
        const i32 scale = clamp((len * unit) / run, unit, 2 * unit);
        const i32 x_q2 = left_px * 4 + static_cast<i32>(((2 * i + 1) * span_px * 2) / cols) + static_cast<i32>(j) * row_dx * 4;
        const i32 y_q2 = (base_y - static_cast<i32>(j) * row_dy) * 4 - ((((vl + vr) >> 1) * height_px * 4) >> 15);
        const bool seam = i == state.seam[j];
        const i32 lum = seam ? 236 - 8 * static_cast<i32>(j) : 70 + ((85 * ((vl + vr) >> 1)) >> 15) + 85 - 6 * static_cast<i32>(j);
        const u32 shade = static_cast<u32>(clamp(((lum - 30) * 2) / 27, 0, 15));
        const u32 want = static_cast<u32>(y_q2 & 1023) | static_cast<u32>((sinv + 1024) & 2047) << 10 | shade << 21;
        if (state.started && want == state.drawn[n]) continue;
        opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), scale, shade, opxy::draw::fill_and_stroke);
        opxy::draw::turn(frame, n, opxy::draw::Turn{cosv, sinv});
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % cells;
    }
    state.started = 1;
}
}
