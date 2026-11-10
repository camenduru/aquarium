// ================================================================
// SEA CREATURES - model construction and posing
// Ported from fish-gen/fishgen/geometry.py (MIT).
// Engine-free: no WickedEngine, no <filesystem>, no exceptions.
// ================================================================
#include "creature/sea/SeaGeometry.h"

#include "creature/sea/SeaNumpyRandom.h"

#include <algorithm>
#include <cmath>

namespace sea {
namespace {

void AppendSkipFirst(std::vector<Vec2>& dst, const std::vector<Vec2>& src) {
    for (size_t i = 1; i < src.size(); i++) dst.push_back(src[i]);
}

/// Fin height profile along the fin (m 0 at the leading root .. 1 at the trailing root).
double FinProfile(FinShape shape, double m) {
    switch (shape) {
        case FinShape::Triangle: {
            const double peak = 0.3;
            const double v = m < peak ? m / peak : (1 - m) / (1 - peak);
            return std::pow(Max(0.0, v), 0.9);
        }
        case FinShape::Rounded:
            return std::pow(std::sin(kPi * m), 0.6);
        case FinShape::Falcate:
            return m < 0.72 ? std::pow(m / 0.72, 0.75) : std::pow(Max(0.0, (1 - m) / 0.28), 2.2);
        case FinShape::Pennant:
            return m < 0.9 ? std::pow(m / 0.9, 0.8) : std::pow(Max(0.0, (1 - m) / 0.1), 1.5);
        case FinShape::Sail:
            return std::pow(std::sin(kPi * std::pow(Clamp(m, 0.0, 1.0), 0.55)), 0.7) * (0.6 + 0.6 * m);
        default:  // Long: a low, even fin that runs the whole length
            return Clamp(std::sin(kPi * m) * 3.0, 0.0, 1.0) * 0.6;
    }
}

/// Outline of the caudal fin from top root to bottom root in (a, y).
std::vector<Vec2> TailShapePoints(const SeaGenome& g, double h) {
    const double L = g.tailLength;
    const double S = g.tailSpread * h * 0.5;
    const double x0 = 0.97;
    std::vector<Vec2> pts;

    switch (g.tailShape) {
        case TailShape::Heterocercal: {
            const Vec2 up(x0 + L * 1.12, S * 1.05), lo(x0 + L * 0.62, -S * 0.74);
            const Vec2 fork(x0 + L * 0.3, -S * 0.06);
            pts = Bezier({x0, S * 0.3}, {x0 + L * 0.5, S * 0.55}, up, 8);
            AppendSkipFirst(pts, Bezier(up, {x0 + L * 1.06, S * 0.78}, {x0 + L * 0.9, S * 0.72}, 4));
            pts.push_back({x0 + L * 0.8, S * 0.62});
            AppendSkipFirst(pts, Bezier({x0 + L * 0.8, S * 0.62}, {x0 + L * 0.45, S * 0.3}, fork, 6));
            AppendSkipFirst(pts, Bezier(fork, {x0 + L * 0.38, -S * 0.42}, lo, 6));
            AppendSkipFirst(pts, Bezier(lo, {x0 + L * 0.22, -S * 0.55}, {x0, -S * 0.3}, 6));
            return pts;
        }
        case TailShape::Flukes: {
            const Vec2 half[5] = {{x0, S * 0.25}, {x0 + L * 0.45, S * 0.75},
                                  {x0 + L * 0.95, S * 1.05}, {x0 + L, S * 0.85},
                                  {x0 + L * 0.8, S * 0.35}};
            for (const Vec2& p : half) pts.push_back(p);
            pts.push_back({x0 + L * 0.68, 0.0});
            for (int i = 4; i >= 0; i--) pts.push_back({half[i].x, -half[i].y});
            return pts;
        }
        case TailShape::Forked:
            return {{x0, S * 0.25}, {x0 + L, S}, {x0 + L * 0.95, S * 0.75},
                    {x0 + L * 0.45, 0.0}, {x0 + L * 0.95, -S * 0.75},
                    {x0 + L, -S}, {x0, -S * 0.25}};
        case TailShape::Lunate: {
            const Vec2 up(x0 + L, S), lo(x0 + L * 0.9, -S * 0.9);
            const Vec2 fork(x0 + L * 0.3, -S * 0.02);
            pts = Bezier({x0, S * 0.22}, {x0 + L * 0.3, S * 0.72}, up, 8);
            AppendSkipFirst(pts, Bezier(up, {x0 + L * 0.45, S * 0.42}, fork, 8));
            AppendSkipFirst(pts, Bezier(fork, {x0 + L * 0.42, -S * 0.38}, lo, 8));
            AppendSkipFirst(pts, Bezier(lo, {x0 + L * 0.28, -S * 0.66}, {x0, -S * 0.22}, 8));
            return pts;
        }
        case TailShape::Truncate:
            return {{x0, S * 0.3}, {x0 + L * 0.9, S * 0.9}, {x0 + L, S * 0.6},
                    {x0 + L, -S * 0.6}, {x0 + L * 0.9, -S * 0.9}, {x0, -S * 0.3}};
        case TailShape::Pointed:
            return {{x0, S * 0.45}, {x0 + L * 0.7, S * 0.35}, {x0 + L * 1.35, 0.0},
                    {x0 + L * 0.7, -S * 0.35}, {x0, -S * 0.45}};
        default: {  // rounded / fan: a lobe that flares out of the body
            const bool fan = g.tailShape == TailShape::Fan;
            const double spread = fan ? 1.15 : 0.85;
            const double power = fan ? 0.6 : 0.8;
            const double reach = fan ? 1.2 : 1.0;
            pts.push_back({x0, S * 0.3});
            for (int k = 0; k < 13; k++) {
                const double t = 0.08 + (0.92 - 0.08) * k / 12.0;
                const double ang = kPi / 2 - t * kPi;
                const double c = std::pow(Max(0.0, std::cos(ang)), power);
                pts.push_back({x0 + L * reach * (0.3 + 0.7 * c), S * spread * std::sin(ang)});
            }
            pts.push_back({x0, -S * 0.3});
            return pts;
        }
    }
}

std::vector<Vec2> TailOutline(const SeaGenome& g, double h, double top, double bot) {
    std::vector<Vec2> pts = TailShapePoints(g, h);
    if (!pts.empty()) {
        pts.front().y = top;
        pts.back().y = -bot;
    }
    return pts;
}

} // namespace

double TurnHeading(double progress, double along, double start, double end, double lag) {
    const double s = Saturate(progress * (1.0 + lag) - lag * along);
    const double ease = s * s * (3.0 - 2.0 * s);
    return start + (end - start) * ease;
}

// ---------------------------------------------------------------- construction
SeaModel::SeaModel(const SeaGenome& genome, int detail) : g_(genome), detail_(ClampI(detail, 8, 512)) {
    rings_ = ClampI((int)(detail_ * 1.2), 24, 200);
    ringVerts_ = ClampI(detail_ / 4, 12, 40);
    finCols_ = ClampI(detail_ / 3, 8, 140);
    finRows_ = ClampI(detail_ / 24, 4, 24);
    centerA_ = g_.profile == "sphere" ? 0.5 : 0.45;
    const double tail = g_.tailShape == TailShape::None ? 0.02 : g_.tailLength * 1.35 + 0.05;
    tailEnd_ = 1.0 + tail;
    // render.py's jaw(): a gaping mouth for a scary fish/shark/whale.
    if (g_.scariness > 0 &&
        (g_.kind == SeaKind::Fish || g_.kind == SeaKind::Shark || g_.kind == SeaKind::Whale)) {
        const double length = Max(g_.mouth, 0.13 * g_.scariness);
        const double a0 = g_.kind == SeaKind::Shark ? Max(0.012, g_.eyePos - 0.03) : 0.0;
        SeaJaw j;
        j.a0 = a0;
        j.a1 = a0 + length;
        j.height = g_.mouthHeight;
        j.rise = 0.12;
        j.band = 0.16;
        j.gape = g_.scariness * Min(0.85 * length, 0.4 * g_.height);
        jaw_ = j;
        hasJaw_ = true;
    }
    Build();

    // Python anchors the animal halfway from the nose to the tail tip:
    //   reach = max(mesh x, spine line x); tail_end = max(tail_end, reach + 0.05)
    //   center_a = 0.5 * (nose + reach)
    // Seeding `reach` from tail_end instead (as this did) pushes the anchor forward,
    // and since the camera fits max|x| symmetrically that shrinks the whole sprite
    // in its cell — the clownfish came out 11% small and sat against the right edge.
    double reach = -1e9;
    for (const Vec3& v : mesh_.verts) reach = Max(reach, v.x);
    {
        std::vector<Vec3> spineRest;
        BuildSpineLines(0.0, 0.0, spineRest);
        for (const Vec3& p : spineRest) reach = Max(reach, p.x);
    }
    if (reach < 0.0) reach = 0.0;
    tailEnd_ = Max(tailEnd_, reach + 0.05);
    if (g_.profile != "sphere") {
        double nose = 0.0;
        for (const Vec3& v : mesh_.verts) nose = Min(nose, v.x);
        centerA_ = 0.5 * (nose + reach);
    }
}

double SeaModel::Neck(double a) const {
    if (g_.neck <= 0) return 1.0;
    const double d = (a - 0.55 * g_.peak) / 0.07;
    return 1.0 - g_.neck * std::exp(-d * d);
}

double SeaModel::HeightProfile(double a) const {
    a = Clamp(a, 0.0, 1.0);
    if (g_.profile == "sphere") {
        return Max(std::sqrt(Max(0.0, 1.0 - (2.0 * a - 1.0) * (2.0 * a - 1.0))), 0.015);
    }
    const bool front = a < g_.peak;
    const double b = Min(g_.beak, g_.peak * 0.8);
    double uf = 0.0;
    if (g_.peak > b) uf = Saturate((a - b) / (g_.peak - b));
    double fFront = std::pow(Max(0.0, 1.0 - (1.0 - uf) * (1.0 - uf)), g_.nose);
    if (b > 0) fFront = Max(fFront, 0.3 * std::sqrt(Saturate(a / (0.35 * b))));
    const double ub = Saturate((a - g_.peak) / (1.0 - g_.peak));
    const double fBack = g_.peduncle + (1.0 - g_.peduncle) *
                                          std::pow(Max(0.0, std::cos(ub * kPi * 0.5)), g_.taper);
    return Max(front ? fFront : fBack, 0.015);
}

double SeaModel::Top(double a) const {
    double t = g_.height * HeightProfile(a) * g_.backArch * Neck(a);
    if (g_.melon > 0) {
        const double w = 0.05 + 0.5 * g_.melon;
        const double m0 = Max(g_.beak, 0.02) + 0.6 * w;
        const double d = (Clamp(a, 0.0, 1.0) - m0) / w;
        t += g_.melon * std::exp(-d * d);
    }
    return t;
}

double SeaModel::Bottom(double a) const {
    return g_.height * HeightProfile(a) * (1.0 - g_.backArch) * Neck(a);
}

double SeaModel::HalfWidth(double a) const {
    a = Clamp(a, 0.0, 1.0);
    double squeeze = 1.0;
    if (g_.profile != "sphere" && g_.peak < 1.0) {
        const double u = Saturate((a - g_.peak) / (1.0 - g_.peak));
        squeeze = 1.0 - 0.55 * u * u * (3.0 - 2.0 * u);
    }
    return 0.5 * g_.height * g_.thickness * HeightProfile(a) * squeeze * Neck(a);
}

double SeaModel::HeadLift(double a) const {
    a = Clamp(a, 0.0, 1.0);
    const double lift = g_.flatHead * (Top(g_.peak) - Top(a)) * 0.85;
    return a < g_.peak ? lift : 0.0;
}

int SeaModel::AddGrid(const std::vector<Vec3>& verts, const std::vector<Vec4>& attrs, int rows, int cols,
                      int part, bool wrap, bool flip, const std::vector<Vec3>* axis) {
    const int start = (int)mesh_.verts.size();
    SeaMesh::Patch patch;
    patch.start = start;
    patch.rows = rows;
    patch.cols = cols;
    patch.kind = wrap ? 0 : (flip ? 2 : 1);

    for (size_t i = 0; i < verts.size(); i++) {
        mesh_.verts.push_back(verts[i]);
        mesh_.attrs.push_back(attrs[i]);
        mesh_.axis.push_back(axis != nullptr ? (*axis)[i] : Vec3());
    }

    for (int r = 0; r + 1 < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (!wrap && c + 1 >= cols) continue;
            const int c1 = (c + 1) % cols;
            const int q00 = start + r * cols + c;
            const int q01 = start + r * cols + c1;
            const int q10 = start + (r + 1) * cols + c;
            const int q11 = start + (r + 1) * cols + c1;
            mesh_.tris.push_back(q00); mesh_.tris.push_back(q10); mesh_.tris.push_back(q11);
            mesh_.triPart.push_back(part);
            mesh_.tris.push_back(q00); mesh_.tris.push_back(q11); mesh_.tris.push_back(q01);
            mesh_.triPart.push_back(part);
        }
    }
    mesh_.patches.push_back(patch);
    return start;
}

