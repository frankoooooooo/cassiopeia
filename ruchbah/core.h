#pragma once
#include "cz_common.h"

namespace ruchbah {
using opxy::i16;
using opxy::i32;
using opxy::u32;
// Controls, in definition order.
enum Control { c_stiff, c_damping, c_excite, c_sweep, control_count };
enum Excite { e_pluck, e_impulse, e_noise, e_triangle, excite_count };
const u32 masses = 32;  // the ring; the voice reads it once per cycle of the note

// The ring the scanner reads: positions and velocities, Q15. Shared by the voice and the screen, which
// runs its own copy at the same rate so its pillars move as the sound does.
struct Ring {
    i16 x[masses], v[masses];
    opxy::u16 lfsr;
};

inline opxy::u16 step_lfsr(opxy::u16 s) { return static_cast<opxy::u16>((s >> 1) ^ (-(s & 1u) & 0xb400u)); }

// Strikes the ring into one of the four shapes, at rest.
inline void excite(Ring& r, u32 kind) {
    for (u32 i = 0; i < masses; ++i) {
        i32 y;
        if (kind == e_pluck) {  // a bump 12% wide at 30% round the ring
            const i32 d = static_cast<i32>(i * 1024 / masses) - 307;
            const i32 a = d < 0 ? -d : d;
            y = a < 123 ? 32767 - a * 266 : 0;
        } else if (kind == e_impulse) {
            y = i == 9 ? 32767 : 0;
        } else if (kind == e_noise) {
            r.lfsr = step_lfsr(r.lfsr ? r.lfsr : 0xace1u);
            y = static_cast<i16>(r.lfsr);
        } else {  // a triangle round the whole ring
            const i32 t = static_cast<i32>(i * 65536 / masses);
            y = t < 32768 ? 2 * t - 32767 : 98303 - 2 * t;
        }
        r.x[i] = static_cast<i16>(y > 32767 ? 32767 : y < -32767 ? -32767 : y);
        r.v[i] = 0;
    }
}

// One step of the mass-spring ring (semi-implicit Euler: velocities, then positions). k (Q15, < 0.45)
// couples neighbours, a weak spring pulls each mass home, and the velocity keeps (1 − damping) of itself.
inline void step(Ring& r, i32 k, i32 keep) {
    i32 left = r.x[masses - 1];
    const i32 first = r.x[0];
    for (u32 i = 0; i < masses; ++i) {
        const i32 xi = r.x[i], right = i + 1 < masses ? r.x[i + 1] : first;
        const i32 force = ((k * (left + right - 2 * xi)) >> 15) - (xi >> 9);
        const i32 v = ((r.v[i] + force) * keep) >> 15;
        r.v[i] = static_cast<i16>(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
        left = xi;
    }
    for (u32 i = 0; i < masses; ++i) {
        const i32 x = r.x[i] + r.v[i];
        r.x[i] = static_cast<i16>(x > 32767 ? 32767 : x < -32767 ? -32767 : x);
    }
}

// Stiffness from Stiff (Q15 amount): 0.02 .. 0.42. Velocity kept per step from Damping, on a squared
// curve: about a 10 s settle at 0 (0.99985), 0.8 s at 0.3, 0.1 s at full (0.985).
inline i32 stiffness(i32 amount15) { return 655 + ((amount15 * 13107) >> 15); }
inline i32 keep(i32 damping15) { return 32763 - ((((damping15 * damping15) >> 15) * 486) >> 15); }

// The ring at a 32-bit phase, linearly interpolated, Q15.
inline i32 scan(const Ring& r, u32 p) {
    const u32 i = p >> 27, j = (i + 1) & (masses - 1);
    const i32 f = static_cast<i32>((p >> 12) & 0x7fffu);
    return r.x[i] + (((r.x[j] - r.x[i]) * f) >> 15);
}

struct Voice {
    Ring ring;
    opxy::u32 phase;
    opxy::i32 env;                      // Sweep envelope, Q31
    opxy::i32 smooth[control_count];    // Q31
    opxy::i32 dc_x, dc_y;               // DC blocker
    opxy::u8 primed;
};
struct Shared {};
}
