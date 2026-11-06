// CZ-101 sound checks, run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>
#include <initializer_list>

namespace {
using opxy::i32;
using opxy::u32;
int failures = 0;
void fail(const char* what, unsigned wave, i32 amount, u32 inc) {
    std::printf("FAIL %s: wave %u amount %d inc %u\n", what, wave, amount, inc);
    ++failures;
}
i32 word(double v) { return static_cast<i32>(v * 2147483647.0); }
i32 wave_word(unsigned w) { return static_cast<i32>((w * 2 + 1) * (2147483647.0 / 16)); }

struct Run { bool bounded; double rms, brightness; };

// Render `blocks` blocks of one note; brightness = mean |second difference| / mean |sample| (a phase
// warp keeps a cosine's first-difference total, so only curvature shows it).
Run run(unsigned wave, double amount, double width, double sweep, u32 inc, unsigned blocks, bool restart_mid) {
    static cz101::Voice v;
    static cz101::Shared s;
    const i32 p[8] = {word(amount), word(width), wave_word(wave), word(sweep), 0, 0, 0, 0};
    u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = inc;
    i32 out[opxy::block_frames];
    cz101::initialize(v, s, 1);
    cz101::start(v, s, 1);
    Run r{true, 0, 0};
    double sq = 0, diff = 0, mag = 0;
    double last = 0, last2 = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        if (restart_mid && b == blocks / 2) cz101::start(v, s, 1);
        cz101::render(v, s, 1, out, p, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            if (out[i] > 1073741824 || out[i] < -1073741824) r.bounded = false;
            if (b >= 8) {  // past the DC blocker's settling
                const double x = out[i] / 2147483648.0;
                sq += x * x;
                mag += x < 0 ? -x : x;
                const double d = x - 2 * last + last2;
                diff += d < 0 ? -d : d;
            }
            last2 = last;
            last = out[i] / 2147483648.0;
        }
    }
    const double n = (blocks - 8) * double(opxy::block_frames);
    r.rms = sq / n;
    r.brightness = mag > 0 ? diff / mag : 0;
    return r;
}
}

int main() {
    const u32 incs[3] = {1947830u, 42852281u, 779132760u};  // 20 Hz, 440 Hz, 8 kHz at 44.1 kHz
    for (unsigned w = 0; w < 8; ++w)
        for (double a : {0.0, 1.0})
            for (double wd : {0.0, 1.0})
                for (double sw : {0.0, 0.5, 1.0})
                    for (u32 inc : incs)
                        for (bool mid : {false, true}) {
                            const Run r = run(w, a, wd, sw, inc, 64, mid);
                            if (!r.bounded) fail("above the declared peak", w, word(a), inc);
                            if (r.rms < 1e-6) fail("silent", w, word(a), inc);
                        }
    // Amount brightens every wave at 220 Hz (Sweep centred, so the envelope stays out).
    for (unsigned w = 0; w < 8; ++w) {
        const double dark = run(w, 0.0, 0, 0.5, 21426140u, 128, false).brightness;
        const double bright = run(w, 1.0, 0, 0.5, 21426140u, 128, false).brightness;
        if (!(bright > dark * 1.2)) {
            std::printf("  brightness %.4f -> %.4f\n", dark, bright);
            fail("Amount does not brighten", w, 0, 21426140u);
        }
    }
    // Saw at Amount 0 is a pure cosine, whose brightness is (2π·f/fs)².
    const double pure = run(0, 0.0, 0, 0.5, 21426140u, 128, false).brightness, w0 = 2 * 3.14159265 * 220 / 44100;
    if (pure > w0 * w0 * 1.1) {
        std::printf("  pure %.6f vs %.6f\n", pure, w0 * w0);
        fail("Amount 0 is not pure", 0, 0, 21426140u);
    }
    // Sweep up brightens the strike and falls back; Sweep down darkens it.
    {
        const double up = run(0, 0.3, 0, 1.0, 21426140u, 40, false).brightness;
        const double mid = run(0, 0.3, 0, 0.5, 21426140u, 40, false).brightness;
        const double down = run(0, 0.3, 0, 0.0, 21426140u, 40, false).brightness;
        if (!(up > mid && mid > down)) {
            std::printf("  sweep %.4f %.4f %.4f\n", up, mid, down);
            fail("Sweep direction", 0, 0, 0);
        }
    }
    if (failures) std::printf("%d failures\n", failures);
    else std::puts("ok");
    return failures ? 1 : 0;
}
