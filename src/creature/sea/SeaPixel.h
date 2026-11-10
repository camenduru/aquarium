#pragma once
// ================================================================
// SEA CREATURES - pixel-art post-process
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/pixel.py (MIT).
//
// The shaded render is reduced to a small palette learned with k-means
// in OKLab (perceptually even, so a blue-grey belly does not eat three
// slots), alpha is forced hard (0 or 255), speckles are voted away,
// isolated pixels are tidied, and a dark outline is grown around the
// silhouette. Output is straight-alpha RGBA at sprite resolution.
// ================================================================

#include "creature/sea/SeaPalette.h"

#include <vector>

namespace sea {

struct SeaPixelOptions {
    int paletteSize = 0;   // 0 = derive from the sprite size
    bool outline = true;
    bool despeckle = true;
    bool tidy = false;   // index-level tidy is not in pixel.py; it erased 1px tentacles
    int despecklePasses = 1;   // pixel.py runs one; a second only eats more detail
};

/// Palette size the reference uses: ~10 colours at 16 px, up to 24 at 128 px.
int SeaPaletteSizeFor(int spriteSize);

/// pixel.py's colour work, done before the palette is learned and before the
/// image is indexed: translucent fins are paled, an edge-preserving blur irons
/// out the shading noise that would only become speckle, and a small contrast
/// and chroma lift keeps the shading from collapsing into one flat colour.
/// Applies in place to `rgba` (w*h*4). Call it on every frame BEFORE
/// SeaLearnPalette, so the palette is fitted to the processed pixels.
void SeaPreparePixels(unsigned char* rgba, int width, int height);

/// Learn up to `maxColors` colours from the opaque pixels of one image.
std::vector<SeaRgba> SeaLearnPalette(const unsigned char* rgba, int width, int height, int maxColors);

/// Map to `palette`, harden alpha, clean up and outline into `out` (w*h*4).
void SeaPixelate(const unsigned char* rgba, int width, int height,
                 const std::vector<SeaRgba>& palette, const SeaPixelOptions& options,
                 std::vector<unsigned char>& out,
                 const std::vector<SeaRgba>* sharedOutline = nullptr,
                 std::vector<SeaRgba>* outlineOut = nullptr,
                 const std::vector<Vec3>* eyes = nullptr,
                 const std::vector<unsigned char>* part = nullptr,
                 bool darkEye = false);

} // namespace sea
