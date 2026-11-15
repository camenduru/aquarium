#pragma once
// ================================================================
// AQUARIUM - water tuning
// ----------------------------------------------------------------
// The parameter set, its ranges and the INI file that drives them.
// Keys are the pixel-art-water-vfx demo's own IDs, so the same
// vocabulary works on both sides; the demo's JSON is not read here
// (deliberately: this project has no JSON parser and no settings UI).
//
// Every animated term is clock*speed+offset. Changing a speed adds
// clock*(old-new) to the offset, exactly like the demo, so the pattern
// never jumps when a value is edited.
// ================================================================

#include <string>
#include <string_view>
#include <vector>

namespace water {

struct Rgb
{
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

struct Grade
{
    float hue      = 0.0f;   // degrees
    float sat      = 1.0f;   // multiplier
    float bright   = 1.0f;   // multiplier
};

/// The tank's procedural geometry. One spec feeds both the masks and the
/// sand/rock plate, so the two can never disagree.
/// Hard cap on procedural corals: each is a small drawing, but the reef is
/// scenery, not wallpaper, and the ghost alpha budget is shared.
constexpr int kMaxCorals = 24;

/// Hard cap on airstone beads; the column is resolved into a fixed array.
constexpr int kMaxBubbles = 128;

/// How many bubbling outlets water.ini may define (one bubble column each).
constexpr int kMaxBubbleColumns = 6;

/// One bubbling outlet: a position along the sand bed, as a fraction of the
/// tank width. The column always rises from the sand horizon at that x.
struct BubbleEmitter
{
    float x = 0.0f;
};

struct AquascapeSpec
{
    unsigned seed       = 7;
    float    sandY      = 0.86f;   // sand horizon as a fraction of logical height
    float    sandCurve  = 3.0f;    // px of sinusoidal sand horizon
    int      rockCount  = 4;
    float    rockScale  = 1.0f;
    bool     frontRock  = true;
    int      coralCount = 9;       // procedural corals, plants and polyps
    int      margin     = 6;       // logical px inset of the tank interior
};

struct WaterTuning
{
    // ---- water -------------------------------------------------
    float clarity        = 0.65f;
    float waterMix       = 0.12f;
    float deepMix        = 0.32f;
    float tideInfluence  = 0.28f;
    float waterLevel     = 0.0f;
    float ghostAlpha     = 0.5f;   // aquarium-only: scales the whole water body
    float strength       = 1.0f;
    float refractionBase = 0.4f;

    // ---- waves -------------------------------------------------
    float wave1Density = 1.0f;
    float wave1Angle   = 0.0f;
    float wave1Speed   = 1.0f;
    float offsetWave1  = 0.0f;
    float wave2Density = 1.0f;
    float wave2Angle   = 0.0f;
    float wave2Speed   = 1.0f;
    float offsetWave2  = 0.0f;
    float wave2Weight  = 0.45f;
    float waveAxisSpeed = 0.9f;

    // ---- caustics ----------------------------------------------
    float causticStrength      = 0.27f;
    float causticDensity       = 0.16f;
    float causticX             = 0.12f;
    float offsetCausticX       = 0.0f;
    float causticY             = -0.08f;
    float offsetCausticY       = 0.0f;
    float causticWidth         = 1.0f;
    float causticDeform        = 1.0f;
    float offsetCausticDeform  = 0.0f;
    float causticBreak         = 1.0f;
    float offsetCausticBreak   = 0.0f;

    // ---- surface foam ------------------------------------------
    float foamStrength         = 0.28f;
    float foamDistance         = 3.0f;
    float foamAmplitude        = 1.3f;
    float foamSpeed            = 0.65f;
    float offsetFoamSpeed      = 0.0f;
    float foamJagged           = 0.65f;
    float foamJaggedSpeed      = 0.4f;
    float offsetFoamJagged     = 0.0f;
    float foamWidth            = 1.0f;
    float foamBreakSpeed       = 0.33f;
    float offsetFoamBreak      = 0.0f;
    float backwashDistance     = 4.0f;
    float backwashWidth        = 0.7f;
    float backwashStrength     = 0.12f;

    // ---- ripples -----------------------------------------------
    float rippleLifetime   = 2.8f;
    float rippleSpeed      = 15.0f;
    float rippleRadius     = 2.0f;
    float rippleAspect     = 1.6f;
    float rippleWidth      = 1.0f;
    float rippleRefraction = 1.4f;
    float rippleHighlight  = 0.3f;

