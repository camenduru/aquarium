// ================================================================
// SEA CREATURES - species presets, colour varieties and sizes
// Ported from fish-gen/fishgen/species.py and sizes.py (MIT).
// ================================================================
#include "creature/sea/SeaSpecies.h"

#include "creature/sea/SeaRandom.h"

#include <cstring>

namespace sea {
namespace {

struct SpeciesRow {
    const char* name;
    SeaKind kind;
    const char* shape;
};

// Every preset, its kind and the body type / variety its kind generator draws.
const SpeciesRow kSpecies[] = {
    {"Random", SeaKind::Fish, nullptr},
    // ---------------------------------------------------------- fish
    {"Goldfish", SeaKind::Fish, "round"},
    {"Clownfish", SeaKind::Fish, "classic"},
    {"Angelfish", SeaKind::Fish, "deep"},
    {"Tuna", SeaKind::Fish, "torpedo"},
    {"Eel", SeaKind::Fish, "slender"},
    {"Pufferfish", SeaKind::Fish, "classic"},
    {"Betta", SeaKind::Fish, "classic"},
    {"Neon tetra", SeaKind::Fish, "torpedo"},
    {"Koi", SeaKind::Fish, "torpedo"},
    {"Discus", SeaKind::Fish, "round"},
    {"Mackerel", SeaKind::Fish, "slender"},
    {"Piranha", SeaKind::Fish, "deep"},
    {"Yellow tang", SeaKind::Fish, "deep"},
    {"Blue tang", SeaKind::Fish, "deep"},
    {"Rainbow trout", SeaKind::Fish, "torpedo"},
    {"Atlantic salmon", SeaKind::Fish, "torpedo"},
    {"Great barracuda", SeaKind::Fish, "slender"},
    {"Ocean sunfish", SeaKind::Fish, "round"},
    // ---------------------------------------------------------- sharks
    {"Tiger shark", SeaKind::Shark, "requiem"},
    {"Great white shark", SeaKind::Shark, "requiem"},
    {"Hammerhead shark", SeaKind::Shark, "hammerhead"},
    {"Whale shark", SeaKind::Shark, "requiem"},
    {"Mako shark", SeaKind::Shark, "requiem"},
    {"Blacktip reef shark", SeaKind::Shark, "requiem"},
    {"Blue shark", SeaKind::Shark, "requiem"},
    {"Nurse shark", SeaKind::Shark, "requiem"},
    // ---------------------------------------------------------- whales & dolphins
    {"Orca", SeaKind::Whale, "whale"},
    {"Bottlenose dolphin", SeaKind::Whale, "dolphin"},
    {"Common dolphin", SeaKind::Whale, "dolphin"},
    {"Beluga", SeaKind::Whale, "whale"},
    {"Humpback whale", SeaKind::Whale, "whale"},
    {"Sperm whale", SeaKind::Whale, "whale"},
    // ---------------------------------------------------------- rays
    {"Cownose ray", SeaKind::Ray, "eagle"},
    {"Manta ray", SeaKind::Ray, "eagle"},
    {"Spotted eagle ray", SeaKind::Ray, "eagle"},
    {"Stingray", SeaKind::Ray, "stingray"},
    {"Bluespotted stingray", SeaKind::Ray, "stingray"},
    // ---------------------------------------------------------- squid & cuttlefish
    {"Squid", SeaKind::Squid, "squid"},
    {"Giant squid", SeaKind::Squid, "squid"},
    {"Cuttlefish", SeaKind::Squid, "cuttlefish"},
    // ---------------------------------------------------------- octopus
    {"Octopus", SeaKind::Octopus, "octopus"},
    {"Blue-ringed octopus", SeaKind::Octopus, "octopus"},
    {"Dumbo octopus", SeaKind::Octopus, "dumbo"},
    // ---------------------------------------------------------- urchins
    {"Purple sea urchin", SeaKind::Urchin, "short"},
    {"Long-spined urchin", SeaKind::Urchin, "long"},
    {"Red pencil urchin", SeaKind::Urchin, "short"},
    {"Green sea urchin", SeaKind::Urchin, "short"},
};

constexpr int kSpeciesCount = (int)(sizeof(kSpecies) / sizeof(kSpecies[0]));

// Measured adult size in cm (nose-tail; rays: disc width; urchins: shell).
struct SizeRow {
    const char* name;
    double typical, low, high;
};

const SizeRow kSizes[] = {
    {"Goldfish", 18, 12, 28},
    {"Clownfish", 8.5, 7, 11},
    {"Angelfish", 13, 10, 15},
    {"Tuna", 150, 100, 200},
    {"Eel", 70, 50, 100},
    {"Pufferfish", 25, 18, 33},
    {"Betta", 6, 5, 7},
    {"Neon tetra", 3.5, 3, 4},
    {"Koi", 60, 45, 80},
    {"Discus", 15, 12, 20},
    {"Mackerel", 30, 20, 45},
    {"Piranha", 28, 20, 35},
    {"Yellow tang", 17, 13, 20},
    {"Blue tang", 25, 18, 31},
    {"Rainbow trout", 55, 40, 76},
    {"Atlantic salmon", 80, 65, 100},
    {"Great barracuda", 120, 80, 160},
    {"Ocean sunfish", 180, 140, 250},
    {"Tiger shark", 375, 325, 450},
    {"Great white shark", 450, 340, 550},
    {"Hammerhead shark", 430, 330, 550},
    {"Whale shark", 1000, 800, 1400},
    {"Mako shark", 250, 200, 320},
    {"Blacktip reef shark", 140, 100, 180},
    {"Blue shark", 220, 170, 310},
    {"Nurse shark", 220, 190, 300},
    {"Orca", 700, 560, 850},
    {"Orca male", 800, 680, 980},
    {"Bottlenose dolphin", 280, 230, 350},
    {"Common dolphin", 200, 170, 240},
    {"Beluga", 420, 330, 550},
    {"Humpback whale", 1400, 1250, 1600},
    {"Sperm whale", 1100, 1000, 1200},
    {"Sperm whale male", 1600, 1450, 1800},
    {"Cownose ray", 90, 75, 110},
    {"Manta ray", 350, 280, 480},
    {"Spotted eagle ray", 150, 110, 230},
    {"Stingray", 90, 55, 150},
    {"Bluespotted stingray", 25, 20, 35},
    {"Squid", 40, 25, 60},
    {"Giant squid", 1000, 750, 1300},
    {"Cuttlefish", 30, 20, 45},
    {"Octopus", 60, 35, 100},
    {"Blue-ringed octopus", 15, 12, 22},
    {"Dumbo octopus", 25, 20, 30},
    {"Purple sea urchin", 7, 5, 9},
    {"Long-spined urchin", 7, 5, 9},
    {"Red pencil urchin", 8, 6, 10},
    {"Green sea urchin", 5, 4, 8},
};

constexpr int kSizeCount = (int)(sizeof(kSizes) / sizeof(kSizes[0]));

/// A preset's complete colour override, so its colours never depend on the seed.
void SetColors(SeaGenome& g, double hue, double sat, double val, double belly,
               PatternKind pattern = PatternKind::None, double scale = 0.5, double offset = 0.0,
               const char* accent = nullptr, const char* fin = nullptr, double finTips = 0.0) {
    ColorGenes c;
    c.hue = hue - std::floor(hue);
    c.saturation = Saturate(sat);
    c.value = Saturate(val);
    c.bellyLightness = Saturate(belly);
    c.pattern = pattern;
    c.patternScale = scale;
    c.patternOffset = offset;
    if (accent != nullptr) c.accentColor = accent;
    if (fin != nullptr) c.finColor = fin;
    c.finTips = finTips;
    g.colors = c;
}

// ---------------------------------------------------------------- fish presets
void ApplyFishPreset(const std::string& n, SeaGenome& g) {
    if (n == "Goldfish") {
        g.tailShape = TailShape::Fan;
        g.tailLength = 0.36;
        g.tailSpread = 1.25;
        g.peduncle = 0.28;
        g.scales = 0.035;
        g.silver = 0.35;
        g.gloss = 0.5;
        g.finAlpha = 0.6;
        g.irisColor = "#d8a850";
        g.dorsal.present = true;
        g.dorsal.shape = FinShape::Rounded;
        g.dorsal.height = 0.14;
        g.anal.present = true;
        g.anal.shape = FinShape::Rounded;
        SetColors(g, 0.07, 0.85, 0.95, 0.45);
    } else if (n == "Clownfish") {
        // fish-gen's Clownfish overrides exactly these keys; anything not listed
        // keeps the value the generator drew for the seed. Pinning thickness,
        // back_arch, eye_pos or eye_height here would quietly turn every
        // individual of the species into the same animal.
        g.height = 0.44;
        g.peak = 0.36;
        g.nose = 0.5;
        g.taper = 1.0;
        g.peduncle = 0.32;
        g.tailShape = TailShape::Rounded;
        g.tailLength = 0.24;
        g.eyeSize = 0.27;
        g.scales = 0.025;
        g.finAlpha = 0.92;
        g.pectoralSize = 0.18;
        g.irisColor = "#e8a020";
        g.gloss = 0.45;
        g.dorsal.present = true;
        g.dorsal.shape = FinShape::Rounded;
        g.dorsal.start = 0.25;
        g.dorsal.end = 0.75;
        g.dorsal.height = 0.12;
        g.anal.present = true;
        g.anal.shape = FinShape::Rounded;
        SetColors(g, 0.065, 0.92, 0.95, 0.15, PatternKind::Stripes, 0.0, 0.25, "#f6f6f0", "#f08a24");
        g.colors.finEdgeColor = "#141012";
        g.colors.accentEdgeColor = "#141012";
    } else if (n == "Angelfish") {
        g.height = 0.62;
        g.thickness = 0.35;
        g.peak = 0.38;
        g.nose = 0.85;
        g.taper = 0.9;
        g.peduncle = 0.22;
        g.eyePos = 0.13;
        g.eyeSize = 0.22;
        g.eyeHeight = 0.3;
        g.tailShape = TailShape::Lunate;
        g.tailLength = 0.3;
        g.tailSpread = 1.25;
        g.scales = 0.02;
        g.silver = 0.45;
        g.finAlpha = 0.55;
        g.irisColor = "#c83a2a";
        g.dorsal = {true, 0.3, 0.66, 0.48, FinShape::Pennant, 0.95};
        g.anal = {true, 0.42, 0.74, 0.46, FinShape::Pennant, 0.95};
        g.pelvic.present = true;
        g.pelvic.start = 0.3;
        g.pelvic.height = 0.32;
        SetColors(g, 0.12, 0.12, 0.9, 0.4, PatternKind::Stripes, 0.0, 0.065, "#24222b", "#e9e3cf");
        g.colors.patternOnFins = true;
    } else if (n == "Tuna") {
        g.height = 0.3;
        g.thickness = 0.75;
        g.nose = 0.5;
        g.peduncle = 0.07;
        g.eyeSize = 0.14;
        g.eyeHeight = 0.3;
        g.tailShape = TailShape::Lunate;
        g.tailLength = 0.28;
        g.tailSpread = 1.7;
        g.finlets = 8;
        g.silver = 0.75;
        g.scales = 0.012;
        g.finAlpha = 0.95;
        g.gloss = 0.6;
        g.irisColor = "#c0c4c8";
        g.dorsal = {true, 0.3, 0.45, 0.1, FinShape::Triangle, g.dorsal.sweep};
        g.dorsal2 = {true, 0.55, 0.64, 0.2, FinShape::Falcate, 0.8};
        g.anal = {true, 0.6, 0.68, 0.19, FinShape::Falcate, 0.8};
        SetColors(g, 0.6, 0.7, 0.3, 1.0, PatternKind::Lateral, 0.0, 0.0, "#d9c25a", "#e8c840");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = 0.2;
        g.colors.tailBodyColor = true;
    } else if (n == "Eel") {
        g.height = 0.1;
        g.peak = 0.3;
        g.nose = 0.6;
        g.taper = 1.7;
        g.peduncle = 0.05;
        g.tailShape = TailShape::None;
        g.scales = 0.0;
        g.gloss = 0.7;
        g.finAlpha = 0.75;
        g.irisColor = "#c8b070";
        g.dorsal = {true, 0.32, 1.0, 0.04, FinShape::Long, 0.1};
        g.anal = {true, 0.55, 1.0, 0.035, FinShape::Long, 0.1};
        g.pelvic.present = false;
        g.pectoralSize = 0.07;
        g.waveCount = 1.4;
        g.swimAmplitude = 0.75;
        SetColors(g, 0.2, 0.45, 0.42, 0.75, PatternKind::None, 0.5, 0.0, nullptr, "#7c8a44");
    } else if (n == "Pufferfish") {
        g.height = 0.38;
        g.thickness = 0.75;
        g.nose = 0.7;
        g.peak = 0.4;
        g.tailShape = TailShape::Rounded;
        g.tailLength = 0.18;
        g.tailSpread = 0.8;
        g.eyeSize = 0.32;
        g.eyeHeight = 0.45;
        g.eyePos = 0.14;
        g.scales = 0.0;
        g.gloss = 0.35;
        g.finAlpha = 0.6;
        g.irisColor = "#58b0a0";
        g.dorsal = {true, 0.68, 0.8, 0.08, FinShape::Rounded, g.dorsal.sweep};
        g.anal = {true, 0.7, 0.82, 0.07, FinShape::Rounded, g.anal.sweep};
        g.pelvic.present = false;
        SetColors(g, 0.13, 0.6, 0.85, 1.0, PatternKind::Spots, 0.6, 0.0, "#4a3a24", "#e8d27a");
    } else if (n == "Betta") {
        g.height = 0.3;
        g.tailShape = TailShape::Fan;
        g.tailLength = 0.55;
        g.tailSpread = 2.6;
        g.scales = 0.03;
        g.silver = 0.25;
        g.finAlpha = 0.85;
        g.hasTailAlpha = true;
        g.tailAlpha = 0.88;
        g.irisColor = "#3a2020";
        g.billow = 1.0;
        g.swimAmplitude = 0.25;
        g.mouth = 0.03;
        g.mouthHeight = 0.1;
        g.dorsal = {true, 0.55, 0.93, 0.26, FinShape::Sail, 1.0};
        g.anal = {true, 0.38, 0.99, 0.36, FinShape::Sail, 1.0};
        g.pelvic.present = true;
        g.pelvic.start = 0.34;
        g.pelvic.height = 0.2;
        SetColors(g, 0.97, 0.85, 0.72, 0.0, PatternKind::None, 0.5, 0.0, nullptr, "#c41a2a");
    } else if (n == "Neon tetra") {
        g.height = 0.28;
        g.tailShape = TailShape::Forked;
        g.tailLength = 0.3;
        g.tailSpread = 1.6;
        g.eyeSize = 0.34;
        g.scales = 0.03;
        g.finAlpha = 0.3;
        g.hasTailAlpha = true;
        g.tailAlpha = 0.8;
        g.irisColor = "#b8c8d8";
        g.dorsal = {true, 0.42, 0.54, 0.08, FinShape::Triangle, 0.5};
        g.anal = {true, 0.6, 0.85, 0.06, FinShape::Long, g.anal.sweep};
        SetColors(g, 0.58, 0.45, 0.38, 0.9, PatternKind::Neon, 0.5, 0.0, "#3fe8ff", "#c7d2dc");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.15;
        g.colors.accent2Color = "#e8283c";
        g.colors.tailColor = "#9aa8b8";
    } else if (n == "Koi") {
        g.height = 0.3;
        g.tailShape = TailShape::Rounded;
        g.pectoralSize = 0.18;
        g.scales = 0.03;
        g.silver = 0.15;
        g.finAlpha = 0.7;
        g.irisColor = "#d0c8b8";
        g.mouth = 0.04;
        g.mouthHeight = -0.2;
        SetColors(g, 0.1, 0.06, 0.95, 0.3, PatternKind::Koi, 0.45, 0.0, "#e24a1e", "#f2ece4");
    } else if (n == "Discus") {
        g.height = 0.85;
        g.nose = 0.9;
        g.peak = 0.5;
        g.tailShape = TailShape::Truncate;
        g.tailLength = 0.16;
        g.scales = 0.015;
        g.finAlpha = 0.75;
        g.irisColor = "#d02820";
        g.dorsal = {true, 0.25, 0.92, 0.1, FinShape::Long, g.dorsal.sweep};
        g.anal = {true, 0.45, 0.92, 0.1, FinShape::Long, g.anal.sweep};
        SetColors(g, 0.04, 0.75, 0.75, 0.0, PatternKind::Discus, 0.6, 0.0, "#3fc0d1");
    } else if (n == "Mackerel") {
        g.height = 0.19;
        g.thickness = 0.7;
        g.nose = 0.75;
        g.peak = 0.38;
        g.taper = 1.1;
        g.peduncle = 0.08;
        g.tailShape = TailShape::Forked;
        g.tailLength = 0.18;
        g.tailSpread = 1.25;
        g.eyeSize = 0.24;
        g.eyeHeight = 0.3;
        g.finlets = 5;
        g.silver = 0.65;
        g.scales = 0.008;
        g.finAlpha = 0.7;
        g.gloss = 0.6;
        g.irisColor = "#c8ccd0";
        g.dorsal = {true, 0.28, 0.4, 0.07, FinShape::Triangle, 0.5};
        g.dorsal2 = {true, 0.6, 0.67, 0.05, FinShape::Triangle, 0.5};
        g.anal = {true, 0.62, 0.69, 0.045, FinShape::Triangle, 0.5};
        g.pelvic.present = true;
        g.pelvic.start = 0.36;
        g.pelvic.height = 0.05;
        SetColors(g, 0.47, 0.5, 0.4, 1.0, PatternKind::Mackerel, 0.4, 0.0, "#0e1a22", "#8fa2af");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.05;
        g.colors.accent2Color = "#2f5e76";
    } else if (n == "Piranha") {
        g.height = 0.55;
        g.nose = 0.7;
        g.tailShape = TailShape::Truncate;
        g.eyeSize = 0.22;
        g.scales = 0.012;
        g.silver = 0.5;
        g.finAlpha = 0.8;
        g.irisColor = "#c83020";
        g.mouth = 0.06;
        g.mouthHeight = -0.35;
        g.teeth = true;
        SetColors(g, 0.58, 0.12, 0.55, 0.0, PatternKind::RedBelly, 0.5, 0.0, "#c83a28", "#3a3f4a");
    } else if (n == "Yellow tang") {
        g.height = 0.62;
        g.thickness = 0.35;
        g.peak = 0.45;
        g.nose = 1.35;
        g.taper = 1.0;
        g.peduncle = 0.16;
        g.eyePos = 0.18;
        g.eyeHeight = 0.35;
        g.eyeSize = 0.2;
        g.tailShape = TailShape::Truncate;
        g.tailLength = 0.2;
        g.tailSpread = 1.0;
        g.scales = 0.008;
        g.finAlpha = 0.85;
        g.irisColor = "#2a2a2a";
        g.mouth = 0.025;
        g.mouthHeight = 0.0;
        g.dorsal = {true, 0.25, 0.92, 0.16, FinShape::Long, g.dorsal.sweep};
        g.anal = {true, 0.45, 0.92, 0.15, FinShape::Long, g.anal.sweep};
        g.pelvic.present = true;
        g.pelvic.start = 0.35;
        g.pelvic.height = 0.08;
        SetColors(g, 0.135, 0.95, 0.97, 0.1, PatternKind::None, 0.5, 0.0, nullptr, "#f4d018");
    } else if (n == "Blue tang") {
        g.height = 0.52;
        g.thickness = 0.35;
        g.peak = 0.42;
        g.nose = 0.8;
        g.peduncle = 0.14;
        g.eyePos = 0.14;
        g.eyeSize = 0.22;
        g.tailShape = TailShape::Truncate;
        g.tailLength = 0.22;
        g.tailSpread = 1.2;
        g.scales = 0.008;
        g.finAlpha = 0.9;
        g.irisColor = "#202020";
        g.mouth = 0.025;
        g.mouthHeight = 0.0;
        g.dorsal = {true, 0.22, 0.92, 0.12, FinShape::Long, g.dorsal.sweep};
        g.anal = {true, 0.45, 0.92, 0.11, FinShape::Long, g.anal.sweep};
        SetColors(g, 0.6, 0.85, 0.85, 0.1, PatternKind::Tang, 0.5, 0.0, "#0c0c14", "#1a3a9a");
        g.colors.finEdgeColor = "#0c0c14";
        g.colors.tailColor = "#f2c818";
    } else if (n == "Rainbow trout") {
        g.height = 0.25;
        g.thickness = 0.65;
        g.nose = 0.7;
        g.peduncle = 0.12;
        g.eyeSize = 0.18;
        g.scales = 0.008;
        g.silver = 0.4;
        g.finAlpha = 0.85;
        g.irisColor = "#c8b880";
        g.mouth = 0.07;
        g.mouthHeight = -0.15;
        g.tailShape = TailShape::Truncate;
        g.tailLength = 0.2;
        g.tailSpread = 1.2;
        g.dorsal = {true, 0.38, 0.5, 0.09, FinShape::Rounded, g.dorsal.sweep};
        g.anal = {true, 0.68, 0.78, 0.07, FinShape::Triangle, g.anal.sweep};
        g.pelvic.present = true;
        g.pelvic.start = 0.5;
        g.pelvic.height = 0.06;
        SetColors(g, 0.22, 0.35, 0.5, 0.95, PatternKind::Trout, 0.8, 0.0, "#1c1c18", "#8a8a66");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.3;
        g.colors.accent2Color = "#d86a80";
        g.colors.patternOnFins = true;
    } else if (n == "Atlantic salmon") {
        g.height = 0.24;
        g.thickness = 0.65;
        g.nose = 0.75;
        g.peduncle = 0.09;
        g.eyeSize = 0.16;
        g.scales = 0.008;
        g.silver = 0.75;
        g.finAlpha = 0.8;
        g.gloss = 0.55;
        g.irisColor = "#c8ccd0";
        g.mouth = 0.07;
        g.mouthHeight = -0.15;
        g.tailShape = TailShape::Truncate;
        g.tailLength = 0.2;
        g.tailSpread = 1.3;
        g.dorsal = {true, 0.38, 0.5, 0.09, FinShape::Triangle, g.dorsal.sweep};
        g.anal = {true, 0.7, 0.78, 0.07, FinShape::Triangle, g.anal.sweep};
        g.pelvic.present = true;
        g.pelvic.start = 0.5;
        g.pelvic.height = 0.06;
        SetColors(g, 0.5, 0.35, 0.45, 1.0, PatternKind::Salmon, 0.7, 0.0, "#141418", "#6a7a80");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = 0.05;
    } else if (n == "Great barracuda") {
        g.height = 0.15;
        g.thickness = 0.75;
        g.nose = 1.4;
        g.peak = 0.45;
        g.taper = 1.1;
        g.peduncle = 0.09;
        g.flatHead = 0.4;
        g.eyeSize = 0.26;
        g.eyePos = 0.12;
        g.eyeHeight = 0.35;
        g.scales = 0.008;
        g.silver = 0.65;
        g.finAlpha = 0.85;
        g.gloss = 0.6;
        g.irisColor = "#d8d0b0";
        g.mouth = 0.14;
        g.mouthHeight = -0.3;
        g.teeth = true;
        g.scariness = 0.75;
        g.tailShape = TailShape::Forked;
        g.tailLength = 0.2;
        g.tailSpread = 1.6;
        g.dorsal = {true, 0.38, 0.46, 0.08, FinShape::Triangle, g.dorsal.sweep};
        g.dorsal2 = {true, 0.72, 0.78, 0.06, FinShape::Triangle, g.dorsal2.sweep};
        g.anal = {true, 0.72, 0.79, 0.06, FinShape::Triangle, g.anal.sweep};
        g.pelvic.present = true;
        g.pelvic.start = 0.45;
        g.pelvic.height = 0.05;
        SetColors(g, 0.52, 0.2, 0.55, 1.0, PatternKind::Barracuda, 0.4, 0.0, "#2a3238", "#5a6870");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.05;
        g.colors.accent2Color = "#0e1014";
    } else if (n == "Ocean sunfish") {
        g.height = 0.9;
        g.thickness = 0.32;
        g.peak = 0.4;
        g.nose = 0.5;
        g.taper = 0.8;
        g.peduncle = 0.55;
        g.eyePos = 0.16;
        g.eyeSize = 0.1;
        g.eyeHeight = 0.25;
        g.tailShape = TailShape::Rounded;
        g.tailLength = 0.1;
        g.tailSpread = 0.62;
        g.scales = 0.0;
        g.finAlpha = 1.0;
        g.gloss = 0.2;
        g.irisColor = "#303030";
        g.mouth = 0.02;
        g.mouthHeight = 0.0;
        g.swimAmplitude = 0.12;
        g.pectoralSize = 0.1;
        g.dorsal = {true, 0.56, 0.86, 0.5, FinShape::Falcate, 0.25};
        g.anal = {true, 0.56, 0.86, 0.47, FinShape::Falcate, 0.25};
        g.pelvic.present = false;
        SetColors(g, 0.58, 0.08, 0.58, 0.6, PatternKind::Blotches, 0.15, 0.0, "#7e888f", "#5f6970");
        g.colors.accent2Color = "#6a737c";
        g.colors.tailBodyColor = true;
    }
}

// ---------------------------------------------------------------- other kinds
void ApplyOtherPreset(const std::string& n, SeaGenome& g) {
    if (n == "Tiger shark") {
        g.nose = 0.95;
        g.height = 0.28;
        g.tailLength = 0.34;
        g.teeth = true;
        g.scariness = 0.7;
        g.scars = 0.4;
        SetColors(g, 0.1, 0.25, 0.58, 1.0, PatternKind::Tiger, 0.3, 0.0, "#3b352c", "#857a68");
        g.colors.tailBodyColor = true;
    } else if (n == "Great white shark") {
        g.height = 0.31;
        g.nose = 0.95;
        g.flatHead = 0.25;
        g.peak = 0.4;
        g.peduncle = 0.09;
        g.tailSpread = 1.8;
        g.tailLength = 0.3;
        g.tailShape = TailShape::Lunate;
        g.dorsal = {true, 0.39, 0.55, 0.24, FinShape::Triangle, 0.55};
        g.pectoralSize = 0.34;
        g.pectoralDroop = 0.75;
        g.mouth = 0.13;
        g.mouthHeight = -0.62;
        g.teeth = true;
        g.scariness = 0.9;
        g.scars = 0.7;
        SetColors(g, 0.57, 0.14, 0.4, 1.0, PatternKind::None, 0.5, 0.0, nullptr, "#5f6770");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.12;
        g.colors.tailBodyColor = true;
    } else if (n == "Hammerhead shark") {
        g.hammer = 0.14;
        g.height = 0.25;
        g.eyeSize = 0.2;
        g.teeth = true;
        g.scariness = 0.35;
        g.dorsal.height = 0.3;
        g.dorsal.shape = FinShape::Falcate;
        g.dorsal.sweep = 0.6;
        SetColors(g, 0.12, 0.15, 0.6, 1.0, PatternKind::None, 0.5, 0.0, nullptr, "#8f8a7c");
        g.colors.tailBodyColor = true;
    } else if (n == "Whale shark") {
        g.height = 0.26;
        g.thickness = 1.2;
        g.nose = 0.4;
        g.peak = 0.32;
        g.flatHead = 0.95;
        g.backArch = 0.6;
        g.taper = 1.1;
        g.eyePos = 0.07;
        g.eyeSize = 0.09;
        g.eyeHeight = -0.15;
        g.mouth = 0.06;
        g.mouthHeight = -0.15;
        g.teeth = false;
        g.scariness = 0.0;
        g.tailLength = 0.38;
        g.tailSpread = 1.9;
        g.pectoralSize = 0.32;
        g.swimAmplitude = 0.3;
        g.tailShape = TailShape::Lunate;
        g.dorsal = {true, 0.5, 0.66, 0.15, FinShape::Triangle, 0.6};
        g.dorsal2.present = true;
        g.dorsal2.start = 0.8;
        g.dorsal2.height = 0.05;
        g.scars = 0.5;
        SetColors(g, 0.6, 0.4, 0.4, 0.9, PatternKind::WhaleShark, 0.4, 0.3, "#eef3f6", "#4a5a70");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.3;
        g.colors.accent2Color = "#a7bccc";
        g.colors.tailBodyColor = true;
    } else if (n == "Mako shark") {
        g.height = 0.22;
        g.nose = 1.75;
        g.flatHead = 0.1;
        g.peduncle = 0.08;
        g.tailSpread = 1.8;
        g.eyeSize = 0.2;
        g.teeth = true;
        g.scariness = 0.8;
        g.tailShape = TailShape::Lunate;
        g.gloss = 0.45;
        SetColors(g, 0.62, 0.65, 0.5, 1.0, PatternKind::None, 0.5, 0.0, nullptr, "#3a4f9a");
        g.colors.tailBodyColor = true;
    } else if (n == "Blacktip reef shark") {
        g.height = 0.25;
        g.nose = 1.3;
        g.teeth = true;
        g.scariness = 0.4;
        g.dorsal.height = 0.19;
        g.dorsal.shape = FinShape::Triangle;
        SetColors(g, 0.1, 0.15, 0.72, 1.0, PatternKind::None, 0.5, 0.0, "#0b0b0e", "#a39d8f");
        g.colors.finTips = 0.45;
        g.colors.tailBodyColor = true;
    } else if (n == "Blue shark") {
        g.height = 0.2;
        g.nose = 1.6;
        g.flatHead = 0.1;
        g.peduncle = 0.09;
        g.tailSpread = 1.6;
        g.tailLength = 0.38;
        g.eyeSize = 0.24;
        g.pectoralSize = 0.29;
        g.pectoralDroop = 0.85;
        g.gloss = 0.45;
        g.teeth = false;
        g.scariness = 0.0;
        g.dorsal = {true, 0.42, 0.54, 0.15, FinShape::Triangle, 0.6};
        SetColors(g, 0.62, 0.72, 0.62, 1.0, PatternKind::None, 0.5, 0.0, nullptr, "#3a5ab8");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.1;
        g.colors.tailBodyColor = true;
    } else if (n == "Nurse shark") {
        g.height = 0.22;
        g.thickness = 1.05;
        g.nose = 0.45;
        g.flatHead = 0.8;
        g.peak = 0.3;
        g.backArch = 0.55;
        g.eyeSize = 0.1;
        g.tailLength = 0.42;
        g.tailSpread = 1.1;
        g.pectoralSize = 0.24;
        g.pectoralDroop = 0.7;
        g.gloss = 0.2;
        g.teeth = false;
        g.scariness = 0.0;
        g.mouth = 0.04;
        g.mouthHeight = -0.5;
        g.dorsal = {true, 0.52, 0.64, 0.12, FinShape::Rounded, 0.3};
        g.dorsal2.present = true;
        g.dorsal2.start = 0.7;
        g.dorsal2.height = 0.09;
        SetColors(g, 0.08, 0.4, 0.48, 0.5, PatternKind::None, 0.5, 0.0, nullptr, "#7a6448");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.4;
        g.colors.tailBodyColor = true;
    } else if (n == "Orca") {
        // fish-gen's Orca override only pins these genes; the rest (eye size,
        // tail length/spread, mouth) keep what the whale generator drew.
        g.height = 0.3;
        g.nose = 0.8;
        g.flatHead = 0.0;
        g.eyePos = 0.1;
        g.eyeHeight = 0.2;
        g.pectoralSize = 0.2;
        g.pectoralDroop = 0.6;
        g.gloss = 0.75;
        g.scars = 0.25;
        SetColors(g, 0.62, 0.1, 0.06, 0.0, PatternKind::Orca, 0.5, 0.0, "#f2f2ee", "#121318");
        g.colors.accent2Color = "#7d828c";
    } else if (n == "Bottlenose dolphin") {
        g.beak = 0.12;
        g.eyePos = 0.19;
        g.height = 0.21;
        g.nose = 0.6;
        g.melon = 0.012;
        g.gloss = 0.7;
        g.scars = 0.25;
        g.dorsal = {true, 0.42, 0.56, 0.13, FinShape::Falcate, 0.75};
        SetColors(g, 0.58, 0.12, 0.55, 0.8);
    } else if (n == "Common dolphin") {
        g.beak = 0.09;
        g.eyePos = 0.16;
        g.height = 0.2;
        g.nose = 0.55;
        g.gloss = 0.7;
        g.dorsal = {true, 0.42, 0.55, 0.14, FinShape::Falcate, 0.8};
        SetColors(g, 0.62, 0.3, 0.2, 1.0, PatternKind::DolphinCape, 0.6, 0.0, "#c8b07a", "#20262f");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.4;
        g.colors.accent2Color = "#8a9098";
    } else if (n == "Beluga") {
        g.height = 0.3;
        g.nose = 0.5;
        g.peak = 0.5;
        g.beak = 0.03;
        g.melon = 0.085;
        g.neck = 0.3;
        g.flatHead = 0.0;
        g.backArch = 0.45;
        g.eyePos = 0.15;
        g.eyeHeight = 0.0;
        g.dorsal.present = false;
        g.tailLength = 0.18;
        g.pectoralSize = 0.15;
        g.pectoralShape = PectoralShape::Lobe;
        g.mouth = 0.05;
        g.mouthHeight = -0.3;
        g.gloss = 0.5;
        g.scars = 0.35;
        SetColors(g, 0.12, 0.06, 0.93, 0.4);
    } else if (n == "Humpback whale") {
        g.height = 0.26;
        g.nose = 0.75;
        g.peak = 0.42;
        g.flatHead = 0.75;
        g.backArch = 0.55;
        g.pectoralSize = 0.55;
        g.pectoralDroop = 0.42;
        g.eyePos = 0.2;
        g.eyeHeight = -0.35;
        g.tailSpread = 1.7;
        g.pleats = true;
        g.knobs = 1.0;
        g.scars = 0.4;
        g.gloss = 0.45;
        g.dorsal = {true, 0.6, 0.66, 0.04, FinShape::Rounded, 0.5};
        SetColors(g, 0.6, 0.12, 0.25, 0.9, PatternKind::Humpback, 0.5, 0.3, "#d8dce0", "#dfe2e6");
        g.colors.hasBellyLevel = true;
        g.colors.bellyLevel = -0.5;
    } else if (n == "Sperm whale") {
        g.height = 0.25;
        g.nose = 0.12;
        g.peak = 0.3;
        g.flatHead = 1.0;
        g.backArch = 0.55;
        g.taper = 1.4;
        g.eyePos = 0.3;
        g.eyeHeight = -0.4;
        g.pectoralSize = 0.1;
        g.mouth = 0.22;
        g.mouthHeight = -0.85;
        g.gloss = 0.35;
        g.scars = 1.0;
        g.tailSpread = 1.5;
        g.dorsal = {true, 0.62, 0.72, 0.035, FinShape::Rounded, 0.4};
        SetColors(g, 0.08, 0.12, 0.25, 0.15);
    } else if (n == "Cownose ray") {
        g.nose = 0.3;
        g.thickness = 1.8;
        g.lobes = LobeShape::Round;
        g.lobeLength = 0.045;
        g.wings.start = 0.1;
        g.wings.end = 0.36;
        g.wings.span = 0.4;
        g.wings.sweep = 1.25;
        g.wings.flap = 0.85;
        g.wings.wave = 0.2;
        g.dorsal = {true, 0.45, 0.5, 0.04, FinShape::Triangle, 0.6};
        SetColors(g, 0.08, 0.5, 0.5, 1.0);
    } else if (n == "Manta ray") {
        g.nose = 0.3;
        g.thickness = 2.4;
        g.height = 0.2;
        g.peak = 0.3;
        g.taper = 3.0;
        g.peduncle = 0.1;
        g.lobes = LobeShape::Horns;
        g.lobeLength = 0.11;
        g.eyePos = 0.07;
        g.eyeHeight = 0.1;
        g.eyeSize = 0.16;
        g.mouth = 0.04;
        g.mouthHeight = -0.15;
        g.scars = 0.3;
        g.wings.start = 0.05;
        g.wings.end = 0.6;
        g.wings.span = 0.85;
        g.wings.sweep = 1.15;
        g.wings.flap = 0.75;
        g.wings.wave = 0.15;
        SetColors(g, 0.6, 0.15, 0.1, 1.0, PatternKind::Chevron, 0.5, 0.0, "#e8ecec");
        g.colors.bellyPattern = true;
    } else if (n == "Spotted eagle ray") {
        g.nose = 0.55;
        g.thickness = 1.5;
        g.lobes = LobeShape::None;
        g.lobeLength = 0.0;
        g.wings.start = 0.06;
        g.wings.end = 0.27;
        g.wings.span = 0.34;
        g.wings.sweep = 1.25;
        g.wings.flap = 0.85;
        g.wings.wave = 0.2;
        SetColors(g, 0.62, 0.3, 0.16, 1.0, PatternKind::Spots, 1.0, 0.1, "#eef0f0");
    } else if (n == "Stingray") {
        g.nose = 0.85;
        g.wings.start = 0.0;
        g.wings.end = 0.5;
        g.wings.span = 0.28;
        g.wings.sweep = 0.45;
        g.wings.shape = WingShape::Diamond;
        g.wings.flap = 0.16;
        g.wings.wave = 1.3;
        SetColors(g, 0.09, 0.3, 0.42, 1.0);
    } else if (n == "Bluespotted stingray") {
        g.nose = 0.6;
        g.wings.start = 0.0;
        g.wings.end = 0.48;
        g.wings.span = 0.2;
        g.wings.sweep = 0.45;
        g.wings.shape = WingShape::Round;
        g.wings.flap = 0.16;
        g.wings.wave = 1.3;
        SetColors(g, 0.11, 0.5, 0.68, 1.0, PatternKind::Spots, 0.4, 0.6, "#2f8ae0");
    } else if (n == "Squid") {
        g.height = 0.22;
        g.arms.length = 0.45;
        g.arms.tentacleLength = 0.75;
        SetColors(g, 0.97, 0.35, 0.85, 0.5, PatternKind::Spots, 1.0, 0.0, "#b0384a");
    } else if (n == "Giant squid") {
        g.height = 0.18;
        g.wings.end = 0.22;
        g.wings.span = 0.12;
        g.wings.dihedral = 0.35;
        g.arms.length = 0.55;
        g.arms.tentacleLength = 1.0;
        g.arms.thickness = 0.028;
        SetColors(g, 0.0, 0.7, 0.55, 0.3);
    } else if (n == "Cuttlefish") {
        SetColors(g, 0.86, 0.45, 0.82, 0.5, PatternKind::Shimmer, 0.4, 0.0, "#4ce6d2");
        g.colors.accent2Color = "#ffc35a";
    } else if (n == "Octopus") {
        g.height = 0.7;
        g.arms.length = 1.3;
        g.arms.thickness = 0.05;
        g.arms.spread = 0.75;
        g.arms.curl = 0.6;
        SetColors(g, 0.03, 0.6, 0.6, 0.45, PatternKind::Blotches, 0.7, 0.0, "#7a2a1c");
        g.colors.accent2Color = "#e8b090";
    } else if (n == "Blue-ringed octopus") {
        g.height = 0.65;
        g.arms.length = 1.1;
        g.arms.thickness = 0.045;
        g.arms.spread = 0.7;
        g.arms.curl = 0.55;
        SetColors(g, 0.12, 0.6, 0.82, 0.3, PatternKind::Rings, 0.4, 0.0, "#2e6bff");
        g.colors.accentEdgeColor = "#141010";
    } else if (n == "Dumbo octopus") {
        SetColors(g, 0.97, 0.35, 0.88, 0.4);
    } else if (n == "Purple sea urchin") {
        g.spines.count = 100;
        g.spines.length = 0.38;
        g.spines.sway = 0.1;
        g.spines.taper = 0.8;
        SetColors(g, 0.8, 0.55, 0.38, 0.0, PatternKind::None, 0.5, 0.0, nullptr, "#8a4bb0");
    } else if (n == "Long-spined urchin") {
        g.spines.count = 70;
        g.spines.length = 0.95;
        g.spines.sway = 0.12;
        g.spines.taper = 0.95;
        SetColors(g, 0.7, 0.2, 0.12, 0.0, PatternKind::Spots, 0.35, 0.0, "#3d8be0", "#25222e");
        g.colors.accent2Color = "#e07a2a";
    } else if (n == "Red pencil urchin") {
        g.spines.count = 34;
        g.spines.length = 0.5;
        g.spines.sway = 0.05;
        g.spines.taper = 0.2;
        SetColors(g, 0.01, 0.6, 0.4, 0.0, PatternKind::None, 0.5, 0.0, nullptr, "#d0533a");
    } else if (n == "Green sea urchin") {
        g.spines.count = 130;
        g.spines.length = 0.26;
        g.spines.sway = 0.1;
        g.spines.taper = 0.75;
        SetColors(g, 0.25, 0.5, 0.4, 0.0, PatternKind::None, 0.5, 0.0, nullptr, "#8db04a");
    }
}

// ---------------------------------------------------------------- morphs
struct MorphChoice {
    const char* name;
    double weight;
    int id;
};

struct MorphTable {
    const char* species;
    int count;
    const MorphChoice* choices;
};

const MorphChoice kGoldfishMorphs[] = {{"Orange", 4, 0}, {"Red and white", 2, 1}, {"Calico", 1, 2}, {"Lemon", 1, 3}};
const MorphChoice kClownfishMorphs[] = {{"Ocellaris", 5, 0}, {"Black", 1, 1}};
const MorphChoice kAngelfishMorphs[] = {{"Silver", 4, 0}, {"Marble", 1, 1}, {"Black", 1, 2}};
const MorphChoice kBettaMorphs[] = {{"Red", 3, 0}, {"Royal blue", 3, 1}, {"Black orchid", 1, 2}, {"Koi", 1, 3}, {"Pastel", 1, 4}};
const MorphChoice kKoiMorphs[] = {{"Kohaku", 4, 0}, {"Taisho Sanke", 3, 1}, {"Showa", 2, 2}, {"Ogon", 1, 3}, {"Chagoi", 1, 4}};
const MorphChoice kDiscusMorphs[] = {{"Red turquoise", 3, 0}, {"Blue diamond", 2, 1}, {"Pigeon blood", 1, 2}};
const MorphChoice kMantaMorphs[] = {{"Chevron", 6, 0}, {"Black", 1, 1}};
const MorphChoice kCuttlefishMorphs[] = {{"Rose", 2, 0}, {"Zebra", 1, 1}};

const MorphTable kMorphs[] = {
    {"Goldfish", 4, kGoldfishMorphs},
    {"Clownfish", 2, kClownfishMorphs},
    {"Angelfish", 3, kAngelfishMorphs},
    {"Betta", 5, kBettaMorphs},
    {"Koi", 5, kKoiMorphs},
    {"Discus", 3, kDiscusMorphs},
    {"Manta ray", 2, kMantaMorphs},
    {"Cuttlefish", 2, kCuttlefishMorphs},
};

const MorphTable* FindMorphTable(const std::string& species) {
    for (const MorphTable& t : kMorphs) {
        if (species == t.species) return &t;
    }
    return nullptr;
}

void ApplyMorph(const std::string& species, u64 seed, const char* requested, SeaGenome& g) {
    const MorphTable* table = FindMorphTable(species);
    if (table == nullptr) return;
    double weights[8] = {0};
    for (int i = 0; i < table->count && i < 8; i++) weights[i] = table->choices[i].weight;
    // Python: random.Random(f"morph:{species}:{seed}").choices(names, weights)[0]
    SeaRandom morphRng("morph:" + species + ":" + std::to_string(seed));
    int chosen = morphRng.Choices(weights, table->count);
    if (requested != nullptr) {
        for (int i = 0; i < table->count; i++) {
            if (std::strcmp(requested, table->choices[i].name) == 0) {
                chosen = i;
                break;
            }
        }
    }
    g.morph = table->choices[chosen].name;
    const int id = table->choices[chosen].id;

    if (species == "Goldfish") {
        if (id == 1) {  // Red and white: white body, red patches
            g.colors.hue = 0.1;
            g.colors.saturation = 0.06;
            g.colors.value = 0.95;
            g.colors.pattern = PatternKind::Koi;
            g.colors.patternScale = 0.45;
            g.colors.accentColor = "#e0401c";
        } else if (id == 2) {  // Calico
            g.silver = 0.1;
            g.colors.hue = 0.6;
            g.colors.saturation = 0.15;
            g.colors.value = 0.85;
            g.colors.pattern = PatternKind::Calico;
            g.colors.patternScale = 0.5;
            g.colors.accentColor = "#e8701c";
            g.colors.accent2Color = "#1c1c22";
        } else if (id == 3) {  // Lemon
            g.colors.hue = 0.13;
            g.colors.saturation = 0.75;
            g.colors.value = 0.98;
        }
    } else if (species == "Clownfish") {
        if (id == 1) {
            g.colors.hue = 0.05;
            g.colors.saturation = 0.4;
            g.colors.value = 0.1;
            g.colors.finColor = "#1c1716";
        }
    } else if (species == "Angelfish") {
        if (id == 1) {
            g.colors.pattern = PatternKind::Blotches;
            g.colors.accentColor = "#1c1a20";
            g.colors.accent2Color = "#f0ece0";
            g.colors.patternOnFins = false;
        } else if (id == 2) {
            g.silver = 0.15;
            g.colors.value = 0.12;
            g.colors.saturation = 0.2;
            g.colors.pattern = PatternKind::None;
            g.colors.finColor = "#1e1d24";
        }
    } else if (species == "Betta") {
        if (id == 1) {
            g.colors.hue = 0.62;
            g.colors.saturation = 0.85;
            g.colors.value = 0.8;
            g.colors.finColor = "#2c5ad8";
        } else if (id == 2) {
            g.colors.hue = 0.72;
            g.colors.saturation = 0.5;
            g.colors.value = 0.18;
            g.colors.finColor = "#3a2a58";
            g.colors.finEdgeColor = "#b8c0e8";
        } else if (id == 3) {
            g.colors.hue = 0.1;
            g.colors.saturation = 0.06;
            g.colors.value = 0.92;
            g.colors.pattern = PatternKind::Calico;
            g.colors.accentColor = "#e83a20";
            g.colors.accent2Color = "#1c2a50";
        } else if (id == 4) {
            g.colors.hue = 0.9;
            g.colors.saturation = 0.15;
            g.colors.value = 0.95;
            g.colors.finColor = "#e8d0e0";
        }
    } else if (species == "Koi") {
        if (id == 1) {
            g.colors.accent2Color = "#141418";
        } else if (id == 2) {
            g.colors.hue = 0.6;
            g.colors.saturation = 0.15;
            g.colors.value = 0.12;
            g.colors.accentColor = "#e24a1e";
            g.colors.accent2Color = "#f2f0ea";
            g.colors.patternScale = 0.5;
        } else if (id == 3) {
            g.silver = 0.7;
            g.colors.hue = 0.12;
            g.colors.saturation = 0.65;
            g.colors.value = 0.95;
            g.colors.pattern = PatternKind::None;
            g.colors.finColor = "#f0c858";
        } else if (id == 4) {
            g.silver = 0.2;
            g.colors.hue = 0.08;
            g.colors.saturation = 0.45;
            g.colors.value = 0.5;
            g.colors.pattern = PatternKind::None;
            g.colors.finColor = "#8a6a48";
        }
    } else if (species == "Discus") {
        if (id == 1) {
            g.colors.hue = 0.58;
            g.colors.saturation = 0.65;
            g.colors.value = 0.8;
            g.colors.pattern = PatternKind::None;
        } else if (id == 2) {
            g.colors.hue = 0.08;
            g.colors.saturation = 0.55;
            g.colors.value = 0.95;
            g.colors.pattern = PatternKind::Discus;
            g.colors.accentColor = "#d8402a";
            g.colors.patternScale = 0.3;
        }
    } else if (species == "Manta ray") {
        if (id == 1) {
            g.colors.bellyLightness = 0.0;
            g.colors.pattern = PatternKind::None;
            g.colors.value = 0.07;
        }
    } else if (species == "Cuttlefish") {
        if (id == 1) {
            g.colors.hue = 0.08;
            g.colors.saturation = 0.3;
            g.colors.value = 0.75;
            g.colors.pattern = PatternKind::Tiger;
            g.colors.accentColor = "#2a201c";
        }
    }
}

// Python draws the adult size from the SAME Random instance as the species'
// special genes (see _orca / _sperm_whale), not from a fresh per-size stream.
double DrawSize(const char* name, SeaRandom& r) {
    for (const SizeRow& row : kSizes) {
        if (std::strcmp(name, row.name) == 0) {
            return Triangular(r, row.low, row.high, row.typical);
        }
    }
    return 30.0;
}

void ApplySpecial(const std::string& species, u64 seed, SeaGenome& g) {
    if (species == "Orca") {
        SeaRandom r("special:Orca:" + std::to_string(seed));
        if (r.Chance(0.5)) {  // male: a tall, straight, upright triangle, and a bigger animal
            const double h = r.Uniform(0.30, 0.36);
            const double sweep = r.Uniform(0.0, 0.1);
            g.dorsal = {true, 0.4, 0.5, h, FinShape::Triangle, sweep};
            g.lengthCm = DrawSize("Orca male", r);
        } else {  // female (and young): a shorter sickle curving back
            const double h = r.Uniform(0.14, 0.17);
            const double sweep = r.Uniform(0.75, 0.9);
            g.dorsal = {true, 0.4, 0.52, h, FinShape::Falcate, sweep};
        }
    } else if (species == "Pufferfish") {
        SeaRandom r("special:Pufferfish:" + std::to_string(seed));
        g.puff = r.Uniform(0.78, 0.88);
        g.puffSpines = r.Range(70, 95);
    } else if (species == "Sperm whale") {
        SeaRandom r("special:Sperm whale:" + std::to_string(seed));
        if (r.Chance(0.5)) g.lengthCm = DrawSize("Sperm whale male", r);
    }
}

} // namespace

// ---------------------------------------------------------------- public API
int SpeciesCount() { return kSpeciesCount; }

const SeaSpeciesInfo& SpeciesAt(int index) {
    static const SeaSpeciesInfo kFallback{"Random", SeaKind::Fish, nullptr};
    if (index < 0 || index >= kSpeciesCount) return kFallback;
    const SpeciesRow& row = kSpecies[index];
    static SeaSpeciesInfo info[kSpeciesCount];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < kSpeciesCount; i++) {
            info[i].name = kSpecies[i].name;
            info[i].kind = kSpecies[i].kind;
            info[i].shape = kSpecies[i].shape;
        }
        built = true;
    }
    (void)row;
    return info[index];
}

