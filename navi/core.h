#pragma once
#include "cz_common.h"

namespace navi {
using opxy::i32;
using opxy::u32;
// Controls, in definition order.
enum Control { c_formant, c_inflect, c_range, c_sweep, control_count };

struct Voice {
    opxy::u32 phase;
    opxy::i32 env;                      // Sweep envelope, Q31
    opxy::i32 smooth[control_count];    // Q31
    opxy::i32 dc_x, dc_y;               // DC blocker
    opxy::u8 primed;
};
struct Shared {};

// The inflection point (d, v), Q16: d from Inflect (0.1..0.9), v from Formant within Range (0.5 up to
// 1.5..8). At (0.5, 0.5) the map is the identity and the voice a pure cosine.
struct Bend { u32 d, v; i32 s1, s2; bool falling; };
inline u32 inflect16(i32 inflect15) { return 6554u + ((static_cast<u32>(inflect15) * 52429u) >> 15); }
inline u32 formant16(i32 formant15, i32 range15) {
    const u32 top = 98304u + ((static_cast<u32>(range15) * 26624u) >> 11);  // 1.5 .. 8 (425984 / 16, then >> 11: 32-bit)
    return 32768u + ((static_cast<u32>(formant15) * ((top - 32768u) >> 4)) >> 11);  // ≤ 2^30 before the shift
}
// x · slope for a slope held as slope·2^23 (below 256), x below 2^16: Q16.
inline u32 scale23(u32 dx, i32 slope23) {
    return static_cast<u32>(opxy::q31::multiply(static_cast<i32>(dx << 15), slope23)) >> 7;
}
// num/den as slope·2^23, num ≤ 2^19, den > 0 (Q16 both): 12 fractional bits, then shifted up.
inline i32 slope23(u32 num, u32 den) { return static_cast<i32>(((num << 12) / den) << 11); }
inline Bend bend(u32 d, u32 v) {
    Bend b;
    b.d = d;
    b.v = v;
    b.s1 = slope23(v, d);
    b.falling = v > 65536u;
    b.s2 = slope23(b.falling ? v - 65536u : 65536u - v, 65536u - d);
    return b;
}
// The bent phase (Q16, may run past 1 for several cycles) at Q16 phase x.
inline u32 bent(u32 x, const Bend& b) {
    if (x < b.d) return scale23(x, b.s1);
    const u32 t = scale23(x - b.d, b.s2);
    return b.falling ? b.v - t : b.v + t;
}
// One sample, Q15: −cos(2π · bent phase).
inline i32 sample(u32 x, const Bend& b) { return -cz::sine((bent(x, b) << 16) + 0x40000000u); }
}
