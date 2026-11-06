// Ruchbah sound checks, run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>
#include <initializer_list>

namespace {
using opxy::i32;
using opxy::u32;
int failures = 0;
void fail(const char* what, double a, double b) {
    std::printf("FAIL %s: %.3f %.3f\n", what, a, b);
    ++failures;
}
i32 word(double v) { return static_cast<i32>(v * 2147483647.0); }
i32 choice_word(u32 c, u32 n) { return static_cast<i32>((c * 2 + 1) * (2147483647.0 / (2 * n))); }

struct Run { bool bounded; double early, late; u32 hash; };

// RMS of the first and the last 0.25 s of `blocks` blocks, and a hash of every sample.
Run run(double stiff, double damping, u32 excite, double sweep, u32 inc, unsigned blocks, bool restart_mid) {
    static ruchbah::Voice v;
    static ruchbah::Shared s;
    const i32 p[8] = {word(stiff), word(damping), choice_word(excite, 4), word(sweep), 0, 0, 0, 0};
    u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = inc;
    i32 out[opxy::block_frames];
    v = ruchbah::Voice{};
    ruchbah::initialize(v, s, 1);
    Run r{true, 0, 0, 2166136261u};
    for (unsigned b = 0; b < blocks; ++b) {
        if (restart_mid && b == blocks / 2) ruchbah::start(v, s, 1);
        ruchbah::render(v, s, 1, out, p, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            if (out[i] > 1073741824 || out[i] < -1073741824) r.bounded = false;
            const double x = out[i] / 2147483648.0;
            if (b >= 8 && b < 8 + 172) r.early += x * x;
            if (b + 172 >= blocks) r.late += x * x;
            r.hash = (r.hash ^ static_cast<u32>(out[i])) * 16777619u;
        }
    }
    return r;
}
}

int main() {
    const u32 incs[3] = {1947830u, 42852281u, 779132760u};
    for (u32 e = 0; e < 4; ++e)
        for (double st : {0.0, 1.0})
            for (double dm : {0.0, 1.0})
                for (double sw : {0.0, 0.5, 1.0})
                    for (u32 inc : incs)
                        for (bool mid : {false, true}) {
                            const Run r = run(st, dm, e, sw, inc, 400, mid);
                            if (!r.bounded) fail("above the declared peak", e, st);
                            if (r.early < 1e-4) fail("silent strike", e, st);
                        }
    // Damping: the sound settles far sooner when high (3 s note).
    const Run loose = run(0.4, 0.0, 0, 0.5, 21426140u, 2067, false), tight = run(0.4, 1.0, 0, 0.5, 21426140u, 2067, false);
    if (!(tight.late < loose.late * 0.1)) fail("Damping does not settle", loose.late, tight.late);
    if (!(loose.late > loose.early * 0.05)) fail("Damping 0 dies within 3 s", loose.early, loose.late);
    // Each Excite strikes a different shape.
    u32 hashes[4];
    for (u32 e = 0; e < 4; ++e) hashes[e] = run(0.4, 0.3, e, 0.5, 21426140u, 64, false).hash;
    for (u32 a = 0; a < 4; ++a)
        for (u32 b = a + 1; b < 4; ++b)
            if (hashes[a] == hashes[b]) fail("two Excite shapes sound the same", a, b);
    // Stiff and Sweep change the motion.
    if (run(0.0, 0.3, 0, 0.5, 21426140u, 200, false).hash == run(1.0, 0.3, 0, 0.5, 21426140u, 200, false).hash) fail("Stiff does nothing", 0, 1);
    if (run(0.3, 0.3, 0, 0.5, 21426140u, 200, false).hash == run(0.3, 0.3, 0, 1.0, 21426140u, 200, false).hash) fail("Sweep does nothing", 0, 1);
    if (failures) std::printf("%d failures\n", failures);
    else std::puts("ok");
    return failures ? 1 : 0;
}
