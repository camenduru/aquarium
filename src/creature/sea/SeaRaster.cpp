// ================================================================
// SEA CREATURES - triangle rasterizer
// Ported from fish-gen/fishgen/raster.py (MIT).
// ================================================================
#include "creature/sea/SeaRaster.h"

#include <algorithm>
#include <cmath>

namespace sea {
namespace {

inline double Edge(double ax, double ay, double bx, double by, double px, double py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

} // namespace

void SeaRaster::Reset(int w, int h) {
    width = w;
    height = h;
    const size_t n = (size_t)Max(0, w) * (size_t)Max(0, h);
    depth.assign(n, kEmptyDepth);
    tri.assign(n, -1);
    bary.assign(n * 3, 0.0f);
}

void Rasterize(const SeaMesh& mesh, const SeaPosed& posed, double scale, double cx, double cy,
               SeaRaster& out) {
    const int w = out.width, h = out.height;
    if (w <= 0 || h <= 0) return;

    const size_t triCount = mesh.tris.size() / 3;
    for (size_t t = 0; t < triCount; t++) {
        const int i0 = mesh.tris[t * 3 + 0];
        const int i1 = mesh.tris[t * 3 + 1];
        const int i2 = mesh.tris[t * 3 + 2];
        if (i0 < 0 || i1 < 0 || i2 < 0) continue;
        if ((size_t)i0 >= posed.cam.size() || (size_t)i1 >= posed.cam.size() ||
            (size_t)i2 >= posed.cam.size())
            continue;

        const Vec3& a = posed.cam[i0];
        const Vec3& b = posed.cam[i1];
        const Vec3& c = posed.cam[i2];
        const double x0 = cx + a.x * scale, y0 = cy - a.y * scale;
        const double x1 = cx + b.x * scale, y1 = cy - b.y * scale;
        const double x2 = cx + c.x * scale, y2 = cy - c.y * scale;

        const double area = Edge(x0, y0, x1, y1, x2, y2);
        if (std::fabs(area) < 1e-12) continue;

        int minX = (int)std::floor(Min(x0, Min(x1, x2)));
        int maxX = (int)std::ceil(Max(x0, Max(x1, x2)));
        int minY = (int)std::floor(Min(y0, Min(y1, y2)));
        int maxY = (int)std::ceil(Max(y0, Max(y1, y2)));
        minX = ClampI(minX, 0, w - 1);
        maxX = ClampI(maxX, 0, w - 1);
        minY = ClampI(minY, 0, h - 1);
        maxY = ClampI(maxY, 0, h - 1);
        if (minX > maxX || minY > maxY) continue;

        const double invArea = 1.0 / area;
        // The reference keeps edge pixels with a small negative epsilon. A
        // strict test lets two adjacent triangles both reject a shared edge
        // pixel, leaving a 1px seam between them.
        const double eps = -1e-7;
        for (int py = minY; py <= maxY; py++) {
            const double sy = py + 0.5;
            for (int px = minX; px <= maxX; px++) {
                const double sx = px + 0.5;
                double w0 = Edge(x1, y1, x2, y2, sx, sy) * invArea;
                double w1 = Edge(x2, y2, x0, y0, sx, sy) * invArea;
                double w2 = 1.0 - w0 - w1;
                if (w0 < eps || w1 < eps || w2 < eps) continue;
                const double z = w0 * a.z + w1 * b.z + w2 * c.z;
                const size_t idx = (size_t)py * w + px;
                if (z <= out.depth[idx]) continue;
                out.depth[idx] = z;
                out.tri[idx] = (int)t;
                out.bary[idx * 3 + 0] = (float)w0;
                out.bary[idx * 3 + 1] = (float)w1;
                out.bary[idx * 3 + 2] = (float)w2;
            }
        }
    }
}

} // namespace sea
