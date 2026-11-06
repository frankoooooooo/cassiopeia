// Caph sound checks, run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>
#include <initializer_list>

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
    static caph::Voice v;
    static caph::Shared s;
    const i32 p[8] = {word(shape), word(width), word(detune), word(sweep), 0, 0, 0, 0};
    u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = inc;
    i32 out[opxy::block_frames];
    caph::initialize(v, s, 1);
    caph::start(v, s, 1);
    Run r{true, 0, 0, 0};
    double sq = 0, diff = 0, mag = 0, last = 0, last2 = 0, lo = 1e9, hi = 0, window = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        if (restart_mid && b == blocks / 2) caph::start(v, s, 1);
        caph::render(v, s, 1, out, p, pitch);
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
    // Shape brightens: triangle < saw at 220 Hz, Sweep centred.
    const double tri = run(0, 0, 0, 0.5, 21426140u, 128, false).brightness;
    const double saw = run(1, 0, 0, 0.5, 21426140u, 128, false).brightness;
    if (!(saw > tri * 2)) { std::printf("  %.5f %.5f\n", tri, saw); fail("Shape does not brighten", 0, 1, 0); }
    // Detune beats: the pair's level wobbles far more at full Detune than at none.
    const double still = run(1, 0, 0, 0.5, 21426140u, 1024, false).wobble;
    const double beat = run(1, 0, 1, 0.5, 21426140u, 1024, false).wobble;
    if (!(beat > still + 0.2)) { std::printf("  wobble %.3f %.3f\n", still, beat); fail("Detune does not beat", 0, 1, 0); }
    // Sweep: up brightens the strike, down darkens it (from the triangle side, where this measure is
    // monotonic: a square's edges score above a saw's).
    const double up = run(0.2, 0, 0, 1.0, 21426140u, 40, false).brightness;
    const double mid = run(0.2, 0, 0, 0.5, 21426140u, 40, false).brightness;
    const double down = run(0.2, 0, 0, 0.0, 21426140u, 40, false).brightness;
    if (!(up > mid && mid > down)) { std::printf("  sweep %.4f %.4f %.4f\n", up, mid, down); fail("Sweep direction", 0, 0, 0); }
    if (failures) std::printf("%d failures\n", failures);
    else std::puts("ok");
    return failures ? 1 : 0;
}
