// ================================================================
// AQUARIUM - water tuning implementation
// ================================================================

#include "water/WaterTuning.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace water {
namespace {

struct FloatDef
{
    const char* section;
    const char* key;
    float WaterTuning::* field;
    float lo, hi;
};

struct BoolDef
{
    const char* key;
    bool WaterTuning::* field;
};

struct IntDef
{
    const char* section;
    const char* key;
    int WaterTuning::* field;
    int lo, hi;
};

struct GradeDef
{
    const char* key;    // "<id>_grade" - hue/saturation/brightness follow
    Grade WaterTuning::* field;
};

struct SpecFloatDef
{
    const char* key;
    float AquascapeSpec::* field;
    float lo, hi;
};

struct SpecIntDef
{
    const char* key;
    int AquascapeSpec::* field;
    int lo, hi;
};

const FloatDef kFloats[] = {
    // [water]
    { "water", "clarity",          &WaterTuning::clarity,         0.0f,   1.0f },
    { "water", "water_mix",        &WaterTuning::waterMix,        0.0f,   1.0f },
    { "water", "deep_mix",         &WaterTuning::deepMix,         0.0f,   1.0f },
    { "water", "tide_influence",   &WaterTuning::tideInfluence,   0.0f,   1.0f },
    { "water", "water_level",      &WaterTuning::waterLevel,     -1.0f,   1.0f },
    { "water", "ghost_alpha",      &WaterTuning::ghostAlpha,      0.0f,   1.0f },
    { "water", "strength",         &WaterTuning::strength,        0.0f,   4.0f },
    { "water", "refraction_base",  &WaterTuning::refractionBase,  0.0f,   2.0f },
    // [waves]
    { "waves", "wave1_density",    &WaterTuning::wave1Density,    0.1f,   4.0f },
    { "waves", "wave1_angle",      &WaterTuning::wave1Angle,   -180.0f, 180.0f },
    { "waves", "wave1_speed",      &WaterTuning::wave1Speed,     -4.0f,   4.0f },
    { "waves", "offset_wave1_speed",  &WaterTuning::offsetWave1, -1000.0f, 1000.0f },
    { "waves", "wave2_density",    &WaterTuning::wave2Density,    0.1f,   4.0f },
    { "waves", "wave2_angle",      &WaterTuning::wave2Angle,   -180.0f, 180.0f },
    { "waves", "wave2_speed",      &WaterTuning::wave2Speed,     -4.0f,   4.0f },
    { "waves", "offset_wave2_speed",  &WaterTuning::offsetWave2, -1000.0f, 1000.0f },
    { "waves", "wave2_weight",     &WaterTuning::wave2Weight,    -1.0f,   1.0f },
    { "waves", "wave_axis_speed",  &WaterTuning::waveAxisSpeed,   0.0f,   4.0f },
    // [caustics]
    { "caustics", "caustic_strength",    &WaterTuning::causticStrength,     0.0f, 2.0f },
    { "caustics", "caustic_density",     &WaterTuning::causticDensity,      0.02f, 0.6f },
    { "caustics", "caustic_x",           &WaterTuning::causticX,           -2.0f, 2.0f },
    { "caustics", "offset_caustic_x",    &WaterTuning::offsetCausticX,   -1000.0f, 1000.0f },
    { "caustics", "caustic_y",           &WaterTuning::causticY,           -2.0f, 2.0f },
    { "caustics", "offset_caustic_y",    &WaterTuning::offsetCausticY,   -1000.0f, 1000.0f },
    { "caustics", "caustic_width",       &WaterTuning::causticWidth,        0.1f, 5.0f },
    { "caustics", "caustic_deform",      &WaterTuning::causticDeform,      -4.0f, 4.0f },
    { "caustics", "offset_caustic_deform", &WaterTuning::offsetCausticDeform, -1000.0f, 1000.0f },
    { "caustics", "caustic_break",       &WaterTuning::causticBreak,       -4.0f, 4.0f },
    { "caustics", "offset_caustic_break", &WaterTuning::offsetCausticBreak, -1000.0f, 1000.0f },
    // [foam]
    { "foam", "foam_strength",     &WaterTuning::foamStrength,     0.0f, 1.0f },
    { "foam", "foam_distance",     &WaterTuning::foamDistance,     0.0f, 30.0f },
    { "foam", "foam_amplitude",    &WaterTuning::foamAmplitude,    0.0f, 20.0f },
    { "foam", "foam_speed",        &WaterTuning::foamSpeed,       -4.0f, 4.0f },
    { "foam", "offset_foam_speed", &WaterTuning::offsetFoamSpeed, -1000.0f, 1000.0f },
    { "foam", "foam_jagged",       &WaterTuning::foamJagged,       0.0f, 5.0f },
    { "foam", "foam_jagged_speed", &WaterTuning::foamJaggedSpeed, -4.0f, 4.0f },
    { "foam", "offset_foam_jagged_speed", &WaterTuning::offsetFoamJagged, -1000.0f, 1000.0f },
    { "foam", "foam_width",        &WaterTuning::foamWidth,        0.1f, 5.0f },
    { "foam", "foam_break_speed",  &WaterTuning::foamBreakSpeed,  -4.0f, 4.0f },
    { "foam", "offset_foam_break_speed", &WaterTuning::offsetFoamBreak, -1000.0f, 1000.0f },
    { "foam", "backwash_distance", &WaterTuning::backwashDistance, 0.0f, 30.0f },
    { "foam", "backwash_width",    &WaterTuning::backwashWidth,    0.1f, 5.0f },
    { "foam", "backwash_strength", &WaterTuning::backwashStrength, 0.0f, 1.0f },
    // [ripples]
    { "ripples", "ripple_lifetime",   &WaterTuning::rippleLifetime,   0.2f, 10.0f },
    { "ripples", "ripple_speed",      &WaterTuning::rippleSpeed,      0.0f, 80.0f },
    { "ripples", "ripple_radius",     &WaterTuning::rippleRadius,     0.0f, 40.0f },
    { "ripples", "ripple_aspect",     &WaterTuning::rippleAspect,     0.2f, 4.0f },
    { "ripples", "ripple_width",      &WaterTuning::rippleWidth,      0.1f, 5.0f },
    { "ripples", "ripple_refraction", &WaterTuning::rippleRefraction, 0.0f, 6.0f },
    { "ripples", "ripple_highlight",  &WaterTuning::rippleHighlight,  0.0f, 1.0f },
    // [falls] - the airstone
    { "falls", "flow_aspect",       &WaterTuning::flowAspect,       0.5f, 5.0f },
    { "falls", "flow_density",      &WaterTuning::flowDensity,      0.02f, 1.0f },
    { "falls", "flow_speed",        &WaterTuning::flowSpeed,       -8.0f, 8.0f },
    { "falls", "offset_flow_speed", &WaterTuning::offsetFlow,     -1000.0f, 1000.0f },
    { "falls", "flow_decay",        &WaterTuning::flowDecay,        8.0f, 240.0f },
    { "falls", "flow_amplitude",    &WaterTuning::flowAmplitude,    0.0f, 4.0f },
    { "falls", "impact_aspect",     &WaterTuning::impactAspect,     0.5f, 5.0f },
    { "falls", "impact_density",    &WaterTuning::impactDensity,    0.02f, 1.0f },
    { "falls", "impact_speed",      &WaterTuning::impactSpeed,     -8.0f, 8.0f },
    { "falls", "offset_impact_speed", &WaterTuning::offsetImpact, -1000.0f, 1000.0f },
    { "falls", "impact_amplitude",  &WaterTuning::impactAmplitude,  0.0f, 6.0f },
    { "falls", "impact_brightness", &WaterTuning::impactBrightness, 0.0f, 2.0f },
    { "falls", "impact_flicker",    &WaterTuning::impactFlicker,    0.0f, 1.0f },
    // [night]
    { "night", "moon_strength",  &WaterTuning::moonStrength,  0.0f, 3.0f },
    { "night", "moon_width",     &WaterTuning::moonWidth,     0.5f, 40.0f },
    { "night", "moon_amplitude", &WaterTuning::moonAmplitude, 0.0f, 20.0f },
    { "night", "moon_speed",     &WaterTuning::moonSpeed,    -4.0f, 4.0f },
    { "night", "offset_moon_speed", &WaterTuning::offsetMoon, -1000.0f, 1000.0f },
};

const IntDef kInts[] = {
    { "falls", "bubble_count", &WaterTuning::bubbleCount, 0, 64 },
};

const BoolDef kBools[] = {
    { "depth_enabled",       &WaterTuning::depthEnabled },
    { "refraction_enabled",  &WaterTuning::refractionEnabled },
    { "caustics_enabled",    &WaterTuning::causticsEnabled },
    { "foam_enabled",        &WaterTuning::foamEnabled },
    { "ripples_enabled",     &WaterTuning::ripplesEnabled },
    { "waterfall_enabled",   &WaterTuning::waterfallEnabled },
    { "environment_enabled", &WaterTuning::environmentEnabled },
    { "submersion_enabled",  &WaterTuning::submersionEnabled },
    { "reflection_enabled",  &WaterTuning::reflectionEnabled },
};

const GradeDef kGrades[] = {
    { "shallow_grade",  &WaterTuning::shallowGrade },
    { "deep_grade",     &WaterTuning::deepGrade },
    { "caustic_grade",  &WaterTuning::causticGrade },
    { "foam_grade",     &WaterTuning::foamGrade },
    { "backwash_grade", &WaterTuning::backwashGrade },
    { "ripple_grade",   &WaterTuning::rippleGrade },
    { "moon_grade",     &WaterTuning::moonGrade },
    { "flow_grade",     &WaterTuning::flowGrade },
};

const SpecFloatDef kSpecFloats[] = {
    { "sand_y",     &AquascapeSpec::sandY,     0.4f,  0.98f },
    { "sand_curve", &AquascapeSpec::sandCurve, 0.0f, 20.0f },
    { "rock_scale", &AquascapeSpec::rockScale, 0.3f,  3.0f },
};

const SpecIntDef kSpecInts[] = {
    { "rock_count",  &AquascapeSpec::rockCount,  0, 14 },
    { "coral_count", &AquascapeSpec::coralCount, 0, kMaxCorals },
    { "margin",      &AquascapeSpec::margin,     2, 40 },
};

std::string Trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string Lower(std::string s)
{
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool ParseFloat(const std::string& text, float& out)
{
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || errno == ERANGE || !std::isfinite(v)) return false;
    while (end != nullptr && (*end == ' ' || *end == '\t')) ++end;
    if (end != nullptr && *end != '\0') return false;
    out = float(v);
    return true;
}

bool ParseBool(const std::string& text, bool& out)
{
    const std::string t = Lower(Trim(text));
    if (t == "1" || t == "true" || t == "on" || t == "yes")  { out = true;  return true; }
    if (t == "0" || t == "false" || t == "off" || t == "no") { out = false; return true; }
    return false;
}

} // namespace

