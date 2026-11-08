#include "display.h"
#include "core.h"

// Navi's screen: banded stacks (the owner's pick: stacks, with the cycle bands in them). Twelve columns
// by three rows of piles of the family's small floating tops, each pile as tall as the bent wave there,
// and filled or outlined by which cosine cycle of the bent phase the column falls in. Formant multiplies
// the cycles before the inflection, so the piles alternate solid and hollow there; Inflect moves where the
// alternation ends. Rows go back in time, each easing toward the one in front, so Sweep's strike rolls away.
namespace navi {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const u32 redraw_budget = 64;
const i32 tau_front = 82;                       // Q10 seconds: the front row eases toward the sound over ~80 ms
const i32 tau_row = 143;                        // Q10 seconds: each row eases toward the one in front over ~140 ms
inline u32 grey(i32 lum) { return static_cast<u32>(opxy::draw::clamp(((lum - 30) * 2) / 27, 0, 15)); }
}

inline void display_reset(DisplayState& state) {
    state.notes.reset();
    state.started = 0;
    state.cursor = 0;
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
        if (!input.reduced_motion) state.env = opxy::q31::maximum;
    }
    state.env -= (state.env >> 9) * dt;
    if (state.env < 0) state.env = 0;

    // The front slot: the bent wave at each column's centre, and which cycle of the bent phase it is in.
    const auto slot = [&](u32 c) { return input.slots[c] < 0 ? 0 : input.slots[c] >> 16; };
    const i32 formant = cz::swept(slot(c_formant), input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    const Bend b = bend(inflect16(slot(c_inflect)), formant16(formant, slot(c_range)));
    const bool snap = !state.started || input.reduced_motion;
    const i32 kf = cz::ease_rate(dt, tau_front), kr = cz::ease_rate(dt, tau_row);
    if (!snap)  // each slot eases toward the one in front; the cycle each column is in follows straight through
        for (u32 j = history - 1; j > 0; --j)
            for (u32 i = 0; i < cols; ++i) {
                state.wave[j][i] = static_cast<opxy::i16>(cz::ease(state.wave[j][i], state.wave[j - 1][i], kr));
                state.cycle[j][i] = state.cycle[j - 1][i];
            }
    for (u32 i = 0; i < cols; ++i) {  // the front slot eases toward the sound
        const u32 x = (2 * (((2 * i + 1) * 32) / 24) + 1) << 10;  // the nearest 1/32 of the cycle to its centre
        const i32 w = sample(x, b);
        state.wave[0][i] = static_cast<opxy::i16>(snap ? w : cz::ease(state.wave[0][i], w, kf));
        state.cycle[0][i] = static_cast<opxy::u8>(bent(x, b) >> 16);
    }
    if (snap)
        for (u32 j = 1; j < history; ++j)
            for (u32 i = 0; i < cols; ++i) { state.wave[j][i] = state.wave[0][i]; state.cycle[j][i] = state.cycle[0][i]; }
    // Pile heights with hysteresis: a pile gains or loses a square only once the wave is 0.3 of a square
    // past the boundary, so a value resting on one doesn't flicker.
    for (u32 j = 0; j < rows; ++j)
        for (u32 i = 0; i < cols; ++i) {
            const i32 raw = ((state.wave[j * 3][i] + 32768) * 7 * 256) >> 16;  // squares, Q8
            state.count[j][i] = static_cast<opxy::u8>(snap ? (raw + 128) >> 8 : cz::sticky(state.count[j][i], raw, 77));
        }

    u32 left = state.started ? redraw_budget : nodes;
    for (u32 k = 0; k < nodes && left; ++k) {
        const u32 n = (state.cursor + k) % nodes, piece = n % pieces, c = n / pieces, j = rows - 1 - c / cols, i = c % cols;
        const bool hollow = state.cycle[j * 3][i] & 1u, shown = piece < state.count[j][i];
        const i32 x_q2 = (24 + static_cast<i32>(i) * 36 + static_cast<i32>(j) * 22) * 4;
        const i32 y_q2 = (206 - static_cast<i32>(j) * 34 - static_cast<i32>(piece) * 6) * 4;
        // Outlines need more light to read than filled tops.
        const u32 shade = grey((hollow ? 150 : 110) + static_cast<i32>(piece) * (hollow ? 12 : 18) - 14 * static_cast<i32>(j));
        const opxy::draw::Paint paint = hollow ? opxy::draw::stroke : opxy::draw::fill_and_stroke;
        const u32 want = shown ? (shade | static_cast<u32>(hollow) << 4) : 0xffu;
        if (state.started && want == state.drawn[n]) continue;
        if (shown) opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), 512, shade, paint);
        else opxy::draw::hide(frame, n);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % nodes;
    }
    state.started = 1;
}
}