namespace {
double FinHalfThickness(SeaKind kind, double R, double M, double rootHalf) {
    if (kind != SeaKind::Shark && kind != SeaKind::Whale) return 0.0;
    const double edge = std::pow(Clamp(1.0 - std::pow(R, 1.5), 0.0, 1.0), 0.6);
    const double ends = Clamp(Min(M, 1.0 - M) * 6.0, 0.25, 1.0);
    return 0.75 * rootHalf * edge * ends;
}
} // namespace

void SeaModel::BuildMedianFin(const FinGenes& fin, int part, double sign) {
    if (!fin.present || fin.end <= fin.start) return;
    const int cols = finCols_;
    const bool thick = g_.kind == SeaKind::Shark || g_.kind == SeaKind::Whale;
    std::vector<Vec3> vs((size_t)finRows_ * cols);
    std::vector<Vec4> at((size_t)finRows_ * cols);
    double maxH = 0;
    for (int c = 0; c < cols; c++) {
        const double m = cols > 1 ? (double)c / (cols - 1) : 0.0;
        const double prof = FinProfile(fin.shape, m);
        const double ra = fin.start + (fin.end - fin.start) * m;
        const double base = sign > 0 ? Top(ra) : Bottom(ra);
        const double rootY = sign * base * 0.85;
        const double h = fin.height * prof;
        maxH = Max(maxH, h);
        const double edgeA = ra + fin.sweep * h * 0.9;
        const double edgeY = sign * (base + h);
        const double rootHalf = HalfWidth(ra) * 0.4;
        for (int r = 0; r < finRows_; r++) {
            const double rr = finRows_ > 1 ? (double)r / (finRows_ - 1) : 0.0;
            const double va = ra + (edgeA - ra) * rr;
            const double vy = rootY + (edgeY - rootY) * rr;
            const size_t idx = (size_t)r * cols + c;
            vs[idx] = Vec3(va, vy, 0.0);
            at[idx] = Vec4(m, rr, va, 0.0);
        }
    }
    (void)maxH;
    // render.py's _fin_thickness(): sharks/whales build two offset sheets,
    // thick at the root and thinning to the edge. Bony fish stay a single sheet.
    const int sides = thick ? 2 : 1;
    for (int sd = 0; sd < sides; sd++) {
        const double side = thick ? (sd == 0 ? -1.0 : 1.0) : 0.0;
        for (int c = 0; c < cols; c++) {
            const double m = cols > 1 ? (double)c / (cols - 1) : 0.0;
            const double prof = FinProfile(fin.shape, m);
            const double ra = fin.start + (fin.end - fin.start) * m;
            const double base = sign > 0 ? Top(ra) : Bottom(ra);
            const double rootY = sign * base * 0.85;
            const double h = fin.height * prof;
            const double edgeA = ra + fin.sweep * h * 0.9;
            const double edgeY = sign * (base + h);
            const double rootHalf = HalfWidth(ra) * 0.4;
            for (int r = 0; r < finRows_; r++) {
                const double rr = finRows_ > 1 ? (double)r / (finRows_ - 1) : 0.0;
                const double va = ra + (edgeA - ra) * rr;
                const double vy = rootY + (edgeY - rootY) * rr;
                const double half = FinHalfThickness(g_.kind, rr, m, rootHalf);
                const size_t idx = (size_t)r * cols + c;
                vs[idx] = Vec3(va, vy, side * half);
                at[idx] = Vec4(m, rr, va, 0.0);
            }
        }
        AddGrid(vs, at, finRows_, cols, part, false, false, nullptr);
    }
}

