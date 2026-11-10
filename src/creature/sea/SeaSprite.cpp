// ================================================================
// SEA CREATURES - sheet assembly and export
// Ported from fish-gen/fishgen/export.py (MIT).
// ================================================================
#include "creature/sea/SeaSprite.h"

#include "creature/CreatureSheetExport.h"
#include "creature/sea/SeaPixel.h"
#include "creature/sea/SeaRender.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <system_error>

namespace sea {
namespace {

std::string Hex(const SeaRgba& c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", (int)c.r, (int)c.g, (int)c.b);
    return std::string(buf);
}

const char* WingShapeId(WingShape s) {
    switch (s) {
        case WingShape::Round: return "round";
        case WingShape::Diamond: return "diamond";
        default: return "pointed";
    }
}

const char* LobeShapeId(LobeShape s) {
    switch (s) {
        case LobeShape::Horns: return "horns";
        case LobeShape::Round: return "round";
        default: return "none";
    }
}

/// The genes, written with the Python generator's field names so a same-seed
/// manifest can be diffed field by field against fish-gen's. Without this the
/// comparison can only say "the pictures differ", never which gene did it.
void WriteGenomeJson(std::ostream& out, const SeaGenome& g) {
    out.precision(12);   // keep the manifest exact enough to diff gene by gene
    const ColorGenes& c = g.colors;
    const auto fin = [&out](const char* name, const FinGenes& f, bool last) {
        out << "    \"" << name << "\": {\"present\": " << (f.present ? "true" : "false")
            << ", \"start\": " << f.start << ", \"end\": " << f.end
            << ", \"height\": " << f.height << ", \"shape\": \"" << FinShapeId(f.shape)
            << "\", \"sweep\": " << f.sweep << "}" << (last ? "\n" : ",\n");
    };
    const auto str = [&out](const char* name, const std::string& v, bool last) {
        out << "    \"" << name << "\": \"" << v << "\"" << (last ? "\n" : ",\n");
    };
    const auto num = [&out](const char* name, double v, bool last) {
        out << "    \"" << name << "\": " << v << (last ? "\n" : ",\n");
    };
    const auto flag = [&out](const char* name, bool v, bool last) {
        out << "    \"" << name << "\": " << (v ? "true" : "false") << (last ? "\n" : ",\n");
    };

    out << "  \"genome\": {\n";
    num("height", g.height, false);
    num("thickness", g.thickness, false);
    num("peak", g.peak, false);
    num("nose", g.nose, false);
    num("taper", g.taper, false);
    num("peduncle", g.peduncle, false);
    num("back_arch", g.backArch, false);
    num("eye_pos", g.eyePos, false);
    num("eye_height", g.eyeHeight, false);
    num("eye_size", g.eyeSize, false);
    str("eye_style", g.eyeStyle == EyeStyle::Dark ? "dark"
                    : g.eyeStyle == EyeStyle::None ? "none" : "fish", false);
    num("gill_slits", (double)g.gillSlits, false);
    num("beak", g.beak, false);
    num("melon", g.melon, false);
    num("neck", g.neck, false);
    num("flat_head", g.flatHead, false);
    num("hammer", g.hammer, false);
    num("mouth", g.mouth, false);
    num("mouth_height", g.mouthHeight, false);
    flag("teeth", g.teeth, false);
    num("scariness", g.scariness, false);
    str("tail_shape", TailId(g.tailShape), false);
    num("tail_length", g.tailLength, false);
    num("tail_spread", g.tailSpread, false);
    num("pectoral_size", g.pectoralSize, false);
    num("pectoral_flap", g.pectoralFlap, false);
    num("pectoral_droop", g.pectoralDroop, false);
    num("finlets", (double)g.finlets, false);
    num("swim_amplitude", g.swimAmplitude, false);
    num("wave_count", g.waveCount, false);
    flag("swim_vertical", g.swimVertical, false);
    num("pulse", g.pulse, false);
    num("turn_lag", g.turnLag, false);
    num("camera_tilt", g.cameraTilt, false);
    num("puff", g.puff, false);
    num("puff_spines", (double)g.puffSpines, false);
    flag("pleats", g.pleats, false);
    num("silver", g.silver, false);
    num("gloss", g.gloss, false);
    num("scales", g.scales, false);
    num("fin_alpha", g.finAlpha, false);
    num("billow", g.billow, false);
    str("iris_color", g.irisColor, false);
    num("scars", g.scars, false);
    num("knobs", g.knobs, false);
    num("length_cm", g.lengthCm, false);
    fin("dorsal", g.dorsal, false);
    fin("dorsal2", g.dorsal2, false);
    fin("anal", g.anal, false);
    fin("pelvic", g.pelvic, false);
    // The Python genome writes these too; without them the field-by-field diff
    // silently compares only 89 of 118 genes.
    num("seed", (double)g.seed, false);
    str("kind", KindId(g.kind), false);
    str("morph", g.morph, false);
    str("profile", g.profile, false);
    str("lobes", LobeShapeId(g.lobes), false);
    num("lobe_length", g.lobeLength, false);
    str("pectoral_shape", g.pectoralShape == PectoralShape::Blade ? "blade" : "lobe", false);
    if (g.hasTailAlpha) num("tail_alpha", g.tailAlpha, false);
    else out << "    \"tail_alpha\": null,\n";
    out << "    \"wings\": {\"present\": " << (g.wings.present ? "true" : "false")
        << ", \"start\": " << g.wings.start << ", \"end\": " << g.wings.end
        << ", \"span\": " << g.wings.span << ", \"sweep\": " << g.wings.sweep
        << ", \"shape\": \"" << WingShapeId(g.wings.shape)
        << "\", \"flap\": " << g.wings.flap << ", \"wave\": " << g.wings.wave
        << ", \"dihedral\": " << g.wings.dihedral << "},\n";
    out << "    \"arms\": {\"count\": " << g.arms.count
        << ", \"length\": " << g.arms.length << ", \"thickness\": " << g.arms.thickness
        << ", \"tentacles\": " << g.arms.tentacles
        << ", \"tentacle_length\": " << g.arms.tentacleLength
        << ", \"spread\": " << g.arms.spread << ", \"curl\": " << g.arms.curl << "},\n";
    out << "    \"spines\": {\"count\": " << g.spines.count
        << ", \"length\": " << g.spines.length << ", \"sway\": " << g.spines.sway
        << ", \"taper\": " << g.spines.taper << "},\n";
    out << "    \"colors\": {\n";
    num("hue", c.hue, false);
    num("saturation", c.saturation, false);
    num("value", c.value, false);
    num("fin_hue_shift", c.finHueShift, false);
    num("accent_hue_shift", c.accentHueShift, false);
    num("belly_lightness", c.bellyLightness, false);
    str("pattern", PatternId(c.pattern), false);
    num("pattern_scale", c.patternScale, false);
    num("pattern_offset", c.patternOffset, false);
    str("fin_color", c.finColor, false);
    str("accent_color", c.accentColor, false);
    str("accent2_color", c.accent2Color, false);
    str("tail_color", c.tailColor, false);
    str("fin_edge_color", c.finEdgeColor, false);
    str("accent_edge_color", c.accentEdgeColor, false);
    num("fin_tips", c.finTips, false);
    flag("pattern_on_fins", c.patternOnFins, false);
    flag("tail_body_color", c.tailBodyColor, false);
    flag("belly_pattern", c.bellyPattern, false);
    if (c.hasBellyLevel) num("belly_level", c.bellyLevel, true);
    else out << "    \"belly_level\": null\n";
    out << "    }\n";
    out << "  },\n";
}

/// "Manta ray" -> "manta_ray" for a file stem.
std::string Slug(const std::string& text) {
    std::string out;
    bool lastUnderscore = false;
    for (char c : text) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out.push_back(c);
            lastUnderscore = false;
        } else if (!lastUnderscore && !out.empty()) {
            out.push_back('_');
            lastUnderscore = true;
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

void Blit(std::vector<unsigned char>& dst, int dstW, int srcW, int srcH,
          const std::vector<unsigned char>& src, int atX, int atY) {
    for (int y = 0; y < srcH; y++) {
        const size_t d = ((size_t)(atY + y) * dstW + atX) * 4;
        const size_t s = (size_t)y * srcW * 4;
        for (int x = 0; x < srcW * 4; x++) dst[d + x] = src[s + x];
    }
}

} // namespace

std::string SeaSpriteId(const SeaGenome& genome) {
    const std::string species = Slug(genome.species);
    return std::string("sea_") + (species.empty() ? KindId(genome.kind) : species) + "_" +
           std::to_string(genome.seed);
}

bool BuildSeaSheet(const SeaGenome& genome, const SeaSpriteOptions& options, SeaSheet& out) {
    const int size = ClampI(options.size, 8, 512);
    const int smoothness = ClampI(options.smoothness, 1, 6);
    const int paletteSize = options.paletteSize > 0 ? ClampI(options.paletteSize, 2, 32)
                                                    : SeaPaletteSizeFor(size);
    const int clipCount = HasPuffClips(genome) ? ClipCount : 4;

    SeaRenderer renderer(genome, size);
    std::vector<std::vector<unsigned char>> frames[ClipCount];
    std::vector<std::vector<Vec3>> frameEyes[ClipCount];
    std::vector<std::vector<unsigned char>> frameParts[ClipCount];
    std::vector<std::vector<SeaPose>> clipPoses(ClipCount);
    std::vector<SeaPose> fitPoses;
    std::vector<unsigned char> aggregate;
    int maxFrames = 0;

    // Gather every pose first: the reference fits one scale to the whole sheet.
    for (int c = 0; c < clipCount; c++) {
        SeaClipPoses(c, smoothness, genome, clipPoses[c]);
        if (clipPoses[c].empty()) return false;
        fitPoses.insert(fitPoses.end(), clipPoses[c].begin(), clipPoses[c].end());
        maxFrames = Max(maxFrames, (int)clipPoses[c].size());
    }
    if (options.scaleOverride > 0.0) renderer.SetFitScale(options.scaleOverride);
    else renderer.SetFitPoses(fitPoses);

    for (int c = 0; c < clipCount; c++) {
        const std::vector<SeaPose>& poses = clipPoses[c];
        frames[c].reserve(poses.size());
        for (const SeaPose& pose : poses) {
            SeaRenderResult rr;
            renderer.Render(pose, rr);
            if ((int)rr.rgba.size() != size * size * 4) return false;
            // The reference's colour work happens before the palette is learned,
            // so both the palette and the indexed frames see the same pixels.
            // --style smooth skips both the pixel-art pre-pass and quantization.
            if (!options.smooth) {
                SeaPreparePixels(rr.rgba.data(), size, size);
                aggregate.insert(aggregate.end(), rr.rgba.begin(), rr.rgba.end());
            }
            frames[c].push_back(rr.rgba);
            frameEyes[c].push_back(rr.eyes);
            frameParts[c].push_back(rr.part);
        }
    }

    std::vector<SeaRgba> colors;
    if (!options.smooth) {
        colors = SeaLearnPalette(aggregate.data(), (int)(aggregate.size() / 4), 1, paletteSize);
        if (colors.empty()) return false;
    }

    const int width = maxFrames * size;
    const int height = clipCount * size;
    out.size = size;
    out.width = width;
    out.height = height;
    out.maxFrames = maxFrames;
    out.clipCount = clipCount;
    out.genome = genome;
    out.shading = renderer.Palette();
    out.colors = colors;
    out.rgba.assign((size_t)width * height * 4, 0);

    SeaPixelOptions pixel;
    pixel.paletteSize = paletteSize;
    pixel.outline = options.outline;
    std::vector<SeaRgba> sharedOutline;

    for (int c = 0; c < ClipCount; c++) {
        out.clips[c] = SeaSheetClip();
        out.clips[c].clip = c;
        out.clips[c].row = clipCount == ClipCount ? SeaClipRowPuff(c) : SeaClipRow(c);
        if (c >= clipCount) continue;
        const int n = (int)frames[c].size();
        const int fps = SeaClipBaseFps(c) * smoothness;
        out.clips[c].columns = n;
        out.clips[c].frames = n;
        out.clips[c].fps = (double)fps;
        out.clips[c].loops = SeaClipLoops(c);
        out.clips[c].cellW = size;
        out.clips[c].cellH = size;
        for (int f = 0; f < n; f++) {
            std::vector<unsigned char> px;
            // One outline-shade set for the whole sheet; the first frame seeds
            // it and every later frame reuses it (pixel.py outlines all frames
            // at once). Per-frame shades made the rim colour flicker and left
            // dozens of near-duplicate colours in the PNG.
            if (options.smooth) {
                px = frames[c][(size_t)f];
            } else {
                SeaPixelate(frames[c][(size_t)f].data(), size, size, colors, pixel, px,
                            sharedOutline.empty() ? nullptr : &sharedOutline,
                            sharedOutline.empty() ? &sharedOutline : nullptr,
                            &frameEyes[c][(size_t)f], &frameParts[c][(size_t)f],
                        genome.eyeStyle == EyeStyle::Dark);
            }
            Blit(out.rgba, width, size, size, px, f * size, out.clips[c].row * size);
        }
    }
    if (!sharedOutline.empty()) {
        for (const SeaRgba& c : sharedOutline) colors.push_back(c);
        out.colors = colors;
    }
    return true;
}

bool RenderSeaPortrait(const SeaGenome& genome, int size, std::vector<unsigned char>& rgba,
                       std::vector<SeaRgba>* paletteOut) {
    size = ClampI(size, 8, 512);
    SeaRenderer renderer(genome, size);
    SeaPose pose;
    pose.phase = 0.0;
    pose.yaw = 0.0;
    pose.pitch = SeaCameraPitch(genome);
    pose.turnLag = genome.turnLag;
    SeaRenderResult rr;
    renderer.Render(pose, rr);
    if ((int)rr.rgba.size() != size * size * 4) return false;
    SeaPreparePixels(rr.rgba.data(), size, size);

    const int paletteSize = SeaPaletteSizeFor(size);
    std::vector<SeaRgba> colors = SeaLearnPalette(rr.rgba.data(), size, size, paletteSize);
    if (colors.empty()) return false;
    SeaPixelOptions pixel;
    pixel.paletteSize = paletteSize;
    SeaPixelate(rr.rgba.data(), size, size, colors, pixel, rgba);
    if (paletteOut != nullptr) *paletteOut = colors;
    return (int)rgba.size() == size * size * 4;
}

bool WriteSeaPng(const unsigned char* rgba, int width, int height, const std::string& path) {
    if (rgba == nullptr || width <= 0 || height <= 0) return false;
    std::vector<unsigned char> png;
    if (!creature::EncodePng(rgba, width, height, png)) return false;

    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(png.data()), (std::streamsize)png.size());
    return file.good();
}

bool WriteSeaSheetPng(const SeaSheet& sheet, const std::string& path) {
    return WriteSeaPng(sheet.rgba.data(), sheet.width, sheet.height, path);
}

bool WriteSeaBundle(const std::string& dir, const SeaGenome& genome, const SeaSpriteOptions& options,
                    std::string& error) {
    error.clear();
    SeaSheet sheet;
    if (!BuildSeaSheet(genome, options, sheet)) {
        error = "BuildSeaSheet failed";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        error = "cannot create directory: " + dir;
        return false;
    }

    const std::string id = SeaSpriteId(genome);
    const std::string pngPath = (std::filesystem::path(dir) / (id + ".png")).string();
    const std::string jsonPath = (std::filesystem::path(dir) / (id + ".json")).string();
    if (!WriteSeaSheetPng(sheet, pngPath)) {
        error = "cannot write " + pngPath;
        return false;
    }

    std::ofstream json(jsonPath, std::ios::binary);
    if (!json.is_open()) {
        error = "cannot write " + jsonPath;
        return false;
    }
    json << "{\n";
    json << "  \"id\": \"" << id << "\",\n";
    json << "  \"generator\": \"sea\",\n";
    json << "  \"seed\": " << genome.seed << ",\n";
    json << "  \"kind\": \"" << KindId(genome.kind) << "\",\n";
    json << "  \"species\": \"" << genome.species << "\",\n";
    json << "  \"morph\": \"" << (genome.morph.empty() ? "" : genome.morph) << "\",\n";
    WriteGenomeJson(json, genome);
    json << "  \"length_cm\": " << genome.lengthCm << ",\n";
    json << "  \"cell_width\": " << sheet.size << ",\n";
    json << "  \"cell_height\": " << sheet.size << ",\n";
    json << "  \"sheet\": \"" << id << ".png\",\n";
    json << "  \"smoothness\": " << ClampI(options.smoothness, 1, 6) << ",\n";
    json << "  \"palette\": [";
    for (size_t i = 0; i < sheet.colors.size(); i++) {
        if (i > 0) json << ", ";
        json << "\"" << Hex(sheet.colors[i]) << "\"";
    }
    json << "],\n";
    json << "  \"animations\": [\n";
    for (int row = 0; row < sheet.clipCount; row++) {
        const SeaSheetClip* found = nullptr;
        for (int c = 0; c < ClipCount; c++) {
            if (sheet.clips[c].row == row) { found = &sheet.clips[c]; break; }
        }
        if (found == nullptr) continue;
        const SeaSheetClip& clip = *found;
        json << "    {\"name\": \"" << SeaClipName(clip.clip) << "\", \"row\": " << clip.row
             << ", \"columns\": " << clip.columns << ", \"frames\": " << clip.frames
             << ", \"fps\": " << clip.fps << ", \"loop\": " << (clip.loops ? "true" : "false")
             << ", \"cell_width\": " << clip.cellW << ", \"cell_height\": " << clip.cellH << "}";
        if (row + 1 < sheet.clipCount) json << ",";
        json << "\n";
    }
    json << "  ]\n}\n";
    if (!json.good()) {
        error = "cannot finish writing " + jsonPath;
        return false;
    }
    return true;
}

double SeaPixelsPerUnit(const SeaGenome& genome, double pxPerM, double minPx, double maxPx) {
    const SeaModel model(genome, 24);
    const double extent = Max(model.RealExtent(), 1e-6);
    const double want = pxPerM * genome.lengthCm / 100.0;
    return Clamp(want, minPx, maxPx) / extent;
}

bool WriteSeaFrames(const SeaSheet& sheet, const std::string& dir, const std::string& stem,
                    int scale, std::string& error) {
    scale = ClampI(scale, 1, 8);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) { error = "cannot create " + dir; return false; }
    const int s = sheet.size;
    for (int c = 0; c < sheet.clipCount; c++) {
        const SeaSheetClip& clip = sheet.clips[c];
        for (int f = 0; f < clip.frames; f++) {
            std::vector<unsigned char> cell((size_t)s * s * 4, 0);
            for (int y = 0; y < s; y++) {
                const size_t src = ((size_t)(clip.row * s + y) * sheet.width + (size_t)f * s) * 4;
                std::memcpy(cell.data() + (size_t)y * s * 4, sheet.rgba.data() + src,
                            (size_t)s * 4);
            }
            std::vector<unsigned char> out;
            const int w = s * scale, h = s * scale;
            if (scale == 1) {
                out = cell;
            } else {
                out.assign((size_t)w * h * 4, 0);
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++) {
                        const size_t d = ((size_t)y * w + x) * 4;
                        const size_t o = ((size_t)(y / scale) * s + (x / scale)) * 4;
                        out[d + 0] = cell[o + 0];
                        out[d + 1] = cell[o + 1];
                        out[d + 2] = cell[o + 2];
                        out[d + 3] = cell[o + 3];
                    }
                }
            }
            const std::string path =
                (std::filesystem::path(dir) /
                 (stem + "_" + SeaClipName(clip.clip) + "_" + std::to_string(f) + ".png")).string();
            if (!WriteSeaPng(out.data(), w, h, path)) {
                error = "cannot write " + path;
                return false;
            }
        }
    }
    return true;
}

