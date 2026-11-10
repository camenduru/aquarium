#pragma once
// ================================================================
// SEA CREATURES - animation clips
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/animation.py (MIT).
//
// A clip is just a list of SeaPose values; the geometry does the rest.
// Swim frames advance the travelling wave, turn frames sweep the head
// to the new heading and let the body follow, and puff frames swell a
// pufferfish out and back in. `smoothness` multiplies both the frame
// count and the fps, so the motion takes the same wall-clock time at a
// higher sampling rate (the reference's --smoothness 3).
// ================================================================

#include "creature/sea/SeaGeometry.h"

#include <vector>

namespace sea {

enum SeaClip {
    ClipSwimRight = 0,
    ClipSwimLeft,
    ClipTurnRightToLeft,
    ClipTurnLeftToRight,
    ClipPuffRight,
    ClipPuffLeft,
    ClipCount
};

const char* SeaClipName(int clip);
bool SeaClipLoops(int clip);
int SeaClipBaseFrames(int clip);
int SeaClipBaseFps(int clip);
/// Four-row sheet order: swim_right, turn_right_to_left, swim_left, turn_left_to_right.
int SeaClipRow(int clip);
/// Six-row puffer sheet order, exactly fish-gen's SHOWCASE_ORDER:
/// swim_right, puff_right, turn_right_to_left, swim_left, puff_left, turn_left_to_right.
int SeaClipRowPuff(int clip);
/// Only pufferfish carry the two puff clips; every other sheet has four rows.
bool HasPuffClips(const SeaGenome& genome);

/// Frame count and fps after the smoothness multiplier (clamped to 1..6).
void SeaTiming(int clip, int smoothness, int& frames, double& fps);

/// The poses of one clip, ready to render in order. The genome supplies the
/// camera tilt and the turn stiffness (fish-gen's `pitch + camera_tilt` and
/// `turn_lag`); the three-argument overload uses the reference defaults.
void SeaClipPoses(int clip, int smoothness, const SeaGenome& genome,
                  std::vector<SeaPose>& out);
void SeaClipPoses(int clip, int smoothness, std::vector<SeaPose>& out);

} // namespace sea
