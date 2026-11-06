// Schedar: phase-distortion voice. A clean-room take on the CZ technique: a linear phase is bent by a
// piecewise-linear map before a cosine read (the DCW "Amount"), or a sine at k × the note is windowed
// once per cycle (the resonant waves). Width ends each cycle early and holds its last value, the
// pulse-like gap; Sweep is the DCW envelope.
#include "core.h"

namespace schedar {
using opxy::i32;
using opxy::u32;
using cz::scale16;

enum Wave { w_saw, w_square, w_pulse, w_double, w_sawpulse, w_res1, w_res2, w_res3, wave_count };

// Q16 points (x, g) → segments, dropping zero-length ones (a knee at rest collapses a hold away).
inline void build(Voice& v, const u32* x, const u32* g, unsigned n, unsigned& count) {
    count = 0;
    for (unsigned i = 0; i + 1 < n; ++i) {
        if (x[i + 1] <= x[i]) continue;
        v.seg_x[count] = x[i];
        v.seg_g[count] = g[i];
        v.seg_s[count] = cz::slope25(g[i + 1] - g[i], x[i + 1] - x[i]);
        ++count;
    }
}

// Phase-distortion waves: a cosine read through the segment map.
__attribute__((noinline)) void map_pass(Voice& v, opxy::Output out, opxy::Pitch pitch, i32 gap, unsigned n) {
    u32 p = v.phase;
    i32 x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        u32 x = scale16(p >> 16, gap);
        if (x > 65535u) x = 65535u;
        unsigned k = 0;
        while (k + 1 < n && x >= v.seg_x[k + 1]) ++k;
        const u32 g = v.seg_g[k] + scale16(x - v.seg_x[k], v.seg_s[k]);
        out[i] = cz::dc_out(cz::sine((g << 16) + 0x40000000u), x1, y1);
        p += pitch[i];
    }
    v.phase = p;
    v.dc_x = x1;
    v.dc_y = y1;
}

// Double sine: a sine at (1 + Amount) × the note under a Hann window, so the cycle edge stays smooth.
__attribute__((noinline)) void double_pass(Voice& v, opxy::Output out, opxy::Pitch pitch, i32 gap, i32 k) {
    u32 p = v.phase;
    i32 x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        u32 x = scale16(p >> 16, gap);
        if (x > 65535u) x = 65535u;
        const i32 hann = (32768 - cz::sine((x << 16) + 0x40000000u)) >> 1;
        const i32 s = cz::sine(scale16(x, k) << 16);
        out[i] = cz::dc_out((hann * s) >> 15, x1, y1);
        p += pitch[i];
    }
    v.phase = p;
    v.dc_x = x1;
    v.dc_y = y1;
}

// Resonant waves: a sine at k × the note, restarted every cycle, under a window that ends at zero.
template <int Window>
__attribute__((noinline)) void res_pass(Voice& v, opxy::Output out, opxy::Pitch pitch, i32 gap, i32 k) {
    u32 p = v.phase;
    i32 x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        u32 x = scale16(p >> 16, gap);
        if (x > 65535u) x = 65535u;
        i32 w;
        if (Window == 0) w = 32767 - static_cast<i32>(x >> 1);  // falling saw
        else if (Window == 1) {                                   // triangle
            const i32 d = static_cast<i32>(x) - 32768;
            w = 32767 - (d < 0 ? -d : d);
        } else {                                                  // trapezoid, quarter-cycle edges
            w = x < 16384u ? static_cast<i32>(x << 1) : x > 49152u ? static_cast<i32>((65536u - x) << 1) : 32767;
            if (w > 32767) w = 32767;
        }
        const i32 s = cz::sine(scale16(x, k) << 16);
        out[i] = cz::dc_out((w * s) >> 15, x1, y1);
        p += pitch[i];
    }
    v.phase = p;
    v.dc_x = x1;
    v.dc_y = y1;
}

inline void start(Voice& v, Shared&, u32) {
    v.phase = 0;
    v.env = opxy::q31::maximum;
}

inline void initialize(Voice& v, Shared& shared, u32 owner) {
    v.primed = 0;
    v.dc_x = v.dc_y = 0;
    start(v, shared, owner);
}

inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& v, Shared&, u32, opxy::Output output, opxy::Parameters parameters,
                   opxy::Pitch pitch) {
    cz::glide(v.smooth, parameters, control_count, v.primed);
    const u32 wave = cz::choice(parameters[c_wave], wave_count);  // not glided: a choice
    i32 a = cz::swept(v.smooth[c_amount] >> 16, v.smooth[c_sweep], v.env);
    const i32 cap = cz::alias_cap(pitch[0]);
    if (a > cap) a = cap;
    v.env = opxy::q31::multiply(v.env, cz::sweep_decay);

    // Width: the cycle runs in (1 − 0.9·width) of the period, then holds.
    const u32 hold = static_cast<u32>((v.smooth[c_width] >> 16) * 58982) >> 15;  // 0.9 in Q16
    const i32 gap = cz::slope25(65536u, 65536u - hold);
    const u32 au = static_cast<u32>(a);

    if (wave == w_double) return double_pass(v, output, pitch, gap, static_cast<i32>((32768u + au) << 10));
    if (wave >= w_res1) {
        const i32 k = static_cast<i32>((32768u + 7u * au) << 10);  // resonant multiple 1..8
        if (wave == w_res1) return res_pass<0>(v, output, pitch, gap, k);
        if (wave == w_res2) return res_pass<1>(v, output, pitch, gap, k);
        return res_pass<2>(v, output, pitch, gap, k);
    }

    const u32 one = 65536u, half = 32768u;
    u32 x[6], g[6];
    unsigned n = 0;
    if (wave == w_square) {  // holds at the top and bottom of the cosine widen with Amount
        const u32 hd = (au * 29491u) >> 15;  // 0.45·a, each hold
        const u32 r = half - hd;             // each ramp
        const u32 xs[5] = {0, hd, hd + r, 2 * hd + r, one}, gs[5] = {0, 0, half, half, one};
        for (n = 0; n < 5; ++n) { x[n] = xs[n]; g[n] = gs[n]; }
    } else {
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
    }
    unsigned count;
    build(v, x, g, n, count);
    map_pass(v, output, pitch, gap, count);
}
}
