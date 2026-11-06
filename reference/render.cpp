// Host render of one family engine into a mono 16-bit WAV, for listening. ENGINE is the machine folder.
// c++ -std=c++17 -O2 -DENGINE=cz101 -I ../open-xy-staging/sdk/include reference/render.cpp -o renders/r
// r <out.wav> <phrase|tour> p0 p1 p2 p3   (normalized control words, as in definition.json presets)
// phrase: C2 G2 C3 Eb3 G3 C4, 0.7 s each, held 0.5 s. tour: one C3 for 4 s with Amount ramped 0 → 1.
#define STR2(x) #x
#define STR(x) STR2(x)
#include STR(../ENGINE/engine.cpp)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace E = ENGINE;
int main(int argc, char** argv) {
    if (argc < 7) { std::fprintf(stderr, "usage: r out.wav phrase|tour p0 p1 p2 p3\n"); return 2; }
    const bool tour = !std::strcmp(argv[2], "tour");
    double d[8] = {0};
    for (int i = 0; i < 4; ++i) d[i] = std::atof(argv[3 + i]);
    static E::Voice v; static E::Shared s;
    const int notes[6] = {36, 43, 48, 51, 55, 60};
    const int count = tour ? 1 : 6, step = tour ? 44100 * 4 : 30870, held = tour ? step - 2205 : 22050;
    const int n = count * step;
    short* pcm = new short[n];
    opxy::i32 out[64], p[8];
    E::initialize(v, s, 1);
    double peak = 0, sq = 0;
    for (int k = 0; k < count; ++k) {
        const double hz = 440 * std::pow(2, ((tour ? 48 : notes[k]) - 69) / 12.0);
        opxy::u32 pitch[64];
        for (int i = 0; i < 64; ++i) pitch[i] = static_cast<opxy::u32>(hz / 44100 * 4294967296.0);
        E::start(v, s, 1);
        for (int b = 0; b < step; b += 64) {
            for (int i = 0; i < 8; ++i) p[i] = static_cast<opxy::i32>(d[i] * 2147483647.0);
            if (tour) p[0] = static_cast<opxy::i32>(std::fmin(1.0, b / double(held)) * 2147483647.0);
            E::render(v, s, 1, out, p, pitch);
            for (int i = 0; i < 64 && b + i < step; ++i) {
                const int t = b + i;
                const double amp = std::fmin(t / 88.0, 1) * (t < held ? 1 : std::exp(-(t - held) / 2000.0));
                const double x = out[i] / 2147483648.0 * amp * 2.4;  // the 0.5 FS declared peak → 1.2 at most; typical ~0.6–0.9
                peak = std::fmax(peak, std::fabs(x));
                sq += x * x;
                pcm[k * step + t] = static_cast<short>(std::fmax(-1, std::fmin(1, x)) * 32767);
            }
        }
    }
    const double rms_db = 10 * std::log10(sq / n + 1e-20);
    std::printf("%s peak %.3f rms %.1f dBFS\n", argv[1], peak, rms_db);
    if (rms_db < -60) { std::fprintf(stderr, "silent render\n"); return 1; }
    FILE* f = std::fopen(argv[1], "wb");
    const int bytes = n * 2, rate = 44100, byte_rate = 88200, sixteen = 16, riff = 36 + bytes;
    const short fmt[] = {1, 1}, align[] = {2, 16};
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f); std::fwrite(&sixteen, 4, 1, f);
    std::fwrite(fmt, 2, 2, f); std::fwrite(&rate, 4, 1, f); std::fwrite(&byte_rate, 4, 1, f); std::fwrite(align, 2, 2, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&bytes, 4, 1, f); std::fwrite(pcm, 2, n, f); std::fclose(f);
    return 0;
}
