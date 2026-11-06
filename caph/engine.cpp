// Caph: the analog pair. Two oscillators a few cents apart, each a triangle → pulse → saw morph (Shape)
// with polyBLEP on every step edge; Width sets the pulse's duty and the triangle's apex; Sweep is the
// family's DCW envelope, here on Shape, so a bright strike falls back toward the triangle.
#include "core.h"

namespace caph {

// 1 + cents·ln2/1200 in Q28 from Q8 cents; first order, 0.07 cent error at 16 cents.
inline u32 cents_ratio_q28(i32 cents_q8) { return static_cast<u32>((1 << 28) + ((cents_q8 * 155054) >> 8)); }
// Reciprocal for polyBLEP: ((p >> 8) * r) >> 16 = p / inc in Q15.
inline u32 blep_reciprocal(u32 inc) { return inc >> 8 ? 0x7fffffffu / (inc >> 8) : 0; }

// The polyBLEP residual for a step at phase 0 (Q15, to add to a rising saw): nonzero within one
// increment either side of the wrap.
inline i32 blep(u32 p, u32 inc, u32 r) {
    if (p < inc) {
        const i32 one = 32768 - static_cast<i32>(((p >> 8) * r) >> 16);
        return (one * one) >> 15;
    }
    if (0u - p < inc) {
        const i32 one = 32768 - static_cast<i32>((((0u - p) >> 8) * r) >> 16);
        return -((one * one) >> 15);
    }
    return 0;
}

// One oscillator's sample: band-limited saw and pulse, a plain triangle (its corners fall off as 1/n²),
// crossfaded by Shape. The pulse is the saw minus a copy delayed by the duty, re-centred to ±1 at any duty.
struct Shape { i32 k; bool upper; u32 d16, d32, rise, fall; };
inline i32 one(u32 p, u32 inc, u32 r, const Shape& s) {
    const i32 saw = static_cast<i32>(p >> 16) - 32768 + blep(p, inc, r);
    const u32 q = p - s.d32;
    const i32 lag = static_cast<i32>(q >> 16) - 32768 + blep(q, inc, r);
    const i32 pulse = saw - lag - static_cast<i32>(s.d16) + 32768;
    if (s.upper) return (pulse * (32767 - s.k) + saw * s.k) >> 15;
    const u32 x = p >> 16;
    const i32 tri = x < s.d16 ? static_cast<i32>(cz::scale16(x, static_cast<i32>(s.rise))) - 32768
                              : 32767 - static_cast<i32>(cz::scale16(x - s.d16, static_cast<i32>(s.fall)));
    return (tri * (32767 - s.k) + pulse * s.k) >> 15;
}

__attribute__((noinline)) void pair_pass(Voice& v, opxy::Output out, u32 ia, u32 ib, const Shape& s) {
    const u32 ra = blep_reciprocal(ia), rb = blep_reciprocal(ib);
    u32 pa = v.pa, pb = v.pb;
    i32 x1 = v.dc_x, y1 = v.dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 mix = (one(pa, ia, ra, s) + one(pb, ib, rb, s)) >> 1;
        out[i] = cz::dc_out(mix, x1, y1);
        pa += ia;
        pb += ib;
    }
    v.pa = pa;
    v.pb = pb;
    v.dc_x = x1;
    v.dc_y = y1;
}

inline void start(Voice& v, Shared&, u32) {
    v.pa = 0;
    v.pb = 0x40000000u;  // a quarter-cycle apart, so the pair doesn't start as one doubled wave
    v.env = opxy::q31::maximum;
}

inline void initialize(Voice& v, Shared& shared, u32 owner) {
    v.primed = 0;
    v.dc_x = v.dc_y = 0;
    start(v, shared, owner);
}

inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& v, Shared&, u32, opxy::Output output, opxy::Parameters parameters,
                   opxy::Pitch pitch) {
    cz::glide(v.smooth, parameters, control_count, v.primed);
    const i32 shape = cz::swept(v.smooth[c_shape] >> 16, v.smooth[c_sweep], v.env);
    v.env = opxy::q31::multiply(v.env, cz::sweep_decay);

    // Detune: 0..35 cents between the pair, split either side of the note.
    const i32 half_cents_q8 = (v.smooth[c_detune] >> 16) * 35 * 128 >> 15;
    const u32 root = pitch[0];
    const u32 ia = cz::mul_u32_q28(root, cents_ratio_q28(-half_cents_q8));
    const u32 ib = cz::mul_u32_q28(root, cents_ratio_q28(half_cents_q8));

    Shape s;
    s.upper = shape >= 16384;
    s.k = s.upper ? (shape - 16384) * 2 : shape * 2;
    if (s.k > 32767) s.k = 32767;
    s.d16 = duty16(v.smooth[c_width] >> 16);
    s.d32 = s.d16 << 16;
    s.rise = static_cast<u32>(cz::slope25(65535u, s.d16));
    s.fall = static_cast<u32>(cz::slope25(65535u, 65536u - s.d16));
    pair_pass(v, output, ia, ib, s);
}
}
