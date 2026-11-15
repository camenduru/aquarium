// ================================================================
// AQUARIUM - water composite implementation
// ================================================================

#include "water/WaterLayer.h"

#include "water/WaterField.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace water {
namespace {

inline uint8_t U8(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    return uint8_t(v * 255.0f + 0.5f);
}

inline Rgb Mix3(const Rgb& a, const Rgb& b, float t)
{
    return { Mix(a.r, b.r, t), Mix(a.g, b.g, t), Mix(a.b, b.b, t) };
}

} // namespace

void WaterLayer::Resize(int logicalW, int logicalH)
{
    w_ = std::max(logicalW, 8);
    h_ = std::max(logicalH, 8);
    stat_.assign(size_t(w_) * size_t(h_), StaticPixel{});
    InvalidateStatic();
}

void WaterLayer::SetInputs(const WaterTuning* tuning, const WaterPalette* palette,
                           const WaterMasks* masks, const RipplePool* ripples)
{
    tuning_ = tuning;
    palette_ = palette;
    masks_ = masks;
    ripples_ = ripples;
}

void WaterLayer::BuildStatic(double clock, float night)
{
    if (!tuning_ || !palette_ || !masks_ || !masks_->Valid()) return;

    // 15 Hz: every term in this pass moves slowly (caustics 0.12, waves 1.0
    // phase/s), so the cache is invisible while the cost is spread over four
    // 60 Hz frames.
    constexpr double kStaticInterval = 1.0 / 15.0;
    if (clock >= lastStaticClock_ && clock - lastStaticClock_ < kStaticInterval)
        return;

    const auto started = std::chrono::steady_clock::now();

    const WaterTuning& t = *tuning_;
    const WaterPalette& p = *palette_;
    const WaterMasks&  m = *masks_;
    night = std::clamp(night, 0.0f, 1.0f);

    // ---- phases, resolved once ---------------------------------
    WaveRig rig;
    rig.cos1 = std::cos(float(t.wave1Angle * float(kPi) / 180.0f));
    rig.sin1 = std::sin(float(t.wave1Angle * float(kPi) / 180.0f));
    rig.cos2 = std::cos(float(t.wave2Angle * float(kPi) / 180.0f));
    rig.sin2 = std::sin(float(t.wave2Angle * float(kPi) / 180.0f));
    rig.density1 = t.wave1Density;
    rig.density2 = t.wave2Density;
    rig.weight2 = t.wave2Weight;
    rig.phase1 = t.Phase(clock, t.wave1Speed, t.offsetWave1);
    rig.phase2 = t.Phase(clock, t.wave2Speed, t.offsetWave2);
    rig.axisPhase1 = clock * double(t.wave1Speed) * double(t.waveAxisSpeed) + double(t.offsetWave1);
    rig.axisPhase2 = clock * double(t.wave2Speed) * double(t.waveAxisSpeed) + double(t.offsetWave2);

    const double phaseCx = t.Phase(clock, t.causticX, t.offsetCausticX);
    const double phaseCy = t.Phase(clock, t.causticY, t.offsetCausticY);
    const double phaseDeform = t.Phase(clock, t.causticDeform, t.offsetCausticDeform);
    const double phaseBreak = t.Phase(clock, t.causticBreak, t.offsetCausticBreak);

    const double phaseFoam = t.Phase(clock, t.foamSpeed, t.offsetFoamSpeed);
    const double phaseJagged = t.Phase(clock, t.foamJaggedSpeed, t.offsetFoamJagged);
    const double phaseFoamBreak = t.Phase(clock, t.foamBreakSpeed, t.offsetFoamBreak);
    const double phaseMoon = t.Phase(clock, t.moonSpeed, t.offsetMoon);
    const double phaseFlow = t.Phase(clock, t.flowSpeed, t.offsetFlow);

    const float stoneX = m.AirstoneX();
    const float stoneY = m.AirstoneY();

    // Only the top band of the tank carries foam, so the sines for it are
    // evaluated there and nowhere else: the whole water column is 12x more
    // pixels than the band.
    const float foamBand = std::max(20.0f, t.foamDistance + t.foamAmplitude
                                          + t.backwashDistance + t.backwashWidth * 2.0f
                                          + t.foamJagged + 8.0f);
    // The airstone's outflow dies off with exp(-r/decay); past three decay
    // lengths its contribution is below the 8-bit floor.
    const float flowReach = std::max(24.0f, 3.0f * t.flowDecay);
    const float impactPhase = float(t.Phase(clock, t.impactSpeed, t.offsetImpact));
    const float impactGlow = std::max(0.0f, t.impactBrightness
                                            + t.impactFlicker * Sin(impactPhase));

    // ---- palette mixes -----------------------------------------
    const Rgb shallow = Mix3(p.shallowDay, p.shallowNight, night);
    const Rgb deep    = Mix3(p.deepDay, p.deepNight, night);
    const Rgb caustic = Mix3(p.causticDay, p.causticNight, night);
    const Rgb foamCol = Mix3(p.foamDay, p.foamNight, night);
    const float moonX = float(w_) * 0.72f;

    lastStaticClock_ = clock;

    // The build is 20+ ms of pure arithmetic over the whole tank, so it is
    // split into horizontal bands across the cores allowed by the plan. Rows
    // are disjoint, so no synchronisation is needed beyond the join.
    auto buildRows = [&](int y0, int y1, int& pixels)
    {
        int inside = 0;
        for (int y = y0; y < y1; ++y)
        {
            for (int x = 0; x < w_; ++x)
            {
                StaticPixel& s = stat_[size_t(y) * size_t(w_) + size_t(x)];
                s = StaticPixel{};

                if (!m.Inside(x, y)) continue;
                ++inside;

                const float px = float(x) + 0.5f;
                const float py = float(y) + 0.5f;

                float depth = std::clamp(m.Depth(x, y) + t.waterLevel * t.tideInfluence, 0.0f, 1.0f);
                const float dist = m.Distance(x, y);

                // ---- refraction displacement ------------------------
                float dispX = 0.0f, dispY = 0.0f;
                if (t.refractionEnabled && t.strength > 0.0f)
                {
                    dispX = WaveAt(px, py, false, rig);
                    dispY = WaveAt(py + 67.0f, px, true, rig);

                    // The airstone's outflow: the demo's waterfall impact, re-aimed
                    // at a column instead of a curtain.
                    if (t.waterfallEnabled && t.flowAmplitude > 0.0f)
                    {
                        const float dx = px - stoneX;
                        const float dy = (py - stoneY) * t.flowAspect;
                        if (dx * dx + dy * dy < flowReach * flowReach)
                        {
                            const float r  = std::sqrt(dx * dx + dy * dy);
                            const float fall = Sin(double(r) * t.flowDensity - double(phaseFlow))
                                             * std::exp(-r / std::max(t.flowDecay, 1.0f))
                                             * Smoothstep(4.0f, 14.0f, r);
                            const float inv = 1.0f / std::max(r, 0.001f);
                            dispX += dx * inv * fall * t.flowAmplitude;
                            dispY += (dy / t.flowAspect) * inv * fall * t.flowAmplitude;
                        }
                    }

                    const float scale = t.strength * (t.refractionBase + depth);
                    s.dx = int8_t(std::clamp(int(std::floor(dispX * scale + 0.5f)), -6, 6));
                    s.dy = int8_t(std::clamp(int(std::floor(dispY * scale + 0.5f)), -6, 6));
                }

                // ---- depth tint -------------------------------------
                if (t.depthEnabled && t.ghostAlpha > 0.0f)
                {
                    const float mix = std::clamp(
                        (t.waterMix + depth * t.deepMix) * (1.15f - t.clarity * 0.55f),
                        0.0f, 1.0f);
                    // The edge taper happens once in Composite, on the finished
                    // pixel, so sand and caustics fade with the body instead of
                    // the body alone fading inside a rectangle of art.
                    const float alpha = t.ghostAlpha * mix;
                    const Rgb tint = Mix3(shallow, deep, depth);
                    s.tr = U8(tint.r);
                    s.tg = U8(tint.g);
                    s.tb = U8(tint.b);
                    s.ta = U8(alpha);
                }

                // ---- caustics, moonlight, airstone glow -------------
                float addR = 0.0f, addG = 0.0f, addB = 0.0f, addA = 0.0f;

                if (t.causticsEnabled && t.causticStrength > 0.0f)
                {
                    const float qx = px * t.causticDensity + float(phaseCx);
                    const float qy = py * t.causticDensity + float(phaseCy);
                    const float field =
                        Sin(double(qx) + double(Sin(double(qy) * 0.9 + phaseDeform * 0.21)))
                      * Sin(double(qy) + double(Sin(double(qx) * 0.8 - phaseDeform * 0.18)));
                    const float lattice = 1.0f - Smoothstep(0.025f * t.causticWidth,
                                                            0.105f * t.causticWidth,
                                                            std::fabs(field));
                    const float broken = Smoothstep(-0.6f, 0.55f,
                                                    Sin(double(qx) * 0.45 + double(qy) * 0.7 + phaseBreak * 0.2));
                    // The demo fades caustics with (1 - depth); a side view wants
                    // them strongest where the light lands: on the sand.
                    const float causticMask = lattice * broken
                                            * (0.06f + 0.94f * depth * depth)
                                            * Smoothstep(1.0f, 6.0f, dist);
                    const float a = causticMask * t.causticStrength * 0.65f;
                    addR += caustic.r * a;
                    addG += caustic.g * a;
                    addB += caustic.b * a;
                    addA += a;
                }

                if (t.waterfallEnabled && t.impactBrightness > 0.0f)
                {
                    // A flickering glow where the bubbles leave the stone.
                    const float dx = px - stoneX;
                    const float dy = py - stoneY;
                    const float r2 = dx * dx + dy * dy;
                    if (r2 < 22.0f * 22.0f)
                    {
                        const float a = (1.0f - std::sqrt(r2) / 22.0f) * impactGlow * 0.05f;
                        addR += p.stone.r * a;
                        addG += p.stone.g * a;
                        addB += p.stone.b * a;
                        addA += a;
                    }
                }

                if (t.environmentEnabled && night > 0.01f)
                {
                    const float mx = moonX + Sin(phaseMoon) * t.moonAmplitude;
                    const float band = std::exp(-std::pow((px - mx) / std::max(t.moonWidth, 0.5f), 2.0f));
                    const float vert = Smoothstep(float(m.Top()), float(m.Top()) + float(h_) * 0.18f, py)
                                     * (1.0f - Smoothstep(float(h_) * 0.50f, float(h_) * 0.92f, py));
                    const float a = band * vert * t.moonStrength * night * 0.5f;
                    addR += p.moon.r * a;
                    addG += p.moon.g * a;
                    addB += p.moon.b * a;
                    addA += a;
                }

                s.cr = U8(addR);
                s.cg = U8(addG);
                s.cb = U8(addB);
                s.ca = U8(addA);

                // ---- surface foam and backwash ----------------------
                if (t.foamEnabled && t.foamStrength > 0.0f && py - float(m.Top()) <= foamBand)
                {
                    const float sY = py - float(m.Top());
                    const float tide = t.foamDistance
                                     + t.foamAmplitude * Sin(phaseFoam + double(px) * 0.016);
                    const float jagged = t.foamJagged
                                       * Sin(double(px) * 0.21 + double(py) * 0.25 + phaseJagged);
                    const float edge = 1.0f - Smoothstep(0.35f * t.foamWidth,
                                                         1.1f * t.foamWidth,
                                                         std::fabs(sY - tide - jagged));
                    const float broken = Smoothstep(-0.5f, 0.05f,
                                                    Sin(double(px) * 0.23 - double(py) * 0.17 + phaseFoamBreak));
                    const float side = Smoothstep(2.0f, 12.0f, std::min(px, float(w_) - 1.0f - px));

                    const float foam = edge * broken * t.foamStrength * side * 0.9f;
                    const float wash = (1.0f - Smoothstep(0.0f, t.backwashWidth,
                                                          std::fabs(sY - tide - t.backwashDistance)))
                                     * t.backwashStrength * side;

                    s.fr = U8(foamCol.r * foam + p.backwash.r * wash);
                    s.fg = U8(foamCol.g * foam + p.backwash.g * wash);
                    s.fb = U8(foamCol.b * foam + p.backwash.b * wash);
                    s.fa = U8(foam + wash);
                }
            }
        }
        pixels = inside;
    };

    const int bands = std::clamp(h_ / 48, 1, 4);
    if (bands <= 1)
    {
        int pixels = 0;
        buildRows(0, h_, pixels);
        stats_.staticPixels = pixels;
    }
    else
    {
        std::vector<int>  counts(size_t(bands), 0);
        std::vector<std::thread> workers;
        workers.reserve(size_t(bands - 1));
        for (int b = 1; b < bands; ++b)
        {
            const int y0 = h_ * b / bands;
            const int y1 = h_ * (b + 1) / bands;
            workers.emplace_back([&buildRows, &counts, b, y0, y1] {
                buildRows(y0, y1, counts[size_t(b)]);
            });
        }
        buildRows(0, h_ / bands, counts[0]);
        for (std::thread& worker : workers) worker.join();

        int total = 0;
        for (int c : counts) total += c;
        stats_.staticPixels = total;
    }

    const auto finished = std::chrono::steady_clock::now();
    stats_.staticMs = std::chrono::duration<double, std::milli>(finished - started).count();
}

