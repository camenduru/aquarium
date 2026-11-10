#pragma once
// ================================================================
// SEA CREATURES - 3D model and posing
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/geometry.py (MIT).
//
// Rest ("local") space uses three coordinates:
//   a  distance along the spine, 0 = nose, 1 = base of the tail
//   y  up
//   z  sideways (out of the animal's right flank)
//
// A SeaPose bends the spine (the travelling swim wave) and points it
// along a heading. Turns are follow-the-leader: the head swings round
// first and each point along the spine follows the same path a moment
// later, so the body folds into a U instead of spinning rigidly.
// The posed mesh is then projected orthographically with a slight
// downward camera pitch, so lateral motion reads in a side view.
//
// Other animals reuse the same spine: whales and dolphins bend it up
// and down, rays and squid add flapping side "wings", cephalopods
// trail tube arms off the end, and urchins carry radial spines drawn
// as camera-space segments.
// ================================================================

#include "creature/sea/SeaGenome.h"

#include <vector>

namespace sea {

// ---------------------------------------------------------------- part ids
enum SeaPart {
    PartBody = 1,
    PartDorsal,
    PartAnal,
    PartCaudal,
    PartPelvicNear,
    PartPelvicFar,
    PartPectoralNear,
    PartPectoralFar,
    PartWingNear,   // ray wings, squid fins, hammerhead head
    PartWingFar,
    PartArm,
    PartLobe,       // manta cephalic fins, cownose lobes
    PartMouth,
    PartTeeth
};

inline bool IsFinPart(int part) {
    return part == PartDorsal || part == PartAnal || part == PartCaudal ||
           part == PartPelvicNear || part == PartPelvicFar ||
           part == PartPectoralNear || part == PartPectoralFar;
}
inline bool IsWingPart(int part) { return part == PartWingNear || part == PartWingFar; }
inline bool IsSkinPart(int part) {
    return part == PartBody || part == PartWingNear || part == PartWingFar ||
           part == PartArm || part == PartLobe;
}

// ---------------------------------------------------------------- pose
struct SeaPose {
    double phase = 0.0;       // swim cycle, 0..1 (wraps)
    double turn = 0.0;        // turn progress, 0..1 (separate from the tail beat)
    double yaw = 0.0;         // head heading: 0 = right, pi = left, pi/2 = toward viewer
    bool turning = false;
    double turnFrom = 0.0;
    double turnTo = 0.0;
    double turnLag = 0.9;     // how far the tail trails the head
    double amplitude = 1.0;   // multiplier on the swim wave
    double pitch = 0.25;      // camera tilt (radians)
    double inflate = 0.0;     // pufferfish: 0 = normal .. 1 = fully puffed
};

/// Heading of the spine point `along` (0 = nose, 1 = tail tip) at turn progress.
double TurnHeading(double progress, double along, double start, double end, double lag);

/// Camera pitch for this animal: the reference's 0.25 rad plus the genome's
/// own tilt (rays, urchins and hammerheads ask for a steeper view).
/// animation.py passes `pitch + genome.camera_tilt` into build_animations.
inline double SeaCameraPitch(const SeaGenome& genome) {
    return 0.25 + genome.cameraTilt;
}

/// One visible eye, posed into camera space.
struct SeaPosedEye {
    Vec3 center;
    Vec3 normal;
    double radius = 0.0;   // body lengths
};

/// A posed model: camera-space vertices, their normals, eyes and spine segments.
struct SeaPosed {
    std::vector<Vec3> cam;
    std::vector<Vec3> normals;
    std::vector<SeaPosedEye> eyes;
    std::vector<Vec3> spineLines;   // pairs: base, tip
};

/// render.py's geometry.Jaw: a gaping mouth in rest space.
struct SeaJaw {
    double a0 = 0, a1 = 0, height = 0, rise = 0.12, band = 0.16, gape = 0;
    double Front() const { return a0 > 0.04 ? a0 - 0.04 : 0.0; }
    double T(double a) const { return Clamp((a - a0) / Max(a1 - a0, 1e-9), 0.0, 1.0); }
    double Hinge(double a) const {
        double h = Clamp((a1 - a) / Max(a1 - a0, 1e-9), 0.0, 1.0);
        if (Front() > 0) h *= Clamp((a - Front()) / Max(a0 - Front(), 1e-9), 0.0, 1.0);
        return h;
    }
    double Line(double a) const { return height + rise * std::pow(T(a), 1.3); }
};

struct SeaTooth {
    double x = 0, side = 1, per = 0;
    bool upper = true;
    int index = 0;
};

/// The rest-space triangle mesh built once from a genome.
struct SeaMesh {
    std::vector<Vec3> verts;
    std::vector<Vec4> attrs;    // (u, v, w, extra)
    std::vector<Vec3> axis;     // tube centreline point per vertex
    std::vector<int> tris;
    std::vector<int> triPart;

