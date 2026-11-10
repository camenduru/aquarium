// ================================================================
// SEA CREATURES - shaded renderer
// Ported from fish-gen/fishgen/render.py (MIT).
// ================================================================
#include "creature/sea/SeaRender.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace sea {
namespace {

Vec3 Rgb(const SeaRgba& c) { return Vec3(c.r / 255.0, c.g / 255.0, c.b / 255.0); }

Vec3 Mul(const Vec3& a, const Vec3& b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }

double Frac(double x) { return x - std::floor(x); }

double Band(double x, double center, double halfWidth) {
    return 1.0 - Saturate(std::fabs(x - center) / Max(halfWidth, 1e-9));
}

/// render.py's band(x, lo, hi): a two-edge smoothstep band, not center/half.
double BandLH(double x, double lo, double hi, double aa) {
    return Smoothstep(lo - aa, lo + aa, x) * (1.0 - Smoothstep(hi - aa, hi + aa, x));
}

/// Project a camera-space point into the high-resolution buffer.
inline void Project(const Vec3& p, double scale, double cx, double cy, double& sx, double& sy) {
    sx = cx + p.x * scale;
    sy = cy - p.y * scale;
}

// ---------------------------------------------------------------- light rig
// render.py:28-37. The reference shades in *linear* light: a warm sun plus a
// blue-green hemisphere, so shadowed skin goes cool instead of just dark.
// Multiplying sRGB by a scalar (what this did) cannot reproduce that.
const Vec3 kSun = Vec3(1.0, 0.96, 0.88) * 0.95;
const Vec3 kSky(0.30, 0.42, 0.50);
const Vec3 kDeep(0.035, 0.055, 0.075);
const Vec3 kRimColor(0.35, 0.55, 0.70);
const Vec3 kSunWorld = Vec3(0.18, 1.0, 0.42).NormalizedOr(Vec3(0, 1, 0));

/// geometry.py's `_camera`: tilt a world direction into camera space.
inline Vec3 CameraDir(const Vec3& p, double pitch) {
    const double c = std::cos(pitch), s = std::sin(pitch);
    return Vec3(p.x, p.y * c - p.z * s, p.z * c + p.y * s);
}

inline double SrgbToLinear(double c) {
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

inline double LinearToSrgb(double c) {
    c = Saturate(c);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

/// render.py's highlight shoulder, applied to linear light before encoding:
/// a knee at 0.8, then an exponential roll-off that never quite reaches 1.
inline double ToneMap(double c) {
    constexpr double knee = 0.8;
    if (c <= knee) return c;
    return knee + (1.0 - knee) * (1.0 - std::exp(-(c - knee) / (1.0 - knee)));
}

/// The inverse, so an overlay that works in linear light can take a buffer that
/// Shade() already tone mapped, shade it, and hand it back unchanged elsewhere.
inline double UnToneMap(double c) {
    constexpr double knee = 0.8;
    if (c <= knee) return c;
    const double t = 1.0 - (c - knee) / (1.0 - knee);
    if (t <= 1e-9) return knee + 12.0;   // fully saturated: any large value round-trips
    return knee - (1.0 - knee) * std::log(t);
}

/// How much the light wraps around the terminator, and how tight the wet sheen is.
double WrapFor(SeaKind kind) {
    switch (kind) {
        case SeaKind::Shark: return 0.15;
        case SeaKind::Whale: return 0.20;
        case SeaKind::Ray: return 0.20;
        case SeaKind::Squid: return 0.45;
        case SeaKind::Octopus: return 0.40;
        case SeaKind::Urchin: return 0.20;
        default: return 0.25;
    }
}

double ShininessFor(SeaKind kind) {
    switch (kind) {
        case SeaKind::Shark: return 28.0;
        case SeaKind::Whale: return 45.0;
        case SeaKind::Ray: return 30.0;
        case SeaKind::Squid: return 70.0;
        case SeaKind::Octopus: return 40.0;
        case SeaKind::Urchin: return 20.0;
        default: return 60.0;
    }
}


} // namespace

namespace {
int DetailFor(int size) {
    // render.py: FishModel(genome, detail=max(width, height)). The old
    // 120*(size/64)^0.9 formula made 128 px use detail 224 instead of 128,
    // which shrank the minimum arm radius (0.0049 vs 0.0086) and broke thin
    // tentacles into sub-pixel dashes.
    return ClampI(size, 8, 512);
}
} // namespace

SeaRenderer::SeaRenderer(const SeaGenome& genome, int size)
    : g_(genome), size_(ClampI(size, 8, 512)), detail_(DetailFor(size_)), model_(genome, detail_),
      palette_(BuildPalette(genome.colors)) {
    // Reference default fit set: one full swim cycle at yaw 0.
    std::vector<SeaPose> poses;
    poses.reserve(8);
    for (int i = 0; i < 8; i++) {
        SeaPose p;
        p.phase = (double)i / 8.0;
        p.pitch = SeaCameraPitch(genome);
        poses.push_back(p);
    }
    ComputeFit(poses);
}

void SeaRenderer::SetFitPoses(const std::vector<SeaPose>& poses) {
    if (!poses.empty()) ComputeFit(poses);
}

void SeaRenderer::ComputeFit(const std::vector<SeaPose>& poses) {
    double extX = 1e-6, extY = 1e-6;
    SeaPosed posed;
    for (const SeaPose& pose : poses) {
        model_.Pose(pose, posed);
        double maxX = 0.0, maxY = 0.0;
        for (const Vec3& p : posed.cam) {
            maxX = Max(maxX, std::fabs(p.x));
            maxY = Max(maxY, std::fabs(p.y));
        }
        for (const Vec3& p : posed.spineLines) {
            maxX = Max(maxX, std::fabs(p.x));
            maxY = Max(maxY, std::fabs(p.y));
        }
        // render.py adds the spine radius (not a flat 0.005) when spines exist.
        double pad = 0.0;
        if (!posed.spineLines.empty()) {
            if (g_.puffSpines > 0) {
                pad = 0.012;
            } else {
                pad = Clamp(0.55 / std::sqrt((double)Max(g_.spines.count, 1)) *
                                (0.5 + g_.spines.length), 0.008, 0.045);
            }
        }
        extX = Max(extX, maxX + pad);
        extY = Max(extY, maxY + pad);
    }
    // pixel.py's pixel-art margin is 2 px (1 px only for a no-outline style).
    const double avail = Max(1.0, size_ * 0.5 - 2.0);
    fitScale_ = Max(1e-6, Min(avail / extX, avail / extY));
}

// ---------------------------------------------------------------- markings
void SeaRenderer::SurfaceCoords(const Vec4& at, double& u1, double& u2, double& ang) const {
    const double a = Clamp(at.x, 0.0, 1.0);
    const double girth = 0.5 * (model_.Top(a) + model_.Bottom(a)) + 0.5 * model_.HalfWidth(a);
    ang = std::atan2(at.y, at.z);
    u1 = at.x;
    u2 = ang * girth * 0.65;
}

Vec3 SeaRenderer::WingAlbedo(const Vec4& at, bool top, double pxLen) const {
    const ColorGenes& c = g_.colors;
    const double a = at.x, r = at.y, side = at.z, m = at.w;
    const double seed = (double)(g_.seed % 997);
    const double span = Max(g_.wings.span, Max(g_.hammer, 0.05));
    const double x = a, y = side * r * span;
    const Vec3 cBase = Rgb(palette_.At(SeaSlotBase));
    const Vec3 cBelly = Rgb(palette_.At(SeaSlotBelly));
    const Vec3 cAccent = Rgb(palette_.At(SeaSlotAccent));
    Vec3 col = cBase * (1.0 + 0.08 * Fbm(x * 30.0, y * 30.0, seed + 2.0, 3));
    if (c.pattern == PatternKind::Spots || c.pattern == PatternKind::Rings) {
        const double k = g_.kind == SeaKind::Ray ? 2.6 : 1.6;
        const double sp = Spots(x * k, y * k, g_.kind == SeaKind::Ray ? 0.035 : 0.05,
                                c.pattern == PatternKind::Rings,
                                g_.kind == SeaKind::Ray ? 0.65 : 0.5, pxLen);
        col = Lerp(col, cAccent, sp);
    } else if (c.pattern == PatternKind::Chevron) {
        // Manta shoulder patches: pale wedges on the back behind the head.
        double wedge = Smoothstep(0.62, 0.5, r + 0.6 * m) * Smoothstep(0.08, 0.2, r) *
                       Smoothstep(0.85, 0.55, m);
        wedge *= 0.75 + 0.25 * Fbm(x * 15.0, y * 15.0, seed, 4);
        col = Lerp(col, cAccent, wedge);
    } else if (c.pattern == PatternKind::Shimmer || c.pattern == PatternKind::Tiger ||
               c.pattern == PatternKind::Blotches) {
        const double n1 = Fbm(x * 18.0, y * 18.0, seed, 3);
        col = Lerp(col, cAccent, Smoothstep(0.15, 0.4, n1) * 0.6);
    }
    col = col * (1.0 - 0.15 * Smoothstep(0.8, 1.0, r));
    if (c.bellyLightness > 0.25 && (g_.kind == SeaKind::Ray || g_.kind == SeaKind::Shark)) {
        Vec3 ucol = cBelly;
        ucol = Lerp(ucol, cBase * 0.6 + cBelly * 0.4, Smoothstep(0.75, 1.0, r) * 0.7);
        if (c.bellyPattern) {
            const double sp = Spots(x * 2.4 + 3.0, y * 2.4, 0.03, false, 0.35, pxLen) *
                              Smoothstep(0.7, 0.3, r);
            ucol = Lerp(ucol, Vec3(0.03, 0.03, 0.04), sp);
        }
        if (!top) col = ucol;
    }
    return col;
}

double SeaRenderer::Spots(double ua, double ub, double size, bool ring, double density,
                          double pxLen) const {
    const ColorGenes& c = g_.colors;
    const double cells = 5.0 + std::floor(c.patternScale * 6.0);
    const double u = ua * cells + c.patternOffset;
    const double v = ub * cells;
    const double iu = std::floor(u), iv = std::floor(v);
    const double h = Hash01(iu, iv, (double)(g_.seed % 997));
    const double fa = u - iu - 0.5 + (Hash01(iu, iv + 7.0, 3.0) - 0.5) * 0.3;
    const double fb = v - iv - 0.5 + (h - 0.5) * 0.4;
    const bool has = std::fmod(h * 7.0, 1.0) < density;
    const double r2 = fa * fa + fb * fb;
    const double rad = size * (0.8 + 0.4 * Hash01(iu + 3.0, iv, 5.0));
    const double scale = 1.0 / Max(pxLen, 1e-9);
    const double edge = Max(0.004, 0.6 / (scale * 0.6 / cells + 1e-6) * 0.05);
    if (!has) return 0.0;
    const double disk = Smoothstep(rad + edge, rad - edge, r2);
    if (!ring) return disk;
    const double hole = (scale * 0.6 / cells > 3.0) ? 0.012 : -1.0;
    return disk * Smoothstep(hole - edge, hole + edge, r2);
}

double SeaRenderer::StripeCoverage(const Vec4& at, bool fin, double grow, double pxLen) const {
    const ColorGenes& c = g_.colors;
    const double u = at.x, v = at.y;
    const double count = 3.0 + std::floor(c.patternScale * 4.0);
    const double phase = Frac((u - c.patternOffset) * count + 0.04 * (1.0 - v * v) * count * 0.3);
    const double gr = grow * count;
    const double width = 0.32 * (1.0 - 0.15 * std::fabs(v)) + 2.0 * gr;
    const double aa = Max(pxLen, 0.004);
    const double ph = Frac(phase + gr);
    const double stripe = Smoothstep(-aa, aa, ph) *
                          (1.0 - Smoothstep(width - aa, width + aa, ph));
    const double face = fin ? 1.0 : ((u > g_.eyePos + 0.08 || u < g_.eyePos - 0.25) ? 1.0 : 0.0);
    return face * (u < 0.95 ? 1.0 : 0.0) * stripe * (v > -0.6 - grow * 4.0 ? 1.0 : 0.0);
}

double SeaRenderer::Scars(double u1, double u2, double pxLen) const {
    std::mt19937 rng((unsigned)(g_.seed + 11));
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    double out = 0.0;
    const double width = Max(0.002, 0.5 * pxLen);
    for (int n = 0; n < 10; n++) {
        const double c0u = 0.1 + u01(rng) * 0.8;
        const double c0v = -0.08 + u01(rng) * 0.16;
        const double ang = (-0.6 + u01(rng) * 1.2) + (u01(rng) < 0.3 ? kPi / 2.0 : 0.0);
        const double ln = 0.015 + u01(rng) * 0.055;
        const double bend = -8.0 + u01(rng) * 16.0;
        const double du = std::cos(ang), dv = std::sin(ang);
        const double relU = u1 - c0u, relV = u2 - c0v;
        const double along = relU * du + relV * dv;
        const double across = -relU * dv + relV * du - bend * (along - ln / 2.0) * (along - ln / 2.0);
        const double inside = (along > 0.0 && along < ln) ? 1.0 : 0.0;
        const double broken = Fbm(along * 200.0, c0u * 50.0, (double)(g_.seed % 97), 1) > -0.3 ? 1.0 : 0.0;
        const double fade = std::pow(std::sin(Clamp(along / ln, 0.0, 1.0) * kPi), 0.5);
        out = Max(out, inside * broken * fade * Smoothstep(width, width * 0.2, std::fabs(across)) *
                          (0.3 + u01(rng) * 0.4));
    }
    return out;
}

Vec3 SeaRenderer::UrchinShell(Vec3 col, double a, double v, double w) const {
    const double x = 2.0 * a - 1.0;
    const double r = std::sqrt(Saturate(1.0 - x * x));
    const double lat = std::asin(Clamp(r * v, -1.0, 1.0));
    const double lon = std::atan2(r * w, x);
    const double twist = (double)(g_.seed % 97) * 0.37;
    const double bands = Smoothstep(0.55, 0.85, std::cos(5.0 * (lon - twist))) *
                         Smoothstep(-0.9, -0.5, lat) * Smoothstep(1.3, 1.1, lat);
    col = Lerp(col, col * 1.45 + Vec3(0.01, 0.01, 0.01), bands * 0.6);
    const double ring = Smoothstep(1.22, 1.3, lat) * Smoothstep(1.45, 1.38, lat);
    col = Lerp(col, col * 0.35, ring);
    col = col * 0.5;
    const double knob = Smoothstep(1.4, 1.46, lat);
    const Vec3 cone = g_.colors.accent2Color.empty() ? Rgb(palette_.At(SeaSlotFin)) * 0.8
                                                     : Rgb(RgbaFromHex(g_.colors.accent2Color,
                                                                       palette_.At(SeaSlotAccent2)));
    return Lerp(col, cone, knob * 0.85);
}

Vec3 SeaRenderer::Pattern(int part, const Vec4& at, const Vec3& base, bool fin, double pxLen) const {
    const ColorGenes& c = g_.colors;
    const PatternKind p = c.pattern;
    if (p == PatternKind::None) return base;
    (void)part;

    const double u = at.x, v = at.y;
    const double body = (u > g_.eyePos + 0.08 || u < g_.eyePos - 0.25) ? 1.0 : 0.0;
    const double s = Max(c.patternScale, 0.05);
    const double off = c.patternOffset;
    const Vec3 accent = Rgb(palette_.At(SeaSlotAccent));
    const Vec3 accent2 = Rgb(palette_.At(SeaSlotAccent2));
    const Vec3 belly = Rgb(palette_.At(SeaSlotBelly));
    const Vec3 dark = Rgb(palette_.At(SeaSlotOutline));

    auto over = [&](const Vec3& top, double t) { return Lerp(base, top, Saturate(t)); };

    switch (p) {
        case PatternKind::Stripes: {
            // Ported from render.py: three bars (plus one per 0.25 of pattern
            // scale), bowed at the flanks, starting behind the eye. The head
            // stays black; wings/fins only get bars when patternOnFins is set.
            if (fin && !c.patternOnFins) return base;
            return over(accent, StripeCoverage(at, fin, 0.0, pxLen));
        }
        case PatternKind::Spots:
        case PatternKind::Rings: {
            double u1, u2, ang;
            SurfaceCoords(at, u1, u2, ang);
            const double angn = ang / kPi;
            if (p == PatternKind::Rings) {
                return over(accent, body * Spots(u, angn * 0.6, 0.06, true, 0.5, pxLen) *
                                        (v > -0.5 ? 1.0 : 0.0));
            }
            if (g_.kind == SeaKind::Ray) {
                return over(accent, Spots(u1 * 2.6, u2 * 2.6, 0.035, false, 0.65, pxLen) *
                                        (v > 0.1 ? 1.0 : 0.0));
            }
            return over(accent, body * Spots(u, angn * 0.6, 0.05, false, 0.5, pxLen) *
                                    (v > -0.5 ? 1.0 : 0.0));
        }
        case PatternKind::Lateral:
            return over(accent, body * BandLH(v, -0.1 - 0.2 * s, 0.15, Max(pxLen, 0.004)));
        case PatternKind::Saddle:
            return over(accent, (v > 0.12 && Frac(u * s * 7.0 + off) < 0.38) ? 0.85 : 0.0);
        case PatternKind::Gradient:
            return Lerp(base, accent, Smoothstep(-0.55, 0.95, v) * 0.75);
        case PatternKind::Bands:
            return over(accent, Frac(v * s * 7.0 + off) < 0.36 ? 0.85 : 0.0);
        case PatternKind::Tiger: {
            const double wobble = Fbm(u * 6.0 * s, v * 2.0, 3.0 + g_.seed * 0.01, 3);
            return over(dark, (v > -0.05 && Frac(u * s * 22.0 + wobble * 1.4 + off) < 0.3) ? 0.9 : 0.0);
        }
        case PatternKind::Shimmer: {
            // render.py: two moving shimmer bands, phase-locked to the swim cycle.
            const double count = 2.0 + c.patternScale * 3.0;
            const double uu = std::sin(kTwoPi * (u * count + 0.35 * v +
                                                 0.2 * std::sin(4.0 * v + 3.0 * u) - posePhase_));
            const double keep = (v > -0.75 && u < 0.98) ? 1.0 : 0.0;
            const double m1 = keep * Smoothstep(0.45, 0.75, uu);
            const double m2 = keep * Smoothstep(-0.5, -0.8, uu);
            return Lerp(over(accent, m1), accent2, m2);
        }
        case PatternKind::Orca: {
            // Ported from render.py: a curved white chin/flank, an oval eye patch
            // behind the eye, and a soft grey saddle behind the dorsal.
            if (fin) return base;
            const double dx = (u - (g_.eyePos + 0.12)) / 0.095;
            const double dy = (v - 0.30) / 0.19;
            const double patch = (1.0 - Smoothstep(0.95, 1.05, dx * dx + dy * dy)) *
                                 (std::fabs(at.z) > 0.15 ? 1.0 : 0.0);

            static const double ka[] = {0.0, 0.08, 0.2, 0.32, 0.42, 0.5, 0.58, 0.66, 0.74, 0.8, 0.86};
            static const double kb[] = {-0.25, -0.12, -0.35, -0.65, -0.75, -0.45, 0.05, 0.12, -0.15, -0.6, -1.2};
            const double o = c.patternOffset * 0.04;
            double edge = kb[0];
            if (u >= ka[10]) {
                edge = kb[10];
            } else if (u > ka[0]) {
                for (int i = 0; i < 10; i++) {
                    if (u <= ka[i + 1]) {
                        const double t = (u - ka[i]) / (ka[i + 1] - ka[i]);
                        edge = Lerp(kb[i], kb[i + 1], t) + (i == 5 ? o : 0.0);
                        break;
                    }
                }
            }
            const double white = (1.0 - Smoothstep(edge - 0.03, edge + 0.03, v)) *
                                 (u < 0.86 ? 1.0 : 0.0);

            double saddle = 0.0;
            if (g_.dorsal.present) {
                const double d0 = g_.dorsal.start + 0.02;
                const double d1 = g_.dorsal.end + 0.14;
                const double along = Smoothstep(d0 - 0.01, d0 + 0.01, u) *
                                     (1.0 - Smoothstep(d1 - 0.01, d1 + 0.01, u));
                const double up = Smoothstep(0.5, 0.6, v - 0.25 * std::fabs(u - g_.dorsal.end - 0.05) / 0.1);
                saddle = along * up * (0.65 + 0.35 * Fbm(u * 60.0, v * 8.0, (double)(g_.seed % 997)));
            }
            Vec3 col = Lerp(base, accent, Max(patch, white));
            return Lerp(col, accent2, Saturate(saddle));
        }
        case PatternKind::Neon:
            return over(accent2, Band(v, 0.02, 0.07) * 1.0);
        case PatternKind::WhaleShark: {
            const double gi = Frac(u * s * 24.0 + off), gj = Frac((v + 1.0) * s * 16.0);
            const bool spot = gi < 0.16 && gj < 0.22;
            const bool line = Band(gj, 0.5, 0.03) > 0.7 && v > 0.15;
            return over(accent2, spot ? 1.0 : (line ? 0.6 : 0.0));
        }
        case PatternKind::Mackerel: {
            const double wob = Fbm(u * 8.0, v * 3.0, 11.0 + g_.seed * 0.01, 2);
            return over(dark, (v > 0.15 && Frac(u * s * 18.0 + wob * 1.2) < 0.28) ? 0.8 : 0.0);
        }
        case PatternKind::Koi: {
            const double n = Fbm(u * 5.0 * s, v * 3.0, 5.0 + g_.seed * 0.01, 4);
            return over(accent, n > 0.18 ? 0.9 : 0.0);
        }
        case PatternKind::Calico: {
            const double n = Fbm(u * 4.0 * s, v * 3.0, 21.0 + g_.seed * 0.01, 3);
            if (n > 0.3) return over(accent, 0.85);
            if (n < -0.28) return over(accent2, 0.7);
            return base;
        }
        case PatternKind::Discus:
            return over(accent, Frac(u * s * 5.0 + off) < 0.24 ? 0.75 : 0.0);
        case PatternKind::Chevron:
            // render.py has no body-pattern case for "chevron": the chevron is
            // a WING marking handled by _wing_albedo(). Returning stripes here
            // painted ribs across the manta's disc.
            return base;
        case PatternKind::Trout:
            if (Band(v, 0.0, 0.12) > 0.2) return over(accent, 0.55);
            return over(accent2, Hash01((int)(u * s * 30.0), (int)((v + 1) * s * 20.0), 3.0) > 0.78 ? 0.9 : 0.0);
        case PatternKind::Salmon:
            return over(accent, Band(v, -0.18, 0.16) * 0.55);
        case PatternKind::Blotches: {
            // render.py: two-tone irregular patches (dark accent plus pale accent2),
            // in surface coordinates, unique per animal.
            double su1, su2, sang;
            SurfaceCoords(at, su1, su2, sang);
            const double freq = 6.0 + 10.0 * c.patternScale;
            const double n1 = Fbm(su1 * freq, su2 * freq, (double)(g_.seed % 997), 4);
            const double m1 = Smoothstep(0.1, 0.35, n1) * 0.8;
            const double m2 = Smoothstep(-0.2, -0.45, n1) * 0.6;
            return Lerp(over(accent, m1), accent2, m2);
        }
        case PatternKind::Tang: {
            const double t = Band(u, 0.72, 0.2);
            return over(accent2, t * 0.7 + Band(v, 0.0, 0.06) * 0.5);
        }
        case PatternKind::DolphinCape:
            return over(dark, Smoothstep(0.15, 0.5, v) * 0.85);
        case PatternKind::Barracuda:
            return over(dark, (u > 0.25 && Hash01((int)(u * s * 24.0), (int)((v + 1) * s * 10.0), 9.0) > 0.8) ? 0.8 : 0.0);
        case PatternKind::Humpback: {
            if (v < -0.4) {
                const double groove = Band(Frac(u * s * 16.0 + off), 0.5, 0.12);
                return over(accent, 0.25 + 0.35 * groove);
            }
            return base;
        }
        case PatternKind::RedBelly:
            return over(accent, Smoothstep(-0.15, -0.75, v) * 0.9);
        default:
            return base;
    }
}

// ---------------------------------------------------------------- shading
Vec3 SeaRenderer::Shade(int part, const Vec4& at, Vec3 n, double pitch, double pxLen,
                        double& alpha, double sunLit, double ao) const {
    alpha = 1.0;
    Vec3 base(0.6, 0.6, 0.6);
    bool fin = IsFinPart(part);
    const bool transparent = fin || IsWingPart(part);
    double finTransmit = 0.0;   // light through a thin membrane
    double finWrap = -1.0;      // -1 = use the kind's wrap
    double glossScale = 1.0;
    double glossValue = -1.0;   // explicit gloss (mouth/teeth)
    bool skipSilver = false;    // fins are not mirror-scaled

    if (part == PartMouth || part == PartTeeth) {
        // render.py's _mouth_albedo(): pink gums darkening to a deep throat,
        // and ivory teeth that yellow toward the root. Linear in the reference.
        const double depth = std::sin(kPi * Clamp(at.y, 0.0, 1.0));
        const Vec3 flesh = Lerp(Vec3(0.36, 0.08, 0.08), Vec3(0.07, 0.01, 0.012),
                                Smoothstep(0.1, 0.6, depth));
        const Vec3 tooth = Vec3(0.88, 0.85, 0.74) * (0.8 + 0.2 * at.y);
        const Vec3 lin = part == PartTeeth ? tooth : flesh;
        base = Vec3(LinearToSrgb(lin.x), LinearToSrgb(lin.y), LinearToSrgb(lin.z));
        glossValue = part == PartTeeth ? 0.7 : 0.55;
        skipSilver = true;
    } else if (part == PartBody) {
        base = Rgb(palette_.At(SeaSlotBase));
        const double v = at.y;
        double u1, u2, ang;
        SurfaceCoords(at, u1, u2, ang);
        const double a = at.x;
        const double seed = (double)(g_.seed % 997);
        // Countershading: the back a touch darker, the flanks a touch lighter.
        // (render.py:419 — it was missing entirely here.)
        base = base * (1.0 + 0.18 * (0.15 - Clamp(v, -1.0, 1.0)));
        // The pale belly only exists above a lightness threshold, and its edge is a
        // soft step at `belly_level` — not a gradient smeared over the whole flank.
        double bellyT = 0.0;
        if (g_.colors.bellyLightness > 0.2) {
            const double level = g_.colors.hasBellyLevel
                                     ? g_.colors.bellyLevel
                                     : -0.05 - 0.2 * (1.0 - g_.colors.bellyLightness);
            double soft = 0.16;
            if (g_.kind == SeaKind::Shark) soft = 0.04;
            else if (g_.kind == SeaKind::Whale) soft = 0.07;
            else if (g_.kind == SeaKind::Ray) soft = 0.10;
            // render.py: sharks and whales get a ragged belly edge and a dark
            // tail stalk, so the pale underside does not run clean into the tail.
            double rag = 0.0;
            if (g_.kind == SeaKind::Shark || g_.kind == SeaKind::Whale) {
                rag = 0.07 * Fbm(u1 * 18.0, u2 * 6.0, seed) +
                      0.04 * std::sin(u1 * 31.0 + 4.0 * g_.colors.patternOffset);
                rag += 0.9 * Smoothstep(0.72, 0.97, a);
            }
            double t = Smoothstep(level + soft, level - soft, v + rag);
            if (g_.kind == SeaKind::Squid || g_.kind == SeaKind::Octopus) t *= 0.6;
            bellyT = t;
            base = Lerp(base, Rgb(palette_.At(SeaSlotBelly)), t);
        }
        base = Pattern(part, at, base, false, pxLen);
        // render.py: a dark rim round each stripe mark (clownfish bands), made
        // by subtracting the un-grown coverage from a version grown by ~1 pixel.
        if (!g_.colors.accentEdgeColor.empty() && g_.colors.pattern == PatternKind::Stripes) {
            const double grow = Max(0.008, 1.2 * pxLen);
            const double m1 = StripeCoverage(at, false, 0.0, pxLen);
            const double grown = StripeCoverage(at, false, grow, pxLen);
            const Vec3 cEdge = Rgb(RgbaFromHex(g_.colors.accentEdgeColor,
                                               palette_.At(SeaSlotAccent)));
            base = Lerp(base, cEdge, Saturate(grown - m1));
        }

        // render.py's _body_albedo() tail: scales + lateral line, skin mottling,
        // scars, knobs/barnacles, gill cover/slits, blowhole and throat grooves.
        if (g_.scales > 0) {
            const double sz = g_.scales;
            const double fade = Smoothstep(1.2, 3.0, sz / pxLen);
            if (fade > 0) {
                const double su = u1 / sz, sv = u2 / (sz * 0.75);
                const double row = std::floor(sv);
                const double su2 = su + 0.5 * std::fmod(row, 2.0);
                double fu = su2 - std::floor(su2);
                const double fv = sv - row;
                fu = fu + 0.12 * (Hash01((int)std::floor(su2), (int)row, seed + 1.0) - 0.5);
                const double rim = Smoothstep(0.7, 0.97, fu + 0.55 * (fv - 0.5) * (fv - 0.5) * 2.0);
                const double cell = Hash01((int)std::floor(su2), (int)row, seed);
                const double keep = Smoothstep(g_.eyePos + 0.08, g_.eyePos + 0.14, a) *
                                    (a < 0.99 ? 1.0 : 0.0);
                const double k = fade * keep * (0.6 + 0.4 * Fbm(u1 * 12.0, u2 * 12.0, seed + 6.0, 2));
                base = base * (1.0 - (0.1 * rim - 0.06 * (cell - 0.5)) * k);
                const double ll = 0.3 * (1.0 - a) + 0.05;
                const double line = Smoothstep(pxLen * 0.9, pxLen * 0.3, std::fabs(u2 - ll * 0.25)) *
                                    (a > g_.eyePos + 0.12 ? 1.0 : 0.0) * (a < 0.97 ? 1.0 : 0.0) *
                                    (std::fabs(ang) < kPi / 2.0 ? 1.0 : 0.0);
                base = base * (1.0 - 0.18 * line * fade);
            }
        }
        // Skin: fine mottling everywhere.
        const double grain = Fbm(u1 * 40.0, u2 * 40.0, seed + 3.0, 3);
        double mott = 0.05;
        if (g_.kind == SeaKind::Shark) mott = 0.07;
        else if (g_.kind == SeaKind::Whale || g_.kind == SeaKind::Ray) mott = 0.08;
        else if (g_.kind == SeaKind::Squid) mott = 0.06;
        else if (g_.kind == SeaKind::Octopus) mott = 0.12;
        else if (g_.kind == SeaKind::Urchin) mott = 0.04;
        base = base * (1.0 + mott * grain);
        if (g_.kind == SeaKind::Squid || g_.kind == SeaKind::Octopus) {
            const double dots = Hash01((int)std::floor(u1 / 0.012), (int)std::floor(u2 / 0.012), seed + 9.0);
            const double chrom = (dots > 0.8 ? 1.0 : 0.0) * Smoothstep(1.0, 2.5, 0.012 / pxLen);
            base = Lerp(base, Mul(base, Vec3(0.55, 0.3, 0.25)), chrom * 0.7);
        }
        if (g_.kind == SeaKind::Urchin) base = UrchinShell(base, a, v, at.z);
        const double scars = Min(1.0, g_.scars + 0.5 * g_.scariness * (g_.kind != SeaKind::Fish ? 1.0 : 0.0));
        if (scars > 0)
            base = Lerp(base, base * 1.6 + Vec3(0.04, 0.04, 0.04), Scars(u1, u2, pxLen) * scars);
        if (g_.knobs > 0) {
            const double kn = Hash01((int)std::floor(u1 / 0.018), (int)std::floor(u2 / 0.018), seed + 5.0);
            const double fu = u1 / 0.018 - std::floor(u1 / 0.018) - 0.5;
            const double fv = u2 / 0.018 - std::floor(u2 / 0.018) - 0.5;
            const double bump = (kn > 0.55 ? 1.0 : 0.0) * Smoothstep(0.2, 0.05, fu * fu + fv * fv) *
                                (a < 0.2 ? 1.0 : 0.0) * (v > -0.2 ? 1.0 : 0.0);
            base = Lerp(base, base * 1.35 + Vec3(0.01, 0.01, 0.01),
                        bump * g_.knobs * Smoothstep(0.0, -0.08, fv) * 0.7);
            base = Lerp(base, base * 0.7, bump * g_.knobs * Smoothstep(0.0, 0.08, fv) * 0.7);
            const double barn = (Fbm(u1 * 30.0, u2 * 30.0, seed + 8.0, 2) > 0.45 ? 1.0 : 0.0) *
                                (a < 0.12 ? 1.0 : 0.0) * (v < -0.2 ? 1.0 : 0.0);
            base = Lerp(base, Vec3(0.55, 0.55, 0.5), barn * g_.knobs);
        }
        // Head details: gill cover or gill slits.
        if (g_.gillSlits == 1 && g_.kind == SeaKind::Fish) {
            const double ga = g_.eyePos + 0.1 + 0.035 * (1.0 - v * v);
            const double d = a - ga;
            const double line = Smoothstep(pxLen * 0.9, 0.0, std::fabs(d)) *
                                (std::fabs(v) < 0.75 ? 1.0 : 0.0) *
                                (std::fabs(ang) < 1.9 ? 1.0 : 0.0);
            const double behind = Smoothstep(0.0, 0.02, d) * Smoothstep(0.05, 0.02, d) *
                                  (std::fabs(v) < 0.75 ? 1.0 : 0.0);
            base = Lerp(base, base * 0.45, line * 0.8);
            base = base * (1.0 + 0.1 * behind);
        } else if (g_.gillSlits > 1 && g_.kind != SeaKind::Shark) {
            // A shark's five slits are nearly invisible at 128 px; in the port
            // the body between them quantized into bright bars over the snout.
            const double gap = Max(0.02, 1.6 * pxLen);
            for (int i = 0; i < g_.gillSlits; i++) {
                if (g_.kind == SeaKind::Ray) continue;
                const double ga = g_.eyePos + 0.09 + i * gap + 0.012 * (1.0 - v * v);
                const double d = a - ga;
                const double slit = Smoothstep(pxLen * 0.8, 0.0, std::fabs(d)) *
                                    (v > -0.5 ? 1.0 : 0.0) * (v < 0.3 + 0.03 * i ? 1.0 : 0.0) *
                                    (std::fabs(ang) < 1.6 ? 1.0 : 0.0);
                const double lip = Smoothstep(pxLen * 1.6, pxLen * 0.8, std::fabs(d - pxLen)) *
                                   (v > -0.5 ? 1.0 : 0.0) * (v < 0.3 ? 1.0 : 0.0);
                base = Lerp(base, base * 0.35, slit * 0.85);
                base = base * (1.0 + 0.12 * lip);
            }
        }
        if (g_.kind == SeaKind::Whale) {
            const double ba = g_.beak != 0.0 ? g_.eyePos * 0.6 + 0.05 : g_.eyePos + 0.03;
            const double hole = Smoothstep(0.012 + pxLen * 0.5, 0.004,
                                           std::hypot(a - ba, std::fabs(ang - kPi / 2.0) * 0.02));
            base = Lerp(base, base * 0.3, hole);
        }
        if (g_.pleats) {
            const double groove = (std::fmod(v * 14.0, 1.0) < 0.25 ? 1.0 : 0.0) * (a > 0.03 ? 1.0 : 0.0) *
                                  (a < 0.48 ? 1.0 : 0.0) * (bellyT > 0.5 ? 1.0 : 0.0) *
                                  Smoothstep(0.5, 0.4, a);
            base = Lerp(base, base * 0.6, groove * Smoothstep(1.0, 2.5, 0.07 / pxLen));
        }
        // mouth: a dark bite on the front underside; teeth are pale chips
    } else if (IsWingPart(part)) {
        // render.py's _wing_albedo(): wings are opaque skin, not fin membrane.
        base = WingAlbedo(at, n.z >= 0.0, pxLen);
        alpha = 1.0;
        finTransmit = (g_.kind == SeaKind::Squid || g_.kind == SeaKind::Octopus) ? 0.25 : 0.08;
        skipSilver = true;
    } else if (fin) {
        // render.py's _shade_fins(): translucent membranes with bony rays and a
        // milky root, or opaque skin fins on sharks and whales.
        const bool skinFins = g_.kind == SeaKind::Shark || g_.kind == SeaKind::Whale ||
                              g_.finAlpha >= 0.98;
        const double m = at.x, r = at.y, fa = at.z;
        const double seed = (double)(g_.seed % 997);
        const Vec3 cBase = Rgb(palette_.At(SeaSlotBase));
        const Vec3 cFin = Rgb(palette_.At(SeaSlotFin));
        const Vec3 cFinDark = Rgb(palette_.At(SeaSlotFinDark));
        const Vec3 cBelly = Rgb(palette_.At(SeaSlotBelly));
        const Vec3 cAccent = Rgb(palette_.At(SeaSlotAccent));
        const bool paired = part == PartPectoralNear || part == PartPectoralFar ||
                            part == PartPelvicNear || part == PartPelvicFar;
        if (skinFins) {
            base = cBase;
            if (!g_.colors.tailBodyColor && !g_.colors.finColor.empty()) base = cFin;
            const Vec3 up = CameraDir(Vec3(0, 1, 0), pitch);
            if (g_.colors.bellyLightness > 0.25 && paired && Dot(n, up) < 0)
                base = Lerp(cBelly, base, 0.3);
            if (!g_.colors.finColor.empty() && g_.colors.tailBodyColor && part != PartCaudal)
                base = Lerp(base, cFin, 0.5);
            base = base * (1.0 + 0.06 * Fbm(fa * 40.0, r * 6.0, seed, 3));
            base = base * (1.0 - 0.22 * Smoothstep(0.55, 1.0, r) -
                           0.1 * Smoothstep(0.6, 1.0, m));
            alpha = 1.0;
        } else {
            base = cFin;
            if (g_.colors.tailBodyColor && part == PartCaudal) base = cBase;
            else if (!g_.colors.tailColor.empty() && part == PartCaudal)
                base = Rgb(RgbaFromHex(g_.colors.tailColor, palette_.At(SeaSlotFin)));
            const double count = part == PartCaudal ? 18.0 : (paired ? 10.0 : 16.0);
            const double span = part == PartCaudal ? g_.tailSpread * g_.height : 0.2;
            const double countClamped = Max(4.0, Min(count, span / (2.2 * pxLen)));
            const double ph = Frac(m * countClamped);
            double ray = Smoothstep(0.6, 0.9, std::fabs(ph - 0.5) * 2.0) *
                         Smoothstep(0.02, 0.15, r);
            ray *= Smoothstep(1.2, 3.0, (span / countClamped) / pxLen);
            base = Lerp(base, base * 0.62, ray * 0.6);
            base = Lerp(base, base * 0.75, Smoothstep(0.25, 0.0, r) * 0.5);
            const double fam = (part == PartCaudal && g_.hasTailAlpha) ? g_.tailAlpha : g_.finAlpha;
            double a = fam * (0.85 + 0.15 * Smoothstep(0.4, 0.0, r)) +
                       (1.0 - fam) * ray * 0.6;
            a *= 1.0 - 0.35 * Smoothstep(0.75, 1.0, r);
            alpha = Clamp(a, 0.15, 1.0);
            if (g_.colors.patternOnFins) {
                if (g_.colors.pattern == PatternKind::Stripes) {
                    const double cnt = 3.0 + std::floor(g_.colors.patternScale * 4.0);
                    const double ph2 = Frac((fa - g_.colors.patternOffset) * cnt);
                    const double stripe = Smoothstep(0.34, 0.3, ph2) *
                        ((part == PartDorsal || part == PartAnal || part == PartCaudal) ? 1.0 : 0.0);
                    base = Lerp(base, cAccent, stripe);
                    alpha = Max(alpha, stripe * 0.95);
                } else if (g_.colors.pattern == PatternKind::Spots ||
                           g_.colors.pattern == PatternKind::Trout ||
                           g_.colors.pattern == PatternKind::Salmon ||
                           g_.colors.pattern == PatternKind::Koi ||
                           g_.colors.pattern == PatternKind::Calico) {
                    const double sp = Spots(fa * 1.5, m * 0.3 + r * 0.25, 0.05, false, 0.5, pxLen);
                    base = Lerp(base, cAccent, sp);
                    alpha = Max(alpha, sp * 0.9);
                }
            }
            if (g_.colors.finTips > 0) {
                const double tip = Smoothstep(1.0 - g_.colors.finTips - 0.04,
                                              1.0 - g_.colors.finTips + 0.04, r);
                base = Lerp(base, cAccent, tip);
                alpha = Max(alpha, tip);
            }
            if (!g_.colors.finEdgeColor.empty()) {
                const double edge = Smoothstep(0.84, 0.94, r);
                const Vec3 cEdge = Rgb(RgbaFromHex(g_.colors.finEdgeColor, palette_.At(SeaSlotFinDark)));
                base = Lerp(base, cEdge, edge);
                alpha = Max(alpha, edge * 0.95);
            }
            finTransmit = 0.9 * (1.0 - g_.finAlpha) + 0.3;
            finWrap = 0.5;
            glossScale = 0.5;
        }
        if (part == PartPectoralFar || part == PartPelvicFar) base = base * 0.8;
        skipSilver = true;
    } else if (part == PartArm) {
        // render.py's _body_albedo() arm tail: the same countershading/mottling/
        // pattern as the body, a pale inner (sucker) side, and the sucker rows.
        // The old shortcut painted at.w > 0.35 with the red pattern accent.
        const Vec3 cBase = Rgb(palette_.At(SeaSlotBase));
        const Vec3 cBelly = Rgb(palette_.At(SeaSlotBelly));
        const double inner = at.w;
        base = cBase * (1.0 + 0.18 * (0.15 - Clamp(at.y, -1.0, 1.0)));
        double u1, u2, ang;
        SurfaceCoords(at, u1, u2, ang);
        const double seed = (double)(g_.seed % 997);
        base = base * (1.0 + 0.12 * Fbm(u1 * 40.0, u2 * 40.0, seed + 3.0, 3));
        base = Pattern(part, at, base, false, pxLen);
        base = Lerp(base, cBelly, Smoothstep(0.1, 0.6, inner) * 0.7);
        const double per = 0.022;
        const double ua = at.x / per;
        const double fu = ua - std::floor(ua) - 0.5;
        const double row = std::fmod(std::floor(ua), 2.0) * 0.5;
        const double fw = Frac(std::atan2(at.y, at.z) * 1.2 + row) - 0.5;
        const double suckFade = Smoothstep(1.0, 2.5, per / Max(pxLen, 1e-9));
        const double suck = Smoothstep(0.2, 0.1, fu * fu + fw * fw) *
                            Smoothstep(0.35, 0.7, inner) * suckFade;
        const double ring = Smoothstep(0.03, 0.0, std::fabs(fu * fu + fw * fw - 0.16)) *
                            Smoothstep(0.35, 0.7, inner);
        base = Lerp(base, Vec3(0.85, 0.72, 0.68), suck * 0.8);
        base = Lerp(base, base * 0.6, ring * 0.5 * suckFade);
    } else if (part == PartLobe) {
        // render.py's _shade_solid() LOBE: dark skin, paler on the inner face/tip.
        const Vec3 cBase = Rgb(palette_.At(SeaSlotBase));
        const Vec3 cBelly = Rgb(palette_.At(SeaSlotBelly));
        base = Lerp(cBase, cBase * 0.5 + cBelly * 0.5, Smoothstep(0.3, 1.0, at.y) * 0.6);
        base = base * (1.0 + 0.08 * Fbm(at.x * 60.0, at.w * 4.0, 1.0, 2));
    } else {
        base = Rgb(palette_.At(SeaSlotBase));
    }

    n = n.NormalizedOr(Vec3(0, 0, 1));
    if (n.z < 0) n = -n;   // two-sided: flat fins and wings are single sheets

    // The reference shades *linear* albedo with a warm sun and a cool hemisphere;
    // multiplying sRGB by a scalar (what this used to do) cannot turn a shadow
    // blue, and it flattens the top-to-bottom falloff.
    Vec3 albedo(SrgbToLinear(base.x), SrgbToLinear(base.y), SrgbToLinear(base.z));

    // Silver fish throw the sky and the sea floor back at the viewer.
    if (!skipSilver && g_.silver > 0) {
        const double fres = std::pow(1.0 - Saturate(std::fabs(n.z)), 3.0) * g_.silver;
        const Vec3 sky(0.66, 0.78, 0.92), sea(0.16, 0.28, 0.30);
        albedo = Lerp(albedo, Lerp(sea, sky, n.y * 0.5 + 0.5), fres);
    }
    if (g_.knobs > 0) {
        const Vec3 hl = Rgb(palette_.At(SeaSlotHighlight));
        albedo = Lerp(albedo, Vec3(SrgbToLinear(hl.x), SrgbToLinear(hl.y), SrgbToLinear(hl.z)),
                      g_.knobs * 0.18);
    }
    // Orca white is paint, not pale skin: keep it bright where the body is in shadow.
    if (g_.colors.pattern == PatternKind::Orca && !fin) {
        const double mark = Saturate((Min(base.x, Min(base.y, base.z)) - 0.35) / 0.4);
        albedo = Lerp(albedo, Vec3(SrgbToLinear(base.x), SrgbToLinear(base.y), SrgbToLinear(base.z)),
                      mark * 0.70);
    }

    const Vec3 L = CameraDir(kSunWorld, pitch);
    const Vec3 up = CameraDir(Vec3(0, 1, 0), pitch);
    const double wrap = finWrap >= 0.0 ? finWrap : WrapFor(g_.kind);
    const double ndl = Dot(n, L);
    const double diff = Saturate((ndl + wrap) / (1.0 + wrap)) * sunLit;
    const double hemi = 0.5 + 0.5 * Dot(n, up);
    const Vec3 ambient = Lerp(kDeep, kSky, hemi) * (1.0 - 0.75 * ao);

    Vec3 col = Mul(albedo, kSun * diff + ambient);

    // Wet sheen: Blinn, with the reference's per-kind exponent.
    const Vec3 H = (L + Vec3(0, 0, 1)).NormalizedOr();
    const double gloss = glossValue >= 0.0 ? glossValue : g_.gloss * glossScale;
    const double spec = std::pow(Saturate(Dot(n, H)), ShininessFor(g_.kind)) * gloss;
    col = col + kSun * (spec * 0.9);

    // The mouth is shaded on the lit colour, the way the reference does it --
    // painting it into the albedo first turned it into a dark wedge.
    if (g_.mouth > 0 && !model_.HasJaw() && !fin && !IsWingPart(part) && part != PartLobe) {
        col = MouthShade(col, at, pxLen);
    }

    // A cool rim of scattered water light around the silhouette.
    const double fres = std::pow(1.0 - Saturate(std::fabs(n.z)), 4.0);
    col = col + kRimColor * (fres * 0.22 * (fin ? 0.5 : 1.0) * (0.4 + 0.6 * hemi));
    if (finTransmit > 0)
        col = col + Mul(albedo, kSun) * (Saturate(-ndl) * finTransmit);

    // The reference tone maps the whole linear buffer before it encodes to sRGB:
    // a soft shoulder above 0.8 so the specular band and the eye's catch-light
    // roll off instead of clipping flat. Without it those highlights are both
    // brighter and wider than the original's.
    return Vec3(LinearToSrgb(ToneMap(col.x)), LinearToSrgb(ToneMap(col.y)),
                LinearToSrgb(ToneMap(col.z)));
}

// ---------------------------------------------------------------- mouth
// render.py's _mouth(): the lip line of a closed mouth, or the gape and the two
// rows of interlocking teeth of an open one. `a` runs along the body, `v` is the
// height across the section, and pxLen is one output pixel in body lengths.
Vec3 SeaRenderer::MouthShade(Vec3 col, const Vec4& at, double pxLen) const {
    const double a = at.x, v = at.y;
    double a0 = Max(0.012, g_.eyePos - (g_.teeth ? 0.03 : 0.07));
    if (g_.kind == SeaKind::Fish) a0 = 0.0;
    const double a1 = a0 + g_.mouth;
    const double t = Saturate((a - a0) / Max(g_.mouth, 1e-6));
    const double mv = g_.mouthHeight + (g_.teeth ? 0.22 : 0.08) * std::pow(t, 1.3);
    const double dv = (v - mv) * g_.height * 0.5;   // in body lengths
    const double ang = std::atan2(v, at.z);
    const bool inside = a >= a0 - pxLen && a <= a1 && std::fabs(ang) < 1.7;
    const double width = pxLen * 0.7 + 0.003;

    if (!g_.teeth) {
        const double line = inside ? Smoothstep(width, width * 0.3, std::fabs(dv)) : 0.0;
        return Lerp(col, col * 0.25, line);
    }

    const double gape = 0.022 * (1.0 - t * t) * Min(1.0, g_.mouth / 0.1) + pxLen;
    if (inside && dv < gape * 0.5 && dv > -gape * 0.5) col = Vec3(0.06, 0.02, 0.02);
    // Interlocking triangular teeth along both jaws.
    const double per = Max(0.008, 1.5 * pxLen);
    const double ph = Frac(a / per);
    const double tri = std::fabs(ph - 0.5) * 2.0;
    const bool upper = inside && dv > gape * 0.5 - (1.0 - tri) * gape * 0.55 && dv < gape * 0.5;
    const bool lower = inside && dv < -gape * 0.5 + tri * gape * 0.45 && dv > -gape * 0.5;
    if ((upper || lower) && t < 0.92) col = Vec3(0.85, 0.82, 0.75);
    const double lip = inside ? (std::fabs(std::fabs(dv) - gape * 0.5) < width ? 1.0 : 0.0) : 0.0;
    return Lerp(col, col * 0.5, lip * 0.6);
}

// ---------------------------------------------------------------- overlays
void SeaRenderer::DrawEyes(const SeaPosed& posed, const SeaRaster& raster, const SeaPose& pose,
                           double scale, double cx, double cy, int ss,
                           std::vector<double>& depth, std::vector<float>& rgb,
                           std::vector<float>& alpha) const {
    if (g_.eyeStyle == EyeStyle::None) return;
    const int w = raster.width, h = raster.height;

    // render.py's c_iris: an explicit iris colour, else the cephalopod tan, the
    // dark-eye near-black, or the default warm tan. SeaSlotEye is only the
    // fallback for a malformed hex string; it is not the iris colour itself.
    // render.py keeps its whole buffer in linear light and only encodes to sRGB
    // once, at the very end, so every overlay above it -- this one included --
    // does its arithmetic on linear values. Multiplying or adding in sRGB is not
    // the same operation: the iris ramp collapses and the catch-light, which is
    // an additive 1.2 against a near-black blue channel, cannot reach white.
    // So the crop under each eye is converted to linear here and back at the end.
    auto toLinear = [](Vec3 c) {
        return Vec3(SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z));
    };
    Vec3 irisBase;
    if (!g_.irisColor.empty()) {
        irisBase = toLinear(Rgb(RgbaFromHex(g_.irisColor, palette_.At(SeaSlotEye))));
    } else if (g_.kind == SeaKind::Squid || g_.kind == SeaKind::Octopus) {
        irisBase = toLinear(Vec3(205 / 255.0, 170 / 255.0, 90 / 255.0));
    } else if (g_.eyeStyle == EyeStyle::Dark) {
        irisBase = toLinear(Vec3(38 / 255.0, 36 / 255.0, 32 / 255.0));
    } else {
        irisBase = toLinear(Vec3(196 / 255.0, 170 / 255.0, 112 / 255.0));
    }

    const bool ceph = g_.kind == SeaKind::Squid || g_.kind == SeaKind::Octopus;
    const double fwd = std::cos(pose.yaw);
    // Already linear in the reference, so they are used unchanged.
    const Vec3 pupilColor(0.004, 0.006, 0.01);
    const Vec3 catchColor(1.2, 1.2, 1.15);
    const Vec3 envTop(0.95, 1.0, 1.0);

    // Which pixels have already been moved into linear light. The two eyes of a
    // fish facing the camera project onto nearly the same disc, so the second
    // eye must not convert the first eye's finished result a second time.
    std::vector<unsigned char> linearised((size_t)w * (size_t)h, 0);

    for (const SeaPosedEye& eye : posed.eyes) {
        // The reference hides an eye only when its axis points strongly away.
        if (eye.normal.z < -0.15) continue;

        double sx, sy;
        Project(eye.center, scale, cx, cy, sx, sy);
        double r = eye.radius * scale;
        if (g_.eyeStyle == EyeStyle::Dark) r *= 0.75;
        r = Max(r, 0.9 * (double)ss);

        const int ix = (int)std::floor(sx), iy = (int)std::floor(sy);
        if (ix < 0 || ix >= w || iy < 0 || iy >= h) continue;
        // This rasterizer stores camera-space z, while the reference stores
        // pixel-scaled z, so convert its pixel fudge back to body lengths.
        const double zEps = Max(1.5, r * 0.8) / Max(scale, 1e-9);
        if (depth[(size_t)iy * w + ix] > eye.center.z + zEps) continue;

        const int R = (int)std::ceil(r * 1.5) + 1;
        const int x0 = ClampI(ix - R, 0, w), x1 = ClampI(ix + R + 1, 0, w);
        const int y0 = ClampI(iy - R, 0, h), y1 = ClampI(iy + R + 1, 0, h);

        // Move this crop into unpremultiplied linear light. `rgb` arrives
        // premultiplied and sRGB-encoded, exactly as Shade() leaves it.
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                const size_t idx = (size_t)y * w + x;
                if (linearised[idx]) continue;
                linearised[idx] = 1;
                const double a = (double)alpha[idx];
                if (a <= 1e-6) continue;
                for (int c = 0; c < 3; c++) {
                    rgb[idx * 3 + c] =
                        (float)UnToneMap(SrgbToLinear((double)rgb[idx * 3 + c] / a));
                }
            }
        }

