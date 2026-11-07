#pragma once
#include <opxy/draw.hpp>
#include "core.h"

namespace ruchbah {
// The cell terrace of masses: 14 columns of the ring by 9 rows of recent history, each cell a top that
// swells with how fast its stretch of the ring is moving. Back row first; node n = (rows − 1 − row)·cols + column.
const opxy::u32 cols = 14, rows = 9, cells = cols * rows;

// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, cells, 0, 16>;

struct DisplayState {
    opxy::draw::NoteTracker notes;
    Ring ring;            // the screen's own copy of the voice's ring, stepped at the same rate
    opxy::u32 serial;     // last note-start serial seen
    opxy::u32 shown;      // the Excite choice last struck
    opxy::i32 rest_mix;   // Q15: how far the terrace has eased into the Excite pose (at rest)
    opxy::i32 gain;       // Q8 display gain on the ring, 1..4×, so dispersed ripples stay visible
    opxy::i16 pose[masses];  // the Excite choice's shape, shown faintly while nothing sounds
    opxy::u32 started;    // 0 until the first frame has placed every part
    opxy::u32 cursor;     // where the redraw scan resumes
    opxy::i32 env;        // the Sweep envelope, Q31, restarted by each note
    opxy::i32 clock;      // Q10 seconds toward the next history step
    opxy::i32 due;        // ring steps owed, Q10 (689 a second)
    opxy::i16 cv[rows][cols];   // each cell's height: the ring there, Q15
    opxy::u16 cs[rows][cols];   // each cell's size: how fast the ring moves there, as a node scale (Q10)
    opxy::i16 calm[cols];       // the front row's height, eased toward the ring (Q15)
    opxy::i16 energy[cols];     // the front row's motion, a follower: quick to rise, slow to fall (Q15)
    opxy::u32 drawn[cells];     // what each node shows: y, scale and shade, packed
};
}
