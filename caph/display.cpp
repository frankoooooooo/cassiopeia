#include "display.h"
#include "core.h"

// Caph's screen: stacks. Twelve columns by three rows of piles, each pile a column of small floating tops
// as tall as the pair's summed wave there, so Shape's profile (triangle, pulse, saw) is the skyline and
// Width moves its edge. Each strike starts the pair together; while notes sound, B drifts against A at a
// beat rate that follows Detune, so the piles rise and fall as the pair beats; at rest they ease back.
// The rows go back in time (210 ms apart), so Sweep's strike rolls away. At rest nothing redraws.
namespace caph {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const u32 redraw_budget = 64;
const i32 history_step = 72;  // Q10 seconds between history slots (70 ms); rows are every third
inline u32 grey(i32 lum) { return static_cast<u32>(opxy::draw::clamp(((lum - 30) * 2) / 27, 0, 15)); }
}

inline void display_reset(DisplayState& state) {
    state.notes.reset();
    state.started = 0;
    state.cursor = 0;
    state.offset = 0;
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
        if (!input.reduced_motion) { state.env = opxy::q31::maximum; state.offset = 0; }
    }
    state.env -= (state.env >> 9) * dt;
    if (state.env < 0) state.env = 0;

    const i32 detune = input.slots[c_detune] < 0 ? 0 : input.slots[c_detune] >> 16;  // Q15
    const bool sounding = input.held_notes != 0 && input.held_notes <= 128;
    // The beat: up to 4 cycles a second at 35 cents (about a 220 Hz note's), only while notes sound;
    // at rest the pair eases back together (the shorter way round).
    if (sounding && !input.reduced_motion) state.offset += static_cast<u32>(detune) * 4u * static_cast<u32>(dt) * 128u;
    else {
        const i32 lead = static_cast<i32>(state.offset);
        state.offset = static_cast<u32>(lead - ((lead >> 9) * dt));
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

    // The front slot: A and B at each column's centre, summed.
    const i32 shape = cz::swept(input.slots[c_shape] < 0 ? 0 : input.slots[c_shape] >> 16, input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    const u32 d = duty16(input.slots[c_width] < 0 ? 0 : input.slots[c_width] >> 16);
    for (u32 i = 0; i < cols; ++i) {
        const u32 x = ((2 * i + 1) << 16) / (2 * cols);
        state.mix[0][i] = static_cast<opxy::i16>((naive_shape(x, shape, d) + naive_shape((x + (state.offset >> 16)) & 0xffffu, shape, d)) >> 1);
    }
    if (!state.started || input.reduced_motion) {
        for (u32 j = 1; j < history; ++j)
            for (u32 i = 0; i < cols; ++i) state.mix[j][i] = state.mix[0][i];
    } else if ((state.clock += dt) >= history_step) {
        state.clock = state.clock >= 4 * history_step ? 0 : state.clock - history_step;
        for (u32 j = history - 1; j > 0; --j)
            for (u32 i = 0; i < cols; ++i) state.mix[j][i] = state.mix[j - 1][i];
    }

    u32 left = state.started ? redraw_budget : nodes;
    for (u32 k = 0; k < nodes && left; ++k) {
        const u32 n = (state.cursor + k) % nodes, piece = n % pieces, c = n / pieces, j = rows - 1 - c / cols, i = c % cols;
        const i32 m = state.mix[j * 3][i];
        const u32 count = static_cast<u32>(((m + 32768) * 7 + 32767) >> 16);  // 0 .. 7 pieces
        const bool shown = piece < count;
        const i32 x_q2 = (24 + static_cast<i32>(i) * 36 + static_cast<i32>(j) * 22) * 4;
        const i32 y_q2 = (206 - static_cast<i32>(j) * 34 - static_cast<i32>(piece) * 6) * 4;
        const u32 shade = grey(110 + static_cast<i32>(piece) * 18 - 14 * static_cast<i32>(j));
        const u32 want = shown ? shade : 0xffu;
        if (state.started && want == state.drawn[n]) continue;
        if (shown) opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), 512, shade, opxy::draw::fill_and_stroke);
        else opxy::draw::hide(frame, n);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % nodes;
    }
    state.started = 1;
}
}
