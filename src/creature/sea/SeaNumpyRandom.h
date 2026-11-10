#pragma once
// ================================================================
// SEA CREATURES - numpy.random.default_rng() compatible stream
// ----------------------------------------------------------------
// fish-gen's geometry uses numpy's default_rng(seed) for the per-arm
// length/phi jitter and for the pufferfish spike jitter:
//
//     rng = np.random.default_rng(g.seed * 31 + k)
//     length = arms.length * rng.uniform(0.9, 1.1)
//     phi    = rng.uniform(0, 2*pi)
//
// That is PCG64 seeded through SeedSequence, not Python's random.Random.
// Reproducing it exactly fixes the arm extent (and therefore the shared
// camera fit) for octopus and the spike extent for pufferfish.
//
// Ported from numpy:
//   numpy/random/bit_generator.pyx  (SeedSequence)
//   numpy/random/src/pcg64/pcg64.h  (PCG64 XSL RR 128/64)
// Engine-free: standard headers only; no __int128 (MSVC).
// ================================================================

#include <cmath>
#include <cstdint>

#include "creature/sea/SeaZiggurat.h"

namespace sea {

class NumpyRandom {
public:
    explicit NumpyRandom(std::uint64_t seed) { Seed(seed); }

    /// numpy's Generator.random(): a double in [0, 1).
    double Random() {
        return (double)(Next64() >> 11) * (1.0 / 9007199254740992.0);
    }
    double Uniform(double lo, double hi) { return lo + (hi - lo) * Random(); }

    /// numpy's Generator.normal(): the 256-entry 128/16 ziggurat.
    double Normal() {
        for (;;) {
            std::uint64_t r = Next64();
            const int idx = (int)(r & 0xffu);
            r >>= 8;
            const int sign = (int)(r & 1u);
            const std::uint64_t rabs = (r >> 1) & 0x000fffffffffffffULL;
            double x = (double)rabs * ziggurat::wi[idx];
            if (sign) x = -x;
            if (rabs < ziggurat::ki[idx]) return x;
            if (idx == 0) {
                for (;;) {
                    const double xx = -ziggurat::nor_inv_r * std::log1p(-Random());
                    const double yy = -std::log1p(-Random());
                    if (yy + yy > xx * xx)
                        return ((rabs >> 8) & 1u) ? -(ziggurat::nor_r + xx) : (ziggurat::nor_r + xx);
                }
            } else if (ziggurat::fi[idx - 1] +
                           (ziggurat::fi[idx] - ziggurat::fi[idx - 1]) * Random() <
                       std::exp(-0.5 * x * x)) {
                return x;
            }
        }
    }

    /// Raw PCG64 output (also handy for tests).
    std::uint64_t Next64() {
        Step();
        const std::uint64_t rot = (hi_ >> 58) & 63u;
        const std::uint64_t v = hi_ ^ lo_;
        return (v >> rot) | (v << ((64u - rot) & 63u));
    }

private:
    static constexpr std::uint32_t kInitA = 0x43b0d7e5u;
    static constexpr std::uint32_t kMultA = 0x931e8875u;
    static constexpr std::uint32_t kInitB = 0x8b51f9ddu;
    static constexpr std::uint32_t kMultB = 0x58f38dedu;
    static constexpr std::uint32_t kMixL = 0xca01f9ddu;
    static constexpr std::uint32_t kMixR = 0x4973f715u;
    static constexpr std::uint64_t kMultHi = 2549297995355413924ULL;
    static constexpr std::uint64_t kMultLo = 4865540595714422341ULL;

    static std::uint32_t HashMix(std::uint32_t value, std::uint32_t& hashConst) {
        value ^= hashConst;
        hashConst *= kMultA;
        value *= hashConst;
        value ^= value >> 16;
        return value;
    }
    static std::uint32_t Mix(std::uint32_t x, std::uint32_t y) {
        std::uint32_t r = kMixL * x - kMixR * y;
        r ^= r >> 16;
        return r;
    }

    static void Mul64(std::uint64_t a, std::uint64_t b, std::uint64_t& hi,
                      std::uint64_t& lo) {
        const std::uint64_t a0 = a & 0xffffffffULL, a1 = a >> 32;
        const std::uint64_t b0 = b & 0xffffffffULL, b1 = b >> 32;
        const std::uint64_t p00 = a0 * b0, p01 = a0 * b1;
        const std::uint64_t p10 = a1 * b0, p11 = a1 * b1;
        const std::uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffULL) +
                                  (p10 & 0xffffffffULL);
        lo = (p00 & 0xffffffffULL) | (mid << 32);
        hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    }

    static void Mul128(std::uint64_t ah, std::uint64_t al, std::uint64_t bh,
                       std::uint64_t bl, std::uint64_t& rh, std::uint64_t& rl) {
        std::uint64_t p0h, p0l, p1h, p1l, p2h, p2l;
        Mul64(al, bl, p0h, p0l);
        Mul64(ah, bl, p1h, p1l);
        Mul64(al, bh, p2h, p2l);
        rl = p0l;
        rh = p0h + p1l + p2l;   // ah*bh * 2^128 drops
    }

    void Step() {
        // state = state * MULT + inc (128-bit, modulo 2^128)
        std::uint64_t nh = 0, nl = 0;
        Mul128(hi_, lo_, kMultHi, kMultLo, nh, nl);
        const std::uint64_t oldLo = nl;
        nl += incLo_;
        nh += incHi_ + (nl < oldLo ? 1u : 0u);
        hi_ = nh;
        lo_ = nl;
    }

    void Seed(std::uint64_t seed) {
        // SeedSequence: entropy words, then mix_entropy.
        std::uint32_t words[2] = {(std::uint32_t)seed, (std::uint32_t)(seed >> 32)};
        const int wordCount = (seed >> 32) != 0 ? 2 : 1;
        std::uint32_t mixer[4];
        std::uint32_t hashConst = kInitA;
        for (int i = 0; i < 4; i++)
            mixer[i] = HashMix(i < wordCount ? words[i] : 0u, hashConst);
        for (int src = 0; src < 4; src++) {
            for (int dst = 0; dst < 4; dst++) {
                if (src != dst) mixer[dst] = Mix(mixer[dst], HashMix(mixer[src], hashConst));
            }
        }

        // generate_state(4, uint64): eight little-endian uint32 -> four uint64.
        std::uint32_t state32[8];
        std::uint32_t hc = kInitB;
        for (int i = 0; i < 8; i++) {
            std::uint32_t d = mixer[i & 3];
            d ^= hc;
            hc *= kMultB;
            d *= hc;
            d ^= d >> 16;
            state32[i] = d;
        }
        std::uint64_t v[4];
        for (int i = 0; i < 4; i++)
            v[i] = (std::uint64_t)state32[i * 2] | ((std::uint64_t)state32[i * 2 + 1] << 32);

        // PCG64: inc = (initseq << 1) | 1; state = 0; step; state += initstate; step.
        incHi_ = (v[2] << 1) | (v[3] >> 63);
        incLo_ = (v[3] << 1) | 1u;
        hi_ = 0;
        lo_ = 0;
        Step();
        const std::uint64_t oldLo = lo_;
        lo_ += v[1];
        hi_ += v[0] + (lo_ < oldLo ? 1u : 0u);
        Step();
    }

    std::uint64_t hi_ = 0, lo_ = 0;       // PCG64 128-bit state
    std::uint64_t incHi_ = 0, incLo_ = 0; // odd PCG64 increment
};

} // namespace sea
