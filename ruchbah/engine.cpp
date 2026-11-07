// Ruchbah: scanned synthesis (after Verplank, Mathews and Shaw, 2000), clean-room. A ring of 32 masses
// joined by springs is struck by each note into one of four shapes (Excite) and left to move at a slow,
// haptic rate (once per block, about 690 times a second) while a scanner reads its shape once per cycle
// of the note: a wavetable that is alive. Stiff couples the masses (how fast the shape travels and
// smooths), Damping sets how soon it settles, and Sweep is the family's envelope on Stiff.
#include "core.h"

namespace ruchbah {

__attribute__((noinline)) void scan_pass(Voice& v, opxy::Output out, opxy::Pitch pitch) {
    u32 p = v.phase;
    i32 x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        out[i] = cz::dc_out(scan(v.ring, p), x1, y1);
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
    v.ring.lfsr = static_cast<opxy::u16>(0xace1u ^ owner);
    for (u32 k = 0; k < control_count; ++k) v.smooth[k] = 0;
    start(v, shared, owner);
}

inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& v, Shared&, u32, opxy::Output output, opxy::Parameters parameters,
                   opxy::Pitch pitch) {
    const bool first = !v.primed;
    cz::glide(v.smooth, parameters, control_count, v.primed);
    if (first) excite(v.ring, cz::choice(parameters[c_excite], excite_count));  // each note strikes with the knob as it is now
    const i32 stiff = cz::swept(v.smooth[c_stiff] >> 16, v.smooth[c_sweep], v.env);
    v.env = opxy::q31::multiply(v.env, cz::sweep_decay);
    step(v.ring, stiffness(stiff), keep(v.smooth[c_damping] >> 16));
    scan_pass(v, output, pitch);
}
}