bool WriteSeaGodot(const SeaSheet& sheet, const std::string& dir, const std::string& stem,
                   const std::string& resDir, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) { error = "cannot create " + dir; return false; }
    int totalFrames = 0;
    for (int c = 0; c < sheet.clipCount; c++) totalFrames += sheet.clips[c].frames;
    const std::string path = (std::filesystem::path(dir) / (stem + ".tres")).string();
    std::ofstream out(path, std::ios::binary);
    if (!out) { error = "cannot write " + path; return false; }
    const int s = sheet.size;
    out << "[gd_resource type=\"SpriteFrames\" load_steps=" << (2 + totalFrames) << " format=3]\n\n";
    out << "[ext_resource type=\"Texture2D\" path=\"" << resDir << "/" << stem << ".png\" id=\"1\"]\n\n";
    for (int c = 0; c < sheet.clipCount; c++) {
        const SeaSheetClip& clip = sheet.clips[c];
        for (int f = 0; f < clip.frames; f++) {
            out << "[sub_resource type=\"AtlasTexture\" id=\"AtlasTexture_" << c << "_" << f << "\"]\n";
            out << "atlas = ExtResource(\"1\")\n";
            out << "region = Rect2(" << f * s << ", " << clip.row * s << ", " << s << ", " << s << ")\n\n";
        }
    }
    out << "[resource]\nanimations = [";
    for (int c = 0; c < sheet.clipCount; c++) {
        const SeaSheetClip& clip = sheet.clips[c];
        out << "{\n\"frames\": [";
        for (int f = 0; f < clip.frames; f++) {
            out << "{\n\"duration\": 1.0,\n\"texture\": SubResource(\"AtlasTexture_" << c << "_" << f
                << "\")\n}" << (f + 1 < clip.frames ? "," : "");
        }
        out << "],\n\"loop\": " << (clip.loops ? "true" : "false")
            << ",\n\"name\": &\"" << SeaClipName(clip.clip) << "\",\n\"speed\": " << clip.fps << "\n}"
            << (c + 1 < sheet.clipCount ? "," : "");
    }
    out << "]\n";
    if (!out.good()) { error = "cannot finish writing " + path; return false; }
    return true;
}

} // namespace sea
