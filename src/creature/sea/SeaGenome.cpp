// ================================================================
// SEA CREATURES - genome generation
// See creature/sea/SeaGenome.h for what this is and where it came from.
// Engine-free: no WickedEngine, no <filesystem>, no exceptions.
// ================================================================
#include "creature/sea/SeaGenome.h"

#include "creature/sea/SeaRandom.h"

#include <cstring>

namespace sea {

// ---------------------------------------------------------------- ids
namespace {

const char* kTailIds[] = {"forked", "rounded", "truncate", "lunate", "pointed",
                          "fan", "heterocercal", "flukes", "none"};
const char* kFinIds[] = {"triangle", "rounded", "sail", "long", "falcate", "pennant"};
const char* kPatternIds[] = {
    "none", "stripes", "spots", "lateral", "saddle", "gradient", "bands", "tiger", "shimmer",
    "orca", "rings", "neon", "whale_shark", "mackerel", "koi", "calico", "discus", "chevron",
    "trout", "salmon", "blotches", "tang", "dolphin_cape", "barracuda", "humpback", "red_belly"};
const char* kArchetypeIds[] = {"slender", "torpedo", "classic", "deep", "round"};
const char* kKindIds[] = {"fish", "shark", "whale", "ray", "squid", "octopus", "urchin"};
const char* kKindLabels[] = {"Fish", "Shark", "Whale & dolphin", "Ray",
                             "Squid & cuttlefish", "Octopus", "Sea urchin"};

FinGenes OffFin() {
    FinGenes f;
    f.present = false;
    f.start = 0.5;
    f.end = 0.6;
    f.height = 0.05;
    f.shape = FinShape::Triangle;
    f.sweep = 0.5;
    return f;
}

FinGenes MakeFin(bool present, double start, double end, double height, FinShape shape, double sweep) {
    FinGenes f;
    f.present = present;
    f.start = start;
    f.end = end;
    f.height = height;
    f.shape = shape;
    f.sweep = sweep;
    return f;
}

WingGenes MakeWings(bool present, double start, double end, double span, double sweep, WingShape shape,
                    double flap, double wave, double dihedral = 0.06) {
    WingGenes w;
    w.present = present;
    w.start = start;
    w.end = end;
    w.span = span;
    w.sweep = sweep;
    w.shape = shape;
    w.flap = flap;
    w.wave = wave;
    w.dihedral = dihedral;
    return w;
}

ArmGenes MakeArms(int count, double length, double thickness, int tentacles, double tentacleLength,
                  double spread, double curl) {
    ArmGenes a;
    a.count = count;
    a.length = length;
    a.thickness = thickness;
    a.tentacles = tentacles;
    a.tentacleLength = tentacleLength;
    a.spread = spread;
    a.curl = curl;
    return a;
}

SpineGenes MakeSpines(int count, double length, double sway, double taper) {
    SpineGenes s;
    s.count = count;
    s.length = length;
    s.sway = sway;
    s.taper = taper;
    return s;
}

ColorGenes MakeColors(double hue, double saturation, double value, double belly,
                      PatternKind pattern = PatternKind::None, double scale = 0.5, double offset = 0.0,
                      const std::string& accent = std::string(), const std::string& fin = std::string(),
                      double finTips = 0.0) {
    ColorGenes c;
    c.hue = hue - std::floor(hue);
    c.saturation = Saturate(saturation);
    c.value = Saturate(value);
    c.bellyLightness = Saturate(belly);
    c.pattern = pattern;
    c.patternScale = scale;
    c.patternOffset = offset;
    c.accentColor = accent;
    c.finColor = fin;
    c.finTips = finTips;
    return c;
}

/// A genome with every gene a non-fish kind doesn't set switched off or neutral.
SeaGenome BaseGenome(u64 seed, SeaKind kind) {
    SeaGenome g;
    g.seed = seed;
    g.kind = kind;
    g.height = 0.3;
    g.thickness = 0.8;
    g.peak = 0.4;
    g.nose = 0.6;
    g.taper = 1.0;
    g.peduncle = 0.15;
    g.backArch = 0.5;
    g.eyePos = 0.1;
    g.eyeHeight = 0.3;
    g.eyeSize = 0.2;
    g.tailShape = TailShape::None;
    g.tailLength = 0.2;
    g.tailSpread = 1.0;
    g.dorsal = OffFin();
    g.anal = OffFin();
    g.pelvic = OffFin();
    g.pectoralSize = 0.0;
    g.swimAmplitude = 0.5;
    g.waveCount = 0.8;
    g.gillSlits = 0;
    g.dorsal2 = OffFin();
    g.pectoralFlap = 1.0;
    g.mouth = 0.0;   // FishGenome's default; SeaGenome's own default is the fish value 0.02
    return g;
}

/// Draw a kind variant (always drawn, so a forced variant does not shift the RNG stream).
const char* PickVariant(SeaRandom& r, SeaKind kind, const char* requested, const double* weights, int count) {
    static const char* kShark[] = {"requiem", "hammerhead"};
    static const char* kWhale[] = {"dolphin", "whale"};
    static const char* kRay[] = {"eagle", "stingray"};
    static const char* kSquid[] = {"squid", "cuttlefish"};
    static const char* kOctopus[] = {"octopus", "dumbo"};
    static const char* kUrchin[] = {"short", "long"};
    const char** list = nullptr;
    switch (kind) {
        case SeaKind::Shark: list = kShark; break;
        case SeaKind::Whale: list = kWhale; break;
        case SeaKind::Ray: list = kRay; break;
        case SeaKind::Squid: list = kSquid; break;
        case SeaKind::Octopus: list = kOctopus; break;
        case SeaKind::Urchin: list = kUrchin; break;
        default: return "";
    }
    const int picked = WeightedPick(r, weights, count);
    if (requested == nullptr || requested[0] == '\0') return list[picked];
    for (int i = 0; i < count; i++) {
        if (std::strcmp(requested, list[i]) == 0) return requested;
    }
    return list[picked];
}

// ---------------------------------------------------------------- kind generators
// Every generator below mirrors the draw order of fish-gen's kinds.py: Python
// evaluates every `uniform` argument before the enclosing `choice`/`choices`
// pick, and argument expressions left to right. The draws are hoisted into
// locals so C++'s unspecified argument-evaluation order cannot reorder the
// stream (MSVC evaluates right to left).
SeaGenome GenShark(u64 seed, SeaRandom& r, const char* requested) {
    static const double kVariantWeights[2] = {5, 1};
    const std::string v = PickVariant(r, SeaKind::Shark, requested, kVariantWeights, 2);
    const bool hammer = v == "hammerhead";

    const double hmer = hammer ? r.Uniform(0.12, 0.15) : 0.0;
    const double dStart = r.Uniform(0.33, 0.38);
    const double tall = hammer ? r.Uniform(0.24, 0.3) : r.Uniform(0.15, 0.21);
    const bool blue = r.Chance(0.25);
    double hue;
    if (blue) {
        hue = r.Uniform(0.58, 0.64);
    } else {
        // Python r.choice([U(0.55,0.62), U(0.08,0.13)]) draws BOTH first.
        const double warm = r.Uniform(0.55, 0.62);
        const double cool = r.Uniform(0.08, 0.13);
        hue = r.Choice(2) == 0 ? warm : cool;
    }
    const double sat = blue ? r.Uniform(0.4, 0.65) : r.Uniform(0.08, 0.3);
    const double val = r.Uniform(0.35, 0.62);

    static const double kPatternWeights[3] = {6, 1.5, 1};
    const int patternIdx = WeightedPick(r, kPatternWeights, 3);
    const PatternKind pattern = patternIdx == 0 ? PatternKind::None
                                 : (patternIdx == 1 ? PatternKind::Tiger : PatternKind::Spots);
    std::string accent;
    if (pattern == PatternKind::Tiger) accent = HexFromHsv(hue, sat + 0.1, val * 0.45);
    else if (pattern == PatternKind::Spots) accent = "#e6ebef";
    const bool tips = r.Chance(0.2);
    if (tips && accent.empty()) accent = "#121216";

    const double scale = r.Uniform(0.2, 0.9);
    const double offset = r.Uniform(0.0, 1.0);
    const std::string finColor = HexFromHsv(hue, sat * 1.1, val * 0.88);

    const double height = r.Uniform(0.23, 0.3);
    const double thickness = r.Uniform(0.68, 0.85);
    const double peak = r.Uniform(0.36, 0.42);
    const double nose = hammer ? 1.0 : r.Uniform(0.9, 1.3);
    const double taper = r.Uniform(1.0, 1.25);
    const double peduncle = r.Uniform(0.08, 0.14);
    const double backArch = hammer ? 0.62 : r.Uniform(0.56, 0.62);
    const double flatHead = hammer ? 0.5 : r.Uniform(0.15, 0.3);
    const double eyePos = r.Uniform(0.08, 0.12);
    const double eyeHeight = r.Uniform(0.25, 0.4);
    const double eyeSize = r.Uniform(0.12, 0.2);
    const double mouth = r.Uniform(0.08, 0.1);
    const double mouthHeight = r.Uniform(-0.7, -0.6);
    const bool teeth = r.Chance(0.5);
    const double tailLength = r.Uniform(0.26, 0.36);
    const double tailSpread = r.Uniform(1.4, 1.9);
    const double dorsalEnd = dStart + r.Uniform(0.13, 0.17);
    const double dorsalSweep = r.Uniform(0.5, 0.8);
    const double dorsal2Start = r.Uniform(0.72, 0.76);
    const double dorsal2Height = r.Uniform(0.035, 0.06);
    const double analStart = r.Uniform(0.75, 0.78);
    const double analHeight = r.Uniform(0.03, 0.05);
    const double pelvicStart = r.Uniform(0.56, 0.6);
    const double pelvicHeight = r.Uniform(0.05, 0.07);
    const double pectoralSize = r.Uniform(0.26, 0.33);
    const double pectoralFlap = r.Uniform(0.1, 0.2);
    const double pectoralDroop = r.Uniform(0.7, 0.9);
    const double swimAmplitude = r.Uniform(0.4, 0.5);
    const double waveCount = r.Uniform(0.65, 0.8);
    const double lengthCm = hammer ? r.Uniform(250, 420) : r.Uniform(150, 400);
    const double gloss = r.Uniform(0.25, 0.35);
    const double scars = r.Uniform(0.0, 0.6);

    SeaGenome g = BaseGenome(seed, SeaKind::Shark);
    g.height = height;
    g.thickness = thickness;
    g.peak = peak;
    g.nose = nose;
    g.taper = taper;
    g.peduncle = peduncle;
    g.backArch = backArch;
    g.flatHead = flatHead;
    g.eyePos = eyePos;
    g.eyeHeight = eyeHeight;
    g.eyeSize = eyeSize;
    g.eyeStyle = EyeStyle::Dark;
    g.gillSlits = 5;
    g.hammer = hmer;
    g.mouth = mouth;
    g.mouthHeight = mouthHeight;
    g.teeth = teeth;
    g.tailShape = TailShape::Heterocercal;
    g.tailLength = tailLength;
    g.tailSpread = tailSpread;
    g.dorsal = MakeFin(true, dStart, dorsalEnd, tall,
                       hammer ? FinShape::Falcate : FinShape::Triangle, dorsalSweep);
    g.dorsal2 = MakeFin(true, dorsal2Start, 0.81, dorsal2Height, FinShape::Triangle, 0.5);
    g.anal = MakeFin(true, analStart, 0.84, analHeight, FinShape::Triangle, 0.5);
    g.pelvic = MakeFin(true, pelvicStart, 0.65, pelvicHeight, FinShape::Triangle, 0.5);
    g.pectoralSize = pectoralSize;
    g.pectoralFlap = pectoralFlap;
    g.pectoralShape = PectoralShape::Blade;
    g.pectoralDroop = pectoralDroop;
    g.swimAmplitude = swimAmplitude;
    g.waveCount = waveCount;
    g.lengthCm = lengthCm;
    g.cameraTilt = hammer ? 0.3 : 0.0;
    g.gloss = gloss;
    g.finAlpha = 1.0;
    g.scars = scars;
    g.colors = MakeColors(hue, sat, val, 1.0, pattern, scale, offset, accent, finColor, tips ? 0.4 : 0.0);
    g.colors.tailBodyColor = true;
    if (g.teeth) g.scariness = SeaRandom("scary:" + std::to_string(seed)).Uniform(0.3, 0.7);
    return g;
}

SeaGenome GenWhale(u64 seed, SeaRandom& r, const char* requested) {
    static const double kVariantWeights[2] = {1, 1};
    const std::string v = PickVariant(r, SeaKind::Whale, requested, kVariantWeights, 2);
    const bool dolphin = v == "dolphin";

    FinGenes dorsal = OffFin();
    if (r.Chance(0.9)) {
        const double dStart = dolphin ? r.Uniform(0.4, 0.5) : r.Uniform(0.38, 0.6);
        // Python r.choice(["falcate","falcate","triangle"]) or the four-way list.
        static const char* kDolphin[3] = {"falcate", "falcate", "triangle"};
        static const char* kBig[4] = {"falcate", "triangle", "rounded", "sail"};
        const int shapeIdx = dolphin ? r.Choice(3) : r.Choice(4);
        const char* shapeName = dolphin ? kDolphin[shapeIdx] : kBig[shapeIdx];
        const FinShape shape = std::strcmp(shapeName, "falcate") == 0 ? FinShape::Falcate
                              : std::strcmp(shapeName, "triangle") == 0 ? FinShape::Triangle
                              : std::strcmp(shapeName, "rounded") == 0 ? FinShape::Rounded : FinShape::Sail;
        const double dEnd = dStart + r.Uniform(0.06, 0.14);
        const double dHeight = dolphin ? r.Uniform(0.08, 0.16) : r.Uniform(0.04, 0.3);
        const double dSweep = shape == FinShape::Falcate ? r.Uniform(0.5, 0.9) : r.Uniform(0.15, 0.9);
        dorsal = MakeFin(true, dStart, dEnd, dHeight, shape, dSweep);
    }
    const bool longFlippers = !dolphin && r.Chance(0.25);
    const double beak = dolphin ? r.Uniform(0.07, 0.11) : 0.0;
    const bool white = r.Chance(0.12);
    const double hue = r.Uniform(0.55, 0.65);
    const double val = white ? r.Uniform(0.9, 0.95) : r.Uniform(0.12, 0.65);

    PatternKind pattern = PatternKind::None;
    if (!white) {
        static const double kWeights[6] = {3, 0, 1.5, 1.5, 1, 1};
        double weights[6];
        for (int i = 0; i < 6; i++) weights[i] = kWeights[i];
        weights[1] = val < 0.3 ? 1 : 0;
        static const PatternKind kChoices[6] = {PatternKind::None, PatternKind::Orca, PatternKind::Lateral,
                                                PatternKind::Saddle, PatternKind::Spots, PatternKind::Gradient};
        pattern = kChoices[WeightedPick(r, weights, 6)];
    }
    // Python builds the whole accent dict, so the lateral hue is drawn even
    // when the chosen pattern is not "lateral".
    const double lateralHue = r.Uniform(0.1, 0.15);
    std::string accent;
    switch (pattern) {
        case PatternKind::Orca: accent = "#f2f2ee"; break;
        case PatternKind::Lateral: accent = HexFromHsv(lateralHue, 0.35, 0.85); break;
        case PatternKind::Saddle: accent = HexFromHsv(hue, 0.2, val * 0.5); break;
        case PatternKind::Spots: accent = HexFromHsv(hue, 0.1, Min(1.0, val + 0.35)); break;
        default: break;
    }
    std::string fin;
    if (val < 0.3) fin = r.Chance(0.25) ? HexFromHsv(hue, 0.05, 0.85) : HexFromHsv(hue, 0.1, val * 0.85);

    const double height = dolphin ? r.Uniform(0.2, 0.26) : r.Uniform(0.26, 0.32);
    const double thickness = r.Uniform(0.75, 0.85);
    const double peak = r.Uniform(0.36, 0.42);
    const double nose = dolphin ? r.Uniform(0.5, 0.65) : r.Uniform(0.7, 1.0);
    const double melon = dolphin ? r.Uniform(0.0, 0.015) : 0.0;
    const double flatHead = dolphin ? 0.0 : r.Uniform(0.0, 0.4);
    const double pectoralDroop = r.Uniform(0.6, 0.8);
    const double lengthCm = dolphin ? r.Uniform(180, 350) : r.Uniform(450, 1200);
    const double gloss = r.Uniform(0.5, 0.7);
    const double scars = dolphin ? r.Uniform(0.0, 0.3) : r.Uniform(0.1, 0.5);
    const double taper = r.Uniform(1.2, 1.4);
    const double peduncle = r.Uniform(0.1, 0.14);
    const double eyePos = dolphin ? beak + r.Uniform(0.06, 0.08) : r.Uniform(0.1, 0.14);
    const double eyeHeight = r.Uniform(-0.1, 0.2);
    const double eyeSize = r.Uniform(0.12, 0.16);
    const double tailLength = r.Uniform(0.18, 0.24);
    const double tailSpread = r.Uniform(1.1, 1.6);
    const double pectoralSize = longFlippers ? r.Uniform(0.3, 0.38) : r.Uniform(0.12, 0.2);
    const double swimAmplitude = r.Uniform(0.4, 0.5);
    const double waveCount = r.Uniform(0.5, 0.6);
    const double sat = r.Uniform(0.05, 0.3);
    const double belly = white ? r.Uniform(0.4, 0.6) : r.Uniform(0.6, 1.0);
    const double patternScale = r.Uniform(0.4, 0.8);

    SeaGenome g = BaseGenome(seed, SeaKind::Whale);
    g.height = height;
    g.thickness = thickness;
    g.peak = peak;
    g.nose = nose;
    g.beak = beak;
    g.melon = melon;
    g.flatHead = flatHead;
    g.pectoralShape = PectoralShape::Blade;
    g.pectoralDroop = pectoralDroop;
    g.lengthCm = lengthCm;
    g.gloss = gloss;
    g.finAlpha = 1.0;
    g.scars = scars;
    g.taper = taper;
    g.peduncle = peduncle;
    g.backArch = 0.5;
    g.eyePos = eyePos;
    g.eyeHeight = eyeHeight;
    g.eyeSize = eyeSize;
    g.eyeStyle = EyeStyle::Dark;
    g.swimVertical = true;
    g.tailShape = TailShape::Flukes;
    g.tailLength = tailLength;
    g.tailSpread = tailSpread;
    g.dorsal = dorsal;
    g.pectoralSize = pectoralSize;
    g.pectoralFlap = 0.15;
    g.swimAmplitude = swimAmplitude;
    g.waveCount = waveCount;
    g.colors = MakeColors(hue, sat, val, belly, pattern, patternScale, 0.0, accent, fin);
    return g;
}

SeaGenome GenRay(u64 seed, SeaRandom& r, const char* requested) {
    static const double kVariantWeights[2] = {3, 2};
    const std::string v = PickVariant(r, SeaKind::Ray, requested, kVariantWeights, 2);
    const bool eagle = v == "eagle";

    WingGenes wings;
    double nose = 0.6, thickness = 2.2, height = 0.1, peak = 0.25, backArch = 0.7;
    double eyePos = 0.1, eyeHeight = 0.8, eyeSize = 0.3;
    double length = 90.0;
    bool horns = false;
    double lobeLength = 0.0;
    FinGenes dorsal = OffFin();

    if (eagle) {
        const double wStart = r.Uniform(0.08, 0.12);
        const double wEnd = r.Uniform(0.38, 0.44);
        const double wSpan = r.Uniform(0.4, 0.6);
        const double wSweep = r.Uniform(1.2, 1.35);
        const double wFlap = r.Uniform(0.75, 0.9);
        const double wWave = r.Uniform(0.1, 0.25);
        const double wDihedral = r.Uniform(0.0, 0.08);
        wings = MakeWings(true, wStart, wEnd, wSpan, wSweep, WingShape::Pointed, wFlap, wWave, wDihedral);
        if (r.Chance(0.5)) dorsal = MakeFin(true, 0.45, 0.5, 0.04, FinShape::Triangle, 0.6);
        nose = r.Uniform(0.45, 0.7);
        thickness = r.Uniform(1.5, 1.9);
        height = r.Uniform(0.14, 0.18);
        peak = r.Uniform(0.2, 0.25);
        backArch = r.Uniform(0.62, 0.7);
        eyePos = r.Uniform(0.06, 0.09);
        eyeHeight = r.Uniform(0.15, 0.35);
        eyeSize = 0.22;
        // Python r.choice([("none",0.0), ("round", U(0.03,0.05))]).
        const double lobe = r.Uniform(0.03, 0.05);
        horns = r.Choice(2) == 1;
        lobeLength = horns ? lobe : 0.0;
        length = r.Uniform(80, 250);
    } else {
        // Python r.choice(["round", "diamond"]).
        const WingShape shape = r.Choice(2) == 0 ? WingShape::Round : WingShape::Diamond;
        const double wStart = r.Uniform(0.0, 0.03);
        const double wEnd = r.Uniform(0.46, 0.54);
        const double wSpan = r.Uniform(0.22, 0.3);
        const double wSweep = r.Uniform(0.42, 0.5);
        const double wFlap = r.Uniform(0.13, 0.2);
        const double wWave = r.Uniform(1.1, 1.5);
        const double wDihedral = r.Uniform(0.0, 0.04);
        wings = MakeWings(true, wStart, wEnd, wSpan, wSweep, shape, wFlap, wWave, wDihedral);
        dorsal = OffFin();
        nose = r.Uniform(0.6, 0.9);
        thickness = r.Uniform(2.2, 2.6);
        height = r.Uniform(0.09, 0.12);
        peak = r.Uniform(0.22, 0.28);
        backArch = r.Uniform(0.66, 0.74);
        eyePos = r.Uniform(0.1, 0.14);
        eyeHeight = 0.8;
        eyeSize = 0.3;
        horns = false;
        length = r.Uniform(40, 150);
    }
    // Python r.choice([U(0.06,0.12), U(0.58,0.64)]).
    const double hueWarm = r.Uniform(0.06, 0.12);
    const double hueCool = r.Uniform(0.58, 0.64);
    const double hue = r.Choice(2) == 0 ? hueWarm : hueCool;
    const double sat = r.Uniform(0.15, 0.55);
    const double val = r.Uniform(0.2, 0.68);
    static const double kPatternWeights[2] = {3, 1.5};
    const bool spots = WeightedPick(r, kPatternWeights, 2) == 1;
    std::string accent;
    if (spots) {
        if (val < 0.4) {
            accent = "#eef0f0";
        } else {
            // Python r.choice(["#2f8ae0", _hex(...)]).
            const int pick = r.Choice(2);
            accent = pick == 0 ? "#2f8ae0" : HexFromHsv(hue, sat + 0.2, val * 0.4);
        }
    }
    const double taper = r.Uniform(6.0, 8.0);
    const double peduncle = r.Uniform(0.12, 0.16);
    const double gloss = r.Uniform(0.3, 0.45);
    const double swimAmplitude = r.Uniform(0.06, 0.1);
    const double waveCount = r.Uniform(1.2, 1.5);
    const double cameraTilt = r.Uniform(0.1, 0.16);
    const double scale = r.Uniform(0.3, 0.7);
    const double offset = r.Uniform(0.0, 1.0);

    SeaGenome g = BaseGenome(seed, SeaKind::Ray);
    g.wings = wings;
    g.nose = nose;
    g.thickness = thickness;
    g.height = height;
    g.peak = peak;
    g.backArch = backArch;
    g.eyePos = eyePos;
    g.eyeHeight = eyeHeight;
    g.eyeSize = eyeSize;
    g.lobes = horns ? LobeShape::Round : LobeShape::None;
    g.lobeLength = lobeLength;
    g.taper = taper;
    g.peduncle = peduncle;
    g.lengthCm = length;
    g.eyeStyle = EyeStyle::Dark;
    g.gloss = gloss;
    g.finAlpha = 1.0;
    g.dorsal = dorsal;
    g.swimAmplitude = swimAmplitude;
    g.waveCount = waveCount;
    g.cameraTilt = cameraTilt;
    g.turnLag = 0.5;
    g.colors = MakeColors(hue, sat, val, 1.0, spots ? PatternKind::Spots : PatternKind::None,
                          scale, offset, accent);
    return g;
}

SeaGenome GenSquid(u64 seed, SeaRandom& r, const char* requested) {
    static const double kVariantWeights[2] = {7, 3};
    const std::string v = PickVariant(r, SeaKind::Squid, requested, kVariantWeights, 2);
    const bool squid = v == "squid";

    // Python r.choice([U(-0.05,0.02), U(0.05,0.1)]).
    const double hueA = r.Uniform(-0.05, 0.02);
    const double hueB = r.Uniform(0.05, 0.1);
    double hue = r.Choice(2) == 0 ? hueA : hueB;
    double sat = r.Uniform(0.3, 0.7), val = r.Uniform(0.55, 0.88);
    WingGenes wings;
    ArmGenes arms;
    double height = 0.22, thickness = 0.9, nose = 0.4, peak = 0.4, eyePos = 0.76, lengthCm = 45;
    PatternKind pattern = PatternKind::None;
    std::string accent, accent2;
    if (squid) {
        height = r.Uniform(0.18, 0.24);
        nose = r.Uniform(0.35, 0.45);
        lengthCm = r.Uniform(30, 60);
        const double wingEnd = r.Uniform(0.28, 0.36);
        const double wingSpan = r.Uniform(0.15, 0.19);
        wings = MakeWings(true, 0.0, wingEnd, wingSpan, 0.55, WingShape::Pointed, 0.4, 0.6, 0.45);
        const double armLen = r.Uniform(0.4, 0.6);
        const double armThick = r.Uniform(0.028, 0.034);
        const double armTent = r.Uniform(0.7, 1.1);
        const double armSpread = r.Uniform(0.4, 0.55);
        const double armCurl = r.Uniform(0.25, 0.35);
        arms = MakeArms(8, armLen, armThick, 2, armTent, armSpread, armCurl);
        static const double kWeights[3] = {1, 2, 0.7};
        static const PatternKind kChoices[3] = {PatternKind::None, PatternKind::Spots, PatternKind::Shimmer};
        pattern = kChoices[WeightedPick(r, kWeights, 3)];
        if (pattern != PatternKind::None) accent = HexFromHsv(hue, sat + 0.2, val * 0.55);
    } else {
        height = r.Uniform(0.36, 0.4);
        thickness = r.Uniform(1.15, 1.25);
        nose = r.Uniform(0.8, 0.9);
        peak = 0.42;
        eyePos = 0.72;
        lengthCm = r.Uniform(25, 45);
        const double wingEnd = r.Uniform(0.65, 0.72);
        const double wingSpan = r.Uniform(0.06, 0.07);
        wings = MakeWings(true, 0.03, wingEnd, wingSpan, 0.5, WingShape::Round, 0.7, 2.0);
        const double armLen = r.Uniform(0.45, 0.55);
        const double armThick = r.Uniform(0.05, 0.058);
        const double armTent = r.Uniform(0.6, 0.75);
        const double armSpread = r.Uniform(0.32, 0.4);
        const double armCurl = r.Uniform(0.35, 0.45);
        arms = MakeArms(8, armLen, armThick, 2, armTent, armSpread, armCurl);
        // Python r.choice([U(0.75,0.85), U(0.88,0.96), U(0.45,0.52), U(0.05,0.1)]).
        const double h0 = r.Uniform(0.75, 0.85);
        const double h1 = r.Uniform(0.88, 0.96);
        const double h2 = r.Uniform(0.45, 0.52);
        const double h3 = r.Uniform(0.05, 0.1);
        const int pick = r.Choice(4);
        hue = pick == 0 ? h0 : pick == 1 ? h1 : pick == 2 ? h2 : h3;
        sat = r.Uniform(0.4, 0.6);
        val = r.Uniform(0.7, 0.9);
        static const double kWeights[3] = {4, 1, 1};
        static const PatternKind kChoices[3] = {PatternKind::Shimmer, PatternKind::Tiger, PatternKind::Spots};
        pattern = kChoices[WeightedPick(r, kWeights, 3)];
        const int a1 = r.Choice(3);
        const double a1Hue = hue + (a1 == 0 ? 0.25 : a1 == 1 ? 0.35 : -0.3);
        const double a1Sat = r.Uniform(0.5, 0.8);
        const double a1Val = r.Uniform(0.85, 1.0);
        accent = HexFromHsv(a1Hue, a1Sat, a1Val);
        const int a2 = r.Choice(3);
        const double a2Hue = hue + (a2 == 0 ? -0.15 : a2 == 1 ? 0.5 : 0.6);
        const double a2Sat = r.Uniform(0.45, 0.75);
        const double a2Val = r.Uniform(0.85, 1.0);
        accent2 = HexFromHsv(a2Hue, a2Sat, a2Val);
    }
    const double belly = r.Uniform(0.3, 0.5);
    const double scale = r.Uniform(0.2, 1.0);
    const double offset = r.Uniform(0.0, 1.0);
    ColorGenes colors = MakeColors(hue, sat, val, belly, pattern, scale, offset, accent);
    colors.accent2Color = accent2;
    const double pulse = r.Uniform(0.1, 0.14);
    const double gloss = r.Uniform(0.5, 0.7);
    const double silver = r.Uniform(0.05, 0.15);

    SeaGenome g = BaseGenome(seed, SeaKind::Squid);
    g.height = height;
    g.thickness = thickness;
    g.nose = nose;
    g.peak = peak;
    g.eyePos = eyePos;
    g.taper = 1.0;
    g.peduncle = 0.55;
    g.backArch = 0.5;
    g.eyeHeight = 0.25;
    g.eyeSize = 0.3;
    g.swimAmplitude = 0.2;
    g.waveCount = 0.8;
    g.pulse = pulse;
    g.wings = wings;
    g.arms = arms;
    g.lengthCm = lengthCm;
    g.gloss = gloss;
    g.silver = silver;
    g.colors = colors;
    return g;
}

SeaGenome GenOctopus(u64 seed, SeaRandom& r, const char* requested) {
    static const double kVariantWeights[2] = {5, 1};
    const std::string v = PickVariant(r, SeaKind::Octopus, requested, kVariantWeights, 2);
    const bool octopus = v == "octopus";

    // Python r.choice([U(0.0,0.1), U(0.94,0.99), U(0.1,0.14), U(0.8,0.9)]).
    const double h0 = r.Uniform(0.0, 0.1);
    const double h1 = r.Uniform(0.94, 0.99);
    const double h2 = r.Uniform(0.1, 0.14);
    const double h3 = r.Uniform(0.8, 0.9);
    const int huePick = r.Choice(4);
    const double hue = huePick == 0 ? h0 : huePick == 1 ? h1 : huePick == 2 ? h2 : h3;
    const double sat = r.Uniform(0.35, 0.65);
    const double val = r.Uniform(0.55, 0.88);
    double height = 0.7, taper = 1.6, peduncle = 0.32, eyePos = 0.68;
    ArmGenes arms;
    WingGenes wings;
    double pulse = 0.08;
    PatternKind pattern = PatternKind::None;
    std::string accent;
    if (octopus) {
        height = r.Uniform(0.6, 0.75);
        taper = 1.6;
        peduncle = 0.32;
        eyePos = 0.68;
        const double armLen = r.Uniform(0.9, 1.4);
        const double armThick = r.Uniform(0.04, 0.055);
        const double armSpread = r.Uniform(0.6, 0.8);
        const double armCurl = r.Uniform(0.45, 0.65);
        arms = MakeArms(8, armLen, armThick, 0, 1.0, armSpread, armCurl);
        wings = WingGenes();
        pulse = 0.08;
        static const double kWeights[3] = {2, 2, 1};
        static const PatternKind kChoices[3] = {PatternKind::None, PatternKind::Spots, PatternKind::Rings};
        pattern = kChoices[WeightedPick(r, kWeights, 3)];
    } else {
        height = r.Uniform(0.75, 0.85);
        taper = 1.4;
        peduncle = 0.4;
        eyePos = 0.62;
        const double armLen = r.Uniform(0.5, 0.7);
        const double armSpread = r.Uniform(0.9, 1.1);
        arms = MakeArms(8, armLen, 0.06, 0, 1.0, armSpread, 0.3);
        const double wingSpan = r.Uniform(0.17, 0.23);
        wings = MakeWings(true, 0.2, 0.36, wingSpan, 0.5, WingShape::Round, 0.7, 0.0);
        pulse = 0.04;
        pattern = PatternKind::None;
    }
    if (pattern == PatternKind::Rings) accent = "#2e6bff";
    else if (pattern == PatternKind::Spots) accent = HexFromHsv(hue, sat + 0.2, val * 0.55);
    const double lengthCm = octopus ? r.Uniform(40, 120) : r.Uniform(18, 30);
    const double gloss = r.Uniform(0.35, 0.5);
    const double belly = r.Uniform(0.3, 0.45);
    const double scale = r.Uniform(0.4, 0.8);
    const double offset = r.Uniform(0.0, 1.0);

    SeaGenome g = BaseGenome(seed, SeaKind::Octopus);
    g.height = height;
    g.thickness = octopus ? 0.85 : 0.95;
    g.peak = octopus ? 0.32 : 0.38;
    g.nose = 1.0;
    g.taper = taper;
    g.peduncle = peduncle;
    g.eyePos = eyePos;
    g.backArch = 0.5;
    g.eyeHeight = 0.25;
    g.eyeSize = 0.22;
    g.lengthCm = lengthCm;
    g.gloss = gloss;
    g.swimAmplitude = 0.2;
    g.waveCount = 0.8;
    g.pulse = pulse;
    g.wings = wings;
    g.arms = arms;
    g.colors = MakeColors(hue, sat, val, belly, pattern, scale, offset, accent);
    return g;
}

SeaGenome GenUrchin(u64 seed, SeaRandom& r, const char* requested) {
    static const double kVariantWeights[2] = {4, 1};
    const std::string v = PickVariant(r, SeaKind::Urchin, requested, kVariantWeights, 2);
    const bool longSpines = v == "long";

    // Python evaluates all six (hue, sat, val) tuples before choice() picks one.
    static const double kHue[6][2] = {{0.77, 0.85}, {0.0, 0.04}, {0.22, 0.3},
                                      {0.65, 0.75}, {0.05, 0.09}, {0.9, 0.96}};
    static const double kSat[6][2] = {{0.4, 0.6}, {0.5, 0.65}, {0.4, 0.55},
                                      {0.1, 0.25}, {0.65, 0.8}, {0.4, 0.55}};
    static const double kVal[6][2] = {{0.35, 0.5}, {0.4, 0.55}, {0.4, 0.5},
                                      {0.12, 0.18}, {0.55, 0.7}, {0.5, 0.65}};
    double hues[6], sats[6], vals[6];
    for (int i = 0; i < 6; i++) {
        hues[i] = r.Uniform(kHue[i][0], kHue[i][1]);
        sats[i] = r.Uniform(kSat[i][0], kSat[i][1]);
        vals[i] = r.Uniform(kVal[i][0], kVal[i][1]);
    }
    const int pick = r.Choice(6);
    const double hue = hues[pick];
    const double sat = sats[pick];
    const double val = vals[pick];

    SpineGenes spines;
    if (longSpines) {
        const int count = r.Range(50, 80);
        const double len = r.Uniform(0.75, 1.0);
        const double sway = r.Uniform(0.08, 0.14);
        const double taper = r.Uniform(0.85, 0.95);
        spines = MakeSpines(count, len, sway, taper);
    } else {
        const int count = r.Range(60, 150);
        const double sway = r.Uniform(0.06, 0.15);
        const double taper = r.Uniform(0.6, 0.85);
        spines = MakeSpines(count, 0.0, sway, taper);
        spines.length = 0.2 + 0.25 * (1.0 - (count - 60) / 90.0) + r.Uniform(-0.03, 0.03);
    }
    const std::string spineHex = HexFromHsv(hue, sat + 0.05, Min(1.0, val * 1.6 + 0.05));
    const double height = r.Uniform(0.66, 0.74);
    const double lengthCm = longSpines ? r.Uniform(8, 11) : r.Uniform(5, 9);

    SeaGenome g = BaseGenome(seed, SeaKind::Urchin);
    g.profile = "sphere";
    g.height = height;
    g.thickness = 1.0 / height;
    g.backArch = 0.6;
    g.eyeStyle = EyeStyle::None;
    g.swimAmplitude = 0.0;
    g.waveCount = 0.0;
    g.turnLag = 0.0;
    g.cameraTilt = 0.25;
    g.spines = spines;
    g.lengthCm = lengthCm;
    g.gloss = 0.08;
    g.colors = MakeColors(hue, sat, val, 0.0, PatternKind::None, 0.5, 0.0, std::string(), spineHex);
    return g;
}

} // namespace

// ---------------------------------------------------------------- ids
const char* KindId(SeaKind kind) {
    return kKindIds[ClampI((int)kind, 0, 6)];
}
const char* KindLabel(SeaKind kind) {
    return kKindLabels[ClampI((int)kind, 0, 6)];
}
bool ParseKind(const std::string& id, SeaKind& out) {
    for (int i = 0; i < 7; i++) {
        if (id == kKindIds[i]) {
            out = (SeaKind)i;
            return true;
        }
    }
    return false;
}
const char* TailId(TailShape shape) {
    return kTailIds[ClampI((int)shape, 0, 8)];
}
const char* FinShapeId(FinShape shape) {
    return kFinIds[ClampI((int)shape, 0, 5)];
}
const char* PatternId(PatternKind pattern) {
    return kPatternIds[ClampI((int)pattern, 0, 25)];
}
PatternKind PatternFromId(const std::string& id) {
    for (int i = 0; i < 26; i++) {
        if (id == kPatternIds[i]) return (PatternKind)i;
    }
    return PatternKind::None;
}
const char* ArchetypeId(BodyArchetype archetype) {
    return kArchetypeIds[ClampI((int)archetype, 0, 4)];
}
bool ParseArchetype(const std::string& id, BodyArchetype& out) {
    for (int i = 0; i < 5; i++) {
        if (id == kArchetypeIds[i]) {
            out = (BodyArchetype)i;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- random fish
SeaGenome RandomFish(u64 seed, const char* archetype) {
    // The draw order here is Python's, statement for statement. fish-gen builds
    // FishGenome.random() out of dict and list literals, which evaluate *every*
    // branch before indexing — `height` is five uniforms, not one — and the
    // constructor's keyword arguments consume the stream left to right. Reorder
    // any of it and the same seed silently becomes a different fish.
    SeaRandom r(seed);

    static const double kArchWeights[5] = {1, 3, 4, 2, 1.5};
    static const char* kArchNames[5] = {"slender", "torpedo", "classic", "deep", "round"};
    int archIdx = r.Choices(kArchWeights, 5);
    if (archetype != nullptr) {
        for (int i = 0; i < 5; i++) {
            if (std::strcmp(archetype, kArchNames[i]) == 0) {
                archIdx = i;
                break;
            }
        }
    }
    const bool round = archIdx == 4;
    const bool slender = archIdx == 0;
    const bool deep = archIdx == 3;

    const double heights[5] = {r.Uniform(0.2, 0.26), r.Uniform(0.24, 0.34), r.Uniform(0.32, 0.45),
                               r.Uniform(0.5, 0.72), r.Uniform(0.55, 0.75)};
    const double height = heights[archIdx];
    const double peak = round ? r.Uniform(0.42, 0.52) : r.Uniform(0.3, 0.48);
    const double nose = round ? r.Uniform(0.6, 1.0) : r.Uniform(0.35, 0.8);
    const double taper = round ? r.Uniform(0.5, 0.9) : r.Uniform(0.8, 1.6);

    static const TailShape kTails[6] = {TailShape::Forked, TailShape::Rounded, TailShape::Truncate,
                                        TailShape::Lunate, TailShape::Pointed, TailShape::Fan};
    TailShape tail = kTails[r.Choice(6)];
    if (slender && r.Chance(0.5)) tail = TailShape::Pointed;

    auto makeFin = [&](double presentP, double s0, double s1, double e0, double e1, double h0, double h1) {
        static const FinShape kShapes[4] = {FinShape::Triangle, FinShape::Rounded, FinShape::Sail,
                                            FinShape::Long};
        const double s = r.Uniform(s0, s1);
        FinGenes f;
        f.present = r.Chance(presentP);
        f.start = s;
        f.end = Max(s + 0.08, r.Uniform(e0, e1));
        f.height = r.Uniform(h0, h1);
        f.shape = kShapes[r.Choice(4)];
        f.sweep = r.Uniform(0.2, 0.9);
        return f;
    };

    const FinGenes dorsal = makeFin(0.95, 0.2, 0.45, 0.5, 0.8, 0.07, 0.2);
    const FinGenes anal = makeFin(0.8, 0.55, 0.7, 0.72, 0.9, 0.06, 0.18);
    const FinGenes pelvic = makeFin(0.6, 0.32, 0.45, 0.42, 0.55, 0.06, 0.13);

    SeaGenome g;
    g.seed = seed;
    g.kind = SeaKind::Fish;

    // ColorGenes(...): hue is already drawn; the rest consume the stream in order.
    const double hue = r.Random();
    ColorGenes& c = g.colors;
    c.hue = hue;
    c.saturation = r.Uniform(0.55, 0.95);
    c.value = r.Uniform(0.62, 0.95);
    // r.choice([0.0, r.uniform(-0.1, 0.1), ...]) builds the whole list first, so
    // all three uniforms are drawn whichever one is picked.
    const double finShift0 = r.Uniform(-0.1, 0.1);
    const double finShift1 = r.Uniform(0.15, 0.3);
    const double finShift2 = r.Uniform(0.35, 0.65);
    switch (r.Choice(4)) {
        case 0: c.finHueShift = 0.0; break;
        case 1: c.finHueShift = finShift0; break;
        case 2: c.finHueShift = finShift1; break;
        default: c.finHueShift = finShift2; break;
    }
    const double accShift0 = r.Uniform(-0.15, 0.15);
    const double accShift1 = r.Uniform(0.25, 0.4);
    const double accShift2 = r.Uniform(0.4, 0.6);
    switch (r.Choice(4)) {
        case 0: c.accentHueShift = 0.0; break;
        case 1: c.accentHueShift = accShift0; break;
        case 2: c.accentHueShift = accShift1; break;
        default: c.accentHueShift = accShift2; break;
    }
    c.bellyLightness = r.Uniform(0.0, 1.0);
    static const PatternKind kPatterns[9] = {
        PatternKind::None, PatternKind::Stripes, PatternKind::Spots, PatternKind::Lateral,
        PatternKind::Saddle, PatternKind::Gradient, PatternKind::Bands, PatternKind::Tiger, PatternKind::Shimmer};
    static const double kPatternWeights[9] = {2, 3, 2.5, 2, 1.5, 1.5, 2, 1.5, 1.2};
    c.pattern = kPatterns[r.Choices(kPatternWeights, 9)];
    c.patternScale = r.Uniform(0.0, 1.0);
    c.patternOffset = r.Random();

    // The second accent picks from a two-element list of uniforms, so both are drawn.
    const double a2Low = r.Uniform(0.25, 0.4);
    const double a2High = r.Uniform(0.6, 0.75);
    const double a2Pick = (double)r.Choice(3);
    const double a2Base = a2Pick == 0.0 ? a2Low : (a2Pick == 1.0 ? a2High : 0.5);
    double a2 = c.hue + a2Base;
    a2 -= std::floor(a2);
    // Hoisted for the same reason as the jitter below: two draws in one call
    // are ordered by the compiler.
    const double a2Sat = r.Uniform(0.6, 0.95);
    const double a2Val = r.Uniform(0.75, 1.0);
    c.accent2Color = HexFromHsv(a2, a2Sat, a2Val);
    c.patternOnFins = c.pattern == PatternKind::Stripes && r.Chance(0.3);

    // length = {"slender": r.uniform(30, 90), ...}[archetype] — five draws again.
    const double lengths[5] = {r.Uniform(30, 90), r.Uniform(20, 60), r.Uniform(8, 30), r.Uniform(10, 30),
                               r.Uniform(8, 20)};
    g.lengthCm = lengths[archIdx];

    // cls(...): keyword arguments, evaluated left to right.
    g.height = height;
    g.thickness = (deep || round) ? r.Uniform(0.35, 0.6) : r.Uniform(0.5, 0.8);
    g.peak = peak;
    g.nose = nose;
    g.taper = taper;
    g.peduncle = r.Uniform(0.12, 0.3);
    g.backArch = r.Uniform(0.45, 0.65);
    g.eyePos = r.Uniform(0.08, 0.16);
    g.eyeHeight = r.Uniform(0.15, 0.45);
    g.eyeSize = r.Uniform(0.18, 0.32);
    g.tailShape = tail;
    g.tailLength = r.Uniform(0.18, 0.32);
    g.tailSpread = (slender || archIdx == 1) ? r.Uniform(1.0, 1.8) : r.Uniform(0.7, 1.2);
    g.dorsal = dorsal;
    g.anal = anal;
    g.pelvic = pelvic;
    g.pectoralSize = r.Uniform(0.1, 0.2);
    g.swimAmplitude = r.Uniform(0.55, 0.75);
    g.waveCount = r.Uniform(0.6, 1.0);
    // silver = r.choice([0.0, 0.0, r.uniform(0.2, 0.6)]): the uniform is drawn first.
    const double silverValue = r.Uniform(0.2, 0.6);
    g.silver = r.Choice(3) == 2 ? silverValue : 0.0;
    g.scales = r.Uniform(0.018, 0.03);
    g.mouth = r.Uniform(0.025, 0.045);
    g.mouthHeight = r.Uniform(-0.15, 0.1);
    static const char* kIris[5] = {"#d8b45a", "#c8ccd0", "#d8a040", "#b8382c", "#e0d8b0"};
    g.irisColor = kIris[r.Choice(5)];
    g.finAlpha = r.Uniform(0.55, 0.85);
    return g;
}

// ---------------------------------------------------------------- random by kind
SeaGenome RandomGenome(SeaKind kind, u64 seed, const char* variant) {
    if (kind == SeaKind::Fish) return RandomFish(seed, variant);
    // Python: random.Random(f"{kind}:{seed}") — the string seed goes through
    // SHA-512 + init_by_array, not splitmix64.
    SeaRandom r(std::string(KindId(kind)) + ":" + std::to_string(seed));
    switch (kind) {
        case SeaKind::Shark: return GenShark(seed, r, variant);
        case SeaKind::Whale: return GenWhale(seed, r, variant);
        case SeaKind::Ray: return GenRay(seed, r, variant);
        case SeaKind::Squid: return GenSquid(seed, r, variant);
        case SeaKind::Octopus: return GenOctopus(seed, r, variant);
        case SeaKind::Urchin: return GenUrchin(seed, r, variant);
        default: return RandomFish(seed, variant);
    }
}

// ---------------------------------------------------------------- individualize
SeaGenome Individualize(SeaGenome g, const std::string& salt) {
    // Python: random.Random(f"individual:{salt}:{seed}") — a *string* seed, which
    // CPython hashes with SHA-512. The port used to hash it with splitmix64, so
    // every individual came out different from the reference.
    SeaRandom r("individual:" + salt + ":" + std::to_string(g.seed));
    auto clip = [](double x, double lo, double hi) { return Clamp(x, lo, hi); };

    g.height *= r.Uniform(0.86, 1.14);
    g.lengthCm *= r.Uniform(0.8, 1.15);
    g.thickness *= r.Uniform(0.9, 1.1);
    g.nose = clip(g.nose * r.Uniform(0.88, 1.12), 0.3, 2.0);
    g.peduncle *= r.Uniform(0.88, 1.12);
    g.eyeSize *= r.Uniform(0.85, 1.15);

    if (g.kind == SeaKind::Fish || g.kind == SeaKind::Shark || g.kind == SeaKind::Whale) {
        g.tailLength *= r.Uniform(0.86, 1.14);
        g.tailSpread *= r.Uniform(0.86, 1.14);
        g.pectoralSize *= r.Uniform(0.82, 1.2);
        const double stretch = g.beak * (r.Uniform(0.75, 1.25) - 1.0);
        g.beak += stretch;
        g.eyePos += stretch;
        g.hammer *= r.Uniform(0.85, 1.15);
        if (g.dorsal.present) {
            g.dorsal.height *= r.Uniform(0.78, 1.22);
            const double shift = r.Uniform(-0.03, 0.03);
            g.dorsal.start += shift;
            g.dorsal.end += shift;
            g.dorsal.sweep = clip(g.dorsal.sweep + r.Uniform(-0.15, 0.15), 0.0, 1.0);
        }
    }
    if (g.wings.present) {
        g.wings.span *= r.Uniform(0.85, 1.15);
        g.wings.sweep = clip(g.wings.sweep + r.Uniform(-0.07, 0.07), 0.0, 1.5);
        g.wings.flap *= r.Uniform(0.9, 1.1);
    }
    if (g.arms.count > 0) {
        g.arms.length *= r.Uniform(0.8, 1.2);
        g.arms.spread *= r.Uniform(0.8, 1.2);
        g.arms.curl = clip(g.arms.curl + r.Uniform(-0.15, 0.15), 0.05, 1.0);
        g.arms.thickness *= r.Uniform(0.9, 1.12);
        g.arms.tentacleLength *= r.Uniform(0.85, 1.2);
        g.pulse *= r.Uniform(0.8, 1.2);
    }
    if (g.spines.count > 0) {
        g.spines.count = (int)Max(8.0, std::round(g.spines.count * r.Uniform(0.85, 1.2)));
        g.spines.length *= r.Uniform(0.85, 1.15);
    }

    ColorGenes& c = g.colors;
    c.hue = c.hue + r.Uniform(-0.02, 0.02);
    c.hue -= std::floor(c.hue);
    c.saturation = clip(c.saturation * r.Uniform(0.88, 1.12), 0.0, 1.0);
    // Two draws in one expression: C++ leaves the order of function arguments
    // unspecified, and MSVC evaluates right to left, which would feed the value
    // jitter the additive term's draw. Python reads left to right.
    const double valueScale = r.Uniform(0.9, 1.1);
    const double valueBias = r.Uniform(-0.02, 0.04);
    c.value = clip(c.value * valueScale + valueBias, 0.05, 1.0);
    if (c.bellyLightness > 0.2) c.bellyLightness = clip(c.bellyLightness + r.Uniform(-0.15, 0.15), 0.2, 1.0);
    c.patternScale = clip(c.patternScale + r.Uniform(-0.3, 0.3), 0.0, 1.0);
    switch (c.pattern) {
        case PatternKind::Spots:
        case PatternKind::Rings:
        case PatternKind::Tiger:
        case PatternKind::Koi:
        case PatternKind::Calico:
        case PatternKind::Blotches:
        case PatternKind::Mackerel:
        case PatternKind::Trout:
        case PatternKind::Salmon:
        case PatternKind::WhaleShark:
        case PatternKind::Barracuda:
        case PatternKind::Shimmer:
        case PatternKind::Humpback:
        case PatternKind::RedBelly:
            c.patternOffset = r.Random();
            break;
        default:
            c.patternOffset += r.Uniform(-0.015, 0.015);
            break;
    }
    if (g.kind == SeaKind::Whale || g.kind == SeaKind::Ray || g.kind == SeaKind::Shark) {
        g.scars = clip(g.scars + r.Uniform(-0.2, 0.3), 0.0, 1.0);
    }
    auto jitterHex = [&](std::string& hex) {
        if (hex.empty()) return;
        Color8 col;
        if (!ParseHexColor(hex, col)) return;
        double h, s, v;
        RgbToHsv(col, h, s, v);
        // Hoisted, in Python's order: three draws inside one call would be
        // ordered by the compiler, not by the reference.
        const double hueJitter = r.Uniform(-0.03, 0.03);
        const double satJitter = r.Uniform(0.8, 1.2);
        const double valJitter = r.Uniform(0.88, 1.12);
        hex = HexFromHsv(h + hueJitter, s * satJitter, v * valJitter);
    };
    jitterHex(c.finColor);
    jitterHex(c.accentColor);
    if (g.scariness > 0) g.scariness = clip(g.scariness + r.Uniform(-0.2, 0.2), 0.1, 1.0);
    return g;
}

} // namespace sea
