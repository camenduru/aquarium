#pragma once
// ================================================================
// SEA CREATURES - math, deterministic RNG and colour helpers
// ----------------------------------------------------------------
// Ported from C:/Users/PC/Documents/content/fish-gen (MIT).
// Engine-free: no WickedEngine, no STL <filesystem>, no exceptions,
// so the standalone harness builds the same code the game does.
//
// The Python original used `random.Random`. SeaRandom reproduces CPython's
// Mersenne Twister bit for bit, so a seed names the same animal in both
// generators for every species and kind. Rng (splitmix64) remains only for
// engine-side scatter (scars, spine placement) that is not part of the
// Python genome stream.
// ================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "creature/sea/SeaRandom.h"

namespace sea {

using u64 = std::uint64_t;
using u32 = std::uint32_t;

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kTwoPi = 6.28318530717958647692;

inline double Clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
inline double Saturate(double v) { return Clamp(v, 0.0, 1.0); }
inline double Lerp(double a, double b, double t) { return a + (b - a) * t; }
inline double Smoothstep(double e0, double e1, double x) {
    const double t = Saturate((x - e0) / (e1 - e0));
    return t * t * (3.0 - 2.0 * t);
}
/// 1 at 0, easing to 0 at |x| >= 1 (the reference's smooth_bump).
inline double SmoothBump(double x) {
    x = Saturate(std::fabs(x));
    return 1.0 - x * x * (3.0 - 2.0 * x);
}
inline int ClampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double Max(double a, double b) { return a > b ? a : b; }
inline double Min(double a, double b) { return a < b ? a : b; }

// ---------------------------------------------------------------- vectors
struct Vec2 {
    double x = 0, y = 0;
    Vec2() = default;
    Vec2(double X, double Y) : x(X), y(Y) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double s) const { return {x * s, y * s}; }
    double Length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double X, double Y, double Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    double Length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 NormalizedOr(const Vec3& fallback = Vec3(0, 0, 1)) const {
        const double n = Length();
        return n > 1e-12 ? Vec3(x / n, y / n, z / n) : fallback;
    }
    double operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    double& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
};

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Lerp(const Vec3& a, const Vec3& b, double t) { return a + (b - a) * t; }

struct Vec4 {
    double x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(double X, double Y, double Z, double W) : x(X), y(Y), z(Z), w(W) {}
    double operator[](int i) const { return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w)); }
};

/// World -> camera: tilt so we look slightly down on the animal.
inline Vec3 Camera(const Vec3& p, double pitch) {
    const double c = std::cos(pitch), s = std::sin(pitch);
    return {p.x, p.y * c - p.z * s, p.z * c + p.y * s};
}

// ---------------------------------------------------------------- hashing / RNG
inline u64 HashCombine(u64 a, u64 b) {
    u64 seed = a ^ (b + 0x9E3779B97F4A7C15ULL + (a << 6) + (a >> 2));
    seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ULL;
    seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBULL;
    return seed ^ (seed >> 31);
}

inline u64 HashString(const std::string& s, u64 seed = 0) {
    u64 h = seed ^ 0xCBF29CE484222325ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001B3ULL;
    }
    return h;
}

/// Deterministic splitmix64 stream. Same seed -> same stream, always.
struct Rng {
    u64 state = 0;
    explicit Rng(u64 seed = 1) : state(seed ? seed : 0x9E3779B97F4A7C15ULL) {}

