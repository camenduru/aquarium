#pragma once
// ================================================================
// SEA CREATURES - genome ("DNA")
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/genome.py and kinds.py (MIT).
//
// An animal's KIND is decided first (fish, shark, whale, ray, squid,
// octopus, urchin) and every gene is then drawn to suit it, so a
// random shark is always a shark and only its proportions, fins,
// colours and markings vary with the seed. A species preset (see
// SeaSpecies.h) asks the kind's generator for a body and then pins
// the genes that make the species.
//
// All lengths are relative to the body length = 1.
// ================================================================

#include "creature/sea/SeaMath.h"

#include <string>

namespace sea {

enum class SeaKind { Fish = 0, Shark, Whale, Ray, Squid, Octopus, Urchin };

/// Caudal fin outline. "Flukes" lie flat (whales), "heterocercal" is the shark tail.
enum class TailShape {
    Forked = 0, Rounded, Truncate, Lunate, Pointed, Fan, Heterocercal, Flukes, None
};

enum class FinShape { Triangle = 0, Rounded, Sail, Long, Falcate, Pennant };

/// Body markings. The first nine are what a random fish may pick; the rest are
/// species-specific (orca, koi, mackerel, ...).
enum class PatternKind {
    None = 0, Stripes, Spots, Lateral, Saddle, Gradient, Bands, Tiger, Shimmer,
    Orca, Rings, Neon, WhaleShark, Mackerel, Koi, Calico, Discus, Chevron,
    Trout, Salmon, Blotches, Tang, DolphinCape, Barracuda, Humpback, RedBelly
};

enum class EyeStyle { Fish = 0, Dark, None };
enum class WingShape { Pointed = 0, Round, Diamond };
enum class PectoralShape { Lobe = 0, Blade };
enum class LobeShape { None = 0, Horns, Round };
enum class BodyArchetype { Slender = 0, Torpedo, Classic, Deep, Round };

/// A median or paired fin.
struct FinGenes {
    bool present = false;
    double start = 0.5;    // along the body (0 = nose, 1 = tail base)
    double end = 0.6;
    double height = 0.05;  // relative to body length
    FinShape shape = FinShape::Triangle;
    double sweep = 0.5;    // how far the tip leans backwards (0..1)
};

/// Flat sheets out of the flanks: ray wings, squid fins, dumbo-octopus ears.
struct WingGenes {
    bool present = false;
    double start = 0.05;   // root leading edge
    double end = 0.45;     // root trailing edge
    double span = 0.4;     // reach from the flank (body length = 1)
    double sweep = 0.6;    // how far back the tip sits
    WingShape shape = WingShape::Pointed;
    double flap = 0.6;     // flap amplitude, a fraction of the span
    double wave = 0.25;    // flap waves along the chord
    double dihedral = 0.06; // resting upward tilt (radians)
};

/// Cephalopod arms trailing behind the head.
struct ArmGenes {
    int count = 0;
    double length = 0.8;
    double thickness = 0.08;
    int tentacles = 0;     // extra feeding tentacles (squid have 2)
    double tentacleLength = 1.3;
    double spread = 0.35;
    double curl = 0.3;
};

/// Urchin spines radiating from the body.
struct SpineGenes {
    int count = 0;
    double length = 0.35;
    double sway = 0.15;
    double taper = 0.85;
};

/// Every colour decision. Optional exact colours are empty strings when unset.
struct ColorGenes {
    double hue = 0.55;          // 0..1
    double saturation = 0.7;
    double value = 0.8;
    double finHueShift = 0.0;
    double accentHueShift = 0.0;
    double bellyLightness = 0.5; // 0 = belly same as body, 1 = very pale
    PatternKind pattern = PatternKind::None;
    double patternScale = 0.5;
    double patternOffset = 0.0;

    std::string finColor;       // "#rrggbb"
    std::string accentColor;
    std::string accent2Color;
    std::string tailColor;      // tail fin when it differs from the other fins
    std::string finEdgeColor;   // thin margin around the fins (clownfish)
    std::string accentEdgeColor; // thin margin round the pattern marks
    double finTips = 0.0;       // outer fraction of each fin painted in the accent
    bool patternOnFins = false; // stripes carry on across the fins
    bool tailBodyColor = false; // tail fin in body colours (sharks)
    bool bellyPattern = false;  // ventral markings too (manta belly spots)
    bool hasBellyLevel = false;
    double bellyLevel = 0.0;    // -1 belly .. 1 back
};

/// One animal's complete description.
struct SeaGenome {
    u64 seed = 1;
    SeaKind kind = SeaKind::Fish;
    std::string profile = "fish";   // "fish" | "sphere"