void SeaModel::BuildCaudal() {
    if (g_.tailShape == TailShape::None) return;
    const bool flat = g_.tailShape == TailShape::Flukes;
    const double pedTop = flat ? HalfWidth(1.0) : Top(1.0);
    const double pedBot = flat ? HalfWidth(1.0) : Bottom(1.0);
    const int n = finCols_ + 4;
    const int rows = finRows_ + 1;
    std::vector<Vec2> edge = Resample(TailOutline(g_, Max(g_.height, 0.3), pedTop * 0.9, pedBot * 0.9), n);
    if ((int)edge.size() != n) return;
    const bool thick = g_.kind == SeaKind::Shark || g_.kind == SeaKind::Whale;
    const double rootHalf = flat ? Top(1.0) * 0.6 : HalfWidth(1.0);

    std::vector<Vec3> vs((size_t)rows * n);
    std::vector<Vec4> at((size_t)rows * n);
    const int sides = thick ? 2 : 1;
    for (int sd = 0; sd < sides; sd++) {
        const double side = thick ? (sd == 0 ? -1.0 : 1.0) : 0.0;
        for (int c = 0; c < n; c++) {
            const double m = n > 1 ? (double)c / (n - 1) : 0.0;
            const double rootA = 0.97;
            const double rootY = Lerp(pedTop, -pedBot, m) * 0.9;
            for (int r = 0; r < rows; r++) {
                const double rr = rows > 1 ? (double)r / (rows - 1) : 0.0;
                const double va = rootA + (edge[c].x - rootA) * rr;
                const double vy = rootY + (edge[c].y - rootY) * rr;
                const double half = FinHalfThickness(g_.kind, rr, m, rootHalf);
                const size_t idx = (size_t)r * n + c;
                vs[idx] = flat ? Vec3(va, side * half, vy) : Vec3(va, vy, side * half);
                at[idx] = Vec4(m, rr, va, 0.0);
            }
        }
        AddGrid(vs, at, rows, n, PartCaudal, false, false, nullptr);
    }
}

void SeaModel::BuildPairedFin(int part, double side, double rootA, double rootY, double length,
                              double droop, double splay, bool blade) {
    if (length <= 0.01) return;
    const int mCount = finCols_ / 2 + 2;
    const int rCount = finRows_;
    std::vector<Vec3> vs((size_t)rCount * mCount);
    std::vector<Vec4> at((size_t)rCount * mCount);
    std::vector<double> out((size_t)rCount * mCount);

    const double root = blade ? 0.45 * length : 0.04;
    const double sink = blade ? 1.0 : 0.5;
    std::vector<double> edgeU(mCount), edgeV(mCount), ra(mCount);
    for (int c = 0; c < mCount; c++) {
        const double m = mCount > 1 ? (double)c / (mCount - 1) : 0.0;
        if (blade) {
            const double prof = m < 0.3 ? std::pow(m / 0.3, 0.7) : std::pow(Max(0.0, (1 - m) / 0.7), 1.3);
            edgeU[c] = root * m + length * 0.8 * prof;
            edgeV[c] = droop * length * prof;
        } else {
            const double prof = std::pow(std::sin(kPi * m), 0.8);
            edgeU[c] = 0.04 * m + length * (0.35 + 0.65 * prof) * (0.6 + 0.4 * m);
            edgeV[c] = droop * length * prof;
        }
        ra[c] = rootA + root * m;
    }

    for (int c = 0; c < mCount; c++) {
        for (int r = 0; r < rCount; r++) {
            const double rr = rCount > 1 ? (double)r / (rCount - 1) : 0.0;
            const double m = mCount > 1 ? (double)c / (mCount - 1) : 0.0;
            const double finU = root * m + (edgeU[c] - root * m) * rr;
            const double finV = edgeV[c] * rr;
            const double rz = side * HalfWidth(ra[c]) * 0.9;
            const double o = side * (splay + (blade ? 0.25 : 0.6) * finV) * rr * length;
            const size_t idx = (size_t)r * mCount + c;
            vs[idx] = Vec3(rootA + finU, rootY - finV * sink, rz + o * 0.9);
            at[idx] = Vec4(m, rr, rootA + finU, 0.0);
            out[idx] = o;
        }
    }

    // Store the rest positions WITHOUT the resting offset: Pose re-applies the flap.
    PairedPatch patch;
    patch.start = (int)mesh_.verts.size();
    patch.count = rCount * mCount;
    patch.part = part;
    patch.out = out;
    for (size_t i = 0; i < vs.size(); i++) vs[i].z -= out[i] * 0.9;
    AddGrid(vs, at, rCount, mCount, part, false, false, nullptr);
    paired_.push_back(patch);
}

void SeaModel::BuildWingVerts(const WingGenes& w, double side, double phase, double amplitude,
                              std::vector<Vec3>& out) const {
    const int rows = 7;
    const int cols = Max(8, finCols_);
    out.assign((size_t)rows * cols, Vec3());

    const double chord = w.end - w.start;
    const double tip = w.start + w.sweep * chord;
    std::vector<double> aFront(rows), aBack(rows);
    for (int r = 0; r < rows; r++) {
        const double rr = rows > 1 ? (double)r / (rows - 1) : 0.0;
        switch (w.shape) {
            case WingShape::Round: {
                const double k = std::sqrt(Max(0.0, 1.0 - rr * rr));
                aFront[r] = tip - (tip - w.start) * k;
                aBack[r] = tip + (w.end - tip) * k;
                break;
            }
            case WingShape::Diamond: {
                const double k = 1.0 - Saturate((rr - 0.8) / 0.2) * Saturate((rr - 0.8) / 0.2) * 0.5;
                double af = w.start + (tip - w.start) * std::pow(rr, 0.9);
                double ab = w.end + (tip - w.end) * std::pow(rr, 1.25);
                aFront[r] = tip - (tip - af) * k;
                aBack[r] = tip + (ab - tip) * k;
                break;
            }
            default: {  // pointed (and the hammerhead bar handled separately)
                aFront[r] = w.start + (tip - w.start) * std::pow(rr, 2.6);
                aBack[r] = w.end + (tip - w.end) * std::pow(rr, 1.6);
                break;
            }
        }
    }

    const double dr = rows > 1 ? 1.0 / (rows - 1) : 0.0;
    const double lag = w.wave < 0.6 ? 0.9 : 0.3;
    std::vector<double> accumOut((size_t)rows * cols, 0.0), accumUp((size_t)rows * cols, 0.0);
    for (int r = 0; r < rows; r++) {
        const double rr = rows > 1 ? (double)r / (rows - 1) : 0.0;
        for (int c = 0; c < cols; c++) {
            const double m = cols > 1 ? (double)c / (cols - 1) : 0.0;
            const double a = aFront[r] + (aBack[r] - aFront[r]) * m;
            // render.py uses root_z from the ROOT row (r=0) for the whole wing,
            // not the current row's half-width.
            const double rootA = aFront[0] + (aBack[0] - aFront[0]) * m;
            const double rootZ = HalfWidth(Clamp(rootA, 0.0, 1.0)) * 0.92;
            const double wave = std::sin(kTwoPi * (phase - w.wave * m) - lag * std::pow(rr, 1.5));
            const double ang = 1.1 * w.flap * amplitude * wave * (0.2 + 0.8 * std::pow(rr, 1.2)) + w.dihedral;
            const size_t idx = (size_t)r * cols + c;
            const size_t prev = (size_t)(r - 1) * cols + c;
            // render.py's dr is np.diff(r, prepend=0.0): the ROOT row advances by
            // zero, so the wing starts exactly on the flank. Giving row 0 a full
            // step pushed every wing span/6 (5-10% of a body length) off the body,
            // which widened a ray's disc, shrank its fitted scale and left the
            // wing-root seam.
            const double step = r == 0 ? 0.0 : w.span * dr;
            accumOut[idx] = (r == 0 ? 0.0 : accumOut[prev]) + step * std::cos(ang);
            accumUp[idx] = (r == 0 ? 0.0 : accumUp[prev]) + step * std::sin(ang);
            out[idx] = Vec3(a, accumUp[idx], side * (rootZ + accumOut[idx]));
        }
    }
}

void SeaModel::BuildWings(const WingGenes& wing, bool flaps, int partNear, int partFar) {
    if (!wing.present) return;
    for (int s = 0; s < 2; s++) {
        const double side = s == 0 ? 1.0 : -1.0;
        const int part = s == 0 ? partNear : partFar;
        std::vector<Vec3> vs;
        BuildWingVerts(wing, side, 0.0, 0.0, vs);
        const int rows = 7;
        const int cols = Max(8, finCols_);
        std::vector<Vec4> at((size_t)rows * cols);
        for (int r = 0; r < rows; r++) {
            const double rr = rows > 1 ? (double)r / (rows - 1) : 0.0;
            for (int c = 0; c < cols; c++) {
                const double m = cols > 1 ? (double)c / (cols - 1) : 0.0;
                at[(size_t)r * cols + c] = Vec4(vs[(size_t)r * cols + c].x, rr, side, m);
            }
        }
        WingPatch patch;
        patch.start = AddGrid(vs, at, rows, cols, part, false, side > 0, nullptr);
        patch.rows = rows;
        patch.cols = cols;
        patch.genes = wing;
        patch.side = side;
        patch.flaps = flaps;
        wingPatches_.push_back(patch);
    }
}

