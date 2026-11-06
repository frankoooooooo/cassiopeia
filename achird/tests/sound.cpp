// Achird sound checks, run by python sdk.py test --machine <folder>.
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
    static achird::Voice v;
    static achird::Shared s;
    const i32 p[8] = {word(shape), word(width), word(detune), word(sweep), 0, 0, 0, 0};
    u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = inc;
    i32 out[opxy::block_frames];
    achird::initialize(v, s, 1);
    achird::start(v, s, 1);
    Run r{true, 0, 0, 0};
    double sq = 0, diff = 0, mag = 0, last = 0, last2 = 0, lo = 1e9, hi = 0, window = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        if (restart_mid && b == blocks / 2) achird::start(v, s, 1);
        achird::render(v, s, 1, out, p, pitch);
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
    // Tilt brightens at 220 Hz (Sweep centred).
    const double dark = run(0, 0.5, 0, 0.5, 21426140u, 128, false).brightness;
    const double harsh = run(1, 0.5, 0, 0.5, 21426140u, 128, false).brightness;
    if (!(harsh > dark * 1.5)) { std::printf("  %.5f %.5f\n", dark, harsh); fail("Tilt does not brighten", 0, 1, 0); }
    // Balance and Fold change the cycle.
    const double odd = run(0.5, 1, 0, 0.5, 21426140u, 64, false).rms, even = run(0.5, 0, 0, 0.5, 21426140u, 64, false).rms;
    if (odd == even) fail("Balance does nothing", 0, 1, 0);
    // Fold adds turning points to the 16-step cycle (where the steps change direction).
    const auto turns = [](i32 fold) {
        i32 lv[16];
        achird::levels(lv, 16384, 16384, fold, 0);
        int t = 0;
        for (int s = 0; s < 16; ++s) {
            const i32 a = lv[(s + 1) & 15] - lv[s], b = lv[(s + 2) & 15] - lv[(s + 1) & 15];
            t += (a > 0 && b < 0) || (a < 0 && b > 0);
        }
        return t;
    };
    if (!(turns(32767) > turns(0))) { std::printf("  turns %d %d\n", turns(0), turns(32767)); fail("Fold does not add turns", 0, 1, 0); }
    // The 16 levels at rest are a ±1-bounded step cycle with a real swing.
    i32 lv[16];
    achird::levels(lv, 9830, 16384, 0, 0);
    i32 lo = 32767, hi = -32768;
    for (i32 x : lv) { lo = x < lo ? x : lo; hi = x > hi ? x : hi; }
    if (hi - lo < 32768) { std::printf("  levels %d .. %d\n", lo, hi); fail("levels barely swing", 0, 0, 0); }
    // Sweep: up brightens the strike, down darkens it.
    const double up = run(0.4, 0.5, 0, 1.0, 21426140u, 40, false).brightness;
    const double mid = run(0.4, 0.5, 0, 0.5, 21426140u, 40, false).brightness;
    const double down = run(0.4, 0.5, 0, 0.0, 21426140u, 40, false).brightness;
    if (!(up > mid && mid > down)) { std::printf("  sweep %.4f %.4f %.4f\n", up, mid, down); fail("Sweep direction", 0, 0, 0); }
    if (failures) std::printf("%d failures\n", failures);
    else std::puts("ok");
    return failures ? 1 : 0;
}