        // Foreshorten the disc when the eye turns away from us: rotate the
        // screen offset into the eye's own frame and squash the along-axis part.
        const double axyX = eye.normal.x, axyY = -eye.normal.y;
        const double nrm = std::hypot(axyX, axyY);
        const double squash = Max(0.2, eye.normal.z);

        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                const double dx = x + 0.5 - sx, dy = y + 0.5 - sy;
                double lx, ly;
                if (nrm > 1e-3) {
                    const double ux = axyX / nrm, uy = axyY / nrm;
                    const double du = (dx * ux + dy * uy) / squash;
                    const double dv = -dx * uy + dy * ux;
                    lx = du * ux - dv * uy;
                    ly = du * uy + dv * ux;
                } else {
                    lx = dx;
                    ly = dy;
                }
                const double rho = std::hypot(lx, ly) / r;
                const size_t idx = (size_t)y * w + x;
                const bool on = alpha[idx] > 0.0f;

                // Socket: a soft dark ring around the eye, heavier above.
                if (on) {
                    const double sock = Smoothstep(1.35, 1.0, rho) *
                                        Smoothstep(0.85, 1.0, rho);
                    if (sock > 0.0) {
                        const double f = 1.0 - 0.35 * sock *
                                                   (0.6 + 0.4 * Smoothstep(0.5, -1.0, dy / r));
                        rgb[idx * 3 + 0] = (float)(rgb[idx * 3 + 0] * f);
                        rgb[idx * 3 + 1] = (float)(rgb[idx * 3 + 1] * f);
                        rgb[idx * 3 + 2] = (float)(rgb[idx * 3 + 2] * f);
                    }
                }