Rgb GradeRgb(Rgb c, const Grade& g)
{
    if (g.hue == 0.0f && g.sat == 1.0f && g.bright == 1.0f) return c;

    const float mx = std::max({ c.r, c.g, c.b });
    const float mn = std::min({ c.r, c.g, c.b });
    const float d  = mx - mn;

    float h = 0.0f;
    if (d > 1e-6f)
    {
        if (mx == c.r)      h = std::fmod((c.g - c.b) / d, 6.0f);
        else if (mx == c.g) h = (c.b - c.r) / d + 2.0f;
        else                h = (c.r - c.g) / d + 4.0f;
        h /= 6.0f;
        if (h < 0.0f) h += 1.0f;
    }
    const float s = mx > 1e-6f ? d / mx : 0.0f;
    const float v = mx;

    float hue = std::fmod(h + g.hue / 360.0f, 1.0f);
    if (hue < 0.0f) hue += 1.0f;
    const float sat = std::clamp(s * g.sat, 0.0f, 1.0f);
    const float val = std::clamp(v * g.bright, 0.0f, 1.0f);

    const float  sector = hue * 6.0f;
    const int    i      = int(std::floor(sector)) % 6;
    const float  f      = sector - std::floor(sector);
    const float  p      = val * (1.0f - sat);
    const float  q      = val * (1.0f - f * sat);
    const float  t      = val * (1.0f - (1.0f - f) * sat);

    switch (i)
    {
    case 0: return { val, t, p };
    case 1: return { q, val, p };
    case 2: return { p, val, t };
    case 3: return { p, q, val };
    case 4: return { t, p, val };
    default: return { val, p, q };
    }
}

