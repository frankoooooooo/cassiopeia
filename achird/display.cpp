#include "display.h"
#include "core.h"

// Achird's screen: the cell terrace as stepped blocks. The 16 columns are the cycle's 16 Walsh steps,
// exactly as the voice builds them, so the terrace is made of flat blocks, and wherever a level steps
// from the column before, a lit riser stands between them. Tilt brings in higher-sequency steps (more
// risers), Balance trades odd against even, Fold wraps tall blocks back down. Going back, each row is the
// terrace 70 ms earlier, so Sweep's strike rolls away. At rest nothing redraws.
namespace achird {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const i32 left_px = 14, span_px = 360;
const i32 row_dx = 11, row_dy = 15, base_y = 180, height_px = 28;
const i32 riser_px = 32;                        // the authored riser's height at scale 1
const u32 redraw_budget = 64;
const i32 history_step = 72;
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
    const auto slot = [&](u32 c) { return input.slots[c] < 0 ? 0 : input.slots[c] >> 16; };
    state.notes.update(input);
    if (!state.started) state.serial = input.note_serial;
    if (input.note_serial != state.serial) {
        state.serial = input.note_serial;
        if (!input.reduced_motion) state.env = opxy::q31::maximum;
    }
    state.env -= (state.env >> 9) * dt;
    if (state.env < 0) state.env = 0;

    const i32 tilt = cz::swept(slot(c_tilt), input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    levels(state.lv[0], tilt, slot(c_balance), slot(c_fold), 0);
    if (!state.started || input.reduced_motion) {
        for (u32 j = 1; j < rows; ++j)
            for (u32 i = 0; i < cols; ++i) state.lv[j][i] = state.lv[0][i];
    } else if ((state.clock += dt) >= history_step) {
        state.clock = state.clock >= 4 * history_step ? 0 : state.clock - history_step;
        for (u32 j = rows - 1; j > 0; --j)
            for (u32 i = 0; i < cols; ++i) state.lv[j][i] = state.lv[j - 1][i];
    }

    u32 left = state.started ? redraw_budget : parts;
    for (u32 k = 0; k < parts && left; ++k) {
        const u32 n = (state.cursor + k) % parts, c = n >> 1, top = n & 1, j = rows - 1 - c / cols, i = c % cols;
        const i32 v = state.lv[j][i];
        const i32 cx_q2 = left_px * 4 + static_cast<i32>(((2 * i + 1) * span_px * 2) / cols) + static_cast<i32>(j) * row_dx * 4;
        const i32 base_q2 = (base_y - static_cast<i32>(j) * row_dy) * 4;
        u32 want;
        if (top) {
            const i32 y_q2 = base_q2 - ((v * height_px * 4) >> 15);
            const i32 lum = 155 + ((85 * v) >> 15) - 6 * static_cast<i32>(j);
            const u32 shade = static_cast<u32>(clamp(((lum - 30) * 2) / 27, 0, 15));
            want = static_cast<u32>(y_q2 & 0xffff) | shade << 16 | 1u << 31;
            if (state.started && want == state.drawn[n]) continue;
            opxy::draw::place(frame, n, cx_q2 * (unit / 4), y_q2 * (unit / 4), unit, shade, opxy::draw::fill_and_stroke);
        } else {  // the riser up from the lower of this level and the column before's, at the cell's front-left corner
            const i32 prev = state.lv[j][(i + cols - 1) % cols];
            const i32 low = v < prev ? v : prev, rise_q2 = (((v > prev ? v - prev : prev - v) * height_px * 4) >> 15);
            const i32 y_q2 = base_q2 - ((low * height_px * 4) >> 15) + 12;  // the tops' front edge sits 3 px low
            const i32 scale = (rise_q2 * unit) / (riser_px * 4);
            const u32 shade = static_cast<u32>(clamp(((200 - 12 * static_cast<i32>(j) - 30) * 2) / 27, 0, 15));
            want = rise_q2 < 4 ? 0x7fffffffu : static_cast<u32>(y_q2 & 0xffff) | static_cast<u32>(scale >> 3) << 16 | shade << 26;
            if (state.started && want == state.drawn[n]) continue;
            if (rise_q2 < 4) opxy::draw::hide(frame, n);  // under a pixel: no riser
            else opxy::draw::place(frame, n, (cx_q2 - 66) * (unit / 4), y_q2 * (unit / 4), clamp(scale, 32, 2 * unit), shade, opxy::draw::fill_and_stroke);
        }
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % parts;
    }
    state.started = 1;
}
}