void SeaModel::BuildArmVerts(const ArmPatch& patch, double phase, double amplitude,
                             std::vector<Vec3>& out, std::vector<Vec3>* axis) const {
    const int rows = patch.rows, cols = patch.cols;
    out.assign((size_t)rows * cols, Vec3());
    if (axis != nullptr) axis->assign((size_t)rows * cols, Vec3());

    const double e0 = std::sin(patch.theta), e1 = std::cos(patch.theta);
    const double ePerp0 = e1, ePerp1 = -e0;
    const double minR = 1.1 / Max((double)detail_, 8.0);
    const double cyc = kTwoPi * phase;
    const double open = 1.0 + 2.5 * g_.pulse * amplitude * std::cos(cyc);
    const double ts = kTwoPi / cols;

    for (int r = 0; r < rows; r++) {
        const double s = rows > 1 ? (double)r / (rows - 1) : 0.0;
        const double A = patch.a0 + patch.length * s;
        double rad = Max(patch.thick * (1.0 - 0.85 * std::pow(s, 0.9)), minR);
        if (patch.tentacle) {
            const double d = (s - 0.9) / 0.07;
            rad = Max(rad, patch.thick * 1.3 * std::exp(-d * d));
        }
        const double rho = patch.rRoot + g_.arms.spread * patch.length * 0.35 * std::pow(s, 0.8) * open;
        const double writhe = g_.arms.curl * patch.length * 0.25 * std::pow(s, 1.5) *
                              std::sin(cyc - kTwoPi * 1.1 * s + patch.phi) * (0.5 + 0.5 * amplitude);
        const double bob = g_.arms.curl * patch.length * 0.12 * s * s *
                           std::cos(cyc - kTwoPi * 0.8 * s + patch.phi);
        const double cy = e0 * (rho + bob) + ePerp0 * writhe;
        const double cz = e1 * (rho + bob) + ePerp1 * writhe;
        for (int c = 0; c < cols; c++) {
            const double t = ts * c;
            const size_t idx = (size_t)r * cols + c;
            out[idx] = Vec3(A, cy + rad * std::sin(t), cz + rad * std::cos(t));
            if (axis != nullptr) (*axis)[idx] = Vec3(A, cy, cz);
        }
    }
}

void SeaModel::BuildArms() {
    const int n = g_.arms.count;
    if (n <= 0 && g_.arms.tentacles <= 0) return;
    for (int k = 0; k < n + g_.arms.tentacles; k++) {
        // numpy default_rng(seed*31 + k), the same stream as _arm_fn.
        NumpyRandom r(g_.seed * 31 + k);
        ArmPatch patch;
        patch.tentacle = k >= n;
        double length, thick;
        if (patch.tentacle) {
            const int j = k - n;
            patch.theta = kPi * ((j % 2 == 0) ? 0.75 : 0.25) + kPi * 0.5 * (j / 2);
            length = g_.arms.tentacleLength;
            thick = g_.arms.thickness * 0.5;
        } else {
            patch.theta = kTwoPi * (k + 0.5) / Max(1, n);
            length = g_.arms.length * r.Uniform(0.9, 1.1);
            thick = g_.arms.thickness;
        }
        patch.phi = r.Uniform(0.0, kTwoPi);
        patch.length = length;
        patch.thick = thick;
        patch.a0 = 0.96;
        patch.rows = ClampI(detail_ / 2, 10, 220);
        patch.cols = ClampI(detail_ / 12, 6, 16);
        patch.rRoot = 0.6 * Min(Min(Top(patch.a0), HalfWidth(patch.a0)), Bottom(patch.a0) * 1.5);

        std::vector<Vec3> vs, ax;
        BuildArmVerts(patch, 0.0, 1.0, vs, &ax);
        std::vector<Vec4> at((size_t)patch.rows * patch.cols);
        const double ts = kTwoPi / patch.cols;
        for (int rr = 0; rr < patch.rows; rr++) {
            const double s = patch.rows > 1 ? (double)rr / (patch.rows - 1) : 0.0;
            const double A = patch.a0 + patch.length * s;
            for (int c = 0; c < patch.cols; c++) {
                const double t = ts * c;
                const double inner = -(std::sin(t) * std::sin(patch.theta) +
                                       std::cos(t) * std::cos(patch.theta));
                at[(size_t)rr * patch.cols + c] = Vec4(A, std::sin(t), std::cos(t), inner);
            }
        }
        patch.start = AddGrid(vs, at, patch.rows, patch.cols, PartArm, true, false, &ax);
        armPatches_.push_back(patch);
    }
}

void SeaModel::BuildLobes() {
    if (g_.lobes == LobeShape::None || g_.lobeLength <= 0) return;
    const int nr = 8, nm = 7;
    const double L = g_.lobeLength;
    const double aRoot = 0.04;
    const double hw0 = HalfWidth(aRoot);
    const double h0 = Top(aRoot) + Bottom(aRoot);

    for (int s = 0; s < 2; s++) {
        const double side = s == 0 ? 1.0 : -1.0;
        std::vector<Vec3> vs((size_t)nr * nm);
        std::vector<Vec4> at((size_t)nr * nm);
        for (int r = 0; r < nr; r++) {
            const double rr = (double)r / (nr - 1);
            const double aC = aRoot - (L + aRoot) * rr;
            for (int c = 0; c < nm; c++) {
                const double m = -1.0 + 2.0 * (double)c / (nm - 1);
                const size_t idx = (size_t)r * nm + c;
                if (g_.lobes == LobeShape::Horns) {
                    const double hgt = 0.5 * h0 * (1.0 - 0.55 * std::pow(rr, 1.5)) *
                                       std::pow(std::sqrt(Saturate(1.0 - m * m)), 0.3);
                    const double y = -0.25 * h0 - 0.35 * h0 * rr * rr + m * hgt * 0.5;
                    const double z = side * (hw0 * 0.8 - 0.35 * hw0 * rr * rr + 0.01 * (1 - m * m));
                    vs[idx] = Vec3(aC, y, z);
                } else {
                    const double zc = 0.5 * hw0;
                    const double wid = 0.5 * hw0 * std::sqrt(Clamp(1.0 - rr * rr, 0.04, 1.0));
                    vs[idx] = Vec3(aC, -0.3 * h0 - 0.1 * h0 * rr, side * (zc + m * wid));
                }
                at[idx] = Vec4(aC, rr, side, (m + 1.0) / 2.0);
            }
        }
        AddGrid(vs, at, nr, nm, PartLobe, false, false, nullptr);
    }
}

std::vector<double> SeaModel::JawRow(double ai) const {
    const int n = ringVerts_;
    const int fineN = 2048;
    const double hi = jaw_.Line(ai);
    const double lo = Max(-1.0, hi - jaw_.band);
    const double tHi = std::asin(Clamp(hi, -1.0, 1.0));
    const double tLo = std::asin(lo);
    const double mid = 0.5 * (tHi + tLo);
    const double half = 0.5 * (tHi - tLo) + 0.05;
    std::vector<double> fine((size_t)fineN + 1, 0.0), dens((size_t)fineN, 0.0),
        cdf((size_t)fineN + 1, 0.0);
    for (int j = 0; j < fineN; j++) {
        fine[(size_t)j] = kTwoPi * j / fineN;
        dens[(size_t)j] = 1.0;
    }
    fine[(size_t)fineN] = kTwoPi;
    for (int c = 0; c < 2; c++) {
        const double cc = c == 0 ? mid : kPi - mid;
        for (int j = 0; j < fineN; j++) {
            const double d = std::atan2(std::sin(fine[(size_t)j] - cc),
                                        std::cos(fine[(size_t)j] - cc));
            dens[(size_t)j] += 6.0 * SmoothBump(d / half);
        }
    }
    for (int j = 0; j < fineN; j++) cdf[(size_t)j + 1] = cdf[(size_t)j] + dens[(size_t)j];
    const double total = cdf[(size_t)fineN];
    for (int j = 0; j <= fineN; j++) cdf[(size_t)j] /= (total > 0 ? total : 1.0);
    std::vector<double> row((size_t)n, 0.0);
    for (int j = 0; j < n; j++) {
        const double target = (double)j / n;
        int a0 = 0, a1 = fineN;
        while (a0 + 1 < a1) {
            const int m = (a0 + a1) / 2;
            if (cdf[(size_t)m] <= target) a0 = m; else a1 = m;
        }
        const double span = cdf[(size_t)a0 + 1] - cdf[(size_t)a0];
        const double f = span > 1e-12 ? (target - cdf[(size_t)a0]) / span : 0.0;
        row[(size_t)j] = Lerp(fine[(size_t)a0], fine[(size_t)a0 + 1], f);
    }
    const double targets[4] = {tHi, tLo, kPi - tHi, kPi - tLo};
    for (int k = 0; k < 4; k++) {
        const double target = std::fmod(targets[k] + kTwoPi, kTwoPi);
        int best = 0;
        double bestD = 1e30;
        for (int j = 0; j < n; j++) {
            const double d = std::fabs(std::atan2(std::sin(row[(size_t)j] - target),
                                                  std::cos(row[(size_t)j] - target)));
            if (d < bestD) { bestD = d; best = j; }
        }
        row[(size_t)best] = target;
    }
    std::sort(row.begin(), row.end());
    return row;
}

