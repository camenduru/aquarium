// ================================================================
// AQUARIUM - save and load a whole tank
// ================================================================

#include "tankfile.h"

#include "creature/sea/SeaSpecies.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace tankfile {
namespace {

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

/// Round-trip exact: 17 significant digits are enough to read a double back
/// unchanged, so a saved x is the same x, not an approximation of it.
std::string Num(double v)
{
    char buffer[40] = {};
    std::snprintf(buffer, sizeof(buffer), "%.17g", v);
    return buffer;
}

bool ParseDouble(const std::string& text, double& out)
{
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || errno == ERANGE || !std::isfinite(v)) return false;
    while (end != nullptr && (*end == ' ' || *end == '\t')) ++end;
    if (end != nullptr && *end != '\0') return false;
    out = v;
    return true;
}

bool ParseInt(const std::string& text, long long& out)
{
    errno = 0;
    char* end = nullptr;
    const long long v = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || errno == ERANGE) return false;
    while (end != nullptr && (*end == ' ' || *end == '\t')) ++end;
    if (end != nullptr && *end != '\0') return false;
    out = v;
    return true;
}

bool ParseSeed(const std::string& text, sea::u64& out)
{
    errno = 0;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(text.c_str(), &end, 10);
    if (end == text.c_str() || errno == ERANGE) return false;
    while (end != nullptr && (*end == ' ' || *end == '\t')) ++end;
    if (end != nullptr && *end != '\0') return false;
    out = sea::u64(v);
    return true;
}

std::vector<std::string> SplitFields(const std::string& text)
{
    std::vector<std::string> fields;
    size_t start = 0;
    for (;;)
    {
        const size_t bar = text.find('|', start);
        if (bar == std::string::npos)
        {
            fields.push_back(Trim(text.substr(start)));
            return fields;
        }
        fields.push_back(Trim(text.substr(start, bar - start)));
        start = bar + 1;
    }
}

/// species|seed|x|base_y|speed|dir|bob_phase|bob_amp|bob_speed|alpha
bool ParseCreature(const std::string& value, TankCreature& out, std::string& error)
{
    const std::vector<std::string> f = SplitFields(value);
    if (f.size() != 10)
    {
        error = "expected 10 fields (species|seed|x|base_y|speed|dir|bob_phase|bob_amp|bob_speed|alpha)";
        return false;
    }

    // A named preset, the bare "Random" of an older save, or the weighted
    // kind spelling this build writes ("Random:Urchin").
    sea::SeaKind kind = sea::SeaKind::Fish;
    if (f[0].empty() ||
        (sea::FindSpecies(f[0]) == nullptr && f[0] != sea::kRandomSpecies &&
         !ParseRandomSpecies(f[0], kind)))
    {
        error = "unknown species '" + f[0] + "'";
        return false;
    }
    out.species = f[0];

    if (!ParseSeed(f[1], out.seed)) { error = "seed: expected an unsigned integer"; return false; }
    if (!ParseDouble(f[2], out.x) || std::abs(out.x) > 100000.0)
    { error = "x: expected a position"; return false; }
    if (!ParseDouble(f[3], out.baseY) || std::abs(out.baseY) > 100000.0)
    { error = "base_y: expected a position"; return false; }
    if (!ParseDouble(f[4], out.speed) || out.speed < 0.0 || out.speed > 400.0)
    { error = "speed: expected 0-400 px/s"; return false; }

    long long dir = 0;
    if (!ParseInt(f[5], dir) || (dir != 1 && dir != -1))
    { error = "dir: expected 1 or -1"; return false; }
    out.dir = int(dir);

    if (!ParseDouble(f[6], out.bobPhase) || std::abs(out.bobPhase) > 1000.0)
    { error = "bob_phase: expected a phase"; return false; }
    if (!ParseDouble(f[7], out.bobAmp) || out.bobAmp < 0.0 || out.bobAmp > 40.0)
    { error = "bob_amp: expected 0-40 px"; return false; }
    if (!ParseDouble(f[8], out.bobSpeed) || out.bobSpeed < 0.0 || out.bobSpeed > 8.0)
    { error = "bob_speed: expected 0-8"; return false; }

    long long alpha = 0;
    if (!ParseInt(f[9], alpha) || alpha < 0 || alpha > 255)
    { error = "alpha: expected 0-255"; return false; }
    out.alpha = int(alpha);
    return true;
}

} // namespace

