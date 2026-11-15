// ================================================================
// AQUARIUM - water masks implementation
// ================================================================

#include "water/WaterMasks.h"

#include "water/WaterField.h"

#include <algorithm>
#include <cmath>

namespace water {
namespace {

constexpr float kFar = 1.0e9f;
constexpr float kDiagonal = 1.41421356f;

/// Two-pass chamfer distance from every `inside` pixel to the nearest
/// outside pixel, in pixels.
void ChamferDistance(const std::vector<uint8_t>& inside, int w, int h,
                     std::vector<float>& distance)
{
    distance.assign(inside.size(), kFar);
    for (size_t i = 0; i < inside.size(); ++i)
        if (inside[i] == 0) distance[i] = 0.0f;

    const auto at = [&](int x, int y) -> float&
    {
        return distance[size_t(y) * size_t(w) + size_t(x)];
    };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float& d = at(x, y);
            if (x > 0)          d = std::min(d, at(x - 1, y) + 1.0f);
            if (y > 0)
            {
                d = std::min(d, at(x, y - 1) + 1.0f);
                if (x > 0)      d = std::min(d, at(x - 1, y - 1) + kDiagonal);
                if (x + 1 < w)  d = std::min(d, at(x + 1, y - 1) + kDiagonal);
            }
        }
    }
    for (int y = h - 1; y >= 0; --y)
    {
        for (int x = w - 1; x >= 0; --x)
        {
            float& d = at(x, y);
            if (x + 1 < w)      d = std::min(d, at(x + 1, y) + 1.0f);
            if (y + 1 < h)
            {
                d = std::min(d, at(x, y + 1) + 1.0f);
                if (x + 1 < w)  d = std::min(d, at(x + 1, y + 1) + kDiagonal);
                if (x > 0)      d = std::min(d, at(x - 1, y + 1) + kDiagonal);
            }
        }
    }
}

} // namespace

void WaterMasks::Build(int logicalW, int logicalH, const AquascapeSpec& spec)
{
    w_ = std::max(logicalW, 8);
    h_ = std::max(logicalH, 8);

    const int margin = std::clamp(spec.margin, 2, std::min(w_, h_) / 4);
    top_    = margin;
    bottom_ = h_ - margin;

    const float radius = float(std::min(14, std::min(w_, h_) / 6));
    const float cx = (float(w_) - 1.0f) * 0.5f;
    const float cy = (float(h_) - 1.0f) * 0.5f;
    const float hx = float(w_) * 0.5f - float(margin) - radius;
    const float hy = float(h_) * 0.5f - float(margin) - radius;
    const float r2 = radius * radius;

    std::vector<uint8_t> inside(size_t(w_) * size_t(h_), 0);
    for (int y = 0; y < h_; ++y)
    {
        for (int x = 0; x < w_; ++x)
        {
            const float dx = std::fabs(float(x) - cx) - hx;
            const float dy = std::fabs(float(y) - cy) - hy;
            const float ox = std::max(dx, 0.0f);
            const float oy = std::max(dy, 0.0f);
            const bool covered = (dx <= 0.0f && dy <= 0.0f) || (ox * ox + oy * oy <= r2);
            inside[size_t(y) * size_t(w_) + size_t(x)] = covered ? 1 : 0;
        }
    }

    std::vector<float> distance;
    ChamferDistance(inside, w_, h_, distance);

    sandBase_  = spec.sandY * float(h_);
    sandCurve_ = spec.sandCurve;
    sandPhase_ = float(spec.seed % 628u) * 0.01f;

    const float span = float(std::max(bottom_ - top_, 1));
    rgba_.assign(size_t(w_) * size_t(h_) * 4, 0);
    for (int y = 0; y < h_; ++y)
    {
        for (int x = 0; x < w_; ++x)
        {
            const size_t i = size_t(y) * size_t(w_) + size_t(x);
            const size_t o = i * 4;
            if (inside[i] == 0) continue;

            float depth = (float(y) - float(top_)) / span;
            depth = std::clamp(depth, 0.0f, 1.0f);
            depth = std::pow(depth, 1.05f);

            rgba_[o + 0] = 255;
            rgba_[o + 1] = uint8_t(std::clamp(distance[i] / 64.0f, 0.0f, 1.0f) * 255.0f + 0.5f);
            rgba_[o + 2] = uint8_t(depth * 255.0f + 0.5f);
            rgba_[o + 3] = 255;
        }
    }
}

bool WaterMasks::Inside(int x, int y) const
{
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return false;
    return rgba_[(size_t(y) * size_t(w_) + size_t(x)) * 4 + 0] >= 128;
}

float WaterMasks::Distance(int x, int y) const
{
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return 0.0f;
    return float(rgba_[(size_t(y) * size_t(w_) + size_t(x)) * 4 + 1]) / 255.0f * 64.0f;
}

float WaterMasks::Depth(int x, int y) const
{
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return 0.0f;
    return float(rgba_[(size_t(y) * size_t(w_) + size_t(x)) * 4 + 2]) / 255.0f;
}

float WaterMasks::SandAt(int x) const
{
    return sandBase_ + sandCurve_ * Sin(double(x) * 0.035 + double(sandPhase_));
}

float WaterMasks::AirstoneY() const
{
    return SandAt(int(AirstoneX())) - 3.0f;
}

} // namespace water
