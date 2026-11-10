#pragma once
// ================================================================
// SEA CREATURES - sprite sheets, portraits and export
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/export.py (MIT).
//
// One animal = one PNG sheet: every clip is a row, every frame a cell,
// all cells at the requested pixel size. The whole sheet shares ONE
// palette learned across all frames, so a swimming fish does not
// shimmer between two almost-equal blues from frame to frame.
//
// PNG writing is delegated to creature::EncodePng so the subsystem
// stays dependency-free and ships the same bytes the game's other
// procedural creatures do.
// ================================================================

#include "creature/sea/SeaAnimation.h"
#include "creature/sea/SeaGeometry.h"
#include "creature/sea/SeaPalette.h"

#include <string>
#include <vector>

namespace sea {

struct SeaSpriteOptions {
    int size = 64;          // cell size in pixels
    int smoothness = 3;     // multiplies frames and fps (1..6)
    int paletteSize = 0;    // 0 = derive from `size`
    bool outline = true;
    bool smooth = false;    // export.py's --style smooth (no pixel-art pass)
    double scaleOverride = 0.0;  // > 0 = real-size render (animation.px_per_unit)
};

struct SeaSheetClip {
    int clip = 0;
    int row = 0;
    int columns = 0;
    int frames = 0;
    double fps = 10.0;
    bool loops = false;
    int cellW = 0, cellH = 0;
};

struct SeaSheet {
    int size = 64;
    int width = 0, height = 0;
    int maxFrames = 0;
    int clipCount = 0;   // 4 normally, 6 for pufferfish
    SeaGenome genome;
    SeaPalette shading;                 // slots used by the renderer
    std::vector<SeaRgba> colors;        // pixel-art palette (shared by all frames)
    std::vector<unsigned char> rgba;    // width*height*4, straight alpha
    SeaSheetClip clips[ClipCount];
};

/// Render, quantize and pack every clip of one animal.
bool BuildSeaSheet(const SeaGenome& genome, const SeaSpriteOptions& options, SeaSheet& out);

/// One idle portrait (swim pose 0), quantized. `paletteOut` may be null.
bool RenderSeaPortrait(const SeaGenome& genome, int size, std::vector<unsigned char>& rgba,
                       std::vector<SeaRgba>* paletteOut = nullptr);

/// A stable file stem such as "sea_shark_12345".
std::string SeaSpriteId(const SeaGenome& genome);

/// Write a raw RGBA buffer as a PNG. False on a bad size or an unwritable path.
bool WriteSeaPng(const unsigned char* rgba, int width, int height, const std::string& path);

/// Write a sheet as a PNG.
bool WriteSeaSheetPng(const SeaSheet& sheet, const std::string& path);

/// animation.px_per_unit(): pixels per body length for a real-size render.
double SeaPixelsPerUnit(const SeaGenome& genome, double pxPerM = 16.0,
                        double minPx = 16.0, double maxPx = 128.0);

/// export.export_frames(): write every cell as a loose PNG.
bool WriteSeaFrames(const SeaSheet& sheet, const std::string& dir, const std::string& stem,
                    int scale, std::string& error);

/// export.export_gifs(): write one GIF per clip plus a showcase GIF.
bool WriteSeaGifs(const SeaSheet& sheet, const std::string& dir, int scale,
                  const std::string& background, std::string& error);

/// export.export_godot(): write a Godot 4 SpriteFrames .tres beside the sheet.
bool WriteSeaGodot(const SeaSheet& sheet, const std::string& dir, const std::string& stem,
                   const std::string& resDir, std::string& error);

/// Build the sheet and write `<dir>/<id>.png` plus `<dir>/<id>.json`.
bool WriteSeaBundle(const std::string& dir, const SeaGenome& genome, const SeaSpriteOptions& options,
                    std::string& error);

} // namespace sea
