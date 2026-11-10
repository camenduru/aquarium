// ================================================================
// SEA CREATURES - OKLab palette + pixel clean-up
// Ported from fish-gen/fishgen/pixel.py (MIT).
// ================================================================
#include "creature/sea/SeaPixel.h"

#include "creature/sea/SeaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>

namespace sea {
namespace {

struct Lab {
    double L = 0, a = 0, b = 0;
};

double SrgbToLinear(double c) {
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double LinearToSrgb(double c) {
    c = Saturate(c);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

Lab ToLab(double r, double g, double b) {
    r = SrgbToLinear(r);
    g = SrgbToLinear(g);
    b = SrgbToLinear(b);
    const double l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
    const double m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
    const double s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;
    const double l_ = std::cbrt(l), m_ = std::cbrt(m), s_ = std::cbrt(s);
    Lab out;
    out.L = 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_;
    out.a = 1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_;
    out.b = 0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_;
    return out;
}

SeaRgba FromLab(const Lab& lab) {
    const double l_ = lab.L + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
    const double m_ = lab.L - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
    const double s_ = lab.L - 0.0894841775 * lab.a - 1.2914855480 * lab.b;
    const double l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    const double r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    const double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    const double b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
    return SeaRgba((unsigned char)(Saturate(LinearToSrgb(r)) * 255.0 + 0.5),
                   (unsigned char)(Saturate(LinearToSrgb(g)) * 255.0 + 0.5),
                   (unsigned char)(Saturate(LinearToSrgb(b)) * 255.0 + 0.5), 255);
}

Lab LabOf(const SeaRgba& c) { return ToLab(c.r / 255.0, c.g / 255.0, c.b / 255.0); }

/// pixel.py's _mark_eyes(): an eye smaller than ~2 output pixels blurs into
/// the skin, so give it a pupil (or a light glint on a dark head).
void MarkEyes(std::vector<unsigned char>& index, int width, int height,
              const std::vector<SeaRgba>& palette, const std::vector<Vec3>& eyes) {
    if (eyes.empty() || palette.empty()) return;
    std::vector<Lab> labs(palette.size());
    for (size_t i = 0; i < palette.size(); i++) labs[i] = LabOf(palette[i]);
    for (const Vec3& e : eyes) {
        if (e.z >= 2.0) continue;
        int ix = (int)std::floor(e.x), iy = (int)std::floor(e.y);
        if (ix < 0 || iy < 0 || ix >= width || iy >= height) continue;
        // The tiny eye often sits on the silhouette edge after quantization;
        // use the nearest opaque pixel so it still gets a pupil.
        if (index[(size_t)iy * width + ix] == 255) {
            bool found = false;
            int bx = ix, by = iy;
            double bestD = 1e30;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    const int x = ix + dx, y = iy + dy;
                    if (x < 0 || y < 0 || x >= width || y >= height) continue;
                    if (index[(size_t)y * width + x] == 255) continue;
                    const double d = (double)dx * dx + (double)dy * dy;
                    if (d < bestD) { bestD = d; bx = x; by = y; found = true; }
                }
            }
            if (!found) continue;
            ix = bx;
            iy = by;
        }
        const size_t p0 = (size_t)iy * width + ix;
        if (index[p0] == 255 || index[p0] >= labs.size()) continue;
        std::vector<double> around;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                const int x = ix + dx, y = iy + dy;
                if (x < 0 || y < 0 || x >= width || y >= height) continue;
                const int q = index[(size_t)y * width + x];
                if (q >= 0 && q != 255 && q < (int)labs.size()) around.push_back(labs[(size_t)q].L);
            }
        }
        if (around.empty()) continue;
        std::sort(around.begin(), around.end());
        const double skin = around[around.size() / 2];
        // Contrast against the local skin, not the palette's darkest black:
        // a light head gets a dark pupil, a dark head a light glint.
        if (skin > 0.3) {
            index[p0] = 0;
        } else {
            index[p0] = (unsigned char)(labs.size() - 1);
        }
    }
}

