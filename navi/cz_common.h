#pragma once
// Shared by every Cassiopeia engine; tools/sync_common.sh copies it into each folder.
#include <opxy/audio.hpp>
#include <opxy/q31.hpp>

namespace cz {
using opxy::i32;
using opxy::u32;

// sin(2π·p) for a 32-bit phase, Q15, from arithmetic alone (parabola plus one correction; 3rd harmonic
// near −55 dB). No table: tables in the module's DDR miss the cache on the device.
inline i32 sine(u32 p) {
    const i32 x = static_cast<i32>(p) >> 16;
    const i32 a = x < 0 ? -x : x;
    const i32 y = (x * (32768 - a)) >> 13;
    const i32 ya = y < 0 ? -y : y;
    return y + (((((y * ya) >> 15) - y) * 7373) >> 15);
}

// dx (Q16, below 2^16) times a slope held as slope·2^25 (slope below 64), in Q16. One Blackfin multiply.
inline u32 scale16(u32 dx, i32 slope25) {
    return static_cast<u32>(opxy::q31::multiply(static_cast<i32>(dx << 15), slope25)) >> 9;
}

// Δg/Δx as slope·2^25 for scale16; once per block (an unsigned divide), Δx > 0, Δg ≤ 2^16.
inline i32 slope25(u32 dg, u32 dx) { return static_cast<i32>(((dg << 15) / dx) << 10); }

// inc × ratio / 2^28 from 16-bit halves (no 64-bit arithmetic), held below Nyquist.
inline u32 mul_u32_q28(u32 inc, u32 ratio) {
    const u32 ih = inc >> 16, il = inc & 0xffffu, rh = ratio >> 16, rl = ratio & 0xffffu;
    const u32 hi = ih * rh;
    if (hi >= (1u << 27)) return 0x7fffffffu;
    const u32 mid = ((ih * rl) >> 4) + ((il * rh) >> 4) + ((il * rl) >> 20);
    const u32 out = (hi << 4) + ((mid + 128) >> 8);
    return out > 0x7fffffffu ? 0x7fffffffu : out;
}

// 2^(octaves) as a Q28 ratio for octaves in Q16, 0 ≤ octaves < 3·2^16 (quadratic fraction, 0.3% error).
inline u32 exp2_q28(u32 octaves16) {
    const u32 k = octaves16 >> 16, f = octaves16 & 0xffffu;
    const u32 m = 65536u + ((f * (43025u + ((22512u * f) >> 16))) >> 16);  // 2^f, Q16
    return (m << 12) << k;
}

// Enum control: index 0..n−1 from its normalized word.
inline u32 choice(i32 word, u32 n) { return ((static_cast<u32>(word < 0 ? 0 : word) >> 16) * n) >> 15; }

// Anti-alias cap on a 0..1 timbre amount (Q15): 1 − 8·f/fs, so the steepest edges and the highest
// resonant multiples stay below Nyquist; zero from fs/8 up. Replaces the VST's 4× oversampler.
inline i32 alias_cap(u32 inc) {
    const i32 cap = 32767 - static_cast<i32>(inc >> 14);
    return cap < 0 ? 0 : cap;
}

// Sweep: the CZ-style DCW envelope, one bipolar knob. Above centre a bright strike falls back to the
// Amount; below centre a dark strike opens up to it. env is Q31, 1 at the strike, ~0.5 s decay.
const i32 sweep_decay = 2141259626;
inline i32 swept(i32 amount, i32 sweep_word, i32 env) {
    const i32 s = sweep_word - 0x40000000;          // −0.5..0.5 in Q31
    const i32 e = opxy::q31::multiply(env, s) >> 16; // Q15 of ±0.5·env
    const i32 room = s > 0 ? 32767 - amount : amount;
    const i32 a = amount + ((2 * e * room) >> 15);
    return a < 0 ? 0 : a > 32767 ? 32767 : a;
}

// Per-block one-pole glide toward each control (first block jumps).
inline void glide(i32* smooth, opxy::Parameters p, unsigned n, opxy::u8& primed) {
    for (unsigned k = 0; k < n; ++k) {
        const i32 t = p[k] < 0 ? 0 : p[k];
        smooth[k] = primed ? smooth[k] + ((t - smooth[k]) >> 2) : t;
    }
    primed = 1;
}

// DC blocker (R = 0.9995) and output scaling from Q15 to 0.24 FS: a held cosine (Width) is a pulse that
// the blocker can swing to nearly twice its amplitude, so this stays under outputPeakQ31 (0.5 FS).
const i32 dc_r = 2146409906;
inline i32 dc_out(i32 s, i32& x1, i32& y1) {
    const i32 x = s * 15729;  // Q15 → Q31 at 0.24
    const i32 y = x - x1 + opxy::q31::multiply(y1, dc_r);
    x1 = x;
    y1 = y;
    return y;
}
}
