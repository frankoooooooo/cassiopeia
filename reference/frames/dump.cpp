// Runs a terrace engine's real display() on the host and writes one 480 x 220 SVG frame.
// c++ -std=c++17 -DENGINE=schedar -I ../open-xy-staging/sdk/include reference/frames/dump.cpp -o d
// d <out.svg> <rest|playing> p0 p1 p2 p3 [label0..3 value0..3]
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
    FILE* o = std::fopen(argv[1], "w");
    std::fprintf(o, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"480\" height=\"220\" viewBox=\"0 0 480 220\">\n<rect width=\"480\" height=\"220\" fill=\"#000\"/>\n");
    const char* steps[8] = {"#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD"};
    for (int i = 0; i < 8; ++i) std::fprintf(o, "<rect x=\"%d\" y=\"0\" width=\"60\" height=\"20\" fill=\"%s\"/>\n", i * 60, steps[i]);
    for (int i = 0; i < 4 && 7 + 2 * i + 1 < argc; ++i)
        std::fprintf(o, "<text x=\"%d\" y=\"13\" font-size=\"10\" fill=\"#CBCBCB\" font-family=\"UniversNextPro-Light, Helvetica\">%s</text><text x=\"%d\" y=\"17\" font-size=\"16\" fill=\"#000\" font-family=\"UniversNextPro-Light, Helvetica\">%s</text>\n",
                     5 + 120 * i, argv[7 + 2 * i], 65 + 120 * i, argv[8 + 2 * i]);
    for (unsigned n = 0; n < E::Frame::node_count; ++n) {
        const opxy::draw::Node& d = fr.nodes[n];
        const int g = static_cast<int>(30 + 13.5 * d.shade + 0.5);
        std::fprintf(o, "<path transform=\"matrix(%.4f 0 0 %.4f %.2f %.2f)\" d=\"M-15.5 3 L-4.5 -3 L15.5 -3 L4.5 3 Z\" fill=\"#%02X%02X%02X\" stroke=\"#%02X%02X%02X\" stroke-width=\"0.5\"/>\n",
                     d.scale / 1024.0, d.scale / 1024.0, d.x / 1024.0, d.y / 1024.0, g, g, g, g, g, g);
    }
    std::fprintf(o, "</svg>\n");
    std::fclose(o);
    return 0;
}
