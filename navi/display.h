#pragma once
#include <opxy/draw.hpp>

namespace navi {
// Banded stacks: 12 columns x 3 rows of piles of up to 7 flat floating tops. Back row first; node
// n = ((2 − row)·12 + column)·7 + piece. History keeps 7 eased slots; rows draw slots 0, 3 and 6.
const opxy::u32 cols = 12, rows = 3, pieces = 7, history = 7;
const opxy::u32 nodes = cols * rows * pieces;

// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, nodes, 0, 16>;

struct DisplayState {
    opxy::draw::NoteTracker notes;
    opxy::u32 serial;     // last note-start serial seen
    opxy::u32 started;    // 0 until the first frame has placed every node
    opxy::u32 cursor;     // where the redraw scan resumes
    opxy::i32 env;        // the Sweep envelope, Q31, restarted by each note
    opxy::i16 wave[history][cols];   // the bent wave at each column's centre, Q15
    opxy::u8 cycle[history][cols];   // which cosine cycle of the bent phase it falls in
    opxy::u8 count[rows][cols];      // each drawn pile's height in squares, with hysteresis
    opxy::u32 drawn[nodes];          // what each node shows, packed
};
}
