// ================================================================
// SEA CREATURES - palette
// Ported from fish-gen/fishgen/palette.py (MIT).
// ================================================================
#include "creature/sea/SeaPalette.h"

namespace sea {
namespace {

double ShiftHue(double h, double target, double amount) {
    double d = std::fmod(target - h + 0.5, 1.0);
    if (d < 0) d += 1.0;
    d -= 0.5;
    const double step = Clamp(d, -amount, amount);
    double out = h + step;
    out -= std::floor(out);
    return out;
}

SeaRgba FromHsv(double h, double s, double v) {
    const Color8 c = HsvToRgb(h, s, v);
    return SeaRgba(c.r, c.g, c.b, 255);
}

} // namespace

const char* SeaSlotName(int slot) {
    static const char* kNames[SeaSlotCount] = {
        "transparent", "outline", "shadow", "base", "highlight",
        "belly", "fin", "fin_dark", "accent", "eye", "accent2"};
    return kNames[ClampI(slot, 0, SeaSlotCount - 1)];
}

SeaRgba RgbaFromHex(const std::string& hex, const SeaRgba& fallback) {
    Color8 c;
    if (!ParseHexColor(hex, c)) return fallback;
    return SeaRgba(c.r, c.g, c.b, 255);
}

SeaPalette BuildPalette(const ColorGenes& g) {
    SeaPalette pal;
    pal.colors[SeaSlotTransparent] = SeaRgba(0, 0, 0, 0);

    const double h = g.hue, s = g.saturation, v = g.value;
    // Classic pixel-art hue shifting: shadows drift toward blue/violet,
    // highlights drift toward yellow.
    pal.colors[SeaSlotShadow] = FromHsv(ShiftHue(h, 0.7, 0.06), s + 0.1, v * 0.68);
    pal.colors[SeaSlotBase] = FromHsv(h, s, v);
    pal.colors[SeaSlotHighlight] = FromHsv(ShiftHue(h, 0.15, 0.05), s * 0.7, Min(1.0, v * 1.18 + 0.08));
    pal.colors[SeaSlotOutline] = FromHsv(ShiftHue(h, 0.7, 0.08), Min(1.0, s + 0.2), 0.16 + v * 0.08);

    const double bl = g.bellyLightness;
    pal.colors[SeaSlotBelly] = FromHsv(ShiftHue(h, 0.15, 0.04), s * (1 - 0.75 * bl),
                                       Min(1.0, v + (1 - v) * bl * 0.9 + 0.05));

    const double fh = h + g.finHueShift;
    double fhUsed = fh, fs = s, fv = v;
    if (!g.finColor.empty()) {
        Color8 c;
        if (ParseHexColor(g.finColor, c)) {
            pal.colors[SeaSlotFin] = SeaRgba(c.r, c.g, c.b, 255);
            RgbToHsv(c, fhUsed, fs, fv);
            pal.colors[SeaSlotFinDark] = FromHsv(ShiftHue(fhUsed, 0.7, 0.05), fs + 0.1, fv * 0.62);
        } else {
            pal.colors[SeaSlotFin] = FromHsv(fh, s * 0.9, Min(1.0, v * 0.95 + 0.05));
        }
    } else if (std::fabs(g.finHueShift) < 0.15) {
        // Same hue family as the body: fins read as thinner/translucent, so go lighter.
        pal.colors[SeaSlotFin] = FromHsv(ShiftHue(fh, 0.15, 0.03), s * 0.75, Min(1.0, v * 1.05 + 0.12));
    } else {
        pal.colors[SeaSlotFin] = FromHsv(fh, s * 0.9, Min(1.0, v * 0.95 + 0.05));
    }
    if (g.finColor.empty()) {
        pal.colors[SeaSlotFinDark] = FromHsv(ShiftHue(fh, 0.7, 0.05), s + 0.1, v * 0.62);
    }

    const double ah = h + g.accentHueShift;
    if (!g.accentColor.empty()) {
        pal.colors[SeaSlotAccent] = RgbaFromHex(g.accentColor, FromHsv(ah, s, v));
    } else if (std::fabs(g.accentHueShift) < 0.2) {
        // Same-ish hue: make the pattern contrast via value instead.
        pal.colors[SeaSlotAccent] = FromHsv(ah, Min(1.0, s + 0.15), v * 0.5);
    } else {
        pal.colors[SeaSlotAccent] = FromHsv(ah, Min(1.0, s + 0.1), Min(1.0, v + 0.05));
    }

    if (!g.accent2Color.empty()) {
        pal.colors[SeaSlotAccent2] = RgbaFromHex(g.accent2Color, FromHsv(h + 0.33, s, v));
    } else {
        pal.colors[SeaSlotAccent2] = FromHsv(h + 0.33, Min(1.0, s + 0.2), Min(1.0, v + 0.15));
    }

    pal.colors[SeaSlotEye] = SeaRgba(245, 245, 235, 255);
    return pal;
}

} // namespace sea
