#pragma once
#include <opxy/draw.hpp>

namespace segin {
// The cell terrace as burst islands: 16 columns of one cycle by 9 rows of recent history. Cells inside a
// pulse rise from the floor; the rest of the cycle is floor, drawn in outline. Back row first; node
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
    opxy::i16 cv[rows][cols];   // height above the floor, Q15; −1 marks floor (outside the burst)
    opxy::u8 lead[rows][cols];  // 1 where a pulse begins
    opxy::u32 drawn[cells];     // what each node shows: y, shade and paint, packed
};
}
