#pragma once
#include <opxy/draw.hpp>

namespace caph {
// The cell terrace as stacks: 12 columns by 3 rows, each column a pile of up to 7 small floating tops as
// tall as the pair's summed wave there. Back row first; node n = ((2 − row)·12 + column)·7 + piece.
const opxy::u32 cols = 12, rows = 3, pieces = 7, history = 9;
const opxy::u32 nodes = cols * rows * pieces;

// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, nodes, 0, 16>;

struct DisplayState {
    opxy::draw::NoteTracker notes;
    opxy::u32 serial;     // last note-start serial seen
    opxy::u32 started;    // 0 until the first frame has placed every node
    opxy::u32 cursor;     // where the redraw scan resumes
    opxy::u32 offset;     // B's phase lead over A, Q32: drifts at the beat rate while notes sound
    opxy::i32 env;        // the Sweep envelope, Q31, restarted by each note
    opxy::i32 clock;      // Q10 seconds toward the next history step
    opxy::i16 mix[history][cols];   // the pair's summed wave per column, Q15; rows 0, 3, 6 are drawn
    opxy::u32 drawn[nodes];         // what each node shows, packed
};
}
