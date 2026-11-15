#pragma once
// ================================================================
// AQUARIUM - the water composite
// ----------------------------------------------------------------
// water.gdshader, restructured for a CPU canvas with per-pixel alpha.
//
// The demo's fragment shader reads a seabed, tints it, refracts the
// sample, adds caustics, foam and ripples, and writes an opaque pixel.
// Here the "seabed" is the procedurally generated plate plus the fish,
// and the output has to stay translucent: the desktop is still the
// background. So every water layer contributes premultiplied colour and
// its own alpha, and the desktop shows through where water is light.
//
// Cost is split in two. Tint, caustics, foam, moonlight and the wave
// displacement move slowly, so they are rebuilt at 30 Hz into a static
// buffer. Ripples and bubbles move at frame rate and are added to that
// buffer per frame, inside their bounding boxes only.
// ================================================================

#include "water/RipplePool.h"
#include "water/WaterMasks.h"
#include "water/WaterTuning.h"

#include <cstdint>
#include <vector>

namespace water {

class WaterLayer
{
public:
    void Resize(int logicalW, int logicalH);

    void SetInputs(const WaterTuning* tuning, const WaterPalette* palette,
                   const WaterMasks* masks, const RipplePool* ripples);

    /// Recompute the slow layers on the next frame (tuning or aquascape edit).
    void InvalidateStatic() { lastStaticClock_ = -1.0e30; }

    /// `content` is straight-alpha RGBA at logical size; `canvas` is a
    /// premultiplied BGRA canvas at `scale` times that size.
    void Composite(const uint8_t* content, uint8_t* canvas,
                   int canvasW, int canvasH, int scale,
                   double clock, float night, bool forceStatic = false);

    bool Valid() const { return w_ > 0 && !stat_.empty(); }

    struct Stats
    {
        double staticMs = 0.0;
        double compositeMs = 0.0;
        int    staticPixels = 0;
        int    activeRipples = 0;
    };
    const Stats& LastStats() const { return stats_; }

private:
    struct StaticPixel
    {
        int8_t  dx = 0, dy = 0;
        uint8_t tr = 0, tg = 0, tb = 0, ta = 0;   // depth tint (straight)
        uint8_t cr = 0, cg = 0, cb = 0, ca = 0;   // caustics + moon (premultiplied)
        uint8_t fr = 0, fg = 0, fb = 0, fa = 0;   // foam + backwash (premultiplied)
    };

    void BuildStatic(double clock, float night);

    int w_ = 0, h_ = 0;
    std::vector<StaticPixel> stat_;
    double lastStaticClock_ = -1.0e30;

    const WaterTuning*  tuning_ = nullptr;
    const WaterPalette* palette_ = nullptr;
    const WaterMasks*   masks_ = nullptr;
    const RipplePool*   ripples_ = nullptr;

    Stats stats_;
};

} // namespace water
