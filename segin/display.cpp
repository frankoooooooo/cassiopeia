#include "display.h"
#include "core.h"

// Segin's screen: the cell terrace as burst islands. Each cycle (drawn for a nominal A2; the screen has
// no pitch) fires the burst: every pulse is an island of cells rising from the floor, each lower than
// the last as Decay bites, and the cell where a pulse begins is lit so the pulses can be counted. Past
// the burst the cycle is floor, drawn as dim outlines. Formant packs more, narrower islands into the
// cycle and is captioned in Hz. Rows going back are 70 ms apart, so Sweep's strike rolls away.
namespace segin {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const i32 left_px = 14, span_px = 360;
const i32 row_dx = 11, row_dy = 15, base_y = 196, height_px = 48;
const u32 redraw_budget = 48;
const i32 history_step = 72;
const u32 nominal_hz = 110;  // A2: low enough that the default burst leaves floor

// "776" or "1.2k": the formant's caption.
inline void caption(char* out, u32 hz) {
    u32 w = 0;
    if (hz < 1000) {
        if (hz >= 100) out[w++] = static_cast<char>('0' + hz / 100);
        if (hz >= 10) out[w++] = static_cast<char>('0' + hz / 10 % 10);
        out[w++] = static_cast<char>('0' + hz % 10);
    } else {
        const u32 tenths = (hz + 50) / 100;
        out[w++] = static_cast<char>('0' + tenths / 10);
        out[w++] = '.';
        out[w++] = static_cast<char>('0' + tenths % 10);
        out[w++] = 'k';
    }
    out[w] = 0;
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
    const auto slot = [&](u32 c) { return input.slots[c] < 0 ? 0 : input.slots[c] >> 16; };
    state.notes.update(input);
    if (!state.started) state.serial = input.note_serial;
    if (input.note_serial != state.serial) {
        state.serial = input.note_serial;
        if (!input.reduced_motion) state.env = opxy::q31::maximum;
    }
    state.env -= (state.env >> 9) * dt;
    if (state.env < 0) state.env = 0;

    char hz[opxy::draw::header_capacity] = {};
    caption(hz, formant_hz(slot(c_formant)));
    opxy::draw::header(frame, c_formant, hz);

    // The front row: pulses per cycle R = formant / nominal note (at least one), in Q8.
    const i32 formant = cz::swept(slot(c_formant), input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    u32 ratio = (formant_hz(formant) << 8) / nominal_hz;
    if (ratio < 256) ratio = 256;
    const u32 pulses = 1 + cz::choice(input.slots[c_pulses], max_pulses);
    const i32 keep = decay_keep(slot(c_decay));
    u32 last_n = 0xffffffffu;
    for (u32 i = 0; i < cols; ++i) {
        const u32 t = ((2 * i + 1) * ratio) / (2 * cols);  // pulses into the cycle, Q8
        const u32 n = t >> 8;
        if (n >= pulses) { state.cv[0][i] = -1; state.lead[0][i] = 0; last_n = n; continue; }
        i32 amp = 32767;
        for (u32 k = 0; k < n; ++k) amp = (amp * keep) >> 15;
        state.cv[0][i] = static_cast<opxy::i16>((amp * hump((t & 255u) << 24)) >> 15);
        state.lead[0][i] = n != last_n;
        last_n = n;
    }
    if (!state.started || input.reduced_motion) {
        for (u32 j = 1; j < rows; ++j)
            for (u32 i = 0; i < cols; ++i) { state.cv[j][i] = state.cv[0][i]; state.lead[j][i] = state.lead[0][i]; }
    } else if ((state.clock += dt) >= history_step) {
        state.clock = state.clock >= 4 * history_step ? 0 : state.clock - history_step;
        for (u32 j = rows - 1; j > 0; --j)
            for (u32 i = 0; i < cols; ++i) { state.cv[j][i] = state.cv[j - 1][i]; state.lead[j][i] = state.lead[j - 1][i]; }
    }

    u32 left = state.started ? redraw_budget : cells;
    for (u32 k = 0; k < cells && left; ++k) {
        const u32 n = (state.cursor + k) % cells, j = rows - 1 - n / cols, i = n % cols;
        const i32 v = state.cv[j][i];
        const bool floor = v < 0;
        const i32 x_q2 = left_px * 4 + static_cast<i32>(((2 * i + 1) * span_px * 2) / cols) + static_cast<i32>(j) * row_dx * 4;
        const i32 y_q2 = (base_y - static_cast<i32>(j) * row_dy) * 4 - (floor ? 0 : ((v * height_px * 4) >> 15));
        const i32 lum = floor ? 58 - 3 * static_cast<i32>(j)
                      : state.lead[j][i] ? 222 - 7 * static_cast<i32>(j) : 90 + ((110 * v) >> 15) - 7 * static_cast<i32>(j);
        const u32 shade = static_cast<u32>(clamp(((lum - 30) * 2) / 27, 0, 15));
        const opxy::draw::Paint paint = floor ? opxy::draw::stroke : opxy::draw::fill_and_stroke;
        const u32 want = static_cast<u32>(y_q2 & 0xffff) | shade << 16 | static_cast<u32>(paint) << 20;
        if (state.started && want == state.drawn[n]) continue;
        opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), unit, shade, paint);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % cells;
    }
    state.started = 1;
}
}
