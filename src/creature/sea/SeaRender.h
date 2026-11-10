#pragma once
// ================================================================
// SEA CREATURES - shaded software renderer
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/render.py (MIT).
//
// Orthographic camera, supersampled, with a key light + hemisphere
// fill, Blinn gloss, rim light, countershading, fin translucency and
// every marking in PatternKind. Eyes and urchin spines are drawn as
// camera-space overlays on top of the rasterized triangles, exactly
// like the reference does after its z-buffered geometry pass.
// ================================================================

#include "creature/sea/SeaGeometry.h"
#include "creature/sea/SeaPalette.h"
#include "creature/sea/SeaRaster.h"

#include <vector>

namespace sea {

struct SeaRenderResult {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;   // width*height*4, straight alpha
    std::vector<unsigned char> part;   // SeaPart per pixel, 0 = empty
    std::vector<unsigned char> alpha;  // width*height coverage
    std::vector<Vec3> eyes;            // visible eyes (x, y, radius) in OUTPUT pixels
};

class SeaRenderer {
public:
    SeaRenderer(const SeaGenome& genome, int size);

    const SeaGenome& Genome() const { return g_; }
    const SeaPalette& Palette() const { return palette_; }
    const SeaModel& Model() const { return model_; }
    int Size() const { return size_; }
    int Detail() const { return detail_; }
    /// Body lengths per pixel: the camera scale the fit settled on. Exposed so a
    /// comparison against the reference can tell a framing difference from a
    /// geometry difference.
    double FitScale() const { return fitScale_; }

    void Render(const SeaPose& pose, SeaRenderResult& out) const;

    /// Fit every frame of a sheet to one camera scale, like the reference's fit_poses.
    void SetFitPoses(const std::vector<SeaPose>& poses);
    /// Use a fixed camera scale instead (animation.px_per_unit / --to-scale).
    void SetFitScale(double scale) { fitScale_ = Max(1e-6, scale); }

private:
    Vec3 Shade(int part, const Vec4& at, Vec3 normal, double pitch, double pxLen,
               double& alpha, double sunLit, double ao) const;
    /// render.py's _mouth(): the lip line, or the gape and interlocking teeth.
    Vec3 MouthShade(Vec3 col, const Vec4& at, double pxLen) const;
    Vec3 Pattern(int part, const Vec4& at, const Vec3& base, bool fin, double pxLen) const;
    /// render.py's _spots(): hash-placed round spots, optionally rings.
    double Spots(double ua, double ub, double size, bool ring, double density, double pxLen) const;
    /// render.py's stripe coverage, with `grow` widening the bar for its dark rim.
    double StripeCoverage(const Vec4& at, bool fin, double grow, double pxLen) const;
    /// render.py's _surface_coords(): (along, around, angle) on the body surface.
    void SurfaceCoords(const Vec4& at, double& u1, double& u2, double& ang) const;
    /// render.py's _wing_albedo(): ray/squid wings and the hammerhead head.
    Vec3 WingAlbedo(const Vec4& at, bool top, double pxLen) const;
    /// render.py's _scars(): faint curved, broken scratches unique to the animal.
    double Scars(double u1, double u2, double pxLen) const;
    /// render.py's _urchin_shell(): tube-foot bands, crown ring and anal cone.
    Vec3 UrchinShell(Vec3 col, double a, double v, double w) const;
    void DrawEyes(const SeaPosed& posed, const SeaRaster& raster, const SeaPose& pose,
                  double scale, double cx, double cy, int ss, std::vector<double>& depth,
                  std::vector<float>& rgb, std::vector<float>& alpha) const;
    void DrawSpines(const SeaPosed& posed, const SeaRaster& raster, double scale, double cx,
                    double cy, double pitch, std::vector<double>& depth, std::vector<float>& rgb,
                    std::vector<float>& alpha) const;

    void ComputeFit(const std::vector<SeaPose>& poses);

    SeaGenome g_;
    int size_ = 64;
    double fitScale_ = 1.0;
    int detail_ = 120;
    SeaModel model_;
    SeaPalette palette_;
    mutable double posePhase_ = 0.0;   // current clip phase, for the shimmer marking
};

} // namespace sea
