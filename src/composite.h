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
{
    if (canvas == nullptr || rgba == nullptr || scale < 1) return;
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
