#pragma once
#include <opxy/draw.hpp>

namespace caph {
// The cell terrace, split: 14 columns of phase by 8 rows of recent history, each cell two half tops,
// oscillator A on the left and its detuned twin B on the right. Back row first; node
// n = ((rows − 1 − row)·cols + column)·2 + half.
const opxy::u32 cols = 14, rows = 8, cells = cols * rows, halves = 2 * cells;

// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, halves, 0, 16>;

struct DisplayState {
    opxy::draw::NoteTracker notes;
    opxy::u32 serial;     // last note-start serial seen
    opxy::u32 started;    // 0 until the first frame has placed every half
    opxy::u32 cursor;     // where the redraw scan resumes
    opxy::u32 offset;     // B's phase lead over A, Q32: drifts at the beat rate while notes sound
    opxy::i32 env;        // the Sweep envelope, Q31, restarted by each note
    opxy::i32 clock;      // Q10 seconds toward the next history step
    opxy::i16 cv[rows][2][cols];  // each half's height: the wave there, Q15
    opxy::u32 drawn[halves];      // what each node shows: y and shade, packed
};
}
