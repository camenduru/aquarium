#pragma once
// ================================================================
// SEA CREATURES - CPython-compatible random.Random
// ----------------------------------------------------------------
// fish-gen draws every gene from Python's `random.Random`, so a seed only
// means the same animal in C++ if the *stream* is the same stream. splitmix64
// was not, which is why the port's clownfish came out a different fish with a
// different hue and a different number of stripes.
//
// This reproduces CPython's Mersenne Twister exactly:
//   * integer seeds  -> absolute value, little-endian 32-bit words, init_by_array
//   * string seeds   -> int.from_bytes(s + sha512(s)) then the same path
//   * random()       -> genrand_res53, two 32-bit draws
//   * choice()       -> _randbelow (rejection sampling, not a modulo)
//   * choices(w)     -> bisect on cumulative weights against random()*total
//
// Engine-free: standard headers only.
// ================================================================

#include <cstdint>
#include <string>

namespace sea {

class SeaRandom {
public:
    /// Python's `random.Random(7)`.
    explicit SeaRandom(std::uint64_t seed);
    /// Python's `random.Random("individual:Clownfish:7")`.
    explicit SeaRandom(const std::string& seed);

    /// `random()`: a double in [0, 1).
    double Random();

    /// Alias so a template written for `Rng` also accepts this stream.
    double NextDouble() { return Random(); }

    /// `uniform(a, b)`: a + (b - a) * random().
    double Uniform(double a, double b);

    /// One raw MT19937 output word, as `getrandbits(32)` would give.
    std::uint32_t Gen();

    /// `getrandbits(k)`, k in 1..64.
    std::uint64_t GetRandBits(int k);

    /// `_randbelow(n)`: uniform in [0, n), by rejection so there is no modulo bias.
    std::uint32_t RandBelow(std::uint64_t n);

    /// `choice(range(n))`.
    int Choice(int n);

    /// `randint(lo, hi)`: inclusive, like Python.
    int Range(int lo, int hi);

    /// `choices(weights)[0]`: picks an index in [0, n) with the given weights.
    int Choices(const double* weights, int n);

    /// Convenience for the `r.random() < p` tests all over genome generation.
    bool Chance(double p) { return Random() < p; }

private:
    void SeedFromWords(const std::uint32_t* key, int length);
    void SeedFromBytes(const std::uint8_t* bytes, std::size_t length);
    std::uint32_t Next();

    std::uint32_t mt_[624];
    int mti_ = 625;
};

} // namespace sea