Vec3 SeaModel::MouthRim(double a, double s, double side) const {
    const double v = jaw_.Line(a) - s * jaw_.band;
    const double y = (v >= 0 ? Top(a) : Bottom(a)) * v;
    const double z = side * HalfWidth(a) * std::sqrt(Clamp(1.0 - v * v, 0.0, 1.0));
    return Vec3(a, y, z);
}

void SeaModel::BuildMouth() {
    std::vector<double> a((size_t)rings_);
    for (int i = 0; i < rings_; i++)
        a[(size_t)i] = std::pow(rings_ > 1 ? (double)i / (rings_ - 1) : 0.0, 1.15);
    const double aOpen = jaw_.Front() + 0.3 * (jaw_.a1 - jaw_.Front());
    std::vector<double> cavity, tip;
    for (double x : a) if (x >= aOpen) cavity.push_back(x);
    for (double x : a) { if (x <= aOpen) tip.push_back(x); else break; }
    if (tip.size() < 2) tip.push_back(aOpen);

    const int S = 11;
    for (int sideI = 0; sideI < 2; sideI++) {
        const double side = sideI == 0 ? 1.0 : -1.0;
        std::vector<Vec3> vs(cavity.size() * (size_t)S);
        std::vector<Vec4> at(cavity.size() * (size_t)S);
        for (size_t i = 0; i < cavity.size(); i++) {
            for (int j = 0; j < S; j++) {
                const double s = (double)j / (S - 1);
                Vec3 p = MouthRim(cavity[i], s, side);
                p.z *= 1.0 - 0.85 * std::pow(std::sin(kPi * s), 0.7);
                const size_t idx = i * (size_t)S + (size_t)j;
                vs[idx] = p;
                at[idx] = Vec4(cavity[i], s, side, 0.0);
            }
        }
        AddGrid(vs, at, (int)cavity.size(), S, PartMouth, false, false, nullptr);
    }

    const int U = 7;
    for (int sf = 0; sf < 2; sf++) {
        const double sv = (double)sf;
        std::vector<Vec3> vs(tip.size() * (size_t)U);
        std::vector<Vec4> at(tip.size() * (size_t)U);
        for (size_t i = 0; i < tip.size(); i++) {
            const double inner = sf == 0 ? Top(tip[i]) : -Bottom(tip[i]);
            for (int j = 0; j < U; j++) {
                const double u = (double)j / (U - 1);
                Vec3 p = MouthRim(tip[i], sv, 1.0);
                p.z *= 1.0 - 2.0 * u;
                p.y += 0.4 * (inner - p.y) * std::sin(kPi * u);
                const size_t idx = i * (size_t)U + (size_t)j;
                vs[idx] = p;
                at[idx] = Vec4(tip[i], sv, 0.0, 0.0);
            }
        }
        AddGrid(vs, at, (int)tip.size(), U, PartMouth, false, false, nullptr);
    }

    const double front = g_.kind == SeaKind::Shark ? jaw_.a0 : 0.01;
    const double span = jaw_.a1 - front;
    const double per = Max(span / 8.0, 2.0 / (0.8 * detail_));
    const int n = Max(2, (int)(span / per));
    std::vector<double> ta((size_t)n);
    for (int i = 0; i < n; i++) ta[(size_t)i] = front + (i + 0.35) * span / n;
    teeth_.clear();
    for (int sideI = 0; sideI < 2; sideI++) {
        const double side = sideI == 0 ? 1.0 : -1.0;
        for (int upperI = 0; upperI < 2; upperI++) {
            const bool upper = upperI == 0;
            for (int i = 0; i < n; i++) {
                const double x = upper ? ta[(size_t)i] : ta[(size_t)i] + 0.5 * span / n;
                if (x >= jaw_.a1 - 0.3 * span / n) continue;
                SeaTooth t;
                t.x = x;
                t.side = side;
                t.upper = upper;
                t.per = span / n;
                t.index = i;
                teeth_.push_back(t);
            }
        }
    }
    teethStart_ = (int)mesh_.verts.size();
    for (size_t k = 0; k < teeth_.size(); k++) {
        std::vector<Vec3> vs(4, Vec3());
        std::vector<Vec4> at(4);
        for (int j = 0; j < 4; j++)
            at[(size_t)j] = Vec4(teeth_[k].x, j < 2 ? 0.0 : 1.0, 0.0, 0.0);
        AddGrid(vs, at, 2, 2, PartTeeth, false, false, nullptr);
    }
    teethCount_ = (int)mesh_.verts.size() - teethStart_;
}

std::vector<Vec3> SeaModel::TeethPose(const SeaPose& pose) const {
    std::vector<Vec3> out;
    out.reserve(teeth_.size() * 4);
    for (const SeaTooth& t : teeth_) {
        const double xs[3] = {t.x - 0.01, t.x, t.x + 0.01};
        std::vector<Vec3> pts(6);
        for (int i = 0; i < 3; i++) {
            pts[(size_t)i] = MouthRim(xs[i], 0.0, t.side * 0.93);
            pts[(size_t)(3 + i)] = MouthRim(xs[i], 1.0, t.side * 0.93);
        }
        Deform(pts, pose);
        const Vec3* top = &pts[0];
        const Vec3* bottom = &pts[3];
        const Vec3* base = t.upper ? top : bottom;
        const Vec3* other = t.upper ? bottom : top;
        const Vec3 gap = other[1] - base[1];
        double length;
        if (g_.kind == SeaKind::Shark) {
            length = gap.Length() * (t.upper ? 0.5 : 0.4);
        } else {
            const double alongJaw = (t.x - jaw_.Front()) / Max(jaw_.a1 - jaw_.Front(), 1e-9);
            const double size = (t.index % 2 == 0 ? 1.0 : 0.5) * (1.3 - 0.6 * alongJaw);
            length = gap.Length() * Min(0.85, (t.upper ? 0.55 : 0.45) * size);
        }
        Vec3 along = (base[2] - base[0]).NormalizedOr(Vec3(1, 0, 0));
        const double width = Min(0.85 * t.per, 1.1 * length + 1e-4);
        const Vec3 dir = gap.NormalizedOr(Vec3(0, 1, 0));
        const Vec3 tip = base[1] + dir * length;
        out.push_back(base[1] - along * (width / 2));
        out.push_back(base[1] + along * (width / 2));
        out.push_back(tip);
        out.push_back(tip);
    }
    return out;
}

