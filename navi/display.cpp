#include "display.h"
#include "core.h"

// Navi's screen: banded stacks (the owner's pick: stacks, with the cycle bands in them). Twelve columns
// by three rows of piles of the family's small floating tops, each pile as tall as the bent wave there,
// and filled or outlined by which cosine cycle of the bent phase the column falls in. Formant multiplies
// the cycles before the inflection, so the piles alternate solid and hollow there; Inflect moves where the
// alternation ends. Rows go back in time (210 ms apart), so Sweep's strike rolls away.
namespace navi {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const u32 redraw_budget = 64;
const i32 history_step = 72;  // Q10 seconds between rows (70 ms)

struct Place { i32 x_q2, y_q2, scale; u32 shade; opxy::draw::Paint paint; bool shown; };

inline u32 grey(i32 lum) { return static_cast<u32>(opxy::draw::clamp(((lum - 30) * 2) / 27, 0, 15)); }

// Where node n goes: n = ((2 − row)·12 + column)·7 + piece, back row first, each pile bottom-up.
inline Place where(const DisplayState& s, u32 n) {
    Place p = {0, 0, 512, 0, opxy::draw::fill_and_stroke, false};
    const u32 piece = n % 7, c = n / 7, j = 2 - c / 12, i = c % 12;
    const u32 k = ((2 * i + 1) * samples) / 24;  // the sample at the column's centre
    const i32 w = s.wave[j * 3][k];
    const bool hollow = s.cycle[j * 3][k] & 1u;
    const u32 count = static_cast<u32>(((w + 32768) * 7 + 32767) >> 16);  // 0 .. 7 pieces
    p.x_q2 = (24 + static_cast<i32>(i) * 36 + static_cast<i32>(j) * 22) * 4;
    p.y_q2 = (206 - static_cast<i32>(j) * 34 - static_cast<i32>(piece) * 6) * 4;
    p.shade = grey((hollow ? 150 : 110) + static_cast<i32>(piece) * (hollow ? 12 : 18) - 14 * static_cast<i32>(j));  // outlines need more light to read
    p.paint = hollow ? opxy::draw::stroke : opxy::draw::fill_and_stroke;
    p.shown = piece < count;
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
