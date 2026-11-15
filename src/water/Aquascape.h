#pragma once
// ================================================================
// AQUARIUM - procedural aquascape
// ----------------------------------------------------------------
// The sand, rock and coral plate is generated, never loaded: this project
// has no texture loader and ships no assets. The look is held together by
// pixel-art rules rather than detail - a hard silhouette, a small
// palette, one outline tone, no anti-aliasing. Softness comes from the
// water (refraction, caustics, tint) applied on top.
//
// Corals are seeded genomes (kind, size, bend, hue, detail seed) drawn into a
// scratch tone mask and blitted with one shared outline. Placement walks the
// width by the golden angle from the coral's index, so adding one never moves
// the ones already there.
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
