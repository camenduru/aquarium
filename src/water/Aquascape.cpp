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

struct Rock
{
    float cx = 0.0f, cy = 0.0f, rx = 0.0f, ry = 0.0f;
};

// ---- corals ----------------------------------------------------
// A coral is a small genome, not a drawing: kind, size, bend, colour and a
// detail seed, exactly like a fish. The kinds are rasterised into a scratch
// mask and then given one shared outline, so every species still reads as the
// same pixel-art scene.
enum class CoralKind { Branch, Fan, Brain, Kelp, Polyp };

struct CoralGenome
{
    CoralKind kind = CoralKind::Branch;
    float x = 0.0f;        // logical px of the stalk's foot
    float y = 0.0f;        // sand line under it
    float height = 20.0f;
    float width = 14.0f;
    float lean = 0.0f;     // -1..1 sideways drift
    float curl = 0.0f;     // -1..1 bend along a limb
    float branch = 0.5f;   // 0..1 how much it splits
    float hue = 0.0f;
    float hue2 = 0.0f;     // tips
    float sat = 0.7f;
    float val = 0.9f;
    float alpha = 0.5f;
    uint32_t seed = 0;
    int depth = 1;         // 0 far, 2 near: size and density
};

// Palette slots written by the shape functions: 1 dark, 2 mid, 3 light,
// 4 accent (tips). 0 means empty.
struct CoralPalette
{
    Colour tone[5] {};
    Colour edge {};
};

Colour Hsv(float h, float s, float v, uint8_t a)
{
    h -= std::floor(h);
    const float sector = h * 6.0f;
    const int   i = int(sector) % 6;
    const float f = sector - std::floor(sector);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);

    float r = v, g = p, b = q;
    switch (i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return Colour { uint8_t(r * 255.0f + 0.5f), uint8_t(g * 255.0f + 0.5f),
                    uint8_t(b * 255.0f + 0.5f), a };
}

