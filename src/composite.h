#pragma once
// ================================================================
// AQUARIUM - one sprite blend, shared by the tank and the window
// ----------------------------------------------------------------
// The canvas is premultiplied BGRA, which is what UpdateLayeredWindow
// with AC_SRC_ALPHA wants. Sea sprite sheets are straight-alpha RGBA,
// so every pixel is premultiplied on the way in, exactly once.
//
//   dst = src + dst * (255 - a) / 255      (per channel, premultiplied)
//
// Skipping the premultiply is what gives a layered window a bright halo
// around every sprite: the colour would be added at full strength and
// the desktop behind would show through the transparent corners.
// ================================================================

#include <cstdint>
#include <cstddef>

/// Blend one straight-alpha RGBA sprite cell into a premultiplied BGRA canvas.
/// Nearest-neighbour integer scaling keeps the pixel art crisp; `alpha` scales
/// the whole sprite (used for depth). The source rect is clipped to the sprite,
/// the destination rect to the canvas.
inline void BlendSprite(uint8_t* canvas, int canvasW, int canvasH,
                        const uint8_t* rgba, int srcW, int srcH,
                        int srcX, int srcY, int srcCellW, int srcCellH,
                        int dstX, int dstY, int scale, int alpha = 255)
{    if (canvas == nullptr || rgba == nullptr || scale < 1) return;
    if (srcCellW < 1 || srcCellH < 1) return;

    for (int oy = 0; oy < srcCellH * scale; ++oy)
    {
        const int ty = dstY + oy;
        if (ty < 0 || ty >= canvasH) continue;
        const int spy = srcY + oy / scale;
        if (spy < 0 || spy >= srcH) continue;

        for (int ox = 0; ox < srcCellW * scale; ++ox)
        {
            const int tx = dstX + ox;
            if (tx < 0 || tx >= canvasW) continue;
            const int spx = srcX + ox / scale;
            if (spx < 0 || spx >= srcW) continue;

            const uint8_t* s = rgba + (size_t(spy) * size_t(srcW) + size_t(spx)) * 4;
            int a = s[3];
            if (a == 0) continue;
            if (alpha < 255) a = a * alpha / 255;
            if (a == 0) continue;

            const int r = s[0] * a / 255;
            const int g = s[1] * a / 255;
            const int b = s[2] * a / 255;
            const int inv = 255 - a;

            uint8_t* d = canvas + (size_t(ty) * size_t(canvasW) + size_t(tx)) * 4;
            d[0] = uint8_t(b + d[0] * inv / 255);
            d[1] = uint8_t(g + d[1] * inv / 255);
            d[2] = uint8_t(r + d[2] * inv / 255);
            d[3] = uint8_t(a + d[3] * inv / 255);
        }
    }
}

/// Blend one straight-alpha RGBA sprite cell into a *straight-alpha* RGBA
/// canvas (the water pipeline's content layer). Scale is fixed at 1: the
/// whole logical canvas is upscaled exactly once, at the end.
///
/// `hit` is one byte per logical pixel and is set wherever the sprite is
/// solid enough to belong to the aquarium. Water never marks it, so water
/// pixels stay click-through even where the tint is opaque enough to pass
/// the alpha threshold.
inline void BlendSpriteLogical(uint8_t* canvas, int canvasW, int canvasH,
                               const uint8_t* rgba, int srcW, int srcH,
                               int srcX, int srcY, int srcCellW, int srcCellH,
                               int dstX, int dstY, int alpha, uint8_t* hit)
{
    if (canvas == nullptr || rgba == nullptr) return;
    if (srcCellW < 1 || srcCellH < 1) return;

    for (int oy = 0; oy < srcCellH; ++oy)
    {
        const int ty = dstY + oy;
        if (ty < 0 || ty >= canvasH) continue;
        const int spy = srcY + oy;
        if (spy < 0 || spy >= srcH) continue;

        for (int ox = 0; ox < srcCellW; ++ox)
        {
            const int tx = dstX + ox;
            if (tx < 0 || tx >= canvasW) continue;
            const int spx = srcX + ox;
            if (spx < 0 || spx >= srcW) continue;

            const uint8_t* s = rgba + (size_t(spy) * size_t(srcW) + size_t(spx)) * 4;
            int sa = s[3];
            if (sa == 0) continue;
            if (alpha < 255) sa = sa * alpha / 255;
            if (sa == 0) continue;

            uint8_t* d = canvas + (size_t(ty) * size_t(canvasW) + size_t(tx)) * 4;
            const int da = d[3];
            const int outA = sa + da * (255 - sa) / 255;
            if (outA <= 0) continue;

            for (int c = 0; c < 3; ++c)
            {
                const int num = s[c] * sa + d[c] * da * (255 - sa) / 255;
                d[c] = uint8_t((num + outA / 2) / outA);
            }
            d[3] = uint8_t(outA);

            if (hit != nullptr && sa > 8)
                hit[size_t(ty) * size_t(canvasW) + size_t(tx)] = 1;
        }
    }
}