void WaterLayer::Composite(const uint8_t* content, uint8_t* canvas,
                           int canvasW, int canvasH, int scale,
                           double clock, float night, bool forceStatic)
{
    if (!tuning_ || !palette_ || !masks_ || !masks_->Valid()) return;
    if (content == nullptr || canvas == nullptr || w_ < 1 || h_ < 1 || scale < 1) return;

    if (forceStatic) lastStaticClock_ = -1.0e30;
    BuildStatic(clock, night);

    const auto started = std::chrono::steady_clock::now();

    const WaterTuning& t = *tuning_;
    const WaterPalette& p = *palette_;
    const WaterMasks&  m = *masks_;
    night = std::clamp(night, 0.0f, 1.0f);

    // ---- gather live ripples -----------------------------------
    struct Active
    {
        float x = 0.0f, y = 0.0f, radius = 0.0f, fade = 0.0f;
    };
    Active active[RipplePool::kSlots];
    int activeCount = 0;

    if (t.ripplesEnabled && ripples_ != nullptr && t.rippleLifetime > 0.0f)
    {
        for (int i = 0; i < RipplePool::kSlots; ++i)
        {
            const RipplePool::Ripple& r = ripples_->At(i);
            if (r.birth < -999.0 || r.amp <= 0.0f) continue;
            const double age = clock - r.birth;
            if (age < 0.0 || age >= double(t.rippleLifetime)) continue;

            Active& a = active[activeCount++];
            a.x = r.x;
            a.y = r.y;
            a.radius = float(age) * t.rippleSpeed + t.rippleRadius + r.offset;
            a.fade = float(1.0 - age / double(t.rippleLifetime)) * r.amp;
        }
    }
    stats_.activeRipples = activeCount;

    const Rgb rippleCol = Mix3(p.rippleDay, p.rippleNight, night);
    const float rippleWidth = std::max(0.1f, t.rippleWidth);
    const float aspect = std::max(0.2f, t.rippleAspect);

    // ---- bubble columns, resolved once -------------------------
    struct Bubble { float x = 0.0f, y = 0.0f, k = 0.0f, r = 1.9f; };
    Bubble bubbles[kMaxBubbles];
    int bubbleCount = 0;
    if (t.waterfallEnabled && t.bubbleCount > 0)
    {
        // Outlets come from water.ini; with none configured the stone's
        // airstone is the single outlet, so a plain file behaves as before.
        float outletX[kMaxBubbleColumns] = {};
        float outletY[kMaxBubbleColumns] = {};
        int outletCount = 0;
        for (const BubbleEmitter& e : t.bubbleEmitters)
        {
            if (outletCount >= kMaxBubbleColumns) break;
            outletX[outletCount] = e.x * float(w_);
            outletY[outletCount] = e.y * float(h_);
            ++outletCount;
        }
        if (outletCount == 0)
        {
            outletX[0] = m.AirstoneX();
            outletY[0] = m.AirstoneY();
            outletCount = 1;
        }

        const float surfaceY = float(m.Top());
        const float bottomY  = float(h_ - 2);

        // Beads share one band per outlet (~18 logical px wide), so a denser
        // column packs them closer instead of fanning out. Each gets a
        // deterministic size and alpha wobble: with many beads on screen at
        // once, a uniform grid would read as a bright bar, while varied ones
        // read as separate rising bubbles.
        for (int c = 0; c < outletCount && bubbleCount < kMaxBubbles; ++c)
        {
            const float originY = std::clamp(outletY[c], surfaceY + 2.0f, bottomY);
            const int beads = std::min(t.bubbleCount, kMaxBubbles - bubbleCount);
            const float half = (float(beads) - 1.0f) * 0.5f;
            const float spacing = beads > 1 ? 18.0f / (float(beads) - 1.0f) : 0.0f;

            for (int k = 0; k < beads; ++k)
            {
                const float phase = std::fmod(float(clock) * 0.42f + float(k) / float(beads), 1.0f);
                const float wobble = 0.5f + 0.5f * Sin(double(k) * 2.399963 + double(c) * 1.7);
                Bubble& b = bubbles[bubbleCount++];
                b.x = outletX[c] + (float(k) - half) * spacing +
                      2.2f * Sin(clock * 1.7 + double(k) * 1.3 + double(c) * 4.1);
                b.y = originY + (surfaceY - originY) * phase;
                b.k = 0.8f * (1.0f - phase) * (0.72f + 0.5f * wobble);
                b.r = 1.5f + 0.55f * wobble;
            }
        }
    }

    // The column is a narrow band: one x test keeps the bubble loop out of the
    // other ~98 % of the frame.
    float bubbleX0 = 0.0f, bubbleX1 = 0.0f, bubbleY0 = 0.0f, bubbleY1 = 0.0f;
    float bubbleR = 2.1f;
    for (int i = 0; i < bubbleCount; ++i)
    {
        const Bubble& bb = bubbles[i];
        if (i == 0) { bubbleX0 = bubbleX1 = bb.x; bubbleY0 = bubbleY1 = bb.y; }
        bubbleX0 = std::min(bubbleX0, bb.x);
        bubbleX1 = std::max(bubbleX1, bb.x);
        bubbleY0 = std::min(bubbleY0, bb.y);
        bubbleY1 = std::max(bubbleY1, bb.y);
        bubbleR = std::max(bubbleR, bb.r);
    }
    bubbleX0 -= bubbleR; bubbleX1 += bubbleR;
    bubbleY0 -= bubbleR; bubbleY1 += bubbleR;

    constexpr float kInv255 = 1.0f / 255.0f;

    // The ghost promise is "no background, no rectangle". The mask edge is a
    // rounded rectangle, and sand/caustics are dense enough that a hard cut
    // there would draw the window's outline on the desktop, so the finished
    // pixel is tapered to nothing over the last 14 logical pixels. The mask's
    // green channel already holds distance/64, so this is a 256-entry table.
    float edgeLut[256];
    for (int i = 0; i < 256; ++i)
        edgeLut[i] = Smoothstep(0.0f, 14.0f, float(i) * (64.0f / 255.0f));

    const bool exact2x = (scale == 2 && canvasW == w_ * 2 && canvasH >= h_ * 2);
    uint32_t*      canvas32 = reinterpret_cast<uint32_t*>(canvas);
    const uint8_t* maskPixels = m.Pixels();

    for (int y = 0; y < h_; ++y)
    {
        const StaticPixel* statRow = stat_.data() + size_t(y) * size_t(w_);
        const uint8_t*     maskRow = maskPixels + size_t(y) * size_t(w_) * 4;
        uint32_t* outRow0 = exact2x ? canvas32 + size_t(y * 2) * size_t(canvasW) : nullptr;
        uint32_t* outRow1 = exact2x ? outRow0 + canvasW : nullptr;

        for (int x = 0; x < w_; ++x)
        {
            if (maskRow[size_t(x) * 4] < 128) continue;

            const StaticPixel& s = statRow[x];
            const float px = float(x) + 0.5f;
            const float py = float(y) + 0.5f;

            // ---- ripples: displacement, then ring light ---------
            float rippleX = 0.0f, rippleY = 0.0f, ring = 0.0f;
            if (activeCount > 0 && t.ripplesEnabled)
            {
                for (int i = 0; i < activeCount; ++i)
                {
                    const Active& a = active[i];
                    const float dx = px - a.x;
                    const float dy = (py - a.y) * aspect;
                    const float limit = a.radius + 6.0f;
                    if (std::fabs(dx) > limit || std::fabs(dy) / aspect > limit) continue;

                    const float d = std::sqrt(dx * dx + dy * dy);
                    const float r = (1.0f - Smoothstep(0.35f * rippleWidth, 1.4f * rippleWidth,
                                                       std::fabs(d - a.radius))) * a.fade;
                    if (r <= 0.0f) continue;
                    ring += r;
                    if (d > 0.0001f)
                    {
                        const float inv = 1.0f / d;
                        rippleX += dx * inv * r * t.rippleRefraction;
                        rippleY += (dy / aspect) * inv * r * t.rippleRefraction;
                    }
                }
            }

            int sx = x + s.dx;
            int sy = y + s.dy;
            if (ring != 0.0f)
            {
                sx += int(std::lround(rippleX));
                sy += int(std::lround(rippleY));
            }
            if ((sx != x || sy != y) && !m.Inside(sx, sy)) { sx = x; sy = y; }

            const uint8_t* c = content + (size_t(sy) * size_t(w_) + size_t(sx)) * 4;

            // ---- content over tint ------------------------------
            const float ca = float(c[3]) * kInv255;
            const float ta = float(s.ta) * kInv255;

            float r, g, b, a;
            if (ca <= 0.0f)
            {
                // Open water: no content sample to composite, which is most
                // of the frame.
                r = float(s.tr) * kInv255 * ta;
                g = float(s.tg) * kInv255 * ta;
                b = float(s.tb) * kInv255 * ta;
                a = ta;
            }
            else
            {
                const float under = 1.0f - ca;
                r = float(c[0]) * kInv255 * ca + float(s.tr) * kInv255 * ta * under;
                g = float(c[1]) * kInv255 * ca + float(s.tg) * kInv255 * ta * under;
                b = float(c[2]) * kInv255 * ca + float(s.tb) * kInv255 * ta * under;
                a = ca + ta * under;
            }

            // ---- caustics, moonlight, stone glow (additive) -----
            r += float(s.cr) * kInv255;
            g += float(s.cg) * kInv255;
            b += float(s.cb) * kInv255;
            a = std::min(1.0f, a + float(s.ca) * kInv255);

            // ---- foam over --------------------------------------
            const float fa = float(s.fa) * kInv255;
            if (fa > 0.0f)
            {
                r = float(s.fr) * kInv255 + r * (1.0f - fa);
                g = float(s.fg) * kInv255 + g * (1.0f - fa);
                b = float(s.fb) * kInv255 + b * (1.0f - fa);
                a = fa + a * (1.0f - fa);
            }

            // ---- bubbles ----------------------------------------
            if (bubbleCount > 0 && px >= bubbleX0 && px <= bubbleX1 &&
                py >= bubbleY0 && py <= bubbleY1)
            {
                for (int i = 0; i < bubbleCount; ++i)
                {
                    const Bubble& bb = bubbles[i];
                    const float dx = px - bb.x;
                    const float dy = py - bb.y;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 > bb.r * bb.r) continue;

                    const float k = bb.k * (1.0f - std::sqrt(d2) / bb.r);
                    if (k <= 0.0f) continue;
                    r += p.stone.r * k;
                    g += p.stone.g * k;
                    b += p.stone.b * k;
                    a = std::min(1.0f, a + k * 0.9f);
                }
            }

            // ---- ripple highlight -------------------------------
            if (ring > 0.0f)
            {
                // Several rings can overlap; cap the sum so a crossing point
                // never lights the desktop to full opacity.
                const float k = std::min(ring, 1.0f) * t.rippleHighlight;
                r += rippleCol.r * k;
                g += rippleCol.g * k;
                b += rippleCol.b * k;
                a = std::min(1.0f, a + k * 0.8f);
            }

            // ---- premultiplied invariant ------------------------
            // Taper to nothing at the mask edge; scaling a premultiplied
            // colour by a factor keeps the rgb <= alpha invariant.
            const float taper = edgeLut[maskRow[size_t(x) * 4 + 1]];
            a *= taper;
            r *= taper;
            g *= taper;
            b *= taper;

            a = std::clamp(a, 0.0f, 1.0f);
            r = std::min(std::max(r, 0.0f), a);
            g = std::min(std::max(g, 0.0f), a);
            b = std::min(std::max(b, 0.0f), a);

            const uint8_t alpha = U8(a);
            const uint8_t blue  = U8(b);
            const uint8_t green = U8(g);
            const uint8_t red   = U8(r);

            // The canvas is BGRA in memory, and 32-bit stores let the upscale
            // write one word per window pixel instead of four bytes.
            const uint32_t packed = uint32_t(blue)
                                  | (uint32_t(green) << 8)
                                  | (uint32_t(red) << 16)
                                  | (uint32_t(alpha) << 24);

            const int px0 = x * scale;
            const int py0 = y * scale;
            if (exact2x)
            {
                const int at = x * 2;
                outRow0[at]     = packed;
                outRow0[at + 1] = packed;
                outRow1[at]     = packed;
                outRow1[at + 1] = packed;
            }
            else
            {
                for (int by = 0; by < scale; ++by)
                {
                    const int ty = py0 + by;
                    if (ty >= canvasH) break;
                    uint32_t* row = canvas32 + size_t(ty) * size_t(canvasW) + size_t(px0);
                    for (int bx = 0; bx < scale; ++bx)
                    {
                        if (px0 + bx >= canvasW) break;
                        row[bx] = packed;
                    }
                }
            }
        }
    }

    const auto finished = std::chrono::steady_clock::now();
    stats_.compositeMs = std::chrono::duration<double, std::milli>(finished - started).count();
}

} // namespace water