void BuildPalette(const WaterTuning& t, WaterPalette& out)
{
    // Base colours are the demo's own: day and night keep their relationship,
    // and the grades are relative adjustments on top of them.
    out.shallowDay   = GradeRgb({ 0.160f, 0.690f, 0.650f }, t.shallowGrade);
    out.deepDay      = GradeRgb({ 0.025f, 0.190f, 0.290f }, t.deepGrade);
    out.shallowNight = GradeRgb({ 0.025f, 0.260f, 0.390f }, t.shallowGrade);
    out.deepNight    = GradeRgb({ 0.010f, 0.045f, 0.140f }, t.deepGrade);
    out.causticDay   = GradeRgb({ 0.520f, 0.930f, 0.870f }, t.causticGrade);
    out.causticNight = GradeRgb({ 0.160f, 0.400f, 0.620f }, t.causticGrade);
    out.foamDay      = GradeRgb({ 0.720f, 0.890f, 0.790f }, t.foamGrade);
    out.foamNight    = GradeRgb({ 0.220f, 0.490f, 0.680f }, t.foamGrade);
    out.backwash     = GradeRgb({ 0.280f, 0.460f, 0.390f }, t.backwashGrade);
    out.rippleDay    = GradeRgb({ 0.650f, 0.880f, 0.810f }, t.rippleGrade);
    out.rippleNight  = GradeRgb({ 0.260f, 0.630f, 0.790f }, t.rippleGrade);
    out.moon         = GradeRgb({ 0.130f, 0.270f, 0.350f }, t.moonGrade);
    out.flowDay      = GradeRgb({ 0.018f, 0.050f, 0.050f }, t.flowGrade);
    out.flowNight    = GradeRgb({ 0.012f, 0.035f, 0.065f }, t.flowGrade);
    out.stone        = { 0.62f, 0.66f, 0.66f };
}