void SeaModel::BuildSpineLines(double phase, double inflate, std::vector<Vec3>& out) const {
    out.clear();
    if (g_.puffSpines > 0) {
        if (inflate <= 0.05) return;
        // Python: default_rng(seed + 7); the n angle jitters are drawn first,
        // then the n length jitters, so the stream cannot interleave.
        NumpyRandom r(g_.seed + 7);
        const int n = g_.puffSpines;
        std::vector<double> angles((size_t)n), lengths((size_t)n);
        for (int i = 0; i < n; i++)
            angles[(size_t)i] = kPi * (3.0 - std::sqrt(5.0)) * (i + 0.5) + r.Uniform(-0.2, 0.2);
        for (int i = 0; i < n; i++) lengths[(size_t)i] = r.Uniform(0.8, 1.15);
        for (int i = 0; i < n; i++) {
            const double t = (i + 0.5) / n;
            const double cy = 1.0 - 2.0 * t;
            const double ring = std::sqrt(Max(0.0, 1.0 - cy * cy));
            const double ang = angles[(size_t)i];
            const double cx = ring * std::cos(ang), cz = ring * std::sin(ang);
            const double ca = 0.06 + 0.84 * (0.5 + 0.5 * cx);
            const Vec3 base(ca, (cy > 0 ? Top(ca) : Bottom(ca)) * cy, HalfWidth(ca) * cz);
            Vec3 d(cx * 0.7 + 0.25, cy, cz);
            d = d.NormalizedOr(Vec3(0, 1, 0));
            const double len = 0.16 * g_.puff * std::pow(inflate, 1.5) * lengths[(size_t)i];
            out.push_back(base);
            out.push_back(base + d * len);
        }
        return;
    }
    if (g_.spines.count <= 0) return;
    const int n = ClampI(g_.spines.count, 0, Max(12, (int)std::round(detail_ * 0.8)));
    if (n <= 0) return;
    // numpy default_rng(seed), with the reference's draw order: twist, then
    // the n lat jitters, the n lon jitters, the n lengths, then 3n normals.
    NumpyRandom rng(g_.seed);
    const int rows = Max(3, (int)std::round(std::sqrt(n / 3.0)));
    std::vector<double> lat((size_t)rows), perRow((size_t)rows), share((size_t)rows);
    double cosSum = 0;
    for (int i = 0; i < rows; i++) {
        lat[(size_t)i] = rows > 1 ? -1.05 + (1.2 + 1.05) * i / (rows - 1) : 0.0;
        cosSum += std::cos(lat[(size_t)i]);
    }
    int assigned = 0;
    for (int i = 0; i < rows; i++) {
        share[(size_t)i] = std::cos(lat[(size_t)i]) / Max(cosSum, 1e-9) * n;
        perRow[(size_t)i] = std::floor(share[(size_t)i]);
        assigned += (int)perRow[(size_t)i];
    }
    // Python adds the remainder to the rows with the largest deficit
    // (argsort(per_row - share)[:n - per_row.sum()]).
    {
        std::vector<int> order((size_t)rows);
        for (int i = 0; i < rows; i++) order[(size_t)i] = i;
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return (perRow[(size_t)a] - share[(size_t)a]) < (perRow[(size_t)b] - share[(size_t)b]);
        });
        for (int i = 0; i < rows && assigned < n; i++) {
            perRow[(size_t)order[(size_t)i]] += 1;
            assigned++;
        }
    }
    const double twist = rng.Uniform(0.0, kTwoPi);
    std::vector<double> latS, lon;
    for (int k = 0; k < rows; k++) {
        for (int j = 0; j < (int)perRow[(size_t)k]; j++) {
            latS.push_back(lat[(size_t)k]);
            lon.push_back(twist + kTwoPi * (j + 0.5 * (k % 2)) / Max(1.0, perRow[(size_t)k]));
        }
    }
    while ((int)latS.size() > n) { latS.pop_back(); lon.pop_back(); }
    const int count = (int)latS.size();
    std::vector<double> latJ((size_t)count), lonJ((size_t)count), lenJ((size_t)count);
    for (int i = 0; i < count; i++) latJ[(size_t)i] = rng.Uniform(-0.04, 0.04);
    for (int i = 0; i < count; i++) lonJ[(size_t)i] = rng.Uniform(-0.06, 0.06);
    for (int i = 0; i < count; i++) lenJ[(size_t)i] = rng.Uniform(0.88, 1.1);
    std::vector<Vec3> normals((size_t)count);
    for (int i = 0; i < count; i++)
        normals[(size_t)i] = Vec3(rng.Normal(), rng.Normal(), rng.Normal());
    for (int i = 0; i < count; i++) {
        const double l = latS[(size_t)i] + latJ[(size_t)i];
        const double o = lon[(size_t)i] + lonJ[(size_t)i];
        const double cy = std::sin(l);
        const double cx = std::cos(l) * std::cos(o);
        const double cz = std::cos(l) * std::sin(o);
        const double ca = 0.5 + 0.5 * cx;
        const Vec3 base(ca, (cy > 0 ? Top(ca) : Bottom(ca)) * cy, HalfWidth(ca) * cz);
        const Vec3 d(cx, cy, cz);
        Vec3 perp = Cross(d, normals[(size_t)i]).NormalizedOr(Vec3(1, 0, 0));
        const double lean = g_.spines.sway * std::sin(kTwoPi * phase + kTwoPi * (ca + 0.3 * cy));
        double len = g_.spines.length * lenJ[(size_t)i];
        if (cy < -0.6) len *= 0.6;
        len *= 1.0 - 0.2 * Saturate((l - 0.8) / 0.4);
        out.push_back(base);
        out.push_back(base + (d + perp * lean) * len);
    }
}