                const double nx = lx / r, ny = ly / r;   // -1..1 across the eye, y down
                double edge = Smoothstep(1.0, 0.85, rho);

                // A glowering brow: a lid slanting down toward the snout hides
                // the top of the eye, and a soft shadow darkens the skin above.
                if (g_.scariness > 0.0) {
                    const double sc = g_.scariness;
                    const double front =
                        nx * (fwd >= 0.0 ? 1.0 : -1.0) * Min(1.0, std::fabs(fwd) * 3.0);
                    const double lid = -1.0 + 0.6 * sc + 0.4 * sc * front;
                    edge *= Smoothstep(lid - 0.05, lid + 0.15, ny);
                    const double brow = Smoothstep(0.35, 0.0, std::fabs(ny - lid + 0.1)) *
                                        (rho < 1.3 ? 1.0 : 0.0) * sc;
                    if (on && brow > 0.0) {
                        const double f = 1.0 - 0.5 * brow;
                        rgb[idx * 3 + 0] = (float)(rgb[idx * 3 + 0] * f);
                        rgb[idx * 3 + 1] = (float)(rgb[idx * 3 + 1] * f);
                        rgb[idx * 3 + 2] = (float)(rgb[idx * 3 + 2] * f);
                    }
                }

                if (!on || rho >= 1.0) continue;

