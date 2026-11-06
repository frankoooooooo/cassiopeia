// Navi sound checks, run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>
#include <initializer_list>
#include <utility>

namespace {
using opxy::i32;
using opxy::u32;
int failures = 0;
void fail(const char* what, double a, double b, u32 inc) {
    std::printf("FAIL %s: %.2f %.2f inc %u\n", what, a, b, inc);
    ++failures;
}
i32 word(double v) { return static_cast<i32>(v * 2147483647.0); }

struct Run { bool bounded; double rms, brightness, wobble; };

// brightness = mean |second difference| / mean |sample|; wobble = spread of 16-block energy (beating).
Run run(double shape, double width, double detune, double sweep, u32 inc, unsigned blocks, bool restart_mid) {
    static navi::Voice v;
    static navi::Shared s;
    const i32 p[8] = {word(shape), word(width), word(detune), word(sweep), 0, 0, 0, 0};
    u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = inc;
    i32 out[opxy::block_frames];
    navi::initialize(v, s, 1);
    navi::start(v, s, 1);
    Run r{true, 0, 0, 0};
    double sq = 0, diff = 0, mag = 0, last = 0, last2 = 0, lo = 1e9, hi = 0, window = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        if (restart_mid && b == blocks / 2) navi::start(v, s, 1);
        navi::render(v, s, 1, out, p, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            if (out[i] > 1073741824 || out[i] < -1073741824) r.bounded = false;
            const double x = out[i] / 2147483648.0;
            if (b >= 8) {
                sq += x * x;
                mag += x < 0 ? -x : x;
                const double d = x - 2 * last + last2;
                diff += d < 0 ? -d : d;
                window += x * x;
            }
            last2 = last;
            last = x;
        }
        if (b >= 8 && (b & 15) == 15) {  // 16-block (23 ms) windows: several periods, far shorter than a beat
            lo = window < lo ? window : lo;
            hi = window > hi ? window : hi;
            window = 0;
        }
    }
    r.rms = sq / ((blocks - 8) * double(opxy::block_frames));
    r.brightness = mag > 0 ? diff / mag : 0;
    r.wobble = hi > 0 ? (hi - lo) / hi : 0;
    return r;
}
}

int main() {
    // Formant's reach: v from 0.5 up to 1.5 (Range 0) .. 8 (Range 1). An overflow here once capped v near 1.
    for (const auto& c : {std::pair<i32, double>{0, 1.5}, {16384, 4.75}, {32767, 8.0}}) {
        const double got = navi::formant16(32767, c.first) / 65536.0;
        if (got < c.second - 0.01 || got > c.second + 0.01) { std::printf("  formant16 range %d: %.3f, want %.3f\n", c.first, got, c.second); fail("Formant reach", 0, 0, 0); }
    }
    if (navi::formant16(0, 32767) != 32768u) fail("Formant 0 is not v = 0.5", 0, 0, 0);
    const u32 incs[3] = {1947830u, 42852281u, 779132760u};  // 20 Hz, 440 Hz, 8 kHz at 44.1 kHz
    for (double sh : {0.0, 0.25, 0.5, 0.75, 1.0})
        for (double wd : {0.0, 1.0})
            for (double dt : {0.0, 1.0})
                for (double sw : {0.0, 0.5, 1.0})
                    for (u32 inc : incs)
                        for (bool mid : {false, true}) {
                            const Run r = run(sh, wd, dt, sw, inc, 64, mid);
                            if (!r.bounded) fail("above the declared peak", sh, wd, inc);
                            if (r.rms < 1e-6) fail("silent", sh, wd, inc);
                        }
    // Formant brightens at 220 Hz, Sweep centred (Inflect 0.5, Range 0.5).
    const double pure = run(0, 0.5, 0.5, 0.5, 21426140u, 128, false).brightness;
    const double bright = run(1, 0.5, 0.5, 0.5, 21426140u, 128, false).brightness;
    if (!(bright > pure * 3)) { std::printf("  %.5f %.5f\n", pure, bright); fail("Formant does not brighten", 0, 1, 0); }
    // At Formant 0 and Inflect 0.5 the voice is a pure cosine: brightness (2π·f/fs)².
    const double w0 = 2 * 3.14159265 * 220 / 44100;
    if (pure > w0 * w0 * 1.1) { std::printf("  pure %.6f vs %.6f\n", pure, w0 * w0); fail("not pure at rest", 0, 0, 0); }
    // Range lifts how far Formant reaches.
    const double narrow = run(1, 0.5, 0, 0.5, 21426140u, 128, false).brightness;
    const double wide = run(1, 0.5, 1, 0.5, 21426140u, 128, false).brightness;
    if (!(wide > narrow * 1.5)) { std::printf("  range %.5f %.5f\n", narrow, wide); fail("Range does not widen", 0, 1, 0); }
    // Sweep: up brightens the strike, down darkens it.
    const double up = run(0.4, 0.5, 0.5, 1.0, 21426140u, 40, false).brightness;
    const double mid = run(0.4, 0.5, 0.5, 0.5, 21426140u, 40, false).brightness;
    const double down = run(0.4, 0.5, 0.5, 0.0, 21426140u, 40, false).brightness;
    if (!(up > mid && mid > down)) { std::printf("  sweep %.4f %.4f %.4f\n", up, mid, down); fail("Sweep direction", 0, 0, 0); }
    if (failures) std::printf("%d failures\n", failures);
    else std::puts("ok");
    return failures ? 1 : 0;
}