void SeaModel::Build() {
    // ---------------------------------------------------------- body tube
    std::vector<Vec3> body((size_t)rings_ * ringVerts_);
    std::vector<Vec4> bodyAt((size_t)rings_ * ringVerts_);
    std::vector<Vec3> bodyAxis((size_t)rings_ * ringVerts_);
    std::vector<std::vector<double>> jawRows;
    if (hasJaw_) {
        jawRows.resize((size_t)rings_);
        for (int i = 0; i < rings_; i++) {
            const double a = std::pow(rings_ > 1 ? (double)i / (rings_ - 1) : 0.0, 1.15);
            jawRows[(size_t)i] = JawRow(a);
        }
    }
    for (int i = 0; i < rings_; i++) {
        const double a = std::pow(rings_ > 1 ? (double)i / (rings_ - 1) : 0.0, 1.15);
        for (int j = 0; j < ringVerts_; j++) {
            const double t = hasJaw_ ? jawRows[(size_t)i][(size_t)j] : kTwoPi * j / ringVerts_;
            const double st = std::sin(t), ct = std::cos(t);
            const double y = (st > 0 ? Top(a) : Bottom(a)) * st;
            const double z = HalfWidth(a) * ct;
            const size_t idx = (size_t)i * ringVerts_ + j;
            body[idx] = Vec3(a, y, z);
            bodyAt[idx] = Vec4(a, st, ct, 0.0);
            bodyAxis[idx] = Vec3(a, 0, 0);
        }
    }
    const size_t bodyTriStart = mesh_.tris.size();
    AddGrid(body, bodyAt, rings_, ringVerts_, PartBody, true, false, &bodyAxis);
    if (hasJaw_) {
        // render.py cuts the strip of skin under the mouth line out of the body,
        // so the cavity/teeth built by _build_mouth() are actually visible.
        std::vector<unsigned char> gone(body.size(), 0);
        for (size_t i = 0; i < body.size(); i++) {
            const Vec4& b = bodyAt[i];
            const double q = (jaw_.Line(b.x) - b.y) / jaw_.band;
            gone[i] = (q > -1e-6 && q < 1.0 + 1e-6 && b.x <= jaw_.a1 && b.x >= jaw_.Front()) ? 1 : 0;
        }
        std::vector<int> keptTris, keptParts;
        const size_t triCount = mesh_.tris.size() / 3;
        for (size_t t = 0; t < triCount; t++) {
            const int i0 = mesh_.tris[t * 3 + 0];
            const int i1 = mesh_.tris[t * 3 + 1];
            const int i2 = mesh_.tris[t * 3 + 2];
            const bool drop = (size_t)t * 3 >= bodyTriStart &&
                              gone[(size_t)i0] && gone[(size_t)i1] && gone[(size_t)i2];
            if (drop) continue;
            keptTris.push_back(i0);
            keptTris.push_back(i1);
            keptTris.push_back(i2);
            keptParts.push_back(mesh_.triPart[t]);
        }
        mesh_.tris.swap(keptTris);
        mesh_.triPart.swap(keptParts);
    }

    // ---------------------------------------------------------- median fins
    BuildMedianFin(g_.dorsal, PartDorsal, 1.0);
    BuildMedianFin(g_.dorsal2, PartDorsal, 1.0);
    BuildMedianFin(g_.anal, PartAnal, -1.0);

    if (g_.finlets > 0) {
        for (int s = 0; s < 2; s++) {
            const double sign = s == 0 ? 1.0 : -1.0;
            const int part = s == 0 ? PartDorsal : PartAnal;
            const FinGenes& after = s == 0 ? (g_.dorsal2.present ? g_.dorsal2 : g_.dorsal) : g_.anal;
            const double a0 = Max(after.end + 0.02, 0.7);
            const double step = (0.97 - a0) / g_.finlets;
            for (int i = 0; i < g_.finlets; i++) {
                const double s0 = a0 + i * step;
                const double h = 0.6 * (sign > 0 ? Top(s0) : Bottom(s0)) * 0.5 + 0.006;
                FinGenes fin;
                fin.present = true;
                fin.start = s0;
                fin.end = s0 + step * 0.8;
                fin.height = h;
                fin.shape = FinShape::Triangle;
                fin.sweep = 0.9;
                BuildMedianFin(fin, part, sign);
            }
        }
    }

    // ---------------------------------------------------------- caudal fin
    BuildCaudal();

    if (hasJaw_) BuildMouth();

    // ---------------------------------------------------------- paired fins
    const bool blade = g_.pectoralShape == PectoralShape::Blade;
    const double pectA = Min(g_.eyePos + 0.16, g_.peak);
    const double pectY = -Bottom(pectA) * (blade ? 0.7 : 0.35);
    if (g_.pectoralSize > 0.01) {
        const double droop = blade ? g_.pectoralDroop : 0.45;
        const double splay = blade ? 0.12 : 0.35;
        BuildPairedFin(PartPectoralNear, 1.0, pectA, pectY, g_.pectoralSize, droop, splay, blade);
        BuildPairedFin(PartPectoralFar, -1.0, pectA, pectY, g_.pectoralSize, droop, splay, blade);
    }
    if (g_.pelvic.present) {
        const double pa = g_.pelvic.start;
        const double py = -Bottom(pa) * 0.9;
        BuildPairedFin(PartPelvicNear, 1.0, pa, py, g_.pelvic.height * 1.4, 1.0, 0.15, false);
        BuildPairedFin(PartPelvicFar, -1.0, pa, py, g_.pelvic.height * 1.4, 1.0, 0.15, false);
    }

    // ---------------------------------------------------------- wings / hammer
    if (g_.wings.present) BuildWings(g_.wings, true, PartWingNear, PartWingFar);
    if (g_.hammer > 0) {
        WingGenes hammer;
        hammer.present = true;
        hammer.start = -0.01;
        hammer.end = 0.085;
        hammer.span = g_.hammer;
        hammer.sweep = 0.45;
        hammer.shape = WingShape::Pointed;
        hammer.flap = 0.0;
        hammer.wave = 0.0;
        // A hammerhead's head is a flat sheet with the eyes on its tips: build it as a
        // boxy wing by hand (the generic pointed wing tapers too much to read as a T).
        for (int s = 0; s < 2; s++) {
            const double side = s == 0 ? 1.0 : -1.0;
            const int nr = 5, nc = 8;
            std::vector<Vec3> vs((size_t)nr * nc);
            std::vector<Vec4> at((size_t)nr * nc);
            for (int r = 0; r < nr; r++) {
                const double rr = (double)r / (nr - 1);
                const double a = -0.01 + 0.09 * rr;
                const double reach = side * g_.hammer * (0.45 + 0.55 * rr);
                for (int c = 0; c < nc; c++) {
                    const double m = (double)c / (nc - 1);
                    const size_t idx = (size_t)r * nc + c;
                    vs[idx] = Vec3(a, 0.0, reach * (0.55 + 0.45 * m));
                    at[idx] = Vec4(a, rr, side, m);
                }
            }
            AddGrid(vs, at, nr, nc, s == 0 ? PartWingNear : PartWingFar, false, side > 0, nullptr);
        }
    }

    // ---------------------------------------------------------- lobes
    BuildLobes();

    // ---------------------------------------------------------- arms
    BuildArms();

    // ---------------------------------------------------------- eyes
    const double ea = g_.eyePos;
    const double ey = Top(ea) * g_.eyeHeight;
    double ez = HalfWidth(ea) * std::sqrt(Max(0.0, 1.0 - g_.eyeHeight * g_.eyeHeight)) * 1.05;
    const double er = g_.eyeSize * g_.height * 0.5;
    eyes_.clear();
    if (g_.eyeStyle != EyeStyle::None) {
        if (g_.hammer > 0) {
            EyeLocal e;
            e.a = 0.04;
            e.y = 0.0;
            e.z = HalfWidth(0.04) * 0.92 + g_.hammer * 0.95;
            e.radius = er;
            eyes_.push_back(e);
            e.z = -e.z;
            eyes_.push_back(e);
        } else {
            EyeLocal e;
            e.a = ea;
            e.y = ey;
            e.z = ez;
            e.radius = er;
            eyes_.push_back(e);
            e.z = -ez;
            eyes_.push_back(e);
        }
    }
}

double SeaModel::RealExtent() const {
    if (g_.profile == "sphere") return 1.0;
    if (g_.kind == SeaKind::Ray && !wingPatches_.empty()) {
        double lo = 1e9, hi = -1e9;
        for (const WingPatch& patch : wingPatches_) {
            for (int i = 0; i < patch.rows * patch.cols; i++) {
                const double z = mesh_.verts[(size_t)patch.start + i].z;
                lo = Min(lo, z);
                hi = Max(hi, z);
            }
        }
        if (hi > lo) return hi - lo;
    }
    double hi = 0.0, lo = 0.0;
    for (const Vec3& v : mesh_.verts) {
        hi = Max(hi, v.x);
        lo = Min(lo, v.x);
    }
    return hi - Min(0.0, lo);
}

// ---------------------------------------------------------------- posing
void SeaModel::Spine(std::vector<double>& a, std::vector<Vec3>& pos, std::vector<double>& heading,
                     const SeaPose& pose) const {
    const int n = (g_.arms.count > 0 || g_.arms.tentacles > 0) ? 200 : 140;
    a.resize(n);
    pos.assign(n, Vec3());
    heading.resize(n);

    const double amp = g_.swimAmplitude * pose.amplitude;
    const double cyc = kTwoPi * pose.phase;
    std::vector<double> bend(n);
    for (int i = 0; i < n; i++) {
        const double s = n > 1 ? (double)i / (n - 1) : 0.0;
        const double av = tailEnd_ * s;
        a[i] = av;
        const double env = std::pow(Max(0.0, (av - 0.15) / 0.85), 1.4);
        double wave = amp * env * std::sin(cyc - kTwoPi * g_.waveCount * av);
        const double tail = tailEnd_ > 1.0 ? Saturate((av - 1.0) / (tailEnd_ - 1.0)) : 0.0;
        wave += amp * 0.9 * tail * std::sin(cyc - kTwoPi * g_.waveCount - 0.9);
        const double headWobble = 0.12 * amp * std::sin(cyc + kPi * 0.85);
        // Turn animations drive the sweep with phase: the head reaches the new
        // heading first, the tail follows a lagged fraction of the same path.
        const double base = pose.turning
                                ? TurnHeading(pose.turn, av / tailEnd_, pose.turnFrom, pose.turnTo, pose.turnLag)
                                : pose.yaw;
        const double b = headWobble + wave;
        bend[i] = b;
        heading[i] = base + (g_.swimVertical ? 0.0 : b);
    }

    for (int i = 1; i < n; i++) {
        const double da = a[i] - a[i - 1];
        const double mid = 0.5 * (heading[i] + heading[i - 1]);
        pos[i] = pos[i - 1] - Vec3(std::cos(mid) * da, 0.0, std::sin(mid) * da);
        if (g_.swimVertical) {
            const double tilt = 0.75 * 0.5 * (bend[i] + bend[i - 1]);
            pos[i].y = pos[i - 1].y + std::sin(tilt) * da;
        }
    }

    if (g_.kind == SeaKind::Ray) {
        const double heave = 0.12 * g_.wings.span * g_.wings.flap * pose.amplitude;
        for (int i = 0; i < n; i++) {
            pos[i].y += -heave * std::sin(cyc - 0.6 - 2.5 * Max(0.0, a[i] - 0.5));
        }
    }
}

void SeaModel::Deform(std::vector<Vec3>& verts, const SeaPose& pose) const {
    if (hasJaw_) {
        // render.py's _open_jaw(): the lower jaw swings about the mouth corner.
        const double gape = jaw_.gape * (0.85 + 0.15 * std::sin(kTwoPi * pose.phase));
        for (Vec3& p : verts) {
            const double a = p.x;
            const double hinge = jaw_.Hinge(a);
            const double ac = Clamp(a, 0.0, 1.0);
            const double denom = Max(p.y >= 0 ? Top(ac) : Bottom(ac), 1e-4);
            const double v = p.y / denom;
            const double q = Clamp((jaw_.Line(a) - v) / jaw_.band, 0.0, 1.0);
            p.y -= gape * hinge * q * q * (3.0 - 2.0 * q);
        }
    }
    const bool puff = g_.puff > 0 && pose.inflate > 0;
    if (!puff && g_.flatHead <= 0) return;
    for (Vec3& p : verts) {
        const double a = Clamp(p.x, 0.0, 1.0);
        if (puff) {
            const double k = pose.inflate * Saturate((1.0 - a) / 0.2);
            const double ball = std::sqrt(Saturate(1.0 - ((a - 0.53) / 0.5) * ((a - 0.53) / 0.5)));
            const double target = 0.5 * g_.puff * ball;
            auto grow = [&](double c, double surf) {
                surf = Max(surf, 1e-4);
                const double f = 1.0 + k * (Max(target / surf, 1.0) - 1.0);
                if (std::fabs(c) <= surf * 1.001) return c * f;
                return c + (c < 0 ? -1.0 : 1.0) * surf * (f - 1.0);
            };
            p.y = grow(p.y, p.y >= 0 ? Top(a) : Bottom(a));
            p.z = grow(p.z, HalfWidth(a));
        }
        if (g_.flatHead > 0) p.y += HeadLift(a);
    }
}