                // Iris: dark limbal ring, brighter collar, fine radial fibres,
                // then a sphere term (lit above, shadowed underneath and by the brow).
                const double irisRamp = 0.35 + 0.65 * Smoothstep(1.0, 0.7, rho);
                const double collar =
                    1.0 + 0.35 * Smoothstep(0.75, 0.55, rho) * Smoothstep(0.45, 0.6, rho);
                const double fibres =
                    1.0 + 0.12 * ValueNoise(std::atan2(ny, nx) * 9.0, rho * 3.0,
                                            (double)(g_.seed % 97));
                const double sph = std::sqrt(Saturate(1.0 - rho * rho));
                const double sphere = 0.55 + 0.45 * sph * Smoothstep(1.0, -0.6, ny);
                Vec3 col = irisBase * (irisRamp * collar * fibres * sphere);

                // Pupil: round, a squid's wide circle, an octopus's horizontal
                // slit, or the larger dark-eye pupil. It looks slightly forward.
                const double pxo = nx - 0.05 * fwd;
                double pup;
                if (ceph) {
                    if (g_.kind == SeaKind::Squid || g_.arms.tentacles > 0) {
                        pup = Smoothstep(0.52, 0.45, std::hypot(pxo, ny));
                    } else {
                        pup = Smoothstep(0.22, 0.16, std::fabs(ny) + 0.25 * pxo * pxo) *
                              (std::fabs(pxo) < 0.7 ? 1.0 : 0.0);
                    }
                } else if (g_.eyeStyle == EyeStyle::Dark) {
                    pup = Smoothstep(0.72, 0.62, std::hypot(pxo, ny));
                } else {
                    pup = Smoothstep(0.58, 0.5, std::hypot(pxo, ny));
                }
                col = Lerp(col, pupilColor, pup);

