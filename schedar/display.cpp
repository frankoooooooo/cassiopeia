#include "display.h"
#include "core.h"

// Schedar's screen: the cell terrace with warped columns. Across is the bent phase: the 17 column
// edges sit where the phase map reaches 0, 1/16, … 1, so cells crowd at the knee and stretch through
// the slow half, and each cell's height is the cosine there. Waves with no map (Dbl, Res 1–3) space
// their columns by where the wave does its work (cumulative |change|). Width pushes every column left of
// the held gap. Going back, each row eases toward the one in front (~140 ms), so Sweep's strike rolls
// away from the front. Grey follows height. At rest the rows converge and nothing redraws.
namespace schedar {
namespace screen {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

const i32 left_px = 14, span_px = 360;          // the front row's extent; each row back shifts right
const i32 row_dx = 11, row_dy = 15, base_y = 180, height_px = 32;  // front troughs at 212, back peaks at 28
const i32 cell_px = 20;                          // the authored top's width at scale 1
const u32 redraw_budget = cells;                 // every node may update each frame (a small frame)
const i32 tau_front = 82;                       // Q10 seconds: the front row eases toward the sound over ~80 ms
const i32 tau_row = 143;                        // Q10 seconds: each row eases toward the one in front over ~140 ms

inline i32 cosine_at(u32 g16) { return cz::sine((g16 << 16) + 0x40000000u); }

// The front row now: 17 column edges (Q16 phase) → each cell's centre, width scale and height.
inline void front_row(DisplayState& s, const opxy::draw::Input& in) {
    const i32 amount = in.slots[c_amount] < 0 ? 0 : in.slots[c_amount] >> 16;
    const u32 wave = cz::choice(in.slots[c_wave], wave_count);
    const u32 au = static_cast<u32>(cz::swept(amount, in.slots[c_sweep] < 0 ? 0 : in.slots[c_sweep], s.env));
    u32 edge[cols + 1];
    i32 height[cols];
    if (is_mapped(wave)) {
        u32 x[6], g[6];
        const unsigned n = map_points(wave, au, x, g);
        for (u32 t = 0; t <= cols; ++t) {  // invert the map: where does g reach t/16?
            const u32 target = t << 12;
            u32 at = 65536u;
            for (unsigned k = 0; k + 1 < n; ++k)
                if (g[k + 1] > g[k] && target >= g[k] && target <= g[k + 1]) {  // 32-bit only: no 64-bit divide links
                    const u32 dg = (g[k + 1] - g[k]) >> 2;
                    at = x[k] + (dg ? (((target - g[k]) >> 1) * ((x[k + 1] - x[k]) >> 1)) / dg : 0);
                    break;
                }
            edge[t] = at;
        }
        for (u32 i = 0; i < cols; ++i) height[i] = cosine_at(((2 * i + 1) << 11));
    } else {  // where the wave works: 64 samples, cumulative |change| plus a floor, inverted
        u32 work[65];
        i32 prev = unmapped_sample(wave, au, 0);
        work[0] = 0;
        for (u32 k = 1; k <= 64; ++k) {
            const i32 y = unmapped_sample(wave, au, k == 64 ? 65535u : k << 10);
            work[k] = work[k - 1] + static_cast<u32>(y > prev ? y - prev : prev - y) + 64;
            prev = y;
        }
        u32 k = 0;
        for (u32 t = 0; t <= cols; ++t) {
            const u32 target = (work[64] * t) >> 4;
            while (k < 63 && work[k + 1] < target) ++k;
            const u32 lo = work[k], hi = work[k + 1];
            edge[t] = (k << 10) + (hi > lo ? ((target > lo ? target - lo : 0) << 10) / (hi - lo) : 0);
            if (edge[t] > 65536u) edge[t] = 65536u;
        }
        for (u32 i = 0; i < cols; ++i) height[i] = unmapped_sample(wave, au, (edge[i] + edge[i + 1]) >> 1);
    }
    // Width: the cycle runs in (1 − 0.9·width) of the period, so every edge moves left of the gap.
    const u32 keep = 65536u - ((static_cast<u32>(in.slots[c_width] < 0 ? 0 : in.slots[c_width] >> 16) * 58982u) >> 15);
    for (u32 i = 0; i < cols; ++i) {
        const u32 a = ((edge[i] >> 1) * (keep >> 1)) >> 14, b = ((edge[i + 1] >> 1) * (keep >> 1)) >> 14;
        const i32 centre_q2 = left_px * 4 + static_cast<i32>(((a + b) * span_px * 2) >> 16);
        i32 scale = static_cast<i32>(((((b - a) * span_px) >> 6) * unit / cell_px) >> 10) - 77;  // less a 1.5 px gap
        s.cx[0][i] = static_cast<opxy::i16>(centre_q2);
        s.cs[0][i] = static_cast<opxy::u16>(opxy::draw::clamp(scale, 358, 2 * unit));
        s.cv[0][i] = static_cast<opxy::i16>(height[i]);
    }
}

inline u32 pack(i32 x, i32 y, i32 scale, u32 shade) {
    return static_cast<u32>(x & 1023) | static_cast<u32>(y & 511) << 10 | static_cast<u32>((scale >> 4) & 255) << 19 | shade << 27;
}
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
    state.env -= (state.env >> 9) * dt;  // exp(−t / 0.5 s), per frame
    if (state.env < 0) state.env = 0;

