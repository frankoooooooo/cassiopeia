// Runs an engine's real display() on the host and writes its frame as text, for compose.py.
// c++ -std=c++17 -DENGINE=schedar -I ../open-xy-staging/sdk/include reference/frames/dump.cpp -o d
// d <out.txt> <rest|playing> p0 p1 p2 p3
#define STR2(x) #x
#define STR(x) STR2(x)
#include STR(../../ENGINE/display.cpp)
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace E = ENGINE;
int main(int argc, char** argv) {
    if (argc < 7) return 2;
    static E::DisplayState st;
    static E::Frame fr;
    opxy::draw::Input in = {};
    for (int i = 0; i < 4; ++i) in.parameters[i] = in.slots[i] = static_cast<opxy::i32>(std::atof(argv[3 + i]) * 2147483647.0);
    for (int i = 0; i < 4; ++i) in.shown[i] = static_cast<opxy::u8>(i);
    in.elapsed = 34;  // ~33 ms frames
    in.amp.attack = in.amp.decay = in.amp.sustain = in.amp.release = -1;
    const bool playing = !std::strcmp(argv[2], "playing");
    E::display_reset(st);
    for (int f = 0; f < 120; ++f) E::display(st, in, fr);  // settle at rest
    if (playing) {
        in.note_serial = 1; in.held_notes = 1;
        for (int f = 0; f < 9; ++f) E::display(st, in, fr);  // 0.3 s after the strike
    }
    for (int f = 0; f < 8; ++f) { in.elapsed = 0; E::display(st, in, fr); }  // flush the redraw budget
    // One line per node (index, x, y, scale, shade, cos, sin, paint; pixels), then the header overrides; compose.py draws them.
    FILE* o = std::fopen(argv[1], "w");
    for (unsigned n = 0; n < E::Frame::node_count; ++n) {
        const opxy::draw::Node& d = fr.nodes[n];
        std::fprintf(o, "node %u %.3f %.3f %.4f %u %.4f %.4f %u\n", n, d.x / 1024.0, d.y / 1024.0, d.scale / 1024.0, d.shade, d.cos / 1024.0, d.sin / 1024.0, static_cast<unsigned>(d.paint));
    }
    for (unsigned i = 0; i < 4; ++i) std::fprintf(o, "header %u %s\n", i, fr.header[i]);
    std::fclose(o);
    return 0;
}