double Dist2(const Lab& a, const Lab& b) {
    const double dL = a.L - b.L, da = a.a - b.a, db = a.b - b.b;
    return dL * dL + da * da + db * db;
}

} // namespace

int SeaPaletteSizeFor(int spriteSize) {
    if (spriteSize <= 16) return 10;
    if (spriteSize >= 128) return 24;
    return ClampI((int)std::lround(10.0 + (spriteSize - 16) * (14.0 / 112.0)), 10, 24);
}

std::vector<SeaRgba> SeaLearnPalette(const unsigned char* rgba, int width, int height, int maxColors) {
    std::vector<SeaRgba> out;
    if (rgba == nullptr || width <= 0 || height <= 0) return out;
    const int total = width * height;
    maxColors = ClampI(maxColors, 1, 32);

    // pixel.py's _unique_colors(): bucket near-identical colours in Oklab so
    // k-means runs on thousands of points instead of millions of pixels.
    constexpr double kQuant = 0.006;
    struct Bucket { double L = 0, a = 0, b = 0; int count = 0; };
    std::map<std::array<int, 3>, Bucket> buckets;
    const int stride = Max(1, total / 400000);
    for (int i = 0; i < total; i += stride) {
        if (rgba[(size_t)i * 4 + 3] < 128) continue;
        const Lab lab = ToLab(rgba[(size_t)i * 4 + 0] / 255.0, rgba[(size_t)i * 4 + 1] / 255.0,
                              rgba[(size_t)i * 4 + 2] / 255.0);
        const std::array<int, 3> key = {
            (int)std::lround(lab.L / kQuant),
            (int)std::lround(lab.a / kQuant),
            (int)std::lround(lab.b / kQuant)};
        Bucket& b = buckets[key];
        b.L += lab.L;
        b.a += lab.a;
        b.b += lab.b;
        b.count++;
    }
    if (buckets.empty()) return out;

    std::vector<Lab> points;
    std::vector<double> weights;
    points.reserve(buckets.size());
    weights.reserve(buckets.size());
    for (const auto& kv : buckets) {
        const Bucket& b = kv.second;
        const double inv = 1.0 / (double)b.count;
        points.push_back(Lab{b.L * inv, b.a * inv, b.b * inv});
        // pixel.py: sqrt weights so small distinct details (eyes, stripes) are
        // not outvoted by the big flat areas.
        weights.push_back(std::sqrt((double)b.count));
    }
    const int n = (int)points.size();
    if (n <= maxColors) {
        for (const Lab& p : points) out.push_back(FromLab(p));
        std::sort(out.begin(), out.end(), [](const SeaRgba& a, const SeaRgba& b) {
            return LabOf(a).L < LabOf(b).L;
        });
        return out;
    }

    const int k = Min(maxColors, n);
    std::vector<Lab> centers;
    centers.reserve((size_t)k);
    // pixel.py's k-means++ seeding, deterministic here (std::mt19937 seed 0).
    std::mt19937 rng(0);
    int first = 0;
    double bestWeight = -1.0;
    for (int i = 0; i < n; i++) {
        if (weights[(size_t)i] > bestWeight) { bestWeight = weights[(size_t)i]; first = i; }
    }
    centers.push_back(points[(size_t)first]);
    std::vector<double> d2((size_t)n, 1e30);
    for (int i = 0; i < n; i++) d2[(size_t)i] = Dist2(points[(size_t)i], centers[0]);
    while ((int)centers.size() < k) {
        std::vector<double> choice((size_t)n);
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            choice[(size_t)i] = d2[(size_t)i] * weights[(size_t)i];
            sum += choice[(size_t)i];
        }
        if (sum <= 0.0) break;
        std::discrete_distribution<int> dist(choice.begin(), choice.end());
        const int pick = dist(rng);
        centers.push_back(points[(size_t)pick]);
        for (int i = 0; i < n; i++)
            d2[(size_t)i] = Min(d2[(size_t)i], Dist2(points[(size_t)i], centers.back()));
    }

    // Weighted Lloyd iterations (40, the reference's count).
    std::vector<int> assign((size_t)n, 0);
    for (int iter = 0; iter < 40; iter++) {
        bool changed = false;
        for (int i = 0; i < n; i++) {
            int bestIdx = 0;
            double bestD = 1e30;
            for (int c = 0; c < (int)centers.size(); c++) {
                const double d = Dist2(points[(size_t)i], centers[(size_t)c]);
                if (d < bestD) { bestD = d; bestIdx = c; }
            }
            if (assign[(size_t)i] != bestIdx) { assign[(size_t)i] = bestIdx; changed = true; }
        }
        std::vector<Lab> sum(centers.size());
        std::vector<double> wsum(centers.size(), 0.0);
        for (int i = 0; i < n; i++) {
            const int a = assign[(size_t)i];
            const double w = weights[(size_t)i];
            sum[(size_t)a].L += points[(size_t)i].L * w;
            sum[(size_t)a].a += points[(size_t)i].a * w;
            sum[(size_t)a].b += points[(size_t)i].b * w;
            wsum[(size_t)a] += w;
        }
        for (size_t c = 0; c < centers.size(); c++) {
            if (wsum[c] > 0) {
                const double inv = 1.0 / wsum[c];
                centers[c] = Lab{sum[c].L * inv, sum[c].a * inv, sum[c].b * inv};
            }
        }
        if (!changed) break;
    }

    for (const Lab& c : centers) out.push_back(FromLab(c));
    std::sort(out.begin(), out.end(), [](const SeaRgba& a, const SeaRgba& b) {
        return LabOf(a).L < LabOf(b).L;
    });
    return out;
}