CoralPalette MakeCoralPalette(const CoralGenome& g)
{
    CoralPalette p;
    const uint8_t a = uint8_t(std::clamp(g.alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    const uint8_t dense = uint8_t(std::min(255, int(a) + 28));

    // Four tones and one outline. The light tone stays under white so a coral
    // still has colour on the sand instead of bleaching out; the tip is the
    // complementary hue, which is what makes a polyp or a branch end read.
    p.tone[1] = Hsv(g.hue + 0.01f, g.sat, g.val * 0.58f, dense);      // dark
    p.tone[2] = Hsv(g.hue, g.sat * 0.88f, g.val * 0.82f, a);          // mid
    p.tone[3] = Hsv(g.hue - 0.02f, g.sat * 0.62f, std::min(1.0f, g.val * 1.06f), a); // light
    p.tone[4] = Hsv(g.hue2, g.sat * 0.9f, std::min(1.0f, g.val * 1.12f), a);         // tips
    p.edge    = Hsv(g.hue, g.sat * 0.95f, g.val * 0.34f, dense);
    return p;
}

/// A bounding-box mask in absolute logical coordinates.
struct Scratch
{
    int x0 = 0, y0 = 0, w = 0, h = 0;
    std::vector<uint8_t> tone;

    void Reset(int ax, int ay, int aw, int ah)
    {
        x0 = ax; y0 = ay;
        w = std::max(aw, 1); h = std::max(ah, 1);
        tone.assign(size_t(w) * size_t(h), 0);
    }

    void Set(int x, int y, uint8_t t)
    {
        const int lx = x - x0, ly = y - y0;
        if (lx < 0 || ly < 0 || lx >= w || ly >= h) return;
        tone[size_t(ly) * size_t(w) + size_t(lx)] = t;
    }

    bool Has(int x, int y) const
    {
        const int lx = x - x0, ly = y - y0;
        if (lx < 0 || ly < 0 || lx >= w || ly >= h) return false;
        return tone[size_t(ly) * size_t(w) + size_t(lx)] != 0;
    }
};

/// Speckle keeps a flat fill from looking like vector art.
uint8_t GrainCoral(uint32_t x, uint32_t y, uint32_t seed, uint8_t base)
{
    const uint32_t h = Hash(x, y, seed);
    if ((h & 15u) == 0u) return base == 1 ? 2 : 1;
    if ((h & 31u) == 1u) return 3;
    return base;
}

void DrawBranchCoral(Scratch& s, const CoralGenome& g)
{
    struct Limb { float x, y, angle, len, width; int level; };

    std::vector<Limb> limbs;
    limbs.push_back({ g.x, g.y, -1.5708f + g.lean * 0.45f, g.height * 0.5f, 1.35f, 0 });

    int guard = 0;
    while (!limbs.empty() && guard++ < 64)
    {
        const Limb limb = limbs.back();
        limbs.pop_back();

        const float bend = g.curl * 0.7f * (limb.level % 2 == 0 ? 1.0f : -1.0f);
        const int steps = std::max(3, int(limb.len));
        float ex = limb.x, ey = limb.y;

        for (int i = 0; i <= steps; ++i)
        {
            const float t = float(i) / float(steps);
            const float a = limb.angle + bend * t;
            ex = limb.x + std::cos(a) * limb.len * t;
            ey = limb.y + std::sin(a) * limb.len * t;

            const uint8_t tone = limb.level == 0 ? 1 : (limb.level >= 2 ? 3 : 2);
            const float thick = limb.width;
            const int px = int(std::floor(ex)), py = int(std::floor(ey));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (float(dx * dx + dy * dy) <= thick * thick + 0.4f)
                        s.Set(px + dx, py + dy, tone);

            if (i == steps && limb.level >= 2)
                s.Set(px, py, 4);                 // bright tip
        }

        if (limb.level < 3 && limb.len > 3.5f)
        {
            const int kids = 2 + (g.branch > 0.55f ? 1 : 0);
            for (int k = 0; k < kids; ++k)
            {
                const float spread = (float(k) - float(kids - 1) * 0.5f)
                                   * (0.55f + 0.45f * g.branch);
                const float angle = limb.angle + bend + spread
                                  + (float(int(Hash(g.seed, uint32_t(limb.level * 8 + k), g.seed) % 7u) - 3) * 0.03f);
                limbs.push_back({ ex, ey, angle, limb.len * 0.7f,
                                  std::max(0.9f, limb.width - 0.35f), limb.level + 1 });
            }
        }
    }
}

void DrawFanCoral(Scratch& s, const CoralGenome& g)
{
    // A filled fan, not a spray of lines: at 600x340 a fan reads as a
    // silhouette with radial ribs, and as a scribble if it is only ribs.
    // The radius is the height, so a fan is as tall as the size table says.
    const float r = std::max(4.0f, g.height * 0.92f);
    const float spread = 0.75f + 0.35f * g.branch;      // half-angle, radians
    const float center = g.lean * 0.28f;
    const float ribPhase = float(g.seed % 32u) * 0.17f;

    for (int y = int(std::floor(g.y - r * 1.1f)); y <= int(std::ceil(g.y)); ++y)
        for (int x = int(std::floor(g.x - r * 1.1f)); x <= int(std::ceil(g.x + r * 1.1f)); ++x)
        {
            const float dx = float(x) + 0.5f - g.x;
            const float dy = g.y - (float(y) + 0.5f);      // up is positive
            if (dy < -0.5f) continue;

            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > r) continue;

            const float angle = std::atan2(dx, std::max(dy, 0.15f)) - center;
            if (std::fabs(angle) > spread) continue;

            // A scissored top edge, so the fan is not a geometric arc.
            const float scallop = 0.85f + 0.15f
                * Sin(double(angle) * 9.0 + double(ribPhase));
            if (dist > r * scallop) continue;

            const float ribs = Sin(double(angle + center) * 13.0 + double(ribPhase) * 3.0);
            uint8_t tone = ribs > 0.45f ? 1 : (ribs > -0.2f ? 2 : 3);
            if (dist < r * 0.22f) tone = 2;                     // the stalk
            if (dist > r * scallop - 1.2f) tone = 4;            // bright rim
            s.Set(x, y, GrainCoral(uint32_t(x), uint32_t(y), g.seed, tone));
        }
}

void DrawBrainCoral(Scratch& s, const CoralGenome& g)
{
    const float rx = std::max(2.0f, g.width * 0.5f);
    const float ry = std::max(2.0f, g.height * 0.9f);
    const float phase = double(g.seed % 97u) * 0.13f;

    for (int y = int(std::floor(g.y - ry)); y <= int(std::ceil(g.y)); ++y)
        for (int x = int(std::floor(g.x - rx)); x <= int(std::ceil(g.x + rx)); ++x)
        {
            const float dx = (float(x) + 0.5f - g.x) / rx;
            const float dy = (float(y) + 0.5f - g.y) / ry;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 1.0f) continue;
            if (y > int(g.y)) continue;                    // dome sits on the sand

            // Wavy, not concentric: one sine of a bent argument gives the
            // meandering groove of a brain coral at any zoom.
            const float groove = Sin(double(d) * 9.0
                                     + Sin(double(dx * 3.1 + dy * 2.3) * 4.0 + double(phase)) * 1.7);
            uint8_t tone = groove > 0.55f ? 3 : (groove < -0.55f ? 1 : 2);
            if (d > 0.92f) tone = 1;                       // the rim stays dark
            s.Set(x, y, GrainCoral(uint32_t(x), uint32_t(y), g.seed, tone));
        }
}