const SeaSpeciesInfo* FindSpecies(const std::string& name) {
    for (int i = 0; i < kSpeciesCount; i++) {
        if (name == kSpecies[i].name) return &SpeciesAt(i);
    }
    return nullptr;
}

std::vector<std::string> SpeciesForKind(SeaKind kind) {
    std::vector<std::string> out;
    out.push_back(kRandomSpecies);
    for (int i = 1; i < kSpeciesCount; i++) {
        if (kSpecies[i].kind == kind) out.push_back(kSpecies[i].name);
    }
    return out;
}

std::vector<std::string> MorphsOf(const std::string& species) {
    std::vector<std::string> out;
    const MorphTable* table = FindMorphTable(species);
    if (table != nullptr) {
        for (int i = 0; i < table->count; i++) out.push_back(table->choices[i].name);
    }
    return out;
}

bool SizeOf(const std::string& species, SeaSize& out) {
    for (const SizeRow& row : kSizes) {
        if (species == row.name) {
            out.typical = row.typical;
            out.low = row.low;
            out.high = row.high;
            return true;
        }
    }
    return false;
}

SeaGenome MakeGenome(const std::string& species, u64 seed, const char* morph, SeaKind randomKind,
                     const char* randomArchetype) {
    if (species.empty() || species == kRandomSpecies) {
        return RandomGenome(randomKind, seed, randomArchetype);
    }
    const SeaSpeciesInfo* info = FindSpecies(species);
    if (info == nullptr) return RandomGenome(randomKind, seed, randomArchetype);

    SeaGenome g = info->kind == SeaKind::Fish ? RandomFish(seed, info->shape)
                                              : RandomGenome(info->kind, seed, info->shape);
    g.species = species;
    if (info->kind == SeaKind::Fish) g.silver = 0.0;  // matte unless the species is silvery

    if (info->kind == SeaKind::Fish) {
        ApplyFishPreset(species, g);
    } else {
        ApplyOtherPreset(species, g);
    }

    ApplyMorph(species, seed, morph, g);
    g = Individualize(g, species);

    SeaSize size;
    if (SizeOf(species, size)) {
        SeaRandom r("size:" + species + ":" + std::to_string(seed));
        g.lengthCm = Triangular(r, size.low, size.high, size.typical);
    }
    ApplySpecial(species, seed, g);
    return g;
}

} // namespace sea
