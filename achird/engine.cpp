// Achird: Walsh-function synthesis, clean-room. The tone is a weighted sum of the first fifteen Walsh
// functions, square-step waves ordered by sequency, so a cycle is exactly 16 steps: a hyper-digital,
// chip-adjacent voice. Tilt moves the weights from dark (1/h²) to harsh (1/√h), Balance trades odd
// sequencies against even, Fold drives the sum into a triangle fold, and Sweep is the family's envelope
// on Tilt. The 16 levels are rebuilt once per block; each step edge gets a polyBLEP, and functions whose
// sequency would pass fs/4 drop out.
#include "core.h"

namespace achird {

// The polyBLEP residual of a step at phase 0 (Q15 for a full −2 step, to add to a rising saw).
inline i32 blep(u32 p, u32 inc, u32 r) {
    if (p < inc) {
        const i32 one = 32768 - static_cast<i32>(((p >> 8) * r) >> 16);
        return (one * one) >> 15;
    }
    if (0u - p < inc) {
        const i32 one = 32768 - static_cast<i32>((((0u - p) >> 8) * r) >> 16);
        return -((one * one) >> 15);
    }
    return 0;
}

__attribute__((noinline)) void step_pass(Voice& v, opxy::Output out, opxy::Pitch pitch) {
    const u32 inc = pitch[0];
    const bool smooth = inc < (1u << 28);           // below fs/16 a step lasts over one sample: BLEP it
    const u32 inc16 = inc << 4, r = smooth && inc16 >> 8 ? 0x7fffffffu / (inc16 >> 8) : 0;
    u32 p = v.phase;
    i32 x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const u32 s = p >> 28, local = p << 4;
        i32 y = v.level[s];
        if (smooth) {
            // Just past edge s, or just before edge s + 1: the step there, as a fraction of a full −2 step.
            if (local < inc16) y -= (blep(local, inc16, r) * ((v.level[s] - v.level[(s - 1) & 15u]) >> 1)) >> 15;  // halved: 32-bit
            else if (0u - local < inc16) y -= (blep(local, inc16, r) * ((v.level[(s + 1) & 15u] - v.level[s]) >> 1)) >> 15;
        }
        out[i] = cz::dc_out(y, x1, y1);
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
    const i32 tilt = cz::swept(v.smooth[c_tilt] >> 16, v.smooth[c_sweep], v.env);
    v.env = opxy::q31::multiply(v.env, cz::sweep_decay);
    levels(v.level, tilt, v.smooth[c_balance] >> 16, v.smooth[c_fold] >> 16, pitch[0]);
    step_pass(v, output, pitch);
}
}
