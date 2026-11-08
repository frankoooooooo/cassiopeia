#pragma once
#include <opxy/draw.hpp>

namespace achird {
// The cell terrace as stepped blocks: 16 columns, one per Walsh step, by 8 rows of recent history. Each
// cell is a riser (where its level steps from the column before) under a top. Back row first; node
// n = ((rows − 1 − row)·cols + column)·2 + (0 riser, 1 top).
const opxy::u32 cols = 16, rows = 8, cells = cols * rows, parts = 2 * cells;

// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, parts, 0, 16>;

struct DisplayState {
    opxy::draw::NoteTracker notes;
    opxy::u32 serial;     // last note-start serial seen
    opxy::u32 started;    // 0 until the first frame has placed every part
    opxy::u32 cursor;     // where the redraw scan resumes
    opxy::i32 env;        // the Sweep envelope, Q31, restarted by each note
    opxy::i32 lv[rows][cols];   // each step's level, Q15
    opxy::u32 drawn[parts];     // what each node shows: y, scale and shade, packed
};
}