    // body
    double height = 0.35;      // max body height
    double thickness = 0.65;   // max width, relative to height
    double peak = 0.4;         // where the height peaks
    double nose = 0.6;         // nose shape exponent
    double taper = 1.0;        // how quickly the rear thins out
    double peduncle = 0.15;    // tail-stem thickness, relative to height
    double backArch = 0.5;     // how much height sits above the spine

    // head
    double eyePos = 0.1;
    double eyeHeight = 0.25;
    double eyeSize = 0.22;
    EyeStyle eyeStyle = EyeStyle::Fish;
    int gillSlits = 1;         // 1 fish gill line, 5 shark slits, 0 none
    double beak = 0.0;         // dolphin rostrum
    double melon = 0.0;        // forehead bulge
    double neck = 0.0;         // neck pinch (belugas)
    double flatHead = 0.0;     // 1 = flat top line
    double hammer = 0.0;       // hammerhead head wing
    double mouth = 0.02;
    double mouthHeight = -0.35;
    bool teeth = false;
    double scariness = 0.0;    // 0..1

    // fins and tail
    TailShape tailShape = TailShape::Rounded;
    double tailLength = 0.24;
    double tailSpread = 1.2;
    FinGenes dorsal;
    // Python's dorsal2 default sits at 0.75..0.82, not at the generic 0.5..0.6;
    // the gene is inert while `present` is false, but a same-seed diff notices.
    FinGenes dorsal2 = {false, 0.75, 0.82, 0.05, FinShape::Triangle, 0.5};
    FinGenes anal;
    FinGenes pelvic;
    double pectoralSize = 0.15;
    double pectoralFlap = 1.0;
    PectoralShape pectoralShape = PectoralShape::Lobe;
    double pectoralDroop = 0.45;
    int finlets = 0;

    // motion
    double swimAmplitude = 0.65;
    double waveCount = 0.8;
    bool swimVertical = false;  // whales and dolphins beat up and down
    double pulse = 0.0;          // squid jet squeeze
    double turnLag = 0.9;
    double cameraTilt = 0.0;

    // other body plans
    WingGenes wings;
    ArmGenes arms;
    SpineGenes spines;
    LobeShape lobes = LobeShape::None;
    double lobeLength = 0.0;
    double puff = 0.0;           // pufferfish inflation
    int puffSpines = 0;
    bool pleats = false;

    // surface
    ColorGenes colors;
    double lengthCm = 30.0;      // real adult size: nose-tail, disc width, shell
    double silver = 0.0;
    double gloss = 0.35;
    double scales = 0.0;
    double finAlpha = 0.8;
    bool hasTailAlpha = false;
    double tailAlpha = 0.8;
    double billow = 0.0;         // cloth-like fins (bettas)
    std::string irisColor;
    double scars = 0.0;
    double knobs = 0.0;
    std::string species;   // preset name; empty for a random animal
    std::string morph;     // colour variety of that preset, if any
};

// ---------------------------------------------------------------- ids
const char* KindId(SeaKind kind);
const char* KindLabel(SeaKind kind);
bool ParseKind(const std::string& id, SeaKind& out);

const char* TailId(TailShape shape);
const char* FinShapeId(FinShape shape);
const char* PatternId(PatternKind pattern);
PatternKind PatternFromId(const std::string& id);

const char* ArchetypeId(BodyArchetype archetype);
bool ParseArchetype(const std::string& id, BodyArchetype& out);

// ---------------------------------------------------------------- generation
/// A random fish for `seed`. `archetype` (an ARCHETYPE id) forces the body type.
SeaGenome RandomFish(u64 seed, const char* archetype = nullptr);

/// A random animal of `kind`. `variant` picks a kind variant ("hammerhead",
/// "dolphin", "stingray", "cuttlefish", "dumbo", "long"); nullptr lets the seed decide.
SeaGenome RandomGenome(SeaKind kind, u64 seed, const char* variant = nullptr);

/// Seeded individual variation for a species preset: proportions, fin/wing/arm
/// sizes and colours shift within a natural range while the species-defining
/// genes stay put. `salt` is normally the species name.
SeaGenome Individualize(SeaGenome genome, const std::string& salt);

} // namespace sea
