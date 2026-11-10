// ================================================================
// SEA CREATURES - animation clips
// Ported from fish-gen/fishgen/animation.py (MIT).
// ================================================================
#include "creature/sea/SeaAnimation.h"

#include <algorithm>
#include <cmath>

namespace sea {
namespace {

struct ClipInfo {
    const char* name;
    int frames;
    int fps;
    bool loops;
};

const ClipInfo kClips[ClipCount] = {
    {"swim_right", 8, 10, true},
    {"swim_left", 8, 10, true},
    {"turn_right_to_left", 10, 10, false},
    {"turn_left_to_right", 10, 10, false},
    {"puff_right", 24, 10, false},   // swim_frames * 3, like animation.py
    {"puff_left", 24, 10, false},
};

} // namespace

const char* SeaClipName(int clip) {
    if (clip < 0 || clip >= ClipCount) return "";
    return kClips[clip].name;
}

bool SeaClipLoops(int clip) {
    if (clip < 0 || clip >= ClipCount) return false;
    return kClips[clip].loops;
}

int SeaClipBaseFrames(int clip) {
    if (clip < 0 || clip >= ClipCount) return 0;
    return kClips[clip].frames;
}

int SeaClipBaseFps(int clip) {
    if (clip < 0 || clip >= ClipCount) return 0;
    return kClips[clip].fps;
}

int SeaClipRow(int clip) {
    switch (clip) {
        case ClipSwimRight: return 0;
        case ClipTurnRightToLeft: return 1;
        case ClipSwimLeft: return 2;
        case ClipTurnLeftToRight: return 3;
        case ClipPuffRight: return 4;
        case ClipPuffLeft: return 5;
        default: return 0;
    }
}

int SeaClipRowPuff(int clip) {
    // fish-gen's SHOWCASE_ORDER, which its exporter uses for the six-row sheet.
    switch (clip) {
        case ClipSwimRight: return 0;
        case ClipPuffRight: return 1;
        case ClipTurnRightToLeft: return 2;
        case ClipSwimLeft: return 3;
        case ClipPuffLeft: return 4;
        case ClipTurnLeftToRight: return 5;
        default: return 0;
    }
}

bool HasPuffClips(const SeaGenome& genome) {
    return genome.puff > 0.0 || genome.puffSpines > 0;
}

void SeaTiming(int clip, int smoothness, int& frames, double& fps) {
    smoothness = ClampI(smoothness, 1, 6);
    frames = SeaClipBaseFrames(clip) * smoothness;
    fps = (double)SeaClipBaseFps(clip) * smoothness;
}

void SeaClipPoses(int clip, int smoothness, std::vector<SeaPose>& out) {
    const SeaGenome defaults;   // 0.25 rad pitch, 0.9 turn lag
    SeaClipPoses(clip, smoothness, defaults, out);
}

void SeaClipPoses(int clip, int smoothness, const SeaGenome& genome,
                  std::vector<SeaPose>& out) {
    out.clear();
    int frames = 0;
    double fps = 0;
    SeaTiming(clip, smoothness, frames, fps);
    if (frames <= 0) return;
    out.reserve((size_t)frames);

    for (int i = 0; i < frames; i++) {
        SeaPose p;
        // animation.py:187 passes pitch + genome.camera_tilt and genome.turn_lag
        // into build_animations for EVERY clip. Rays, urchins and hammerheads
        // set those genes; hardcoding 0.25 / 0.9 here made them dead.
        p.pitch = SeaCameraPitch(genome);
        p.turnLag = genome.turnLag;

        switch (clip) {
            case ClipSwimRight:
            case ClipSwimLeft:
                // Loops: the last frame is just before the first.
                p.phase = (double)i / frames;
                p.yaw = clip == ClipSwimRight ? 0.0 : kPi;
                p.amplitude = 1.0;
                break;

            case ClipTurnRightToLeft:
            case ClipTurnLeftToRight: {
                // animation.py's turn_poses(): the heading progress is a
                // separate `turn` field; the tail keeps beating at the swim
                // rate and lands on a whole number of cycles.
                const int swimFrames = SeaClipBaseFrames(ClipSwimRight) * ClampI(smoothness, 1, 6);
                const int cycles = Max(1, (int)std::lround((double)frames / Max(1, swimFrames)));
                const double t = (i + 0.5) / frames;
                const double mid = std::sin(kPi * t);
                p.phase = frames > 0 ? std::fmod((double)(i * cycles) / frames, 1.0) : 0.0;
                p.turn = t;
                p.turning = true;
                p.turnFrom = clip == ClipTurnRightToLeft ? 0.0 : kPi;
                p.turnTo = clip == ClipTurnRightToLeft ? kPi : 0.0;
                p.yaw = p.turnFrom;
                p.amplitude = 1.0 - 0.6 * mid;
                break;
            }

            case ClipPuffRight:
            case ClipPuffLeft: {
                // animation.py: t = j / frames (not i/(frames-1)); swell fast,
                // hold, shrink back, and keep the tail beating at the swim rate.
                const double t = frames > 0 ? (double)i / frames : 1.0;
                const double up = Min(1.0, t / 0.22);
                const double down = Min(1.0, Max(0.0, (1.0 - t) / 0.3));
                double k = Min(up, down);
                k = k * k * (3.0 - 2.0 * k);
                p.inflate = k;
                const int swimFrames = SeaClipBaseFrames(ClipSwimRight) * ClampI(smoothness, 1, 6);
                p.phase = swimFrames > 0 ? std::fmod((double)i / swimFrames, 1.0) : 0.0;
                p.yaw = clip == ClipPuffRight ? 0.0 : kPi;
                p.amplitude = 1.0 - 0.85 * k;
                break;
            }

            default:
                break;
        }
        out.push_back(p);
    }
}

} // namespace sea
