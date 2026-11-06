#pragma once
#include <opxy/draw.hpp>

namespace schedar {
// The cell terrace: 16 columns of phase by 9 rows of recent history, oblique, under the stock header.
// Nodes are drawn back row first (node n = (rows − 1 − row)·cols + column), so near cells hide far ones.
const opxy::u32 cols = 16, rows = 9, cells = cols * rows;

// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, cells, 0, 16>;

struct DisplayState {
    opxy::draw::NoteTracker notes;
    opxy::u32 serial;     // last note-start serial seen
    opxy::u32 started;    // 0 until the first frame has placed every cell
    opxy::u32 cursor;     // where the redraw scan resumes
    opxy::i32 env;        // the Sweep envelope, Q31, restarted by each note
    opxy::i32 clock;      // Q10 seconds toward the next history step
    opxy::i16 cx[rows][cols];  // each cell's centre, px Q2, before the row's oblique offset
    opxy::u16 cs[rows][cols];  // its width as a node scale, Q10
    opxy::i16 cv[rows][cols];  // its height, the wave's value there, Q15
    opxy::u32 drawn[cells];    // what each node shows: x, y, scale and shade, packed
};
}
