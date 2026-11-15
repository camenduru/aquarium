#pragma once
// ================================================================
// AQUARIUM - ripple slots
// ----------------------------------------------------------------
// The demo keeps 16 events as shader uniforms: centre, birth time and
// amplitude. A ring is an ellipse because the camera looks down at the
// water, so the vertical axis is compressed by `ripple_aspect`.
//
// Changing the ripple speed mid-run must not teleport existing rings,
// so each slot also carries a radius offset that absorbs the change -
// the same trick the demo applies to every animated term.
// ================================================================

#include <algorithm>
#include <cmath>

namespace water {

class RipplePool
{
public:
    static constexpr int kSlots = 16;

    struct Ripple
    {
        float  x = 0.0f;
        float  y = 0.0f;
        float  amp = 0.0f;
        float  offset = 0.0f;
        double birth = -1000.0;
    };

    void Reset()
    {
        for (Ripple& r : slots_)
        {
            r.x = r.y = r.amp = r.offset = 0.0f;
            r.birth = -1000.0;
        }
        cursor_ = 0;
    }

    void Add(float x, float y, double clock, float amp)
    {
        Ripple& r = slots_[cursor_];
        r.x = x;
        r.y = y;
        r.amp = std::max(amp, 0.0f);
        r.offset = 0.0f;
        r.birth = clock;
        cursor_ = (cursor_ + 1) % kSlots;
    }

    /// Keep every live ring at the radius it already had after a speed edit.
    void Compensate(float oldSpeed, float newSpeed, double clock)
    {
        for (Ripple& r : slots_)
        {
            if (r.birth < -999.0) continue;
            const float age = float(std::max(0.0, clock - r.birth));
            r.offset += age * (oldSpeed - newSpeed);
        }
    }

    const Ripple& At(int i) const { return slots_[std::clamp(i, 0, kSlots - 1)]; }

private:
    Ripple slots_[kSlots] {};
    int    cursor_ = 0;
};

} // namespace water
