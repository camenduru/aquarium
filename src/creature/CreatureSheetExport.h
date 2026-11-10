#pragma once
// ================================================================
// AQUARIUM - PNG writer for the sea module
// ----------------------------------------------------------------
// The sea generator (src/creature/sea) is a verbatim copy of
// RoguelikeGame's module, which delegates its PNG writing to
// creature::EncodePng from the game's CreatureSheetExport. Copying that
// whole translation unit would drag in the unrelated creature-preset
// system, so only the encoder itself is reproduced here - same stored-
// deflate zlib stream, same straight-alpha RGBA, no external libraries.
// ================================================================

#include <string>
#include <vector>

namespace creature {

/// Encode 8-bit straight-alpha RGBA pixels as a PNG. False on a bad size.
bool EncodePng(const unsigned char* rgba, int width, int height,
               std::vector<unsigned char>& out);

/// Encode and write to `path`, creating the parent directory if needed.
bool WritePngFile(const unsigned char* rgba, int width, int height,
                  const std::string& path);

} // namespace creature