void DrawKelpCoral(Scratch& s, const CoralGenome& g)
{
    // Two-pixel blades, not hairs: a seaweed ribbon is dark on one side and
    // light on the other, and it zigzags, which is all the eye needs at this
    // scale to read "plant" instead of "wire".
    const int stalks = 2 + int(g.branch * 2.0f);
    for (int k = 0; k < stalks; ++k)
    {
        const float sx = g.x + (float(k) - float(stalks - 1) * 0.5f) * (2.6f + g.width * 0.2f);
        const float len = g.height * (0.62f + 0.38f * float(Hash(g.seed, uint32_t(k), 9u) % 5u) / 4.0f);
        const int steps = std::max(8, int(len));
        const float phase = double(g.seed % 41u) * 0.14;

        for (int i = 0; i <= steps; ++i)
        {
            const float u = float(i) / float(steps);
            const float wave = Sin(double(u) * 2.4 + double(k) * 1.6 + phase);
            const int px = int(std::floor(sx + wave * (1.8f + g.curl * 1.6f) * u
                                         + g.lean * u * 2.2f));
            const int py = int(std::floor(g.y - len * u));

            // Dark leading edge, light trailing edge; the tip is the accent.
            s.Set(px, py, 1);
            s.Set(px + 1, py, u > 0.9f ? 2 : 3);

            // A broad side blade every seven rows, alternating sides.
            if (i > 4 && (i % 7) == 0)
            {
                const int dir = (((i / 7) + k) & 1) ? 1 : -1;
                const int reach = 3 + int(Hash(g.seed, uint32_t(i), uint32_t(k)) % 2u);
                for (int j = 1; j <= reach; ++j)
                {
                    const int lx = px + dir * j, ly = py - (j >> 1);
                    s.Set(lx, ly, j == reach ? 4 : 2);
                    s.Set(lx, ly - 1, j == reach ? 3 : 2);
                }
            }
            if (i == steps) s.Set(px + 1, py, 4);
        }
    }
}

