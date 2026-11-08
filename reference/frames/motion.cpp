// Plays a scripted take through an engine's real display() at 30 frames a second and writes the frames
// as JSON deltas (only nodes that changed), for the review page's player.
// c++ -std=c++17 -iquote <engine folder> -I ../open-xy-staging/sdk/include -DENGINE=navi motion.cpp -o m
// m <out.json> p0 p1 p2 p3 sweep first_to second_to [frames, default 150]
// Take (150 frames, 5 s): rest 1 s; a held note at 1 s; control 1 ramps to first_to over 2.5..3.8 s;
// control 2 ramps to second_to over 3.8..5 s; the note is released at 4.7 s.
#include "display.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace E = ENGINE;
int main(int argc, char** argv) {
    if (argc < 9) return 2;
    static E::DisplayState st;
    static E::Frame fr;
    double p[4];
    for (int i = 0; i < 4; ++i) p[i] = std::atof(argv[2 + i]);
    p[3] = std::atof(argv[6]);
    const double to1 = std::atof(argv[7]), to2 = std::atof(argv[8]), from1 = p[0], from2 = p[1];
    opxy::draw::Input in = {};
    for (int i = 0; i < 4; ++i) in.shown[i] = static_cast<opxy::u8>(i);
    in.elapsed = 34;
    in.amp.attack = in.amp.decay = in.amp.sustain = in.amp.release = -1;
    E::display_reset(st);
    static int last[E::Frame::node_count][7];
    static char last_header[4][8];
    FILE* o = std::fopen(argv[1], "w");
    std::fprintf(o, "[");
    const int total = argc > 9 ? std::atoi(argv[9]) : 150;  // longer takes show the settle after release
    for (int f = 0; f < total; ++f) {
        if (f == 30) { in.note_serial = 1; in.held_notes = 1; }
        if (f == 141) { in.held_notes = 0; in.note_off_serial = 1; }
        const double r1 = f < 75 ? 0 : f >= 114 ? 1 : (f - 75) / 39.0, r2 = f < 114 ? 0 : (f - 114) / 35.0;
        const double now[4] = {from1 + (to1 - from1) * r1, from2 + (to2 - from2) * r2, p[2], p[3]};
        for (int i = 0; i < 4; ++i) in.parameters[i] = in.slots[i] = static_cast<opxy::i32>(now[i] * 2147483647.0);
        E::display(st, in, fr);
        std::fprintf(o, "%s{\"c\":[%.4f,%.4f,%.4f,%.4f],\"n\":[", f ? "," : "", now[0], now[1], now[2], now[3]);
        bool first = true;
        for (unsigned n = 0; n < E::Frame::node_count; ++n) {
            const opxy::draw::Node& d = fr.nodes[n];
            const int v[7] = {d.x / 256, d.y / 256, d.scale / 16, static_cast<int>(d.shade), static_cast<int>(d.paint), d.cos, d.sin};  // x, y Q2; scale /64; turn Q10
            if (f && !std::memcmp(v, last[n], sizeof(v))) continue;
            std::memcpy(last[n], v, sizeof(v));
            std::fprintf(o, "%s[%u,%d,%d,%d,%d,%d,%d,%d]", first ? "" : ",", n, v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
            first = false;
        }
        std::fprintf(o, "],\"h\":[");
        for (int i = 0; i < 4; ++i) { std::fprintf(o, "%s\"%s\"", i ? "," : "", fr.header[i]); }
        std::fprintf(o, "]}");
    }
    std::fprintf(o, "]\n");
    std::fclose(o);
    return 0;
}
