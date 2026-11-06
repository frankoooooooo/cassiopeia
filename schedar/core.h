#pragma once
#include "cz_common.h"

namespace schedar {
// Controls, in definition order.
enum Control { c_amount, c_width, c_wave, c_sweep, control_count };
const unsigned max_segments = 5;

struct Voice {
    opxy::u32 phase;
    opxy::i32 env;                      // Sweep envelope, Q31
    opxy::i32 smooth[control_count];    // Q31
    opxy::i32 dc_x, dc_y;               // DC blocker
    // This block's phase map: piecewise-linear segments in Q16 (start x, start g, slope·2^25).
    opxy::u32 seg_x[max_segments], seg_g[max_segments];
    opxy::i32 seg_s[max_segments];
    opxy::u8 primed;
};
struct Shared {};
}