    // The front row eases toward the sound; each row behind eases toward the one in front, so changes
    // glide back through the terrace instead of stepping.
    opxy::i16 ox[cols], ov[cols];
    opxy::u16 os[cols];
    for (u32 i = 0; i < cols; ++i) { ox[i] = state.cx[0][i]; os[i] = state.cs[0][i]; ov[i] = state.cv[0][i]; }
    front_row(state, input);
    if (!state.started || input.reduced_motion) {  // every row shows now
        for (u32 j = 1; j < rows; ++j)
            for (u32 i = 0; i < cols; ++i) { state.cx[j][i] = state.cx[0][i]; state.cs[j][i] = state.cs[0][i]; state.cv[j][i] = state.cv[0][i]; }
    } else {
        const i32 kf = cz::ease_rate(dt, tau_front), kr = cz::ease_rate(dt, tau_row);
        for (u32 j = rows - 1; j > 0; --j)
            for (u32 i = 0; i < cols; ++i) {
                state.cx[j][i] = static_cast<opxy::i16>(cz::ease(state.cx[j][i], state.cx[j - 1][i], kr));
                state.cs[j][i] = static_cast<opxy::u16>(cz::ease(state.cs[j][i], state.cs[j - 1][i], kr));
                state.cv[j][i] = static_cast<opxy::i16>(cz::ease(state.cv[j][i], state.cv[j - 1][i], kr));
            }
        for (u32 i = 0; i < cols; ++i) {
            state.cx[0][i] = static_cast<opxy::i16>(cz::ease(ox[i], state.cx[0][i], kf));
            state.cs[0][i] = static_cast<opxy::u16>(cz::ease(os[i], state.cs[0][i], kf));
            state.cv[0][i] = static_cast<opxy::i16>(cz::ease(ov[i], state.cv[0][i], kf));
        }
    }

    // Place what differs, at most redraw_budget nodes a frame (all of them on the first frame).
    u32 left = state.started ? redraw_budget : cells;
    for (u32 k = 0; k < cells && left; ++k) {
        const u32 n = (state.cursor + k) % cells, j = rows - 1 - n / cols, i = n % cols;
        const i32 v = state.cv[j][i];
        const i32 x_q2 = state.cx[j][i] + static_cast<i32>(j) * row_dx * 4;
        const i32 y_q2 = (base_y - static_cast<i32>(j) * row_dy) * 4 - ((v * height_px * 4) >> 15);
        const i32 lum = 155 + ((85 * v) >> 15) - 6 * static_cast<i32>(j);
        const u32 shade = static_cast<u32>(clamp(((lum - 30) * 2) / 27, 0, 15));
        const u32 want = pack(x_q2 >> 2, y_q2 >> 2, state.cs[j][i], shade);
        if (state.started && want == state.drawn[n]) continue;
        opxy::draw::place(frame, n, x_q2 * (unit / 4), y_q2 * (unit / 4), state.cs[j][i], shade, opxy::draw::fill_and_stroke);
        state.drawn[n] = want;
        --left;
        if (state.started && !left) state.cursor = (n + 1) % cells;
    }
    state.started = 1;
}
}