void DrawPolypCoral(Scratch& s, const CoralGenome& g)
{
    const float rx = std::max(3.0f, g.width * 0.5f);
    const float ry = std::max(2.5f, g.height * 0.95f);

    // A dome of tissue, lit from the left.
    for (int y = int(std::floor(g.y - ry)); y <= int(std::ceil(g.y)); ++y)
        for (int x = int(std::floor(g.x - rx)); x <= int(std::ceil(g.x + rx)); ++x)
        {
            const float dx = (float(x) + 0.5f - g.x) / rx;
            const float dy = (float(y) + 0.5f - g.y) / ry;
            if (dx * dx + dy * dy > 1.0f) continue;
            if (y > int(g.y)) continue;

            const uint8_t tone = dx < -0.35f ? 1 : (dx > 0.3f ? 3 : 2);
            s.Set(x, y, GrainCoral(uint32_t(x), uint32_t(y), g.seed, tone));
        }

    // A crown of finger polyps standing on the dome: two pixels wide and two
    // to four tall, in the accent hue. This is the silhouette that says
    // "polyp" rather than "painted rock".
    const int polyps = 4 + int(g.branch * 5.0f);
    for (int i = 0; i < polyps; ++i)
    {
        const float t = (float(i) + 0.5f) / float(polyps);
        const float a = 0.30f + t * 2.55f;
        const int px = int(std::floor(g.x + std::cos(a) * rx * 0.78f));
        const int py = int(std::floor(g.y - std::sin(a) * ry * 0.92f));
        const int tall = 2 + int(Hash(g.seed, uint32_t(i), 5u) % 3u);

        for (int j = 1; j <= tall; ++j)
        {
            s.Set(px, py - j, j == tall ? 3 : 4);
            s.Set(px + 1, py - j, 4);
        }
        s.Set(px + 1, py, 2);                 // seat the polyp in the tissue
    }
}

/// Blit the scratch mask with one shared outline. This is the demo's
/// shared-outline trick: every species gets the same 1 px edge, which is what
/// makes a branch, a fan and a dome read as one scene.
void BlitCoral(const Scratch& s, const CoralPalette& pal, uint8_t* rgba,
               int w, const WaterMasks& masks)
{
    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x)
        {
            if (s.tone[size_t(y) * size_t(s.w) + size_t(x)] != 0) continue;

            const int ax = s.x0 + x, ay = s.y0 + y;
            const bool edge = s.Has(ax - 1, ay) || s.Has(ax + 1, ay)
                           || s.Has(ax, ay - 1) || s.Has(ax, ay + 1);
            if (edge && masks.Inside(ax, ay)) Put(rgba, w, ax, ay, pal.edge);
        }

    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x)
        {
            const uint8_t tone = s.tone[size_t(y) * size_t(s.w) + size_t(x)];
            if (tone == 0) continue;
            const int ax = s.x0 + x, ay = s.y0 + y;
            if (!masks.Inside(ax, ay)) continue;
            // 1..4 index the palette directly; `tone & 3` would fold the
            // accent (4) back onto the mid tone and lose every bright tip.
            Put(rgba, w, ax, ay, pal.tone[tone > 4 ? 4 : tone]);
        }
}

CoralGenome MakeCoral(std::mt19937& rng, int index, int w,
                      const WaterMasks& masks)
{
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    CoralGenome g;

    // Weighted vocabulary: branches and fans read best at this scale.
    static const CoralKind kKinds[10] = {
        CoralKind::Branch, CoralKind::Fan, CoralKind::Branch, CoralKind::Kelp,
        CoralKind::Brain, CoralKind::Branch, CoralKind::Fan, CoralKind::Polyp,
        CoralKind::Kelp, CoralKind::Brain
    };
    g.kind = kKinds[rng() % 10u];

    // A golden-angle walk places corals independently of how many exist, so
    // adding one never moves the others.
    const float spread = std::fmod(0.09f + float(index) * 0.6180339887f, 1.0f);
    g.x = std::clamp(spread, 0.02f, 0.98f) * float(w);

    g.depth = int(rng() % 3u);
    const float scale = 0.82f + 0.18f * float(g.depth);

    // Each species gets a size band that reads at 600x340: a branch is tall
    // and narrow, a brain is a wide low dome, a polyp is a small mound, and a
    // fan is as wide as it is tall. One shared range made everything look like
    // the same weed.
    float h0 = 18.0f, h1 = 36.0f, w0 = 10.0f, w1 = 24.0f;
    switch (g.kind)
    {
    case CoralKind::Branch: h0 = 20.0f; h1 = 38.0f; w0 = 14.0f; w1 = 26.0f; break;
    case CoralKind::Fan:    h0 = 16.0f; h1 = 34.0f; w0 = 20.0f; w1 = 40.0f; break;
    case CoralKind::Brain:  h0 = 10.0f; h1 = 18.0f; w0 = 16.0f; w1 = 30.0f; break;
    case CoralKind::Kelp:   h0 = 24.0f; h1 = 46.0f; w0 =  8.0f; w1 = 18.0f; break;
    case CoralKind::Polyp:  h0 =  7.0f; h1 = 13.0f; w0 = 14.0f; w1 = 24.0f; break;
    }
    g.height = (h0 + u01(rng) * (h1 - h0)) * scale;
    g.width  = (w0 + u01(rng) * (w1 - w0)) * scale;

    g.lean   = u01(rng) * 2.0f - 1.0f;
    g.curl   = u01(rng) * 2.0f - 1.0f;
    g.branch = u01(rng);

    // Hue walks a curated reef palette rather than the wheel: pinks, oranges,
    // yellows, greens, teals, blues and violets, laid out so neighbours and
    // corals seven apart never share a colour.
    static const float kHues[7] = { 0.95f, 0.08f, 0.13f, 0.45f, 0.55f, 0.72f, 0.33f };
    g.hue  = std::fmod(kHues[index % 7] + u01(rng) * 0.05f - 0.025f + 1.0f, 1.0f);
    g.hue2 = std::fmod(g.hue + 0.5f + u01(rng) * 0.1f, 1.0f);
    g.sat  = 0.66f + u01(rng) * 0.28f;
    g.val  = 0.70f + u01(rng) * 0.22f;
    g.alpha = (0.4f + u01(rng) * 0.18f) * (0.92f + 0.08f * float(g.depth));

    g.seed = uint32_t(rng());
    g.y = masks.SandAt(int(g.x)) + 1.0f + float(g.depth) * 2.5f;
    return g;
}