bool Save(const std::string& path, const TankSave& save, std::string& error)
{
    // An empty tank is a tank: the water, the sand and the reef are still
    // worth keeping, so count = 0 with no fish lines is a valid save.
    if (int(save.creatures.size()) > kMaxTankFish)
    {
        error = "too many fish for a save file";
        return false;
    }

    const std::vector<std::string> header = {
        "aquarium tank save - water settings, aquascape and every fish",
        "load it from the right-click menu, Ctrl+Shift+L, or --load-tank <path>",
        "the settings sections are water.ini keys, so this file doubles as a preset",
    };
    if (!water::WriteIni(path, save.tuning, save.spec, error, header))
        return false;

    std::ofstream file(path, std::ios::app);
    if (!file)
    {
        error = "cannot append " + path;
        return false;
    }

    file << "\n[creatures]\n"
            "; one fish per line: species|seed|x|base_y|speed|dir|bob_phase|bob_amp|bob_speed|alpha\n";
    file << "count = " << save.creatures.size() << "\n";
    for (size_t i = 0; i < save.creatures.size(); ++i)
    {
        const TankCreature& c = save.creatures[i];
        file << "fish_" << (i + 1) << " = " << c.species << '|'
             << c.seed << '|'
             << Num(c.x) << '|' << Num(c.baseY) << '|' << Num(c.speed) << '|'
             << (c.dir >= 0 ? 1 : -1) << '|'
             << Num(c.bobPhase) << '|' << Num(c.bobAmp) << '|' << Num(c.bobSpeed) << '|'
             << std::clamp(c.alpha, 0, 255) << "\n";
    }

    // The window goes last, after [creatures], so everything above it stays a
    // valid water.ini: --water-preset rejects unknown keys and [window] is not
    // a water key. A headless run has no window, so it writes no section at
    // all and a later load leaves the window where the process put it.
    if (save.window.valid)
    {
        file << "\n[window]\n"
                "; where the tank sat and how big it was, so a load puts it back\n";
        file << "x = " << save.window.x << "\n";
        file << "y = " << save.window.y << "\n";
        file << "width = " << save.window.width << "\n";
        file << "height = " << save.window.height << "\n";
    }

    if (!file)
    {
        error = "write failed: " + path;
        return false;
    }
    return true;
}

