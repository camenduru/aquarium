#pragma once
// ================================================================
// AQUARIUM - procedural aquascape
// ----------------------------------------------------------------
// The sand and rock plate is generated, never loaded: this project has
// no texture loader and ships no assets. The look is held together by
// pixel-art rules rather than detail - a hard silhouette, a 6-8 entry
// palette, one outline tone, no anti-aliasing. Softness comes from the
// water (refraction, caustics, tint) applied on top.
//
// Rocks are behind the fish. One optional "front rock" is drawn after
// them, so it reads as nearer the viewer - the demo's occlusion idea,
// reduced to one object.
// ================================================================

#include "water/WaterMasks.h"
#include "water/WaterTuning.h"

#include <cstdint>
#include <vector>

namespace water {

class Aquascape
{
public:
    void Build(int logicalW, int logicalH, const AquascapeSpec& spec,
               const WaterMasks& masks);

    bool Valid() const { return !plate_.empty(); }
    int  Width() const { return w_; }
    int  Height() const { return h_; }

    /// Back of the scene: the plate is copied, then fish are drawn on it.
    void DrawBack(uint8_t* straightRgba) const;

    /// Front rock, over the fish.
    void DrawFront(uint8_t* straightRgba) const;

    const std::vector<uint8_t>& Plate() const { return plate_; }

private:
    int w_ = 0, h_ = 0;
    std::vector<uint8_t> plate_;
    std::vector<uint8_t> front_;
};

} // namespace water
