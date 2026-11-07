#include "display.h"
#include "core.h"

// Navi's screen: cycle bands (revision pass 01, v2, the owner's pick). A low, flat field of the family's
// floating tops seen from above, 16 columns by 9 rows of recent history (70 ms apart, so Sweep's strike
// rolls away). Height is the bent wave; cells alternate filled and outlined with each cosine cycle the
// bend squeezes in, so Formant's cycles can be counted as bands and Inflect moves where they end.
namespace navi {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const u32 redraw_budget = 64;
const i32 history_step = 72;  // Q10 seconds between rows (70 ms)

struct Place { i32 x_q2, y_q2, scale; u32 shade; opxy::draw::Paint paint; bool shown; };

inline u32 grey(i32 lum) { return static_cast<u32>(opxy::draw::clamp(((lum - 30) * 2) / 27, 0, 15)); }

// Where node n goes: back row first, node n = (8 − row)·16 + column.
inline Place where(const DisplayState& s, u32 n) {
    Place p = {0, 0, unit, 0, opxy::draw::fill_and_stroke, false};
    const i32 left = 14, span = 360;
    // A low, flat field: rows 9 px apart and 6 px across, heights kept small (±14 px).
    if (n < 144) {
        const u32 j = 8 - n / 16, i = n % 16;
        const i32 w = s.wave[j][2 * i];
        const bool odd = s.cycle[j][2 * i] & 1u;
        p.x_q2 = left * 4 + static_cast<i32>(((2 * i + 1) * span * 2) / 16) + static_cast<i32>(j) * 24;
        p.y_q2 = (196 - static_cast<i32>(j) * 9) * 4 - ((w * 14 * 4) >> 15);
        p.shade = grey((odd ? 120 : 205) - 7 * static_cast<i32>(j));
        p.paint = odd ? opxy::draw::stroke : opxy::draw::fill_and_stroke;
        p.shown = true;
    }

    return p;
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

    // The front row: the bent wave at 32 points, and which cycle of the bent phase each falls in.
    const auto slot = [&](u32 c) { return input.slots[c] < 0 ? 0 : input.slots[c] >> 16; };
    const i32 formant = cz::swept(slot(c_formant), input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    const Bend b = bend(inflect16(slot(c_inflect)), formant16(formant, slot(c_range)));
    for (u32 k = 0; k < samples; ++k) {
        const u32 x = (2 * k + 1) << 10, g = bent(x, b);
        state.wave[0][k] = static_cast<opxy::i16>(sample(x, b));
        state.cycle[0][k] = static_cast<opxy::u8>(g >> 16);
    }
    const auto copy_row = [&](u32 to, u32 from) {
        for (u32 k = 0; k < samples; ++k) { state.wave[to][k] = state.wave[from][k]; state.cycle[to][k] = state.cycle[from][k]; }
    };
    if (!state.started || input.reduced_motion) {
        for (u32 j = 1; j < history; ++j) copy_row(j, 0);
    } else if ((state.clock += dt) >= history_step) {
        state.clock = state.clock >= 4 * history_step ? 0 : state.clock - history_step;
        for (u32 j = history - 1; j > 0; --j) copy_row(j, j - 1);
    }

    u32 left = state.started ? redraw_budget : node_budget;
    for (u32 k = 0; k < node_budget && left; ++k) {
        const u32 n = (state.cursor + k) % node_budget;
        const Place p = where(state, n);
        const u32 want = p.shown ? (static_cast<u32>(p.y_q2 & 0x3ff) | static_cast<u32>(p.x_q2 & 0x7ff) << 10 | static_cast<u32>((p.scale >> 6) & 31) << 21 | p.shade << 26 | static_cast<u32>(p.paint == opxy::draw::stroke) << 30)
                                 : 0xffffffffu;
        if (state.started && want == state.drawn[n]) continue;
        if (p.shown) opxy::draw::place(frame, n, p.x_q2 * (unit / 4), p.y_q2 * (unit / 4), p.scale, p.shade, p.paint);
        else opxy::draw::hide(frame, n);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % node_budget;
    }
    state.started = 1;
}
}
