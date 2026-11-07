// Navi: vector phaseshaping (after Kleimola, Lazzarini, Timoney and Välimäki, 2011), clean-room. The
// phase is bent through one movable inflection point (d, v) before a cosine read: Inflect slides d
// across the cycle, Formant lifts v so the first part of the cycle runs several cosine periods (a
// formant that stays put across the keyboard's timbre), Range sets how far Formant can lift it, and
// Sweep is the family's envelope on Formant. The map ends at 1 whatever (d, v), so the cycle edge
// never clicks.
#include "core.h"

namespace navi {

__attribute__((noinline)) void bend_pass(Voice& v, opxy::Output out, opxy::Pitch pitch, const Bend& b) {
    u32 p = v.phase;
    i32 x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        out[i] = cz::dc_out(sample(p >> 16, b), x1, y1);
        p += pitch[i];
    }
    v.phase = p;
    v.dc_x = x1;
    v.dc_y = y1;
}

inline void start(Voice& v, Shared&, u32) {
    v.phase = 0;
    v.env = opxy::q31::maximum;
    v.primed = 0;  // the first block takes this note's knob values outright (a reused voice held the last note's)
}

inline void initialize(Voice& v, Shared& shared, u32 owner) {
    v.dc_x = v.dc_y = 0;
    start(v, shared, owner);
}

inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& v, Shared&, u32, opxy::Output output, opxy::Parameters parameters,
                   opxy::Pitch pitch) {
    cz::glide(v.smooth, parameters, control_count, v.primed);
    const i32 formant = cz::swept(v.smooth[c_formant] >> 16, v.smooth[c_sweep], v.env);
    v.env = opxy::q31::multiply(v.env, cz::sweep_decay);
    const u32 d = inflect16(v.smooth[c_inflect] >> 16);
    u32 vv = formant16(formant, v.smooth[c_range] >> 16);
    // Anti-alias: each segment's local rate (v/d, |v − 1|/(1 − d) times the note) stays under fs/6.
    // r = (fs/6)/f in Q6, capped at 16 (beyond it nothing binds); 32-bit only, no 64-bit divide links.
    const u32 inc = pitch[0] >> 8 ? pitch[0] >> 8 : 1;  // the note's rate, Q24 of fs
    u32 r = (2796203u << 6) / inc;
    if (r > 1024u) r = 1024u;
    const u32 lim1 = (d * r) >> 6, lim2 = 65536u + (((65536u - d) * r) >> 6);
    if (vv > lim1 && lim1 >= 32768u) vv = lim1;
    if (vv > lim2) vv = lim2;
    if (vv < 32768u) vv = 32768u;
    bend_pass(v, output, pitch, bend(d, vv));
}
}