void SeaModel::Pose(const SeaPose& pose, SeaPosed& out) const {
    std::vector<Vec3> local = mesh_.verts;

    // Paired fins flap: rotate their outward offset around the root line.
    const double flap = 0.5 + 0.35 * g_.pectoralFlap * std::sin(kTwoPi * (pose.phase * 2.0 + 0.25));
    for (const PairedPatch& patch : paired_) {
        const double k = (patch.part == PartPectoralNear || patch.part == PartPectoralFar) ? flap : 0.6;
        for (int i = 0; i < patch.count; i++) {
            local[(size_t)patch.start + i].z += patch.out[(size_t)i] * (0.3 + k);
        }
    }
    // Wings and arms are pose-dependent; rebuild their vertex blocks.
    for (const WingPatch& patch : wingPatches_) {
        if (!patch.flaps) continue;
        std::vector<Vec3> vs;
        BuildWingVerts(patch.genes, patch.side, pose.phase, pose.amplitude, vs);
        for (size_t i = 0; i < vs.size() && i < (size_t)(patch.rows * patch.cols); i++) {
            local[(size_t)patch.start + i] = vs[i];
        }
    }
    for (const ArmPatch& patch : armPatches_) {
        std::vector<Vec3> vs;
        BuildArmVerts(patch, pose.phase, pose.amplitude, vs, nullptr);
        for (size_t i = 0; i < vs.size() && i < (size_t)(patch.rows * patch.cols); i++) {
            local[(size_t)patch.start + i] = vs[i];
        }
    }

    std::vector<Vec3> axisLocal = mesh_.axis;
    Deform(local, pose);
    if (hasJaw_ && teethStart_ >= 0 && teethCount_ > 0) {
        const std::vector<Vec3> tp = TeethPose(pose);
        for (size_t i = 0; i < tp.size() && (int)i < teethCount_; i++)
            local[(size_t)teethStart_ + i] = tp[i];
    }
    Deform(axisLocal, pose);

    std::vector<double> as;
    std::vector<Vec3> spinePos;
    std::vector<double> heading;
    Spine(as, spinePos, heading, pose);

    auto sample = [&](double a, double& px, double& py, double& pz, double& h) {
        if (as.empty()) { px = py = pz = h = 0; return; }
        const double aa = Clamp(a, as.front(), as.back());
        const size_t idx = (size_t)(std::upper_bound(as.begin(), as.end(), aa) - as.begin());
        const size_t i1 = ClampI((int)idx, 1, (int)as.size() - 1);
        const size_t i0 = i1 - 1;
        const double span = as[i1] - as[i0];
        const double t = span > 1e-12 ? (aa - as[i0]) / span : 0.0;
        px = Lerp(spinePos[i0].x, spinePos[i1].x, t);
        py = Lerp(spinePos[i0].y, spinePos[i1].y, t);
        pz = Lerp(spinePos[i0].z, spinePos[i1].z, t);
        h = Lerp(heading[i0], heading[i1], t);
    };

    auto posePoint = [&](const Vec3& l) {
        double px, py, pz, h;
        sample(l.x, px, py, pz, h);
        const double ahead = Min(l.x, 0.0);
        px -= ahead * std::cos(h);
        pz -= ahead * std::sin(h);
        const double latX = -std::sin(h), latZ = std::cos(h);
        return Vec3(px + l.z * latX, l.y + py, pz + l.z * latZ);
    };

    const Vec3 center = posePoint(Vec3(centerA_, 0, 0));
    std::vector<Vec3> world(local.size());
    for (size_t i = 0; i < local.size(); i++) world[i] = posePoint(local[i]) - center;
    std::vector<Vec3> axisWorld(axisLocal.size());
    for (size_t i = 0; i < axisLocal.size(); i++) axisWorld[i] = posePoint(axisLocal[i]) - center;

    // ---------------------------------------------------------- normals
    std::vector<Vec3> normals(world.size(), Vec3());
    for (const SeaMesh::Patch& patch : mesh_.patches) {
        const int rows = patch.rows, cols = patch.cols;
        auto at = [&](int r, int c) -> const Vec3& {
            return world[(size_t)patch.start + (size_t)r * cols + (size_t)c];
        };
        std::vector<Vec3> pn((size_t)rows * cols);
        Vec3 mean;
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                Vec3 da;
                if (rows > 1) {
                    if (r == 0) da = at(1, c) - at(0, c);
                    else if (r == rows - 1) da = at(r, c) - at(r - 1, c);
                    else da = (at(r + 1, c) - at(r - 1, c)) * 0.5;
                }
                Vec3 dm;
                if (patch.kind == 0) {
                    dm = at(r, (c + 1) % cols) - at(r, (c - 1 + cols) % cols);
                } else if (cols > 1) {
                    if (c == 0) dm = at(r, 1) - at(r, 0);
                    else if (c == cols - 1) dm = at(r, c) - at(r, c - 1);
                    else dm = (at(r, c + 1) - at(r, c - 1)) * 0.5;
                }
                Vec3 n = patch.kind == 0 ? Cross(dm, da) : Cross(da, dm);
                if (patch.kind == 2) n = -n;
                pn[(size_t)r * cols + c] = n;
                mean += n;
            }
        }
        if (patch.kind != 0) {
            for (Vec3& n : pn) {
                if (Dot(n, mean) < 0) n = -n;
            }
        }
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                const size_t idx = (size_t)patch.start + (size_t)r * cols + (size_t)c;
                Vec3 n = pn[(size_t)r * cols + c].NormalizedOr(Vec3(0, 0, 1));
                if (patch.kind == 0) {
                    const Vec3 outward = world[idx] - axisWorld[idx];
                    const double sign = Dot(n, outward);
                    if (sign < 0) n = -n;
                }
                normals[idx] = n;
            }
        }
    }

    // ---------------------------------------------------------- camera
    out.cam.resize(world.size());
    out.normals.resize(world.size());
    for (size_t i = 0; i < world.size(); i++) {
        out.cam[i] = Camera(world[i], pose.pitch);
        out.normals[i] = Camera(normals[i], pose.pitch).NormalizedOr(Vec3(0, 0, 1));
    }

    // ---------------------------------------------------------- eyes
    out.eyes.clear();
    for (const EyeLocal& e : eyes_) {
        std::vector<Vec3> lp = {Vec3(e.a, e.y, e.z)};
        Deform(lp, pose);
        const Vec3 cw = posePoint(lp[0]) - center;
        Vec3 axisLocalEye(e.a, e.y, e.z * 1.5);
        std::vector<Vec3> lp2 = {axisLocalEye};
        Deform(lp2, pose);
        const Vec3 aw = posePoint(lp2[0]) - center;
        SeaPosedEye pe;
        pe.center = Camera(cw, pose.pitch);
        pe.normal = Camera((aw - cw).NormalizedOr(Vec3(0, 0, 1)), pose.pitch).NormalizedOr(Vec3(0, 0, 1));
        pe.radius = e.radius;
        out.eyes.push_back(pe);
    }

    // ---------------------------------------------------------- spines
    std::vector<Vec3> lineLocal;
    BuildSpineLines(pose.phase, pose.inflate, lineLocal);
    // Puffer spikes ride out on the swelling skin.
    if (g_.puffSpines > 0 && !lineLocal.empty()) {
        std::vector<Vec3> deformed = lineLocal;
        Deform(deformed, pose);
        for (size_t i = 0; i < lineLocal.size(); i++) {
            lineLocal[i] += deformed[i] - lineLocal[i];
        }
    }
    out.spineLines.resize(lineLocal.size());
    for (size_t i = 0; i < lineLocal.size(); i++) {
        out.spineLines[i] = Camera(posePoint(lineLocal[i]) - center, pose.pitch);
    }
}

} // namespace sea