bool ApplySetting(const std::string& key, const std::string& value,
                  WaterTuning& tuning, AquascapeSpec& spec, std::string& error)
{
    const std::string k = Trim(key);

    for (const FloatDef& def : kFloats)
    {
        if (k != def.key) continue;
        float v = 0.0f;
        if (!ParseFloat(value, v)) { error = k + ": not a number"; return false; }
        if (v < def.lo || v > def.hi)
        {
            std::ostringstream msg;
            msg << k << ": " << v << " outside [" << def.lo << ", " << def.hi << "]";
            error = msg.str();
            return false;
        }
        tuning.*(def.field) = v;
        return true;
    }

    for (const BoolDef& def : kBools)
    {
        if (k != def.key) continue;
        bool v = false;
        if (!ParseBool(value, v)) { error = k + ": expected 0/1"; return false; }
        tuning.*(def.field) = v;
        return true;
    }

    for (const IntDef& def : kInts)
    {
        if (k != def.key) continue;
        float parsed = 0.0f;
        if (!ParseFloat(value, parsed) || parsed != std::floor(parsed))
        {
            error = k + ": expected an integer";
            return false;
        }
        const int v = int(parsed);
        if (v < def.lo || v > def.hi)
        {
            std::ostringstream msg;
            msg << k << ": " << v << " outside [" << def.lo << ", " << def.hi << "]";
            error = msg.str();
            return false;
        }
        tuning.*(def.field) = v;
        return true;
    }

    for (const GradeDef& def : kGrades)
    {
        const std::string base = std::string(def.key) + "_";
        if (k.rfind(base, 0) != 0) continue;
        const std::string leaf = k.substr(base.size());
        float v = 0.0f;
        if (!ParseFloat(value, v)) { error = k + ": not a number"; return false; }
        Grade& g = tuning.*(def.field);
        if (leaf == "hue")
        {
            if (v < -360.0f || v > 360.0f) { error = k + ": outside [-360, 360]"; return false; }
            g.hue = v;
            return true;
        }
        if (leaf == "saturation")
        {
            if (v < 0.0f || v > 4.0f) { error = k + ": outside [0, 4]"; return false; }
            g.sat = v;
            return true;
        }
        if (leaf == "brightness")
        {
            if (v < 0.0f || v > 4.0f) { error = k + ": outside [0, 4]"; return false; }
            g.bright = v;
            return true;
        }
        error = k + ": unknown grade channel";
        return false;
    }

    // [aquascape]
    if (k == "seed")
    {
        errno = 0;
        char* end = nullptr;
        const unsigned long v = std::strtoul(Trim(value).c_str(), &end, 10);
        if (end == value.c_str() || errno == ERANGE) { error = "seed: not an integer"; return false; }
        spec.seed = unsigned(v);
        return true;
    }
    if (k == "front_rock")
    {
        bool v = false;
        if (!ParseBool(value, v)) { error = "front_rock: expected 0/1"; return false; }
        spec.frontRock = v;
        return true;
    }
    for (const SpecFloatDef& def : kSpecFloats)
    {
        if (k != def.key) continue;
        float v = 0.0f;
        if (!ParseFloat(value, v)) { error = k + ": not a number"; return false; }
        if (v < def.lo || v > def.hi)
        {
            std::ostringstream msg;
            msg << k << ": " << v << " outside [" << def.lo << ", " << def.hi << "]";
            error = msg.str();
            return false;
        }
        spec.*(def.field) = v;
        return true;
    }
    for (const SpecIntDef& def : kSpecInts)
    {
        if (k != def.key) continue;
        float parsed = 0.0f;
        if (!ParseFloat(value, parsed) || parsed != std::floor(parsed))
        {
            error = k + ": expected an integer";
            return false;
        }
        const int v = int(parsed);
        if (v < def.lo || v > def.hi)
        {
            std::ostringstream msg;
            msg << k << ": " << v << " outside [" << def.lo << ", " << def.hi << "]";
            error = msg.str();
            return false;
        }
        spec.*(def.field) = v;
        return true;
    }

    // [falls] outlets: "bubble_column_N = x" along the sand. Repeatable lines;
    // deleting one removes that bubble column on the next reload.
    if (k.rfind("bubble_column", 0) == 0)
    {
        float x = 0.0f;
        if (!ParseFloat(value, x))
        {
            error = k + ": expected x (a fraction of the tank width, e.g. 0.16)";
            return false;
        }
        if (x < 0.0f || x > 1.0f)
        {
            error = k + ": outside [0, 1]";
            return false;
        }
        if (int(tuning.bubbleEmitters.size()) >= kMaxBubbleColumns)
        {
            std::ostringstream msg;
            msg << k << ": at most " << kMaxBubbleColumns << " outlets";
            error = msg.str();
            return false;
        }
        tuning.bubbleEmitters.push_back({ x });
        return true;
    }

    error = "unknown key: " + k;
    return false;
}