bool Load(const std::string& path, TankSave& out, std::string& error)
{
    std::ifstream input(path);
    if (!input)
    {
        error = "cannot open " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    // Split the file at the [creatures] heading: the part above it is the
    // water.ini grammar, the part below it is the fish list.
    size_t lineStart = 0;
    size_t sectionStart = std::string::npos;
    while (lineStart <= text.size())
    {
        const size_t lineEnd = text.find('\n', lineStart);
        const size_t stop = lineEnd == std::string::npos ? text.size() : lineEnd;
        if (Lower(Trim(text.substr(lineStart, stop - lineStart))) == "[creatures]")
        {
            sectionStart = lineStart;
            break;
        }
        if (lineEnd == std::string::npos) break;
        lineStart = lineEnd + 1;
    }
    if (sectionStart == std::string::npos)
    {
        error = path + ": no [creatures] section";
        return false;
    }

    const size_t headingEnd = text.find('\n', sectionStart);
    const size_t bodyStart = headingEnd == std::string::npos ? text.size() : headingEnd + 1;

    // [window] is written after [creatures], so look for it before parsing the
    // fish: the fish loop only understands `key = value` and would report a
    // heading as a line without an equals sign.
    size_t windowStart = std::string::npos;
    size_t windowBody = std::string::npos;
    for (size_t at = bodyStart; at < text.size(); )
    {
        const size_t lineEnd = text.find('\n', at);
        const size_t stop = lineEnd == std::string::npos ? text.size() : lineEnd;
        if (Lower(Trim(text.substr(at, stop - at))) == "[window]")
        {
            windowStart = at;
            windowBody = lineEnd == std::string::npos ? text.size() : lineEnd + 1;
            break;
        }
        if (lineEnd == std::string::npos) break;
        at = lineEnd + 1;
    }

    const std::string head = text.substr(0, sectionStart);
    const std::string tail = text.substr(bodyStart, (windowStart == std::string::npos
                                                   ? text.size() : windowStart) - bodyStart);

    water::IniReport report;
    if (!water::LoadIniText(path, head, out.tuning, out.spec, report))
    {
        error = report.errors.empty() ? path + ": bad settings" : report.errors.front();
        return false;
    }

    out.creatures.clear();
    out.window = WindowPlace{};
    long long declared = -1;
    std::istringstream lines{ tail };
    std::string line;
    int lineNumber = 0;
    while (std::getline(lines, line))
    {
        ++lineNumber;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;

        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos)
        {
            std::ostringstream msg;
            msg << path << ": creature line " << lineNumber << ": expected key = value";
            error = msg.str();
            return false;
        }

        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string val = Trim(trimmed.substr(eq + 1));

        if (key == "count")
        {
            if (!ParseInt(val, declared) || declared < 0 || declared > kMaxTankFish)
            {
                error = path + ": count: expected 0-" + std::to_string(kMaxTankFish);
                return false;
            }
            continue;
        }
        if (key.rfind("fish_", 0) != 0)
        {
            error = path + ": unknown creature key: " + key;
            return false;
        }

        TankCreature creature;
        std::string why;
        if (!ParseCreature(val, creature, why))
        {
            std::ostringstream msg;
            msg << path << ": " << key << ": " << why;
            error = msg.str();
            return false;
        }
        out.creatures.push_back(std::move(creature));
    }

    // Empty is only valid when the file says so: count = 0 is an emptied tank,
    // while a save with no count and no fish lines is a broken file.
    if (out.creatures.empty() && declared != 0)
    {
        error = path + ": no fish in the save";
        return false;
    }
    if (declared >= 0 && declared != (long long)out.creatures.size())
    {
        std::ostringstream msg;
        msg << path << ": count = " << declared << " but " << out.creatures.size()
            << " fish line(s)";
        error = msg.str();
        return false;
    }

    // [window] is optional: a save from before the window was recorded has no
    // section, and a load then leaves the window alone. When the section is
    // there all four numbers are required, so a truncated file cannot half-place
    // the tank at a position it never had.
    if (windowBody != std::string::npos)
    {
        bool haveX = false, haveY = false, haveW = false, haveH = false;
        std::istringstream wlines{ text.substr(windowBody) };
        std::string wline;
        int wlineNumber = 0;
        while (std::getline(wlines, wline))
        {
            ++wlineNumber;
            const std::string trimmed = Trim(wline);
            if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;

            const size_t eq = trimmed.find('=');
            if (eq == std::string::npos)
            {
                std::ostringstream msg;
                msg << path << ": window line " << wlineNumber << ": expected key = value";
                error = msg.str();
                return false;
            }

            const std::string key = Trim(trimmed.substr(0, eq));
            const std::string val = Trim(trimmed.substr(eq + 1));
            const bool isPlace = key == "x" || key == "y";
            const bool isSize  = key == "width" || key == "height";
            if (!isPlace && !isSize)
            {
                error = path + ": unknown window key: " + key;
                return false;
            }

            long long v = 0;
            if (!ParseInt(val, v) || v < (isPlace ? -100000 : 1) || v > 100000)
            {
                error = path + ": " + key + (isSize ? ": expected 1-100000 px"
                                                    : ": expected -100000 to 100000");
                return false;
            }

            if (key == "x")      { out.window.x = int(v);      haveX = true; }
            else if (key == "y") { out.window.y = int(v);      haveY = true; }
            else if (key == "width")  { out.window.width = int(v);  haveW = true; }
            else                 { out.window.height = int(v); haveH = true; }
        }

        if (!(haveX && haveY && haveW && haveH))
        {
            error = path + ": [window] needs x, y, width and height";
            return false;
        }
        out.window.valid = true;
    }

    return true;
}

} // namespace tankfile
