#pragma once
#include "cz_common.h"

namespace segin {
using opxy::i32;
using opxy::u32;
// Controls, in definition order.
enum Control { c_formant, c_decay, c_pulses, c_sweep, control_count };
const u32 max_pulses = 8;

struct Voice {
    opxy::u32 phase;      // the note's cycle
    opxy::u32 pulse;      // the current pulse's phase: one sin² hump per wrap
    opxy::u32 count;      // pulses fired this cycle
    opxy::i32 amp;        // the current pulse's height, Q15
    opxy::i32 env;        // Sweep envelope, Q31
    opxy::i32 smooth[control_count];    // Q31
    opxy::i32 dc_x, dc_y;               // DC blocker
    opxy::u8 primed;
};
struct Shared {};

// Formant in Hz from its amount (Q15): 150 Hz · 2^(4.75·amount), 150 .. 4040 Hz. Returned as the
// pulse's phase increment at 44.1 kHz (one sin² hump per formant period).
inline u32 formant_inc(i32 amount15) {
    const u32 octaves = (static_cast<u32>(amount15) * 9728u) >> 10;  // 4.75 octaves, Q16
    return cz::mul_u32_q28(14608728u << (octaves >> 16), cz::exp2_q28(octaves & 0xffffu));  // 150 Hz · 2^32 / fs
}
inline u32 formant_hz(i32 amount15) {
    const u32 octaves = (static_cast<u32>(amount15) * 9728u) >> 10;
    return (150u << (octaves >> 16)) * (cz::exp2_q28(octaves & 0xffffu) >> 14) >> 14;
}
// Each pulse keeps this much of the last one's height (Q15): 1.0 at Decay 0, 0.2 at full.
inline i32 decay_keep(i32 decay15) { return 32767 - ((decay15 * 26214) >> 15); }
// One sin² hump over a 32-bit pulse phase, Q15.
inline i32 hump(u32 q) { return (32768 - cz::sine(q + 0x40000000u)) >> 1; }
}
