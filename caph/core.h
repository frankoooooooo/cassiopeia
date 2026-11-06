#pragma once
#include "cz_common.h"

namespace caph {
using opxy::i32;
using opxy::u32;
// Controls, in definition order.
enum Control { c_shape, c_width, c_detune, c_sweep, control_count };

struct Voice {
    opxy::u32 pa, pb;                   // the pair's phases: A a little flat, B a little sharp
    opxy::i32 env;                      // Sweep envelope, Q31
    opxy::i32 smooth[control_count];    // Q31
    opxy::i32 dc_x, dc_y;               // DC blocker
    opxy::u8 primed;
};
struct Shared {};

// Pulse duty and triangle apex from Width, Q16: 50% at 0, 5% at full.
inline u32 duty16(i32 width15) { return 32768u - ((static_cast<u32>(width15) * 29491u) >> 15); }

// The three shapes without band-limiting (Q15, Q16 phase x): the screen draws these; the engine adds
// polyBLEP to the saw and pulse edges.
inline i32 naive_saw(u32 x) { return static_cast<i32>(x) - 32768; }
inline i32 naive_pulse(u32 x, u32 d) { return x < d ? 32767 : -32768; }
inline i32 naive_triangle(u32 x, u32 d) {
    return x < d ? static_cast<i32>((x * 65535u) / d) - 32768
                 : 32767 - static_cast<i32>(((x - d) * 65535u) / (65536u - d));
}
// Shape (Q15): triangle at 0, pulse at the middle, saw at full, crossfaded between.
inline i32 naive_shape(u32 x, i32 shape15, u32 d) {
    if (shape15 < 16384) {
        const i32 k = shape15 * 2;
        return (naive_triangle(x, d) * (32767 - k) + naive_pulse(x, d) * k) >> 15;
    }
    const i32 k = (shape15 - 16384) * 2;
    return (naive_pulse(x, d) * (32767 - k) + naive_saw(x) * k) >> 15;
}
}
