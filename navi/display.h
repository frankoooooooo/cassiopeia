#pragma once
#include <opxy/draw.hpp>

namespace navi {
// Banded stacks: 12 columns x 3 rows of piles of up to 7 flat floating tops. History keeps 9 slots, 70 ms
// apart (rows draw every third); the front slot is sampled at 32 points across the cycle.
const opxy::u32 samples = 32, history = 9;
const opxy::u32 node_budget = 252;

// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, node_budget, 0, 16>;

struct DisplayState {
    opxy::draw::NoteTracker notes;
    opxy::u32 serial;     // last note-start serial seen
    opxy::u32 started;    // 0 until the first frame has placed every node
    opxy::u32 cursor;     // where the redraw scan resumes
    opxy::i32 env;        // the Sweep envelope, Q31, restarted by each note
    opxy::i32 clock;      // Q10 seconds toward the next history step
    opxy::i16 wave[history][samples];   // the bent wave at (k + 0.5)/32 of the cycle, Q15
    opxy::u8 cycle[history][samples];   // which cosine cycle of the bent phase each sample falls in
    opxy::u32 drawn[node_budget];       // what each node shows, packed
};
}
