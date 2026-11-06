#pragma once
#include "cz_common.h"

namespace schedar {
// Controls, in definition order.
enum Control { c_amount, c_width, c_wave, c_sweep, control_count };
const unsigned max_segments = 5;

struct Voice {
    opxy::u32 phase;
    opxy::i32 env;                      // Sweep envelope, Q31
    opxy::i32 smooth[control_count];    // Q31
    opxy::i32 dc_x, dc_y;               // DC blocker
    // This block's phase map: piecewise-linear segments in Q16 (start x, start g, slope·2^25).
    opxy::u32 seg_x[max_segments], seg_g[max_segments];
    opxy::i32 seg_s[max_segments];
    opxy::u8 primed;
};
struct Shared {};

using opxy::i32;
using opxy::u32;
enum Wave { w_saw, w_square, w_pulse, w_double, w_sawpulse, w_res1, w_res2, w_res3, wave_count };
inline bool is_mapped(u32 wave) { return wave <= w_pulse || wave == w_sawpulse; }

// A mapped wave's phase map as Q16 points (x, g), at most 6, for Amount au (Q15); returns the count.
// The engine reads a cosine through it; the screen spaces its columns by it.
inline unsigned map_points(u32 wave, u32 au, u32* x, u32* g) {
    const u32 one = 65536u, half = 32768u;
    unsigned n = 0;
    if (wave == w_square) {  // holds at the top and bottom of the cosine widen with Amount
        const u32 hd = (au * 29491u) >> 15;  // 0.45·a, each hold
        const u32 r = half - hd;             // each ramp
        const u32 xs[5] = {0, hd, hd + r, 2 * hd + r, one}, gs[5] = {0, 0, half, half, one};
        for (n = 0; n < 5; ++n) { x[n] = xs[n]; g[n] = gs[n]; }
        return n;
    }
    const u32 m = half - ((au * 32113u) >> 15);  // the knee: 0.5 − 0.49·a
    if (wave == w_saw) {
        const u32 xs[3] = {0, m, one}, gs[3] = {0, half, one};
        for (n = 0; n < 3; ++n) { x[n] = xs[n]; g[n] = gs[n]; }
    } else if (wave == w_pulse) {  // the whole cosine squeezed into 2m, then rests at the top
        const u32 xs[4] = {0, m, 2 * m, one}, gs[4] = {0, half, one, one};
        for (n = 0; n < 4; ++n) { x[n] = xs[n]; g[n] = gs[n]; }
    } else {  // saw-pulse: the knee applied twice
        const u32 m2 = (2 * m * m) >> 16;
        const u32 g2 = half + (half * (half - m)) / (one - m);
        const u32 xs[4] = {0, m2, m, one}, gs[4] = {0, half, g2, one};
        for (n = 0; n < 4; ++n) { x[n] = xs[n]; g[n] = gs[n]; }
    }
    return n;
}

// The resonant waves' windows over the cycle (Q16 x → Q15), each ending at zero: falling saw, triangle,
// trapezoid with quarter-cycle edges.
template <int Window>
inline i32 res_window(u32 x) {
    if (Window == 0) return 32767 - static_cast<i32>(x >> 1);
    if (Window == 1) {
        const i32 d = static_cast<i32>(x) - 32768;
        return 32767 - (d < 0 ? -d : d);
    }
    const i32 w = x < 16384u ? static_cast<i32>(x << 1) : x > 49152u ? static_cast<i32>((65536u - x) << 1) : 32767;
    return w > 32767 ? 32767 : w;
}

// One sample (Q15) of an unmapped wave (Dbl, Res 1–3) at Q16 phase x, for Amount au: the screen's
// scalar copy of the engine's double and resonant passes.
inline i32 unmapped_sample(u32 wave, u32 au, u32 x) {
    if (wave == w_double) {
        const i32 hann = (32768 - cz::sine((x << 16) + 0x40000000u)) >> 1;
        return (hann * cz::sine(cz::scale16(x, static_cast<i32>((32768u + au) << 10)) << 16)) >> 15;
    }
    const i32 s = cz::sine(cz::scale16(x, static_cast<i32>((32768u + 7u * au) << 10)) << 16);
    const i32 w = wave == w_res1 ? res_window<0>(x) : wave == w_res2 ? res_window<1>(x) : res_window<2>(x);
    return (w * s) >> 15;
}
}
