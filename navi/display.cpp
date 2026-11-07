#include "display.h"
#include "core.h"

// Navi's screen, revision pass 01: five ways to draw the bent cosine with the family's flat floating
// tops (NAVI_VARIANT, 1..5). All share the front-row sampling and the history rows (70 ms apart, so
// Sweep's strike rolls away); each variant only says where every node goes.
//  1 Vector handle: a plain terrace of the bent wave, and one bright large square floating at the
//    inflection point (d across, v up), with its trail going back through the rows.
//  2 Cycle bands: a low, flat field seen from above; cells alternate filled and outlined with each
//    cosine cycle the bend squeezes in, so the formant's cycles can be counted as bands.
//  3 Stacks: 12 columns by 3 rows, each column a pile of small squares as tall as the wave there.
//  4 Two densities: before the inflection, half-size cells at twice the density (where the formant
//    runs fast); after it, full cells; a gap at the seam.
//  5 Ghost pairs: each solid cell floats at the bent height over an outlined ghost at the plain
//    cosine's, so the bend is the gap between them.
namespace navi {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const u32 redraw_budget = 64;
const i32 history_step = 72;  // Q10 seconds between rows (70 ms)

struct Place { i32 x_q2, y_q2, scale; u32 shade; opxy::draw::Paint paint; bool shown; };

inline u32 grey(i32 lum) { return static_cast<u32>(opxy::draw::clamp(((lum - 30) * 2) / 27, 0, 15)); }
inline i32 plain(u32 k) { return -cz::sine(((2 * k + 1) << 26) + 0x40000000u); }  // −cos at (k + 0.5)/32

// Where node n goes, for this variant.
inline Place where(const DisplayState& s, u32 n) {
    Place p = {0, 0, unit, 0, opxy::draw::fill_and_stroke, false};
    const i32 left = 14, span = 360;
#if NAVI_VARIANT == 1
    // 16 columns x 9 rows of the wave (nodes 0..143, back row first), then 9 handles (144..152).
    if (n < 144) {
        const u32 j = 8 - n / 16, i = n % 16;
        const i32 w = s.wave[j][2 * i];
        p.x_q2 = left * 4 + static_cast<i32>(((2 * i + 1) * span * 2) / 16) + static_cast<i32>(j) * 44;
        p.y_q2 = (178 - static_cast<i32>(j) * 15) * 4 - ((w * 26 * 4) >> 15);
        p.shade = grey(110 + ((60 * w) >> 15) - 6 * static_cast<i32>(j));
        p.shown = true;
    } else if (n < 153) {
        const u32 j = 152 - n;  // back handles first
        p.x_q2 = left * 4 + static_cast<i32>((static_cast<u32>(s.d[j]) * span * 4) >> 16) + static_cast<i32>(j) * 44;
        // Floats 16 px over its row, plus up to 40 px as v rises (v / 8); never into the header.
        p.y_q2 = opxy::draw::clamp((178 - static_cast<i32>(j) * 15 - 16) * 4 - static_cast<i32>((static_cast<u32>(s.v[j]) * 4 * 40) >> 16), 28 * 4, 220 * 4);
        p.scale = unit + 640 - static_cast<i32>(j) * 70;
        p.shade = grey(j ? 200 - 16 * static_cast<i32>(j) : 245);
        p.shown = true;
    }
#elif NAVI_VARIANT == 2
    // A low, flat field: 16 columns x 9 rows packed close (rows 7 px apart), height kept small.
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
#elif NAVI_VARIANT == 3
    // 12 columns x 3 rows x 7 squares: a pile per column, as many squares as the wave is tall.
    if (n < 252) {
        const u32 piece = n % 7, c = n / 7, j = 2 - c / 12, i = c % 12;
        const u32 k = ((2 * i + 1) * samples) / 24;
        const i32 w = s.wave[j * 3][k];
        const u32 count = static_cast<u32>(((w + 32768) * 7 + 32767) >> 16);  // 0 .. 7
        p.x_q2 = (24 + static_cast<i32>(i) * 36 + static_cast<i32>(j) * 22) * 4;
        p.y_q2 = (206 - static_cast<i32>(j) * 34 - static_cast<i32>(piece) * 6) * 4;
        p.scale = 512;
        p.shade = grey(110 + static_cast<i32>(piece) * 18 - 14 * static_cast<i32>(j));
        p.shown = piece < count;
    }
#elif NAVI_VARIANT == 4
    // 7 rows x 32 slots: before the seam every slot, half size; after it every other slot, full size.
    if (n < 224) {
        const u32 j = 6 - n / 32, k = n % 32;
        const u32 seam = (static_cast<u32>(s.d[j]) * 32) >> 16;
        const i32 w = s.wave[j][k];
        p.x_q2 = left * 4 + static_cast<i32>(((2 * k + 1) * span * 2) / 32) + static_cast<i32>(j) * 40;
        p.y_q2 = (180 - static_cast<i32>(j) * 19) * 4 - ((w * 28 * 4) >> 15);
        if (k < seam) { p.scale = 512; p.shown = true; p.shade = grey(170 + ((60 * w) >> 15) - 9 * static_cast<i32>(j)); }
        else if (k > seam && ((k - seam) & 1u) == 0) {
            p.x_q2 += span * 2 / 32;  // centred on its pair of slots
            p.shown = true;
            p.shade = grey(105 + ((60 * w) >> 15) - 7 * static_cast<i32>(j));
        }
    }
#else
    // 16 columns x 7 rows of pairs: an outlined ghost at the plain cosine, then the solid bent cell.
    if (n < 224) {
        const u32 pair = n / 2, ghost = (n & 1u) == 0, j = 6 - pair / 16, i = pair % 16;
        const i32 w = ghost ? plain(2 * i) : s.wave[j][2 * i];
        p.x_q2 = (8 + static_cast<i32>(((2 * i + 1) * 420) / 32)) * 4 + static_cast<i32>(j) * 28;
        p.y_q2 = (190 - static_cast<i32>(j) * 15) * 4 - ((w * 30 * 4) >> 15);
        const i32 bend = s.wave[j][2 * i] - plain(2 * i);
        p.shade = ghost ? grey(150 - 9 * static_cast<i32>(j)) : grey(120 + (((bend < 0 ? -bend : bend) * 110) >> 16) - 8 * static_cast<i32>(j));
        p.paint = ghost ? opxy::draw::stroke : opxy::draw::fill_and_stroke;
        p.shown = true;
    }
#endif
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

    // The front row: the bent wave at 32 points, which cycle of the bent phase each falls in, and (d, v).
    const auto slot = [&](u32 c) { return input.slots[c] < 0 ? 0 : input.slots[c] >> 16; };
    const i32 formant = cz::swept(slot(c_formant), input.slots[c_sweep] < 0 ? 0 : input.slots[c_sweep], state.env);
    const u32 d = inflect16(slot(c_inflect)), vv = formant16(formant, slot(c_range));
    const Bend b = bend(d, vv);
    for (u32 k = 0; k < samples; ++k) {
        const u32 x = (2 * k + 1) << 10, g = bent(x, b);
        state.wave[0][k] = static_cast<opxy::i16>(sample(x, b));
        state.cycle[0][k] = static_cast<opxy::u8>(g >> 16);
    }
    state.d[0] = static_cast<opxy::u16>(d > 65535u ? 65535u : d);
    state.v[0] = static_cast<opxy::u16>(vv >> 3);
    const auto copy_row = [&](u32 to, u32 from) {
        for (u32 k = 0; k < samples; ++k) { state.wave[to][k] = state.wave[from][k]; state.cycle[to][k] = state.cycle[from][k]; }
        state.d[to] = state.d[from];
        state.v[to] = state.v[from];
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