                // Wet cornea: a white catch-light toward the sun, a fainter
                // second glint, and the bright water mirrored in the upper half.
                const double hl = Smoothstep(0.17, 0.07, std::hypot(nx + 0.3, ny + 0.36));
                const double hl2 =
                    Smoothstep(0.1, 0.04, std::hypot(nx - 0.32, ny - 0.38)) * 0.3;
                col = col + catchColor * (hl + hl2);
                col = col + envTop * (0.06 * Smoothstep(0.2, -0.9, ny) * sph);

                // The buffer under this pixel is now unpremultiplied linear, so
                // the reference's straight _mix(region, col, edge) applies as is.
                rgb[idx * 3 + 0] = (float)Lerp(rgb[idx * 3 + 0], col.x, edge);
                rgb[idx * 3 + 1] = (float)Lerp(rgb[idx * 3 + 1], col.y, edge);
                rgb[idx * 3 + 2] = (float)Lerp(rgb[idx * 3 + 2], col.z, edge);
            }
        }
    }

    // Back to the buffer's own space: unpremultiplied linear -> sRGB, then
    // premultiplied by coverage the way the rest of the pipeline expects.
    for (size_t idx = 0; idx < linearised.size(); idx++) {
        if (!linearised[idx]) continue;
        const double a = (double)alpha[idx];
        if (a <= 1e-6) continue;
        for (int c = 0; c < 3; c++) {
            rgb[idx * 3 + c] = (float)(Saturate(LinearToSrgb(ToneMap((double)rgb[idx * 3 + c]))) * a);
        }
    }
}

