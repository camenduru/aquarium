#pragma once
// ================================================================
// AQUARIUM - water field math
// ----------------------------------------------------------------
// The per-pixel pieces of water.gdshader, ported as plain arithmetic.
// Nothing here allocates, and every sine is a lookup: the shader calls
// sin() a dozen times per pixel, which at 600x340 would cost more than
// the whole fish renderer.
//
// The sine table's error is far below one 8-bit level and the results
// are quantised to whole pixels anyway, so this is invisible.
// ================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace water {

constexpr double kPi = 3.14159265358979323846;

/// 4096-entry sine table with linear interpolation.
inline float Sin(double phase)
{
    struct Table
    {
        float v[4097];
        Table()
        {
            for (int i = 0; i <= 4096; ++i)
                v[i] = float(std::sin(double(i) * (2.0 * kPi / 4096.0)));
        }
    };
    static const Table table;

    // Float from here on: the table's own error is far larger than anything
    // the reduced precision adds.
    constexpr float kTurns = float(4096.0 / (2.0 * kPi));
    const float turns = float(phase) * kTurns;
    const int   base  = int(std::floor(turns));
    const int   index = base & 4095;
    const float frac  = turns - float(base);
    return table.v[index] + (table.v[index + 1] - table.v[index]) * frac;
}

inline float Cos(double phase) { return Sin(phase + kPi * 0.5); }

inline float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

/// GLSL smoothstep, including the degenerate edge case.
inline float Smoothstep(float edge0, float edge1, float x)
{
    if (edge1 <= edge0) return x < edge0 ? 0.0f : 1.0f;
    const float t = Saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

inline float Mix(float a, float b, float t) { return a + (b - a) * t; }

struct Vec2
{
    float x = 0.0f, y = 0.0f;
};

/// Rotate a point by `degrees`, matching the demo's turn() helper.
inline Vec2 Turn(Vec2 p, float degrees, float cosA, float sinA)
{
    if (degrees == 0.0f) return p;
    return { cosA * p.x - sinA * p.y, sinA * p.x + cosA * p.y };
}

/// The demo's two-wave displacement along one axis. `axis` selects the
/// phase variant used for the transposed coordinate.
struct WaveRig;

/// Rotation constants for the two wave directions, resolved once per
/// static build instead of two cos+sin per pixel.
struct WaveRig
{
    float cos1 = 1.0f, sin1 = 0.0f;
    float cos2 = 1.0f, sin2 = 0.0f;
    float density1 = 1.0f, density2 = 1.0f;
    float weight2 = 0.45f;
    double phase1 = 0.0, phase2 = 0.0;
    double axisPhase1 = 0.0, axisPhase2 = 0.0;
};

inline float WaveAt(float x, float y, bool axis, const WaveRig& rig)
{
    // a = turn(p * density1, angle1)
    const float ax = rig.density1 * x;
    const float ay = rig.density1 * y;
    const float a_x = rig.cos1 * ax - rig.sin1 * ay;
    const float a_y = rig.sin1 * ax + rig.cos1 * ay;
    // b = turn(p * density2, angle2)
    const float bx = rig.density2 * x;
    const float by = rig.density2 * y;
    const float b_x = rig.cos2 * bx - rig.sin2 * by;
    const float b_y = rig.sin2 * bx + rig.cos2 * by;

    const double t1 = axis ? rig.axisPhase1 : rig.phase1;
    const double t2 = axis ? rig.axisPhase2 : rig.phase2;

    return Sin(double(a_x) * 0.102 + double(a_y) * 0.073 + t1 * 0.8)
         + rig.weight2 * Sin(double(b_x) * -0.067 + double(b_y) * 0.134 - t2 * 1.1);
}

} // namespace water