bool LoadIniText(const std::string& path, std::string_view text,
                 WaterTuning& tuning, AquascapeSpec& spec, IniReport& report)
{
    std::istringstream file{ std::string(text) };

    std::string line;
    std::string section;
    int lineNumber = 0;
    bool sawAnyValue = false;

    // Outlets are a list, so a reload rebuilds it from the file: deleting a
    // bubble_column line removes that column instead of leaving a stale one.
    tuning.bubbleEmitters.clear();

    while (std::getline(file, line))
    {
        ++lineNumber;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']')
        {
            section = Lower(Trim(trimmed.substr(1, trimmed.size() - 2)));
            continue;
        }

        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos)
        {
            std::ostringstream msg;
            msg << path << ":" << lineNumber << ": expected key = value";
            report.errors.push_back(msg.str());
            continue;
        }

        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string val = Trim(trimmed.substr(eq + 1));
        if (key.empty())
        {
            std::ostringstream msg;
            msg << path << ":" << lineNumber << ": empty key";
            report.errors.push_back(msg.str());
            continue;
        }

        std::string error;
        if (ApplySetting(key, val, tuning, spec, error))
        {
            report.applied.push_back(key);
            sawAnyValue = true;
        }
        else
        {
            std::ostringstream msg;
            msg << path << ":" << lineNumber << ": " << error;
            report.errors.push_back(msg.str());
        }
    }

    // A section header earns its keep even when every key under it is bad:
    // an unknown section is not an error on its own, but a file with no
    // values at all is almost certainly the wrong file.
    if (!sawAnyValue && report.errors.empty())
        report.errors.push_back(path + ": no settings found");

    return report.errors.empty();
}

bool LoadIni(const std::string& path, WaterTuning& tuning, AquascapeSpec& spec,
             IniReport& report)
{
    std::ifstream file(path);
    if (!file)
    {
        report.errors.push_back("cannot open " + path);
        return false;
    }

    std::ostringstream text;
    text << file.rdbuf();
    return LoadIniText(path, text.str(), tuning, spec, report);
}