void SeaPreparePixels(unsigned char* rgba, int width, int height) {
    if (rgba == nullptr || width <= 0 || height <= 0) return;
    const int w = width, h = height, n = w * h;

    std::vector<Lab> lab((size_t)n);
    std::vector<unsigned char> solid((size_t)n, 0);
    for (int i = 0; i < n; i++) {
        lab[(size_t)i] = ToLab(rgba[(size_t)i * 4 + 0] / 255.0, rgba[(size_t)i * 4 + 1] / 255.0,
                               rgba[(size_t)i * 4 + 2] / 255.0);
        solid[(size_t)i] = rgba[(size_t)i * 4 + 3] >= 128 ? 1 : 0;
    }

    // pixel.py's _tidy_mask(): drop a pixel with no opaque neighbour at all,
    // then fill a 1-px pinhole (four side neighbours opaque).
    {
        static const int k8[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                     {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        static const int k4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        std::vector<unsigned char> dropped = solid;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const size_t i = (size_t)y * w + x;
                if (!solid[i]) continue;
                int n8 = 0;
                for (int d = 0; d < 8; d++) {
                    const int nx = x + k8[d][0], ny = y + k8[d][1];
                    if (nx >= 0 && ny >= 0 && nx < w && ny < h && solid[(size_t)ny * w + nx]) n8++;
                }
                if (n8 == 0) dropped[i] = 0;
            }
        }
        std::vector<unsigned char> filled = dropped;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const size_t i = (size_t)y * w + x;
                if (dropped[i]) continue;
                int n4 = 0;
                for (int d = 0; d < 4; d++) {
                    const int nx = x + k4[d][0], ny = y + k4[d][1];
                    if (nx >= 0 && ny >= 0 && nx < w && ny < h && dropped[(size_t)ny * w + nx]) n4++;
                }
                if (n4 == 4) filled[i] = 1;
            }
        }
        solid = filled;
        for (int i = 0; i < n; i++) rgba[(size_t)i * 4 + 3] = solid[(size_t)i] ? 255 : 0;
    }

    // pixel.py's _see_through(): pixel art has no half-transparent pixels, so a
    // see-through fin is drawn the way a painter would -- paler and softer than
    // the body, by how transparent it is.
    for (int i = 0; i < n; i++) {
        const double clear = 1.0 - Saturate(rgba[(size_t)i * 4 + 3] / 255.0);
        if (clear <= 0.0) continue;
        Lab& p = lab[(size_t)i];
        p.L += (0.92 - p.L) * clear * 0.45;
        p.a *= 1.0 - 0.45 * clear;
        p.b *= 1.0 - 0.45 * clear;
    }

    // pixel.py's _smooth(): a small bilateral filter. Fine skin texture and
    // shading noise would only become speckle once the palette is squeezed down,
    // while stripes, spots, eyes and shading edges have to survive untouched.
    {
        constexpr double kSigma = 0.045;
        constexpr double kTwoSigma2 = 2.0 * kSigma * kSigma;
        static const int kOff[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                       {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        std::vector<Lab> next((size_t)n);
        for (int pass = 0; pass < 2; pass++) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    const size_t i = (size_t)y * w + x;
                    if (!solid[i]) { next[i] = lab[i]; continue; }
                    const Lab& self = lab[i];
                    double accL = self.L, accA = self.a, accB = self.b, wsum = 1.0;
                    for (int d = 0; d < 8; d++) {
                        const int nx = x + kOff[d][0], ny = y + kOff[d][1];
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        const size_t j = (size_t)ny * w + nx;
                        if (!solid[j]) continue;
                        const Lab& q = lab[j];
                        const double dL = q.L - self.L, da = q.a - self.a, db = q.b - self.b;
                        const double d2 = dL * dL + da * da + db * db;
                        const double wgt = std::exp(-d2 / kTwoSigma2) * (d >= 4 ? 0.6 : 1.0);
                        accL += q.L * wgt;
                        accA += q.a * wgt;
                        accB += q.b * wgt;
                        wsum += wgt;
                    }
                    const double inv = 1.0 / wsum;
                    next[i].L = accL * inv;
                    next[i].a = accA * inv;
                    next[i].b = accB * inv;
                }
            }
            lab.swap(next);
        }
    }

    // pixel.py's _punch(): spread the lightness range about its median and lift
    // chroma a little. Shading that reads at full resolution flattens out at
    // 16-64 px without it.
    {
        std::vector<double> ls;
        ls.reserve((size_t)n);
        for (int i = 0; i < n; i++)
            if (solid[(size_t)i]) ls.push_back(lab[(size_t)i].L);
        if (!ls.empty()) {
            std::sort(ls.begin(), ls.end());
            const double mid = ls[ls.size() / 2];
            for (int i = 0; i < n; i++) {
                if (!solid[(size_t)i]) continue;
                Lab& p = lab[(size_t)i];
                p.L = Saturate(mid + (p.L - mid) * 1.15);
                p.a *= 1.06;
                p.b *= 1.06;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        const SeaRgba c = FromLab(lab[(size_t)i]);
        rgba[(size_t)i * 4 + 0] = c.r;
        rgba[(size_t)i * 4 + 1] = c.g;
        rgba[(size_t)i * 4 + 2] = c.b;
    }
}

void SeaPixelate(const unsigned char* rgba, int width, int height,
                 const std::vector<SeaRgba>& palette, const SeaPixelOptions& options,
                 std::vector<unsigned char>& out,
                 const std::vector<SeaRgba>* sharedOutline,
                 std::vector<SeaRgba>* outlineOut,
                 const std::vector<Vec3>* eyes,
                 const std::vector<unsigned char>* part,
                 bool darkEye) {
    out.assign((size_t)Max(0, width) * Max(0, height) * 4, 0);
    if (rgba == nullptr || width <= 0 || height <= 0 || palette.empty()) return;

    const int n = width * height;
    const int k = Min((int)palette.size(), 32);
    std::vector<Lab> labs((size_t)k);
    for (int i = 0; i < k; i++) labs[(size_t)i] = LabOf(palette[(size_t)i]);

    std::vector<unsigned char> index((size_t)n, 255);
    for (int i = 0; i < n; i++) {
        if (rgba[(size_t)i * 4 + 3] < 128) continue;
        const Lab p = ToLab(rgba[(size_t)i * 4 + 0] / 255.0, rgba[(size_t)i * 4 + 1] / 255.0,
                            rgba[(size_t)i * 4 + 2] / 255.0);
        int best = 0;
        double bestD = 1e30;
        for (int c = 0; c < k; c++) {
            const double d = Dist2(p, labs[(size_t)c]);
            if (d < bestD) { bestD = d; best = c; }
        }
        index[(size_t)i] = (unsigned char)best;
    }

    auto at = [&](int x, int y) -> int {
        if (x < 0 || y < 0 || x >= width || y >= height) return 255;
        return index[(size_t)y * width + x];
    };

    // ---------------------------------------------------------- despeckle
    // pixel.py's _despeckle(): a pixel is absorbed only when ALL FOUR side
    // neighbours already agree on one other colour AND that colour is close to
    // its own (Oklab distance < 0.09). The previous version used an 8-neighbour
    // majority vote, which is far greedier: it flattened the eye's one-pixel
    // catch-light, since three orange iris neighbours outvoted it.
    if (options.despeckle) {
        for (int pass = 0; pass < Max(0, options.despecklePasses); pass++) {
            // Seed the scratch buffer from THIS pass's input. Reusing the
            // previous pass's buffer made pass 2 write on top of the ORIGINAL
            // image, silently reverting everything pass 1 had fixed.
            std::vector<unsigned char> next = index;
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    const int self = index[(size_t)y * width + x];
                    if (self == 255 || self >= k) continue;
                    const int n0 = at(x, y - 1), n1 = at(x, y + 1);
                    const int n2 = at(x - 1, y), n3 = at(x + 1, y);
                    if (n0 != n1 || n0 != n2 || n0 != n3) continue;
                    if (n0 < 0 || n0 == 255 || n0 >= k || n0 == self) continue;
                    if (std::sqrt(Dist2(labs[(size_t)self], labs[(size_t)n0])) < 0.09) {
                        next[(size_t)y * width + x] = (unsigned char)n0;
                    }
                }
            }
            index.swap(next);
        }
    }

    // ---------------------------------------------------------- tidy
    if (options.tidy) {
        std::vector<unsigned char> next = index;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int self = index[(size_t)y * width + x];
                if (self != 255) {
                    int opaque = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            if ((dx || dy) && at(x + dx, y + dy) != 255) opaque++;
                    if (opaque <= 1) next[(size_t)y * width + x] = 255;
                } else {
                    int counts[32] = {0};
                    int opaque = 0;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (!dx && !dy) continue;
                            const int q = at(x + dx, y + dy);
                            if (q == 255 || q >= k) continue;
                            counts[q]++;
                            opaque++;
                        }
                    }
                    if (opaque >= 6) {
                        int best = 0, bestCount = 0;
                        for (int c = 0; c < k; c++)
                            if (counts[c] > bestCount) { bestCount = counts[c]; best = c; }
                        if (bestCount >= 5) next[(size_t)y * width + x] = (unsigned char)best;
                    }
                }
            }
        }
        index.swap(next);
    }

    // pixel.py's _mouth_colors()/_mark_mouth(): a small maw reads as a dark
    // hole with crisp teeth. Match the rendered maw/teeth mean to the palette.
    if (part != nullptr && part->size() == (size_t)n && !palette.empty()) {
        Lab maw, tooth;
        int mawN = 0, toothN = 0;
        for (int i = 0; i < n; i++) {
            const int q = (*part)[(size_t)i];
            if (q != PartMouth && q != PartTeeth) continue;
            const Lab lab = ToLab(rgba[(size_t)i * 4 + 0] / 255.0, rgba[(size_t)i * 4 + 1] / 255.0,
                                  rgba[(size_t)i * 4 + 2] / 255.0);
            if (q == PartMouth) { maw.L += lab.L; maw.a += lab.a; maw.b += lab.b; mawN++; }
            else { tooth.L += lab.L; tooth.a += lab.a; tooth.b += lab.b; toothN++; }
        }
        int mawIdx = 0;
        int toothIdx = (int)palette.size() - 1;
        if (mawN > 0) {
            maw.L = Min(maw.L / mawN, 0.3);
            maw.a /= mawN;
            maw.b /= mawN;
            double best = 1e30;
            for (size_t c = 0; c < palette.size(); c++) {
                const double d = Dist2(maw, LabOf(palette[c]));
                if (d < best) { best = d; mawIdx = (int)c; }
            }
        }
        if (toothN > 0) {
            tooth.L = Max(tooth.L / toothN, 0.88);
            tooth.a /= toothN;
            tooth.b /= toothN;
        } else {
            tooth = Lab{0.95, 0.0, 0.01};
        }
        {
            double best = 1e30;
            for (size_t c = 0; c < palette.size(); c++) {
                const double d = Dist2(tooth, LabOf(palette[c]));
                if (d < best) { best = d; toothIdx = (int)c; }
            }
        }
        for (int i = 0; i < n; i++) {
            const int q = (*part)[(size_t)i];
            if (q == PartMouth) index[(size_t)i] = (unsigned char)mawIdx;
            else if (q == PartTeeth) index[(size_t)i] = (unsigned char)toothIdx;
        }
    }

    // ---------------------------------------------------------- colour
    for (int i = 0; i < n; i++) {
        const int c = index[(size_t)i];
        if (c == 255 || c >= k) continue;
        out[(size_t)i * 4 + 0] = palette[(size_t)c].r;
        out[(size_t)i * 4 + 1] = palette[(size_t)c].g;
        out[(size_t)i * 4 + 2] = palette[(size_t)c].b;
        out[(size_t)i * 4 + 3] = 255;
    }


    // ---------------------------------------------------------- outline
    // pixel.py's _outline(): each transparent pixel touching the sprite becomes
    // a dark version of the colours it borders. Those rim colours (not the whole
    // palette) are clustered to three shades.
    if (options.outline) {
        constexpr int kShades = 3;
        std::vector<Lab> rimCols;
        std::vector<int> rimPix;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const size_t i = (size_t)y * width + x;
                if (index[i] != 255) continue;
                Lab mean;
                int hit = 0;
                const int nx[4] = {x - 1, x + 1, x, x};
                const int ny[4] = {y, y, y - 1, y + 1};
                for (int d = 0; d < 4; d++) {
                    const int q = at(nx[d], ny[d]);
                    if (q == 255 || q >= k) continue;
                    mean.L += labs[(size_t)q].L;
                    mean.a += labs[(size_t)q].a;
                    mean.b += labs[(size_t)q].b;
                    hit++;
                }
                if (hit == 0) continue;
                const double inv = 1.0 / hit;
                Lab rim{mean.L * inv, mean.a * inv, mean.b * inv};
                rim.L = Min(rim.L * 0.42, 0.3);
                rim.a *= 0.45;
                rim.b = rim.b * 0.45 - 0.012;
                rimCols.push_back(rim);
                rimPix.push_back((int)i);
            }
        }
        if (!rimCols.empty()) {
            std::vector<SeaRgba> shadeRgb;
            if (sharedOutline != nullptr && !sharedOutline->empty()) {
                // One set of three rim shades for the whole sheet, like pixel.py's
                // _outline() over every frame at once (per-frame shades flicker).
                shadeRgb = *sharedOutline;
            } else {
                constexpr double kQuant = 0.006;
                struct RB { double L = 0, a = 0, b = 0; int count = 0; };
                std::map<std::array<int, 3>, RB> buckets;
                for (const Lab& c : rimCols) {
                    const std::array<int, 3> key = {
                        (int)std::lround(c.L / kQuant),
                        (int)std::lround(c.a / kQuant),
                        (int)std::lround(c.b / kQuant)};
                    RB& b = buckets[key];
                    b.L += c.L; b.a += c.a; b.b += c.b; b.count++;
                }
                std::vector<Lab> pts;
                std::vector<double> wts;
                for (const auto& kv : buckets) {
                    const RB& b = kv.second;
                    const double inv = 1.0 / (double)b.count;
                    pts.push_back(Lab{b.L * inv, b.a * inv, b.b * inv});
                    wts.push_back((double)b.count);
                }
                const int kk = Min(kShades, (int)pts.size());
                std::vector<Lab> centers;
                centers.push_back(pts[0]);
                while ((int)centers.size() < kk) {
                    int best = 0;
                    double bestD = -1.0;
                    for (int i = 0; i < (int)pts.size(); i++) {
                        double nearest = 1e30;
                        for (const Lab& c : centers) nearest = Min(nearest, Dist2(pts[(size_t)i], c));
                        if (nearest > bestD) { bestD = nearest; best = i; }
                    }
                    centers.push_back(pts[(size_t)best]);
                }
                for (int iter = 0; iter < 40; iter++) {
                    std::vector<int> ass(pts.size(), 0);
                    for (int i = 0; i < (int)pts.size(); i++) {
                        int bi = 0; double bd = 1e30;
                        for (int c = 0; c < (int)centers.size(); c++) {
                            const double d = Dist2(pts[(size_t)i], centers[(size_t)c]);
                            if (d < bd) { bd = d; bi = c; }
                        }
                        ass[(size_t)i] = bi;
                    }
                    std::vector<Lab> sum(centers.size());
                    std::vector<double> ws(centers.size(), 0.0);
                    for (int i = 0; i < (int)pts.size(); i++) {
                        const int a = ass[(size_t)i];
                        const double w = wts[(size_t)i];
                        sum[(size_t)a].L += pts[(size_t)i].L * w;
                        sum[(size_t)a].a += pts[(size_t)i].a * w;
                        sum[(size_t)a].b += pts[(size_t)i].b * w;
                        ws[(size_t)a] += w;
                    }
                    for (size_t c = 0; c < centers.size(); c++) if (ws[c] > 0) {
                        const double inv = 1.0 / ws[c];
                        centers[c] = Lab{sum[c].L * inv, sum[c].a * inv, sum[c].b * inv};
                    }
                }
                for (const Lab& c : centers) shadeRgb.push_back(FromLab(c));
                if (outlineOut != nullptr) *outlineOut = shadeRgb;
            }
            if (!shadeRgb.empty()) {
                for (size_t p = 0; p < rimCols.size(); p++) {
                    int best = 0; double bd = 1e30;
                    for (int s = 0; s < (int)shadeRgb.size(); s++) {
                        const double d = Dist2(rimCols[p], LabOf(shadeRgb[(size_t)s]));
                        if (d < bd) { bd = d; best = s; }
                    }
                    const size_t i = (size_t)rimPix[p];
                    out[i * 4 + 0] = shadeRgb[(size_t)best].r;
                    out[i * 4 + 1] = shadeRgb[(size_t)best].g;
                    out[i * 4 + 2] = shadeRgb[(size_t)best].b;
                    out[i * 4 + 3] = 255;
                }
            }
        }
    }
    // A 1 px eye on the silhouette is swallowed by the rim. If the eye centre
    // is still on the body the drawn iris/pupil survives; only when it falls
    // outside the sprite do we stamp a contrasting pupil/glint on the nearest
    // body pixel (pale skin -> dark pupil, dark skin -> light glint).
    if (darkEye && eyes != nullptr && !eyes->empty() && !palette.empty()) {
        for (const Vec3& e : *eyes) {
            if (e.z >= 2.0) continue;
            const int ix = (int)std::floor(e.x), iy = (int)std::floor(e.y);
            if (ix < 0 || iy < 0 || ix >= width || iy >= height) continue;
            int bx = -1, by = -1;
            double best = 1e30;
            for (int dy = -3; dy <= 3; dy++) {
                for (int dx = -3; dx <= 3; dx++) {
                    const int x = ix + dx, y = iy + dy;
                    if (x < 0 || y < 0 || x >= width || y >= height) continue;
                    if (out[((size_t)y * width + x) * 4 + 3] < 128) continue;
                    const double d = (double)dx * dx + (double)dy * dy;
                    if (d < best) { best = d; bx = x; by = y; }
                }
            }
            if (bx < 0) continue;
            const size_t p = ((size_t)by * width + bx) * 4;
            const SeaRgba skin(out[p + 0], out[p + 1], out[p + 2], 255);
            const SeaRgba& pick = LabOf(skin).L > 0.3 ? palette.front() : palette.back();
            out[p + 0] = pick.r;
            out[p + 1] = pick.g;
            out[p + 2] = pick.b;
            out[p + 3] = 255;
        }
    }

}

} // namespace sea
