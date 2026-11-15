#pragma once
// ================================================================
// AQUARIUM - water masks
// ----------------------------------------------------------------
// The demo's bathymetry.png, computed instead of loaded: the window has
// no texture loader and ships no assets, so the same four channels are
// derived from the aquascape spec at startup.
//
//   R = water          inside the tank's rounded interior
//   G = edge distance  distance to the nearest tank edge, /64 px
//   B = depth          0 at the surface, 1 at the sand
//   A = 255            (unused; runtime hit-testing has its own buffer)
//
// The distance channel is a chamfer transform, which is exact for the
// 1/1.4142 weights at this size and costs a fraction of a Euclidean
// transform.
// ================================================================

#include "water/WaterTuning.h"

#include <cstdint>
#include <vector>

namespace water {

class WaterMasks
{
public:
    void Build(int logicalW, int logicalH, const AquascapeSpec& spec);

    int  Width() const { return w_; }
    int  Height() const { return h_; }
    bool Valid() const { return !rgba_.empty(); }

    bool  Inside(int x, int y) const;
    float Distance(int x, int y) const;   // logical pixels from the tank edge
    float Depth(int x, int y) const;      // 0 = surface, 1 = sand
    float SandAt(int x) const;            // sand horizon, logical pixels

    /// Airstone position: where the bubble column leaves the sand.
    float AirstoneX() const { return w_ * 0.16f; }
    float AirstoneY() const;

    const uint8_t* Pixels() const { return rgba_.data(); }
    int            Top() const { return top_; }
    int            Bottom() const { return bottom_; }

private:
    int   w_ = 0, h_ = 0;
    int   top_ = 0, bottom_ = 0;
    float sandBase_ = 0.0f;
    float sandCurve_ = 0.0f;
    float sandPhase_ = 0.0f;
    std::vector<uint8_t> rgba_;
};

} // namespace water
