#pragma once
#include <opxy/draw.hpp>

namespace navi {
// The cell terrace as tilted slats: 16 columns of phase by 9 rows of recent history, each cell a thin
// slat turned and stretched so its ends sit at the wave's height at either edge. Back row first; node
// n = (rows − 1 − row)·cols + column.
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
    opxy::u16 seam[rows];               // the column holding the inflection point, per row
    opxy::i16 ev[rows][cols + 1];       // the wave at each column edge, Q15
    opxy::u32 drawn[cells];             // what each node shows: y, turn and shade, packed
};
}