void SeaRenderer::DrawSpines(const SeaPosed& posed, const SeaRaster& raster, double scale, double cx,
                             double cy, double pitch, std::vector<double>& depth,
                             std::vector<float>& rgb, std::vector<float>& alpha) const {
    if (posed.spineLines.size() < 2) return;
    const int w = raster.width, h = raster.height;
    const double ss = Max(1.0, scale / Max(fitScale_, 1e-9));
    // render.py's _spine_radius(): base radius in body lengths.
    double baseR;
    if (g_.puffSpines > 0) {
        baseR = 0.012;
    } else {
        baseR = Clamp(0.55 / std::sqrt((double)Max(g_.spines.count, 1)) *
                          (0.5 + g_.spines.length), 0.008, 0.045);
    }
    const double taper = g_.puffSpines > 0 ? 0.85 : g_.spines.taper;
    const Vec3 sunC = CameraDir(kSunWorld, pitch);
    const Vec3 finSrgb = Rgb(palette_.At(SeaSlotFin));
    const Vec3 finLin(SrgbToLinear(finSrgb.x), SrgbToLinear(finSrgb.y), SrgbToLinear(finSrgb.z));

    const size_t pxCount = (size_t)w * (size_t)h;
    std::vector<double> winZ(pxCount, kEmptyDepth);
    std::vector<double> winT(pxCount, 0.0);
    std::vector<int> winK(pxCount, -1), winOx(pxCount, 0), winOy(pxCount, 0);

    const int spineCount = (int)posed.spineLines.size() / 2;
    std::vector<double> cylV((size_t)spineCount, 0.0), towardV((size_t)spineCount, 0.0),
        ballV((size_t)spineCount, 0.0);

    for (int k = 0; k < spineCount; k++) {
        const Vec3 p0 = posed.spineLines[(size_t)k * 2 + 0];
        const Vec3 p1 = posed.spineLines[(size_t)k * 2 + 1];
        const double x0 = cx + p0.x * scale, y0 = cy - p0.y * scale, z0 = p0.z * scale;
        const double x1 = cx + p1.x * scale, y1 = cy - p1.y * scale, z1 = p1.z * scale;
        const Vec3 d3 = (p1 - p0).NormalizedOr(Vec3(0, 1, 0));
        const double dl = Dot(d3, sunC);
        const double cyl = std::sqrt(Saturate(1.0 - dl * dl));
        const double toward = Clamp(d3.z, -1.0, 1.0);
        const double ball = 0.5 + 0.5 * Dot(d3, sunC);
        cylV[(size_t)k] = cyl;
        towardV[(size_t)k] = toward;
        ballV[(size_t)k] = ball;
        const double span = Max(std::fabs(x1 - x0), std::fabs(y1 - y0));
        const int steps = (int)Clamp(std::round(span * 1.2), 4.0, 400.0);
        double rmax = 0.5;
        std::vector<double> rad((size_t)steps + 1);
        for (int st = 0; st <= steps; st++) {
            const double t = steps > 0 ? (double)st / (double)steps : 1.0;
            rad[(size_t)st] = Max(baseR * scale, 0.8 * ss) *
                              std::pow(Max(1.0 - taper * t, 0.05), 0.9);
            rmax = Max(rmax, rad[(size_t)st]);
        }
        const int R = (int)std::ceil(rmax);
        for (int st = 0; st <= steps; st++) {
            const double t = steps > 0 ? (double)st / (double)steps : 1.0;
            const double px = Lerp(x0, x1, t), py = Lerp(y0, y1, t);
            const double pz = Lerp(z0, z1, t);
            const double rr = rad[(size_t)st];
            const double rr2 = Max(rr, 0.5) * Max(rr, 0.5);
            for (int oy = -R; oy <= R; oy++) {
                for (int ox = -R; ox <= R; ox++) {
                    const double dist2 = (double)ox * ox + (double)oy * oy;
                    if (dist2 > rr2) continue;
                    const int ix = (int)std::floor(px + ox);
                    const int iy = (int)std::floor(py + oy);
                    if (ix < 0 || iy < 0 || ix >= w || iy >= h) continue;
                    const size_t idx = (size_t)iy * w + ix;
                    const double zPix = pz + std::sqrt(Max(rr * rr - dist2, 0.0));
                    const double zBody = zPix / Max(scale, 1e-9);
                    if (!(zBody > depth[idx] - 0.5 / Max(scale, 1e-9) || alpha[idx] == 0.0f))
                        continue;
                    if (zBody <= winZ[idx]) continue;
                    winZ[idx] = zBody;
                    winT[idx] = t;
                    winK[idx] = k;
                    winOx[idx] = ox;
                    winOy[idx] = oy;
                }
            }
        }
    }

    for (size_t idx = 0; idx < pxCount; idx++) {
        if (winK[idx] < 0) continue;
        const int k = winK[idx];
        const double tt = winT[idx];
        const double rr = Max(baseR * scale, 0.8 * ss) *
                          std::pow(Max(1.0 - taper * tt, 0.05), 0.9);
        const double across = (double)-winOy[idx] / Max(rr, 0.5);
        const double side = 0.75 + 0.25 * Clamp(across, -1.0, 1.0);
        const double root = 0.2 + 0.8 * Smoothstep(0.0, 0.6, tt);
        const double lit = ballV[(size_t)k] * ballV[(size_t)k];
        const double light = root * (0.12 + 0.88 * lit * cylV[(size_t)k] * side) +
                             0.12 * Clamp(towardV[(size_t)k], 0.0, 1.0);
        const Vec3 col = finLin * (0.8 + 0.4 * tt);
        Vec3 shade(col.x * (kSun.x * light * 0.9 + kSky.x * (0.25 + 0.35 * ballV[(size_t)k])),
                   col.y * (kSun.y * light * 0.9 + kSky.y * (0.25 + 0.35 * ballV[(size_t)k])),
                   col.z * (kSun.z * light * 0.9 + kSky.z * (0.25 + 0.35 * ballV[(size_t)k])));
        const double spec = std::pow(cylV[(size_t)k], 30.0) * g_.gloss * 0.6 * lit * root;
        shade = shade + Vec3(spec, spec, spec);
        rgb[idx * 3 + 0] = (float)LinearToSrgb(ToneMap(shade.x));
        rgb[idx * 3 + 1] = (float)LinearToSrgb(ToneMap(shade.y));
        rgb[idx * 3 + 2] = (float)LinearToSrgb(ToneMap(shade.z));
        alpha[idx] = 1.0f;
        depth[idx] = winZ[idx];
    }
}