bool WriteIni(const std::string& path, const WaterTuning& tuning,
              const AquascapeSpec& spec, std::string& error,
              const std::vector<std::string>& header)
{
    std::ofstream file(path);
    if (!file)
    {
        error = "cannot write " + path;
        return false;
    }

    if (!header.empty())
    {
        for (const std::string& line : header)
            file << "; " << line << "\n";
    }
    else
    {
        file << "; aquarium water settings\n"
                "; keys are the pixel-art-water-vfx demo's IDs; edit and the running\n"
                "; aquarium reloads this file automatically within half a second.\n"
                "; every value is clamped to its documented range; a bad line is\n"
                "; reported on the console and the previous value stays.\n";
    }

    // The file is the whole settings UI, so each section says what it moves.
    const auto blurb = [](const char* name) -> const char*
    {
        if (std::strcmp(name, "water") == 0)    return "body: tint strength, tide, refraction";
        if (std::strcmp(name, "waves") == 0)    return "two sine waves; edit offset_* with *_speed to keep the phase";
        if (std::strcmp(name, "caustics") == 0) return "light lattice, strongest on the sand";
        if (std::strcmp(name, "foam") == 0)     return "surface line and backwash";
        if (std::strcmp(name, "ripples") == 0)  return "rings from clicks, turns and the airstone";
        if (std::strcmp(name, "falls") == 0)    return "airstone bubble columns (the demo's waterfall terms)";
        if (std::strcmp(name, "night") == 0)    return "moon band";
        return nullptr;
    };

    const char* section = nullptr;
    const auto writeSectionTail = [&](const char* name)
    {
        if (name == nullptr) return;
        for (const IntDef& def : kInts)
            if (std::strcmp(def.section, name) == 0)
                file << def.key << " = " << (tuning.*(def.field)) << "\n";

        if (std::strcmp(name, "falls") != 0) return;
        file << "; bubbling outlets, one line each; x is a fraction of the tank\n"
                "; width and the column rises from the sand at that x. Add a line\n"
                "; to add a column, delete one to remove it. With no line at all\n"
                "; the single outlet sits on the stone's airstone.\n";
        if (tuning.bubbleEmitters.empty())
            file << "; bubble_column_1 = 0.16\n";
        for (size_t i = 0; i < tuning.bubbleEmitters.size(); ++i)
            file << "bubble_column_" << (i + 1) << " = "
                 << tuning.bubbleEmitters[i].x << "\n";
    };
    for (const FloatDef& def : kFloats)
    {
        if (section == nullptr || std::strcmp(section, def.section) != 0)
        {
            writeSectionTail(section);
            section = def.section;
            file << "\n[" << section << "]\n";
            if (const char* text = blurb(section)) file << "; " << text << "\n";
        }
        file << def.key << " = " << (tuning.*(def.field)) << "\n";
    }
    writeSectionTail(section);

    file << "\n[effects]\n"
            "; effect switches, then the HSV grades (0 = untouched)\n";
    for (const BoolDef& def : kBools)
        file << def.key << " = " << ((tuning.*(def.field)) ? 1 : 0) << "\n";

    for (const GradeDef& def : kGrades)
    {
        const Grade& g = tuning.*(def.field);
        file << def.key << "_hue = " << g.hue << "\n";
        file << def.key << "_saturation = " << g.sat << "\n";
        file << def.key << "_brightness = " << g.bright << "\n";
    }

    file << "\n[aquascape]\n"
            "; the sand, rocks, corals and water masks are built from these together\n";
    file << "seed = " << spec.seed << "\n";
    file << "sand_y = " << spec.sandY << "\n";
    file << "sand_curve = " << spec.sandCurve << "\n";
    file << "rock_count = " << spec.rockCount << "\n";
    file << "rock_scale = " << spec.rockScale << "\n";
    file << "front_rock = " << (spec.frontRock ? 1 : 0) << "\n";
    file << "coral_count = " << spec.coralCount << "\n";
    file << "margin = " << spec.margin << "\n";

    if (!file)
    {
        error = "write failed: " + path;
        return false;
    }
    return true;
}

} // namespace water
