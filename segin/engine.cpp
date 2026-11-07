// Segin: VOSIM (after Kaegi and Tempelaars, 1978), clean-room. Each cycle of the note fires a burst of
// sin² pulses whose width is one period of a fixed formant frequency, each lower than the last, then
// rests until the next cycle: a formant that stays where it is across the keyboard, so notes read as one
// vowel. Formant sets the pulse width (150 Hz .. 4 kHz), Decay how fast the burst fades, Pulses how
// many fire (1 .. 8), and Sweep is the family's envelope on Formant.
#include "core.h"

namespace segin {

__attribute__((noinline)) void burst_pass(Voice& v, opxy::Output out, opxy::Pitch pitch, u32 qinc, u32 pulses, i32 keep) {
    u32 p = v.phase, q = v.pulse, n = v.count;
    i32 amp = v.amp, x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 s = 0;
        if (n < pulses) {
            s = (amp * hump(q)) >> 15;
            const u32 next = q + qinc;
            if (next < q) {  // this pulse ended: the next is lower
                ++n;
                amp = (amp * keep) >> 15;
            }
            q = next;
        }
        out[i] = cz::dc_out(s, x1, y1);
        const u32 np = p + pitch[i];
        if (np < p) { q = 0; n = 0; amp = 32767; }  // a new cycle: a new burst
        p = np;
    }
    v.phase = p;
    v.pulse = q;
    v.count = n;
    v.amp = amp;
    v.dc_x = x1;
    v.dc_y = y1;
}

inline void start(Voice& v, Shared&, u32) {
    v.phase = 0;
    v.pulse = 0;
    v.count = 0;
    v.amp = 32767;
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
    u32 qinc = formant_inc(formant);
    if (qinc > 0x40000000u) qinc = 0x40000000u;  // at most fs/4
    if (qinc < pitch[0]) qinc = pitch[0];         // at least one whole hump a cycle: high notes stay voiced
    const u32 pulses = 1 + cz::choice(parameters[c_pulses], max_pulses);  // a count: not glided
    burst_pass(v, output, pitch, qinc, pulses, decay_keep(v.smooth[c_decay] >> 16));
}
}
