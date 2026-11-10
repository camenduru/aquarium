#pragma once
// ================================================================
// SEA CREATURES - GIF89a writer
// ----------------------------------------------------------------
// Port of export.py's save_gif()/export_gifs() (MIT). The sprite frames
// already share one palette, so they go into the GIF exactly; the writer
// only needs a global colour table, transparent index 0, one Netscape
// loop block, and an LZW encoder.
// ================================================================

#include "creature/sea/SeaPalette.h"

#include <string>
#include <vector>

namespace sea {

/// Write RGBA frames as an animated GIF. `background` is "#rrggbb" or empty
/// (transparent). `scale` upsamples each cell with nearest-neighbour.
bool WriteSeaGif(const std::vector<std::vector<unsigned char>>& frames, int width, int height,
                 const std::vector<SeaRgba>& palette, double fps, int scale,
                 const std::string& path, const std::string& background, std::string& error);

} // namespace sea
