#pragma once
// ================================================================
// AQUARIUM - save and load a whole tank
// ----------------------------------------------------------------
// One text file holds everything that makes this aquarium this aquarium:
//
//   [water] [waves] [caustics] [foam] [ripples] [falls] [night] [effects]
//       the water.ini keys, so the tint, the bubbles and the night mood
//       come back exactly as they were
//   [aquascape]
//       the seed and the shape knobs, so the sand, the rocks and every
//       coral are regenerated from the same numbers
//   [creatures]
//       one line per fish: species, genome seed and the agent state, so
//       the same animals reappear at the same places
//
// The format is the project's `key = value` INI (no JSON, no binary blob),
// and the settings part is a valid water.ini, so a saved tank can also be
// handed to --water-preset.
// ================================================================

#include "tank.h"
#include "water/WaterTuning.h"

#include <string>
#include <vector>

namespace tankfile {

constexpr const char* kDefaultName = "tank.ini";

struct TankSave
{
    water::WaterTuning        tuning;
    water::AquascapeSpec      spec;
    std::vector<TankCreature> creatures;
};

/// Write `save` to `path`. False on I/O failure or a creature list that cannot
/// be represented (empty, or larger than kMaxTankFish).
bool Save(const std::string& path, const TankSave& save, std::string& error);

/// Read `path` back. Settings are applied all-or-nothing: a single bad line
/// fails the load, so a half-written file never leaves a half-built tank.
bool Load(const std::string& path, TankSave& out, std::string& error);

} // namespace tankfile
