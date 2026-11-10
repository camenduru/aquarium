#pragma once
// ================================================================
// SEA CREATURES - palette derived from colour genes
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/palette.py (MIT). Fixed slot indices,
// the same as the renderer: index 0 is transparent, then the dark
// outline, shadow, base, highlight, belly, fin, dark fin, accent,
// eye and the second accent.
// ================================================================

#include "creature/sea/SeaGenome.h"

namespace sea {

enum SeaSlot {
    SeaSlotTransparent = 0,
    SeaSlotOutline,
    SeaSlotShadow,
    SeaSlotBase,
    SeaSlotHighlight,
    SeaSlotBelly,
    SeaSlotFin,
    SeaSlotFinDark,
    SeaSlotAccent,
    SeaSlotEye,
    SeaSlotAccent2,
    SeaSlotCount
};

struct SeaRgba {
    unsigned char r = 0, g = 0, b = 0, a = 0;
    SeaRgba() = default;
    SeaRgba(unsigned char R, unsigned char G, unsigned char B, unsigned char A = 255)
        : r(R), g(G), b(B), a(A) {}
    Color8 Rgb() const { return Color8(r, g, b); }
};

struct SeaPalette {
    SeaRgba colors[SeaSlotCount];
    SeaRgba At(int slot) const {
        if (slot <= SeaSlotTransparent || slot >= SeaSlotCount) return SeaRgba();
        return colors[slot];
    }
};

/// Key colours from the genome's colour genes.
SeaPalette BuildPalette(const ColorGenes& genes);

/// "#rrggbb" or a fallback colour.
SeaRgba RgbaFromHex(const std::string& hex, const SeaRgba& fallback);

/// Slot name for metadata (index 1..SeaSlotCount-1).
const char* SeaSlotName(int slot);

} // namespace sea