    u64 Next() {
        state += 0x9E3779B97F4A7C15ULL;
        u64 z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    /// Uniform in [0, 1).
    double NextDouble() { return (double)(Next() >> 11) * (1.0 / 9007199254740992.0); }
    double Uniform(double lo, double hi) { return lo + (hi - lo) * NextDouble(); }
    /// Inclusive integer range.
    int Range(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + (int)(Next() % (u64)(hi - lo + 1));
    }
    bool Chance(double p) { return NextDouble() < p; }
};

/// A named sub-stream: HashString(tag) folded with the seed.
inline Rng RngFor(const std::string& tag, u64 seed) {
    return Rng(HashCombine(HashString(tag), seed));
}

/// Triangular draw over [low, high] peaking at `typical` (the size database).
inline double Triangular(Rng& r, double low, double high, double typical) {
    if (high <= low) return typical;
    const double c = Saturate((typical - low) / (high - low));
    const double u = r.NextDouble();
    if (u < c) return low + std::sqrt(u * (high - low) * (typical - low));
    return high - std::sqrt((1.0 - u) * (high - low) * (high - typical));
}

/// Python's `random.triangular(low, high, mode)`: one draw, and the same
/// reflect-and-swap when the sample lands past the mode.
inline double Triangular(SeaRandom& r, double low, double high, double typical) {
    if (high <= low) return typical;
    const double c = Saturate((typical - low) / (high - low));
    const double u = r.Random();
    if (u < c) return low + std::sqrt(u * (high - low) * (typical - low));
    return high - std::sqrt((1.0 - u) * (high - low) * (high - typical));
}

/// Picks an index from a weight table; returns 0 for an empty table.
/// Templated so the single draw can come from either Rng or SeaRandom.
template <typename R>
inline int WeightedPick(R& r, const double* weights, int count) {
    double total = 0;
    for (int i = 0; i < count; i++) total += weights[i] > 0 ? weights[i] : 0;
    if (total <= 0) return 0;
    double u = r.NextDouble() * total;
    for (int i = 0; i < count; i++) {
        u -= weights[i] > 0 ? weights[i] : 0;
        if (u < 0) return i;
    }
    return count - 1;
}

// ---------------------------------------------------------------- noise
inline double Hash01(int i, int j, double seed) {
    const double h = std::sin(i * 127.1 + j * 311.7 + seed * 74.7) * 43758.5453;
    return h - std::floor(h);
}

/// Smooth value noise in -1..1.
inline double ValueNoise(double x, double y, double seed) {
    const double xi = std::floor(x), yi = std::floor(y);
    const double xf = x - xi, yf = y - yi;
    const double u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    const int ix = (int)xi, iy = (int)yi;
    const double a = Hash01(ix, iy, seed);
    const double b = Hash01(ix + 1, iy, seed);
    const double c = Hash01(ix, iy + 1, seed);
    const double d = Hash01(ix + 1, iy + 1, seed);
    return 2.0 * (a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v) - 1.0;
}

inline double Fbm(double x, double y, double seed, int octaves = 4) {
    double total = 0, amp = 1, norm = 0;
    for (int k = 0; k < octaves; k++) {
        total += amp * ValueNoise(x, y, seed + 13.0 * k);
        norm += amp;
        x = x * 2.03 + 1.7;
        y = y * 2.03 - 0.9;
        amp *= 0.5;
    }
    return norm > 0 ? total / norm : 0.0;
}

// ---------------------------------------------------------------- colour
struct Color8 {
    unsigned char r = 0, g = 0, b = 0;
    Color8() = default;
    Color8(unsigned char R, unsigned char G, unsigned char B) : r(R), g(G), b(B) {}
};

inline Color8 HsvToRgb(double h, double s, double v) {
    h = h - std::floor(h);
    s = Saturate(s);
    v = Saturate(v);
    const double hp = h * 6.0;
    const int i = ClampI((int)std::floor(hp), 0, 5);
    const double f = hp - i;
    const double p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    double r = v, g = t, b = p;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return Color8((unsigned char)(Saturate(r) * 255.0 + 0.5),
                  (unsigned char)(Saturate(g) * 255.0 + 0.5),
                  (unsigned char)(Saturate(b) * 255.0 + 0.5));
}

inline Color8 RgbToHsvHelperless(double r, double g, double b) {
    // Kept as bytes after the conversion below; this struct is only a carrier.
    return Color8((unsigned char)(Saturate(r) * 255.0 + 0.5),
                  (unsigned char)(Saturate(g) * 255.0 + 0.5),
                  (unsigned char)(Saturate(b) * 255.0 + 0.5));
}

inline std::string HexFromHsv(double h, double s, double v) {
    const Color8 c = HsvToRgb(h, s, v);
    static const char* kHex = "0123456789abcdef";
    std::string out = "#000000";
    const unsigned char rgb[3] = {c.r, c.g, c.b};
    for (int i = 0; i < 3; i++) {
        out[1 + i * 2] = kHex[rgb[i] >> 4];
        out[2 + i * 2] = kHex[rgb[i] & 15];
    }
    return out;
}

/// "#rrggbb" -> bytes. Returns false when the string is not a colour.
inline bool ParseHexColor(const std::string& text, Color8& out) {
    std::string s;
    for (char c : text) {
        if (c == '#' || c == ' ') continue;
        s.push_back(c);
    }
    if (s.size() != 6) return false;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    int v[6];
    for (int i = 0; i < 6; i++) {
        v[i] = nib(s[i]);
        if (v[i] < 0) return false;
    }
    out = Color8((unsigned char)(v[0] * 16 + v[1]), (unsigned char)(v[2] * 16 + v[3]),
                 (unsigned char)(v[4] * 16 + v[5]));
    return true;
}

/// RGB -> HSV (h in 0..1, s/v in 0..1); used to re-derive fins from an exact colour.
inline void RgbToHsv(const Color8& c, double& h, double& s, double& v) {
    const double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    const double mx = Max(Max(r, g), b), mn = Min(Min(r, g), b);
    v = mx;
    s = mx > 1e-9 ? (mx - mn) / mx : 0.0;
    if (mx - mn < 1e-9) {
        h = 0.0;
    } else if (mx == r) {
        h = (g - b) / (mx - mn) / 6.0;
    } else if (mx == g) {
        h = (2.0 + (b - r) / (mx - mn)) / 6.0;
    } else {
        h = (4.0 + (r - g) / (mx - mn)) / 6.0;
    }
    if (h < 0) h += 1.0;
}

// ---------------------------------------------------------------- utility
/// Resample a 2D polyline to n points evenly spaced by arc length.
inline std::vector<Vec2> Resample(const std::vector<Vec2>& pts, int n) {
    std::vector<Vec2> out;
    if (pts.empty() || n <= 0) return out;
    std::vector<double> cum(pts.size(), 0.0);
    for (size_t i = 1; i < pts.size(); i++) {
        cum[i] = cum[i - 1] + (pts[i] - pts[i - 1]).Length();
    }
    const double total = cum.back();
    if (total <= 1e-9) {
        out.assign((size_t)n, pts[0]);
        return out;
    }
    for (int k = 0; k < n; k++) {
        const double target = total * k / (double)(n - 1 > 0 ? n - 1 : 1);
        size_t i = 1;
        while (i + 1 < pts.size() && cum[i] < target) i++;
        const double seg = cum[i] - cum[i - 1];
        const double t = seg > 1e-12 ? (target - cum[i - 1]) / seg : 0.0;
        out.push_back(pts[i - 1] + (pts[i] - pts[i - 1]) * Saturate(t));
    }
    return out;
}

/// Quadratic Bezier from p0 to p1 bent toward c.
inline std::vector<Vec2> Bezier(Vec2 p0, Vec2 c, Vec2 p1, int n) {
    std::vector<Vec2> out;
    for (int i = 0; i < n; i++) {
        const double t = n > 1 ? (double)i / (double)(n - 1) : 0.0;
        const double u = 1.0 - t;
        out.push_back(p0 * (u * u) + c * (2.0 * u * t) + p1 * (t * t));
    }
    return out;
}

} // namespace sea