// ---------------------------------------------------------------- render
void SeaRenderer::Render(const SeaPose& poseIn, SeaRenderResult& out) const {
    const SeaPose pose = poseIn;
    posePhase_ = pose.phase;

    SeaPosed posed;
    model_.Pose(pose, posed);
    if (posed.cam.empty() || posed.normals.size() != posed.cam.size()) {
        out.width = size_;
        out.height = size_;
        out.rgba.assign((size_t)size_ * size_ * 4, 0);
        out.part.assign((size_t)size_ * size_, 0);
        out.alpha.assign((size_t)size_ * size_, 0);
        return;
    }

    // render.py's _supersample(), matched exactly. The old thresholds dropped to
    // 2x at 128 px where the reference still uses 4x, which softened the
    // silhouette by a pixel and averaged the eye's one-pixel catch-light away.
    const int ss = size_ <= 128 ? 4 : (size_ <= 320 ? 3 : (size_ <= 900 ? 2 : 1));
    const int rw = size_ * ss, rh = size_ * ss;

    // One scale and one centre for every frame. Without this, a pose that turns
    // toward the camera gets refitted and zooms up instead of foreshortening.
    const double scale = fitScale_ * ss;
    const double cx = rw * 0.5;
    const double cy = rh * 0.5;

    SeaRaster raster;
    raster.Reset(rw, rh);
    Rasterize(model_.Mesh(), posed, scale, cx, cy, raster);

    // render.py _shadow_map(): depth of the solid animal seen from the sun.
    const Vec3 sunC = CameraDir(kSunWorld, pose.pitch);
    Vec3 lu = Cross(Vec3(0, 0, 1), sunC).NormalizedOr(Vec3(1, 0, 0));
    const Vec3 lv = Cross(sunC, lu);
    auto toLight = [&](const Vec3& p) {
        return Vec3(Dot(lu, p), Dot(lv, p), Dot(sunC, p));
    };
    double loX = 1e30, loY = 1e30, hiX = -1e30, hiY = -1e30;
    for (const Vec3& c : posed.cam) {
        const Vec3 lp = toLight(c) * scale;
        loX = Min(loX, lp.x - 2.0);
        loY = Min(loY, lp.y - 2.0);
        hiX = Max(hiX, lp.x + 2.0);
        hiY = Max(hiY, lp.y + 2.0);
    }
    const double shRes = ss <= 2 ? 1.0 : 0.5;
    const int Wl = (int)std::ceil((hiX - loX) * shRes) + 1;
    const int Hl = (int)std::ceil((hiY - loY) * shRes) + 1;
    SeaRaster shadowRaster;
    shadowRaster.Reset(Wl, Hl);
    SeaPosed lightPosed = posed;
    for (size_t i = 0; i < posed.cam.size(); i++) {
        const Vec3 lp = toLight(posed.cam[i]) * scale;
        // Rasterize always flips y; pre-flip so the light map keeps world-up
        // rows, matching the lookup below (pixel.py's rasterize does not flip).
        lightPosed.cam[i] = Vec3((lp.x - loX) * shRes, -(lp.y - loY) * shRes, lp.z * shRes);
    }
    // render.py's _shadow_map(): fins do not cast, so their roots do not
    // shadow the torso.
    SeaMesh shadowMesh = model_.Mesh();
    {
        std::vector<int> tris, parts;
        const size_t tc = shadowMesh.tris.size() / 3;
        for (size_t t = 0; t < tc; t++) {
            const int tp = shadowMesh.triPart[t];
            if (IsFinPart(tp)) continue;
            tris.push_back(shadowMesh.tris[t * 3 + 0]);
            tris.push_back(shadowMesh.tris[t * 3 + 1]);
            tris.push_back(shadowMesh.tris[t * 3 + 2]);
            parts.push_back(tp);
        }
        shadowMesh.tris.swap(tris);
        shadowMesh.triPart.swap(parts);
    }
    Rasterize(shadowMesh, lightPosed, 1.0, 0.0, 0.0, shadowRaster);
    const double soft = Max(1.0, 0.012 * scale * shRes);

    // render.py _ssao(): darken where nearer surfaces crowd around a pixel.
    std::vector<float> ao((size_t)rw * rh, 0.0f);
    std::vector<float> depthPx((size_t)rw * rh, 0.0f);
    for (int i = 0; i < rw * rh; i++) depthPx[(size_t)i] = (float)(raster.depth[(size_t)i] * scale);
    for (int y = 1; y < rh - 1; y++) {
        for (int x = 1; x < rw - 1; x++) {
            const size_t i = (size_t)y * rw + x;
            if (raster.tri[i] < 0) continue;
            double occ = 0.0;
            int n = 0;
            const double radii[2] = {Max(2.0, 0.02 * scale), Max(4.0, 0.05 * scale)};
            for (int ri = 0; ri < 2; ri++) {
                const double rad = radii[ri];
                for (int k = 0; k < 8; k++) {
                    const double ang = k * kPi / 4.0 + rad;
                    const int dx = (int)std::lround(std::cos(ang) * rad);
                    const int dy = (int)std::lround(std::sin(ang) * rad);
                    const int nx = ClampI(x + dx, 0, rw - 1);
                    const int ny = ClampI(y + dy, 0, rh - 1);
                    const size_t j = (size_t)ny * rw + nx;
                    const double nd = raster.tri[j] < 0 ? kEmptyDepth : (double)depthPx[j];
                    const double diff = nd - (double)depthPx[i];
                    occ += Clamp((diff - 0.3 * rad) / (1.5 * rad), 0.0, 1.0) *
                           (diff < 6.0 * rad ? 1.0 : 0.0);
                    n++;
                }
            }
            ao[i] = (float)(occ / n);
        }
    }

    std::vector<float> rgb((size_t)rw * rh * 3, 0.0f);
    std::vector<float> cov((size_t)rw * rh, 0.0f);
    std::vector<unsigned char> part((size_t)rw * rh, 0);

    for (int y = 0; y < rh; y++) {
        for (int x = 0; x < rw; x++) {
            const size_t idx = (size_t)y * rw + x;
            const int t = raster.tri[idx];
            if (t < 0) continue;
            if (ss == 1) {
                // render.py's _cleanup(): drop a lone part pixel with no 4-neighbour.
                bool any = false;
                const int nx4[4] = {x - 1, x + 1, x, x};
                const int ny4[4] = {y, y, y - 1, y + 1};
                for (int d = 0; d < 4; d++) {
                    if (nx4[d] < 0 || ny4[d] < 0 || nx4[d] >= rw || ny4[d] >= rh) continue;
                    if (raster.tri[(size_t)ny4[d] * rw + nx4[d]] >= 0) { any = true; break; }
                }
                if (!any) continue;
            }
            const int i0 = model_.Mesh().tris[(size_t)t * 3 + 0];
            const int i1 = model_.Mesh().tris[(size_t)t * 3 + 1];
            const int i2 = model_.Mesh().tris[(size_t)t * 3 + 2];
            const float b0 = raster.bary[idx * 3 + 0];
            const float b1 = raster.bary[idx * 3 + 1];
            const float b2 = raster.bary[idx * 3 + 2];
            const Vec4& a0 = model_.Mesh().attrs[(size_t)i0];
            const Vec4& a1 = model_.Mesh().attrs[(size_t)i1];
            const Vec4& a2 = model_.Mesh().attrs[(size_t)i2];
            const Vec4 at(a0.x * b0 + a1.x * b1 + a2.x * b2,
                          a0.y * b0 + a1.y * b1 + a2.y * b2,
                          a0.z * b0 + a1.z * b1 + a2.z * b2,
                          a0.w * b0 + a1.w * b1 + a2.w * b2);
            Vec3 n = posed.normals[(size_t)i0] * b0 + posed.normals[(size_t)i1] * b1 +
                     posed.normals[(size_t)i2] * b2;
            const int p = model_.Mesh().triPart[(size_t)t];
            double a = 1.0;
            const bool isFin = IsFinPart(p);
            const double px = x + 0.5 - cx;
            const double py = cy - (y + 0.5);
            const double pz = raster.depth[idx] * scale;
            const Vec3 q = toLight(Vec3(px, py, pz));
            const double qx = (q.x - loX) * shRes;
            const double qy = (q.y - loY) * shRes;
            const double qz = q.z * shRes;
            const double bias = (isFin ? 2.0 : 1.5) +
                                (isFin ? 0.01 : 0.02) * scale * (1.0 - std::fabs(Dot(n, sunC)));
            static const int kTaps[5][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}, {0, 0}};
            int litTaps = 0;
            for (int tap = 0; tap < 5; tap++) {
                const int ix = ClampI((int)std::floor(qx + kTaps[tap][0] * soft), 0, Wl - 1);
                const int iy = ClampI((int)std::floor(qy + kTaps[tap][1] * soft), 0, Hl - 1);
                const double occ = shadowRaster.depth[(size_t)iy * Wl + ix];
                if (occ == kEmptyDepth || occ <= qz + bias * shRes + soft * 0.8) litTaps++;
            }
            const double sunLit = litTaps / 5.0;
            // render.py uses px_len = 1 / self.scale (one FINAL pixel), even
            // though shading runs at supersample resolution.
            const Vec3 c = Shade(p, at, n, pose.pitch, 1.0 / Max(fitScale_, 1e-9), a,
                                 sunLit, (double)ao[idx]);
            rgb[idx * 3 + 0] = (float)(c.x * a);
            rgb[idx * 3 + 1] = (float)(c.y * a);
            rgb[idx * 3 + 2] = (float)(c.z * a);
            cov[idx] = (float)a;
            part[idx] = (unsigned char)ClampI(p, 0, 255);
        }
    }

    DrawEyes(posed, raster, pose, scale, cx, cy, ss, raster.depth, rgb, cov);
    for (const SeaPosedEye& eye : posed.eyes) {
        if (eye.normal.z <= 0.2) continue;
        double sx = 0.0, sy = 0.0;
        Project(eye.center, scale, cx, cy, sx, sy);
        out.eyes.push_back(Vec3(sx / ss, sy / ss, eye.radius * scale / ss));
    }
    DrawSpines(posed, raster, scale, cx, cy, pose.pitch, raster.depth, rgb, cov);

    // ---------------------------------------------------------- downsample
    out.width = size_;
    out.height = size_;
    out.rgba.assign((size_t)size_ * size_ * 4, 0);
    out.part.assign((size_t)size_ * size_, 0);
    out.alpha.assign((size_t)size_ * size_, 0);

    for (int y = 0; y < size_; y++) {
        for (int x = 0; x < size_; x++) {
            double r = 0, g = 0, b = 0, a = 0;
            double partWeight[16] = {0};
            for (int sy = 0; sy < ss; sy++) {
                for (int sx = 0; sx < ss; sx++) {
                    const size_t idx = (size_t)(y * ss + sy) * rw + (size_t)(x * ss + sx);
                    const double ca = cov[idx];
                    r += rgb[idx * 3 + 0];
                    g += rgb[idx * 3 + 1];
                    b += rgb[idx * 3 + 2];
                    a += ca;
                    if (ca > 0) {
                        const int p = part[idx];
                        if (p >= 0 && p < 16) partWeight[p] += ca;
                    }
                }
            }
            const int count = ss * ss;
            a /= count;
            const size_t o = ((size_t)y * size_ + x) * 4;
            if (a < 0.02) continue;
            const double inv = 1.0 / (a * count);
            out.rgba[o + 0] = (unsigned char)(Saturate(r * inv) * 255.0 + 0.5);
            out.rgba[o + 1] = (unsigned char)(Saturate(g * inv) * 255.0 + 0.5);
            out.rgba[o + 2] = (unsigned char)(Saturate(b * inv) * 255.0 + 0.5);
            out.rgba[o + 3] = (unsigned char)(Saturate(a) * 255.0 + 0.5);
            out.alpha[(size_t)y * size_ + x] = out.rgba[o + 3];
            int best = 0;
            double bestW = 0;
            for (int p = 0; p < 16; p++) {
                if (partWeight[p] > bestW) { bestW = partWeight[p]; best = p; }
            }
            out.part[(size_t)y * size_ + x] = (unsigned char)best;
        }
    }
}

} // namespace sea
