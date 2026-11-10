// ================================================================
// SEA CREATURES - CPython-compatible random.Random
// See creature/sea/SeaRandom.h for why this exists.
// ================================================================
#include "creature/sea/SeaRandom.h"

#include <cstring>
#include <vector>

namespace sea {
namespace {

inline std::uint32_t Rotl32(std::uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

// ---------------------------------------------------------------- SHA-512
// Only used to turn a string seed into an integer the way Python's
// `int.from_bytes(s + sha512(s))` does. Straight FIPS 180-4.

inline std::uint64_t Rotr64(std::uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }

const std::uint64_t kSha512K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

void Sha512Block(const std::uint8_t* p, std::uint64_t h[8]) {
    std::uint64_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = 0;
        for (int b = 0; b < 8; b++) w[i] = (w[i] << 8) | p[i * 8 + b];
    }
    for (int i = 16; i < 80; i++) {
        const std::uint64_t s0 = Rotr64(w[i - 15], 1) ^ Rotr64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        const std::uint64_t s1 = Rotr64(w[i - 2], 19) ^ Rotr64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint64_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint64_t e = h[4], f = h[5], g = h[6], k = h[7];
    for (int i = 0; i < 80; i++) {
        const std::uint64_t S1 = Rotr64(e, 14) ^ Rotr64(e, 18) ^ Rotr64(e, 41);
        const std::uint64_t ch = (e & f) ^ (~e & g);
        const std::uint64_t t1 = k + S1 + ch + kSha512K[i] + w[i];
        const std::uint64_t S0 = Rotr64(a, 28) ^ Rotr64(a, 34) ^ Rotr64(a, 39);
        const std::uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint64_t t2 = S0 + maj;
        k = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

void Sha512(const std::uint8_t* data, std::size_t len, std::uint8_t out[64]) {
    std::uint64_t h[8] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL,
                          0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
                          0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};
    const std::size_t full = len / 128 * 128;
    for (std::size_t off = 0; off < full; off += 128) Sha512Block(data + off, h);

    std::uint8_t tail[256];
    std::memset(tail, 0, sizeof(tail));
    const std::size_t rem = len - full;
    if (rem > 0) std::memcpy(tail, data + full, rem);
    tail[rem] = 0x80;
    const std::size_t tailLen = (rem + 1 + 16 <= 128) ? 128 : 256;
    const std::uint64_t bits = (std::uint64_t)len * 8;
    for (int i = 0; i < 8; i++) tail[tailLen - 1 - i] = (std::uint8_t)(bits >> (8 * i));
    for (std::size_t off = 0; off < tailLen; off += 128) Sha512Block(tail + off, h);

    for (int i = 0; i < 8; i++) {
        for (int b = 0; b < 8; b++) out[i * 8 + b] = (std::uint8_t)(h[i] >> (56 - 8 * b));
    }
}

} // namespace

// ---------------------------------------------------------------- seeding

void SeaRandom::SeedFromWords(const std::uint32_t* key, int length) {
    const int N = 624;
    mt_[0] = 19650218U;
    for (int i = 1; i < N; i++) {
        mt_[i] = 1812433253U * (mt_[i - 1] ^ (mt_[i - 1] >> 30)) + (std::uint32_t)i;
    }
    int i = 1, j = 0;
    for (int k = (N > length ? N : length); k; k--) {
        mt_[i] = (mt_[i] ^ ((mt_[i - 1] ^ (mt_[i - 1] >> 30)) * 1664525U)) + key[j] + (std::uint32_t)j;
        i++; j++;
        if (i >= N) { mt_[0] = mt_[N - 1]; i = 1; }
        if (j >= length) j = 0;
    }
    for (int k = N - 1; k; k--) {
        mt_[i] = (mt_[i] ^ ((mt_[i - 1] ^ (mt_[i - 1] >> 30)) * 1566083941U)) - (std::uint32_t)i;
        i++;
        if (i >= N) { mt_[0] = mt_[N - 1]; i = 1; }
    }
    mt_[0] = 0x80000000U;
    mti_ = N;
}

void SeaRandom::SeedFromBytes(const std::uint8_t* bytes, std::size_t length) {
    // Python: int.from_bytes(bytes, "big"), then CPython turns that integer into
    // ceil(bit_length/32) little-endian 32-bit words.
    std::size_t start = 0;
    while (start < length && bytes[start] == 0) start++;
    if (start == length) {                       // the value is zero
        const std::uint32_t zero = 0;
        SeedFromWords(&zero, 1);
        return;
    }
    const std::size_t significant = length - start;
    int topBits = 0;
    for (int b = 7; b >= 0; b--) {
        if (bytes[start] & (std::uint8_t)(1u << b)) { topBits = b + 1; break; }
    }
    const std::uint64_t bits = (std::uint64_t)(significant - 1) * 8 + (std::uint64_t)topBits;
    const int keymax = (int)((bits + 31) / 32);

    std::vector<std::uint32_t> key((std::size_t)keymax, 0);
    for (std::size_t i = 0; i < (std::size_t)keymax * 4 && i < length; i++) {
        const std::uint8_t byte = bytes[length - 1 - i];   // least significant byte first
        key[i / 4] |= (std::uint32_t)byte << (8 * (i % 4));
    }
    SeedFromWords(key.data(), keymax);
}

SeaRandom::SeaRandom(std::uint64_t seed) {
    std::uint8_t bytes[8];
    for (int i = 0; i < 8; i++) bytes[i] = (std::uint8_t)(seed >> (56 - 8 * i));
    SeedFromBytes(bytes, 8);
}

SeaRandom::SeaRandom(const std::string& seed) {
    // Python: a = a.encode(); a = int.from_bytes(a + sha512(a).digest())
    std::vector<std::uint8_t> buf(seed.begin(), seed.end());
    std::uint8_t digest[64];
    Sha512(buf.data(), buf.size(), digest);
    buf.insert(buf.end(), digest, digest + 64);
    SeedFromBytes(buf.data(), buf.size());
}

// ---------------------------------------------------------------- draws

std::uint32_t SeaRandom::Next() {
    const int N = 624, M = 397;
    static const std::uint32_t kMatrixA = 0x9908b0dfU;
    if (mti_ >= N) {
        int kk = 0;
        for (; kk < N - M; kk++) {
            const std::uint32_t y = (mt_[kk] & 0x80000000U) | (mt_[kk + 1] & 0x7fffffffU);
            mt_[kk] = mt_[kk + M] ^ (y >> 1) ^ ((y & 1U) ? kMatrixA : 0U);
        }
        for (; kk < N - 1; kk++) {
            const std::uint32_t y = (mt_[kk] & 0x80000000U) | (mt_[kk + 1] & 0x7fffffffU);
            mt_[kk] = mt_[kk + (M - N)] ^ (y >> 1) ^ ((y & 1U) ? kMatrixA : 0U);
        }
        const std::uint32_t y = (mt_[N - 1] & 0x80000000U) | (mt_[0] & 0x7fffffffU);
        mt_[N - 1] = mt_[M - 1] ^ (y >> 1) ^ ((y & 1U) ? kMatrixA : 0U);
        mti_ = 0;
    }
    std::uint32_t y = mt_[mti_++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680U;
    y ^= (y << 15) & 0xefc60000U;
    y ^= (y >> 18);
    return y;
}

std::uint32_t SeaRandom::Gen() { return Next(); }

double SeaRandom::Random() {
    const std::uint32_t a = Next() >> 5;
    const std::uint32_t b = Next() >> 6;
    return ((double)a * 67108864.0 + (double)b) * (1.0 / 9007199254740992.0);
}

double SeaRandom::Uniform(double a, double b) { return a + (b - a) * Random(); }

std::uint64_t SeaRandom::GetRandBits(int k) {
    if (k <= 0) return 0;
    if (k <= 32) return (std::uint64_t)(Next() >> (32 - k));
    std::uint64_t value = 0;
    int shift = 0, left = k;
    while (left > 0) {
        std::uint32_t r = Next();
        if (left < 32) r >>= (32 - left);
        value |= (std::uint64_t)r << shift;
        shift += 32;
        left -= 32;
    }
    return value;
}

std::uint32_t SeaRandom::RandBelow(std::uint64_t n) {
    if (n == 0) return 0;
    int k = 0;
    for (std::uint64_t t = n; t; t >>= 1) k++;
    std::uint64_t r = GetRandBits(k);
    while (r >= n) r = GetRandBits(k);
    return (std::uint32_t)r;
}

int SeaRandom::Choice(int n) { return n <= 0 ? 0 : (int)RandBelow((std::uint64_t)n); }

int SeaRandom::Range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)RandBelow((std::uint64_t)(hi - lo + 1));
}

int SeaRandom::Choices(const double* weights, int n) {
    if (n <= 0) return 0;
    std::vector<double> cum((std::size_t)n);
    double total = 0.0;
    for (int i = 0; i < n; i++) { total += weights[i]; cum[(std::size_t)i] = total; }
    const double x = Random() * total;
    int lo = 0, hi = n - 1;                      // bisect_right over cum[0:hi]
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (x < cum[(std::size_t)mid]) hi = mid; else lo = mid + 1;
    }
    return lo;
}

} // namespace sea