    // ---- airstone (the demo's waterfall block, re-aimed) -------
    int   bubbleCount         = 20;    // beads per outlet (0-64)
    std::vector<BubbleEmitter> bubbleEmitters;   // empty = the stone's airstone
    float flowAspect          = 1.8f;
    float flowDensity         = 0.22f;
    float flowSpeed           = 2.8f;
    float offsetFlow          = 0.0f;
    float flowDecay           = 90.0f;
    float flowAmplitude       = 0.65f;
    float impactAspect        = 2.5f;
    float impactDensity       = 0.28f;
    float impactSpeed         = 3.0f;
    float offsetImpact        = 0.0f;
    float impactAmplitude     = 1.4f;
    float impactBrightness    = 0.94f;
    float impactFlicker       = 0.06f;

    // ---- night -------------------------------------------------
    float moonStrength  = 0.65f;
    float moonWidth     = 6.0f;
    float moonAmplitude = 3.0f;
    float moonSpeed     = 0.65f;
    float offsetMoon    = 0.0f;

    // ---- fish (aquarium-only) ----------------------------------
    /// How fast a clicked fish flees, as a multiple of its cruise speed.
    /// The alarm lasts 1.6 s and eases from this value back to 1.
    float escapeSpeed = 2.8f;

    /// Relative odds of each kind when a random fish is added, in SeaKind
    /// order (fish, shark, whale, ray, squid, octopus, urchin). Only the
    /// ratios matter; 0 keeps that kind out of the random draw entirely.
    /// The defaults are a reef ladder: small fish are the school, urchins are
    /// a find.
    int weightFish    = 30;
    int weightShark   = 12;
    int weightWhale   = 8;
    int weightRay     = 5;
    int weightSquid   = 3;
    int weightOctopus = 2;
    int weightUrchin  = 1;

    // ---- effect toggles ----------------------------------------
    bool depthEnabled      = true;
    bool refractionEnabled = true;
    bool causticsEnabled   = true;
    bool foamEnabled       = true;
    bool ripplesEnabled    = true;
    bool waterfallEnabled  = true;
    bool environmentEnabled = true;
    bool submersionEnabled = true;
    bool reflectionEnabled = false;

    // ---- relative colour grades (the demo's 16, trimmed) ------
    Grade shallowGrade;    //   0 deg / 1.0 / 1.0
    Grade deepGrade;
    Grade causticGrade;
    Grade foamGrade;
    Grade backwashGrade;
    Grade rippleGrade;
    Grade moonGrade;
    Grade flowGrade;

    // ---- phase helpers ----------------------------------------
    /// clock*speed + offset for one animated term.
    double Phase(double clock, float speed, float offset) const
    {
        return clock * double(speed) + double(offset);
    }
};

struct WaterPalette
{
    Rgb shallowDay, deepDay, shallowNight, deepNight;
    Rgb causticDay, causticNight;
    Rgb foamDay, foamNight;
    Rgb backwash;
    Rgb rippleDay, rippleNight;
    Rgb moon;
    Rgb flowDay, flowNight;
    Rgb stone;
};

/// HSV relative grade: hue shifts, saturation and brightness scale.
Rgb GradeRgb(Rgb c, const Grade& g);

/// Resolve every base colour and grade into constants the composite can
/// mix per pixel without calling HSV code.
void BuildPalette(const WaterTuning& tuning, WaterPalette& out);

struct IniReport
{
    std::vector<std::string> applied;
    std::vector<std::string> errors;
};

/// Parse one `key = value` pair against the registry. Used by the INI
/// loader and by --water-set, so both validate identically.
bool ApplySetting(const std::string& key, const std::string& value,
                  WaterTuning& tuning, AquascapeSpec& spec, std::string& error);

/// Load a whole INI. Values are applied individually: a bad line is
/// reported and skipped, never partially written into a field.
bool LoadIni(const std::string& path, WaterTuning& tuning, AquascapeSpec& spec,
             IniReport& report);

/// Same parser over text already in memory. `path` only names the source in
/// messages, so a caller that splits a file (a tank save, whose creature list
/// is not a water setting) can feed the rest without a temp file.
bool LoadIniText(const std::string& path, std::string_view text,
                 WaterTuning& tuning, AquascapeSpec& spec, IniReport& report);

/// Write a fully commented template holding the current values. `header`
/// replaces the default comment block; each entry becomes one `; ` line.
bool WriteIni(const std::string& path, const WaterTuning& tuning,
              const AquascapeSpec& spec, std::string& error,
              const std::vector<std::string>& header = {});

} // namespace water
