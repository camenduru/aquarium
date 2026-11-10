#pragma once
// ================================================================
// SEA CREATURES - z-buffered triangle rasterizer
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/raster.py (MIT).
//
// Orthographic projection, one depth value and three interpolated
// barycentric weights per pixel. Larger camera z is nearer, so a
// fragment wins when its z compares greater than the stored depth.
// ================================================================

#include "creature/sea/SeaGeometry.h"

#include <vector>

namespace sea {

inline constexpr double kEmptyDepth = -1e9;

struct SeaRaster {
    int width = 0;
    int height = 0;
    std::vector<double> depth;
    std::vector<int> tri;      // index into mesh.tris/3, -1 = empty
    std::vector<float> bary;   // 3 weights per pixel

    void Reset(int w, int h);
};

/// Rasterize the posed mesh. `scale` is pixels per body length; screen x grows
/// right, screen y grows down (world y is flipped), so the camera stays upright.
void Rasterize(const SeaMesh& mesh, const SeaPosed& posed, double scale, double cx, double cy,
               SeaRaster& out);

} // namespace sea
