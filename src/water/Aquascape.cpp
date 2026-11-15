// ================================================================
// AQUARIUM - procedural aquascape implementation
// ================================================================

#include "water/Aquascape.h"

#include "water/WaterField.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace water {
namespace {

struct Colour
{
    uint8_t r, g, b, a;
};

// One palette for the whole plate: every written pixel is one of these,
// so the water tint never reveals a gradient band inside the art.
constexpr Colour kSandLight { 208, 186, 138, 120 };
constexpr Colour kSandMid   { 186, 164, 118, 118 };
constexpr Colour kSandShade { 156, 134,  94, 116 };
constexpr Colour kSandEdge  { 214, 194, 150, 135 };

constexpr Colour kRockLight { 132, 138, 144, 100 };
constexpr Colour kRockMid   { 102, 108, 116,  98 };
constexpr Colour kRockDark  {  74,  80,  88,  96 };
constexpr Colour kRockEdge  {  46,  50,  56, 130 };

struct Rock
{
    float cx = 0.0f, cy = 0.0f, rx = 0.0f, ry = 0.0f;
};

uint32_t Hash(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t h = x * 374761393u + y * 668265263u + seed * 2654435761u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void Put(uint8_t* rgba, int w, int x, int y, Colour c)
{
    uint8_t* d = rgba + (size_t(y) * size_t(w) + size_t(x)) * 4;
    d[0] = c.r;
    d[1] = c.g;
    d[2] = c.b;
    d[3] = c.a;
}

/// Ellipse test with a per-pixel ragged edge, so the silhouette reads as
/// rock rather than geometry.
float RockRadius(const Rock& rock, float x, float y)
{
    const float dx = (x - rock.cx) / rock.rx;
    const float dy = (y - rock.cy) / rock.ry;
    return std::sqrt(dx * dx + dy * dy);
}

Colour RockColour(const Rock& rock, float x, float y, float r, uint32_t seed)
{
    if (r > 0.87f) return kRockEdge;
    const float n = ((x - rock.cx) / rock.rx) * 0.6f + ((y - rock.cy) / rock.ry) * 0.9f;
    if (n < -0.28f) return kRockLight;
    if (n > 0.30f)  return kRockDark;
    // A little grain keeps the mid tone from looking like flat vector art.
    return (Hash(uint32_t(x), uint32_t(y), seed) & 7u) == 0u ? kRockLight : kRockMid;
}

} // namespace

void Aquascape::Build(int logicalW, int logicalH, const AquascapeSpec& spec,
                      const WaterMasks& masks)
{
    w_ = std::max(logicalW, 8);
    h_ = std::max(logicalH, 8);
    plate_.assign(size_t(w_) * size_t(h_) * 4, 0);
    front_.assign(size_t(w_) * size_t(h_) * 4, 0);

    if (!masks.Valid()) return;

    // ---- sand ---------------------------------------------------
    for (int x = 0; x < w_; ++x)
    {
        const float horizon = masks.SandAt(x);
        const int   first   = std::max(0, int(std::ceil(horizon)));
        const float columnDepth = float(std::max(h_ - first, 1));

        for (int y = first; y < h_; ++y)
        {
            if (!masks.Inside(x, y)) continue;

            const float down = float(y - first) / columnDepth;
            const uint32_t h4 = Hash(uint32_t(x), uint32_t(y), spec.seed);

            Colour c;
            if (y <= first + 1)             c = kSandEdge;
            else if (down > 0.62f && (h4 & 3u) != 0u) c = kSandShade;
            else
            {
                const uint32_t pick = h4 & 31u;
                if (pick < 9u)       c = kSandLight;
                else if (pick < 25u) c = kSandMid;
                else                 c = kSandShade;
            }
            Put(plate_.data(), w_, x, y, c);
        }
    }

    // ---- rocks --------------------------------------------------
    std::mt19937 rng(spec.seed * 2654435761u + 17u);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    std::vector<Rock> rocks;
    rocks.reserve(size_t(std::max(spec.rockCount, 0)));
    for (int i = 0; i < spec.rockCount; ++i)
    {
        const float t = (float(i) + 0.5f) / float(std::max(spec.rockCount, 1));
        const float jitter = (u01(rng) - 0.5f) * (0.7f / float(std::max(spec.rockCount, 1)));

        Rock rock;
        rock.cx = (std::clamp(t + jitter, 0.06f, 0.94f)) * float(w_);
        rock.rx = (10.0f + u01(rng) * 17.0f) * spec.rockScale;
        rock.ry = (6.5f + u01(rng) * 10.0f) * spec.rockScale;
        // Sit the rock so its lower third is buried in the sand.
        rock.cy = masks.SandAt(int(rock.cx)) - rock.ry * (0.25f + u01(rng) * 0.35f);
        rocks.push_back(rock);
    }
    // Back to front, so nearer rocks overlap the ones behind them.
    std::sort(rocks.begin(), rocks.end(),
              [](const Rock& a, const Rock& b) { return a.cy < b.cy; });

    for (const Rock& rock : rocks)
    {
        const int x0 = std::max(0, int(rock.cx - rock.rx) - 1);
        const int x1 = std::min(w_ - 1, int(rock.cx + rock.rx) + 1);
        const int y0 = std::max(0, int(rock.cy - rock.ry) - 1);
        const int y1 = std::min(h_ - 1, int(rock.cy + rock.ry) + 1);

        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                if (!masks.Inside(x, y)) continue;
                const float r = RockRadius(rock, float(x), float(y));
                const float ragged = (float(int(Hash(uint32_t(x), uint32_t(y), spec.seed) % 21u) - 10) * 0.012f);
                if (r > 1.0f + ragged) continue;
                Put(plate_.data(), w_, x, y, RockColour(rock, float(x), float(y), r, spec.seed));
            }
        }
    }

    // ---- front rock ---------------------------------------------
    if (spec.frontRock && masks.Valid())
    {
        Rock rock;
        rock.cx = float(w_) * 0.84f;
        rock.rx = float(w_) * 0.15f;
        rock.ry = float(h_) * 0.105f;
        rock.cy = float(h_) * 0.965f;

        const int x0 = std::max(0, int(rock.cx - rock.rx) - 1);
        const int x1 = std::min(w_ - 1, int(rock.cx + rock.rx) + 1);
        const int y0 = std::max(0, int(rock.cy - rock.ry) - 1);
        const int y1 = std::min(h_ - 1, int(rock.cy + rock.ry) + 1);

        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                if (!masks.Inside(x, y)) continue;
                const float r = RockRadius(rock, float(x), float(y));
                const float ragged = float(int(Hash(uint32_t(x), uint32_t(y), spec.seed + 3u) % 21u) - 10) * 0.012f;
                if (r > 1.0f + ragged) continue;
                Colour c = RockColour(rock, float(x), float(y), r, spec.seed + 3u);
                c.a = uint8_t(c.a * 3 / 4);   // in front of the fish, but still ghosted
                Put(front_.data(), w_, x, y, c);
            }
        }
    }
}

void Aquascape::DrawBack(uint8_t* rgba) const
{
    if (rgba == nullptr || plate_.empty()) return;
    std::memcpy(rgba, plate_.data(), plate_.size());
}

void Aquascape::DrawFront(uint8_t* rgba) const
{
    if (rgba == nullptr || front_.empty()) return;

    // Straight-alpha src-over; the water composite expects un-premultiplied
    // content and premultiplies exactly once on the way out.
    for (size_t i = 0; i + 3 < front_.size(); i += 4)
    {
        const unsigned sa = front_[i + 3];
        if (sa == 0) continue;
        uint8_t* d = rgba + i;
        const unsigned da = d[3];
        const unsigned outA = sa + da * (255u - sa) / 255u;
        if (outA == 0) continue;

        for (int c = 0; c < 3; ++c)
        {
            const unsigned num = unsigned(front_[i + c]) * sa
                               + unsigned(d[c]) * da * (255u - sa) / 255u;
            d[c] = uint8_t((num + outA / 2u) / outA);
        }
        d[3] = uint8_t(outA);
    }
}

} // namespace water