void DrawCoral(uint8_t* rgba, int w, int h, const WaterMasks& masks,
               const CoralGenome& g)
{
    // Generous box, then the shape's own tests decide what is painted.
    const int reach = int(std::ceil(std::max(g.height, g.width) * 1.4f)) + 6;
    const int x0 = std::max(0, int(g.x) - reach);
    const int y0 = std::max(0, int(g.y) - reach);
    const int x1 = std::min(w - 1, int(g.x) + reach);
    const int y1 = std::min(h - 1, int(g.y) + reach);
    if (x1 <= x0 || y1 <= y0) return;

    Scratch s;
    s.Reset(x0, y0, x1 - x0 + 1, y1 - y0 + 1);

    switch (g.kind)
    {
    case CoralKind::Branch: DrawBranchCoral(s, g); break;
    case CoralKind::Fan:    DrawFanCoral(s, g);    break;
    case CoralKind::Brain:  DrawBrainCoral(s, g);  break;
    case CoralKind::Kelp:   DrawKelpCoral(s, g);   break;
    case CoralKind::Polyp:  DrawPolypCoral(s, g);  break;
    }

    BlitCoral(s, MakeCoralPalette(g), rgba, w, masks);
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
    // ---- corals -------------------------------------------------
    // One coral per index from a stream seeded by the aquascape seed, so a
    // reef is reproducible and "add a coral" only appends: no existing coral
    // changes its shape or place.
    std::mt19937 coralRng(spec.seed * 2654435761u + 0xC0FFEEu);
    std::vector<CoralGenome> corals;
    corals.reserve(size_t(std::max(spec.coralCount, 0)));
    for (int i = 0; i < spec.coralCount; ++i)
        corals.push_back(MakeCoral(coralRng, i, w_, masks));

    // Rocks go down back to front, then every coral on top of them, then the
    // front rock. Corals are foreground growth: if one shared the rocks' own
    // depth sort, a coral behind a big rock would vanish rather than peek.
    std::sort(rocks.begin(), rocks.end(),
              [](const Rock& a, const Rock& b) { return a.cy < b.cy; });

    const auto drawRock = [&](const Rock& rock, uint8_t* target)
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
                Put(target, w_, x, y, RockColour(rock, float(x), float(y), r, spec.seed));
            }
        }
    };

    for (const Rock& rock : rocks)
        drawRock(rock, plate_.data());

    // Nearest coral last, so tall kelp in front of a low polyp overlaps it.
    std::sort(corals.begin(), corals.end(),
              [](const CoralGenome& a, const CoralGenome& b) { return a.y < b.y; });
    for (const CoralGenome& coral : corals)
        DrawCoral(plate_.data(), w_, h_, masks, coral);

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