    struct Patch {
        int start = 0, rows = 0, cols = 0;
        int kind = 0;   // 0 tube (wraps round), 1 sheet, 2 sheet_flip
    };
    std::vector<Patch> patches;
};

class SeaModel {
public:
    explicit SeaModel(const SeaGenome& genome, int detail = 32);

    const SeaGenome& Genome() const { return g_; }
    const SeaMesh& Mesh() const { return mesh_; }

    /// Build the posed mesh in camera space.
    void Pose(const SeaPose& pose, SeaPosed& out) const;

    // Rest-space body profile (all lengths in body lengths).
    double Top(double a) const;
    double Bottom(double a) const;
    double HalfWidth(double a) const;
    double HeightProfile(double a) const;

    double TailEnd() const { return tailEnd_; }
    bool HasJaw() const { return hasJaw_; }
    double CenterA() const { return centerA_; }
    /// The model size the genome's real length measures (body lengths).
    double RealExtent() const;

private:
    struct PairedPatch {
        int start = 0, count = 0, part = 0;
        std::vector<double> out;
    };
    struct WingPatch {
        int start = 0, rows = 0, cols = 0;
        WingGenes genes;
        double side = 1.0;
        bool flaps = true;
    };
    struct ArmPatch {
        int start = 0, rows = 0, cols = 0;
        bool tentacle = false;
        double theta = 0.0, phi = 0.0, length = 0.5, thick = 0.05;
        double a0 = 0.96, rRoot = 0.1;
    };
    struct EyeLocal {
        double a = 0.1, y = 0.0, z = 0.0, radius = 0.05;
    };

    int AddGrid(const std::vector<Vec3>& verts, const std::vector<Vec4>& attrs, int rows, int cols,
                int part, bool wrap, bool flip, const std::vector<Vec3>* axis);

    void Build();
    void BuildMedianFin(const FinGenes& fin, int part, double sign);
    void BuildCaudal();
    void BuildPairedFin(int part, double side, double rootA, double rootY, double length,
                        double droop, double splay, bool blade);
    void BuildWings(const WingGenes& wing, bool flaps, int partNear, int partFar);
    void BuildWingVerts(const WingGenes& wing, double side, double phase, double amplitude,
                        std::vector<Vec3>& out) const;
    void BuildArms();
    void BuildArmVerts(const ArmPatch& patch, double phase, double amplitude,
                       std::vector<Vec3>& out, std::vector<Vec3>* axis) const;
    void BuildLobes();
    void BuildSpineLines(double phase, double inflate, std::vector<Vec3>& out) const;
    void BuildMouth();
    std::vector<double> JawRow(double ai) const;
    Vec3 MouthRim(double a, double s, double side) const;
    std::vector<Vec3> TeethPose(const SeaPose& pose) const;

    double Neck(double a) const;
    double HeadLift(double a) const;
    void Deform(std::vector<Vec3>& verts, const SeaPose& pose) const;
    void Spine(std::vector<double>& a, std::vector<Vec3>& pos, std::vector<double>& heading,
               const SeaPose& pose) const;

    SeaGenome g_;
    SeaMesh mesh_;
    int rings_ = 24, ringVerts_ = 12, finCols_ = 8, finRows_ = 4, detail_ = 32;
    double centerA_ = 0.5, tailEnd_ = 1.0;
    bool hasJaw_ = false;
    SeaJaw jaw_;
    std::vector<SeaTooth> teeth_;
    int teethStart_ = -1;
    int teethCount_ = 0;
    std::vector<EyeLocal> eyes_;
    std::vector<PairedPatch> paired_;
    std::vector<WingPatch> wingPatches_;
    std::vector<ArmPatch> armPatches_;
};

} // namespace sea
