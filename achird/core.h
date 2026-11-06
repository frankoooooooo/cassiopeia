#pragma once
#include "cz_common.h"

namespace achird {
using opxy::i32;
using opxy::u32;
// Controls, in definition order.
enum Control { c_tilt, c_balance, c_fold, c_sweep, control_count };
const u32 steps = 16;

// Walsh functions 1..15 in Paley order. Each is ±1 on the 16 steps of a cycle: function k's sign at step s
// is the parity of k & (s with its 4 bits reversed). Their sequency (half the sign changes round the
// cycle) plays the part of a harmonic number: weights fall with it, and Balance trades odd against even.
const opxy::u8 sequency[15] = {1, 2, 1, 4, 3, 2, 3, 8, 7, 6, 7, 4, 5, 6, 5};
const opxy::i16 inv_square[15] = {32767, 8192, 32767, 2048, 3641, 8192, 3641, 512, 669, 910, 669, 2048, 1311, 910, 1311};
const opxy::i16 inv_root[15] = {32767, 23170, 32767, 16384, 18918, 23170, 18918, 11585, 12385, 13377, 12385, 16384, 14654, 13377, 14654};
inline u32 reverse4(u32 s) { return ((s & 1u) << 3) | ((s & 2u) << 1) | ((s & 4u) >> 1) | ((s & 8u) >> 3); }
inline bool negative(u32 k, u32 s) {
    u32 b = k & reverse4(s);
    b ^= b >> 2;
    b ^= b >> 1;
    return b & 1u;
}

// The cycle's 16 step levels (Q15) for Tilt (Q15: 0 dark, 1/h² weights; 1 harsh, 1/√h), Balance (Q15:
// 0.5 even; the weaker side keeps 10%) and Fold (Q15: gain 1..5 into a triangle fold; 5, not 6, which
// folds a ±1 square to silence), leaving out functions whose sequency × the
// note passes fs/4 (inc: the note's phase increment; 0 keeps all).
inline void levels(i32* out, i32 tilt, i32 balance, i32 fold, u32 inc) {
    i32 w[15];
    i32 total = 0;
    for (u32 k = 0; k < 15; ++k) {
        const u32 h = sequency[k];
        if (inc && (inc >> 8) * h > (1u << 22)) { w[k] = 0; continue; }  // h · f > fs/4
        i32 x = ((inv_square[k] * (32767 - tilt)) >> 15) + ((inv_root[k] * tilt) >> 15);
        const i32 lean = h & 1u ? balance : 32767 - balance;
        const i32 side = 3277 + ((lean * 58982) >> 15);  // Q15, 0.1 .. 1.9
        x = (x * (side > 32767 ? 32767 : side)) >> 15;
        w[k] = x;
        total += x;
    }
    const i32 r = total > 0 ? (1 << 30) / total : 0;  // one divide a block
    const i32 gain = 4096 + (fold >> 1);             // Q12, 1 .. 5
    for (u32 s = 0; s < steps; ++s) {
        i32 sum = 0;
        for (u32 k = 0; k < 15; ++k) sum += negative(k + 1, s) ? -w[k] : w[k];
        const i32 level = (sum * r) >> 15;           // Q15, within ±1
        // Triangle fold: the level times the gain, reflected back into ±1.
        u32 m = static_cast<u32>(((level * gain) >> 12) + 32768) & 131071u;
        out[s] = m < 65536u ? static_cast<i32>(m) - 32768 : 98304 - static_cast<i32>(m);
        if (out[s] > 32767) out[s] = 32767;
    }
}

struct Voice {
    opxy::u32 phase;
    opxy::i32 env;                      // Sweep envelope, Q31
    opxy::i32 smooth[control_count];    // Q31
    opxy::i32 dc_x, dc_y;               // DC blocker
    opxy::i32 level[steps];             // this block's step levels, Q15
    opxy::u8 primed;
};
struct Shared {};
}
