#pragma once
// ================================================================
// SEA CREATURES - named species presets
// ----------------------------------------------------------------
// Ported from fish-gen/fishgen/species.py and sizes.py (MIT).
//
// A preset asks its kind's generator for a random animal of the right
// body type / variety and then pins the genes that make the species.
// The seed still varies everything the preset leaves alone, so
// "Koi, seed 3" and "Koi, seed 4" are recognizably koi but not twins.
//
// Every preset's real adult length (disc width for rays, shell
// diameter for urchins) comes from the measured size database, so
// animals drawn to scale come out the right size next to each other.
// ================================================================

#include "creature/sea/SeaGenome.h"

#include <string>
#include <vector>

namespace sea {

struct SeaSpeciesInfo {
    const char* name = "";
    SeaKind kind = SeaKind::Fish;
    const char* shape = nullptr;  // fish archetype, or kind variant
};

constexpr const char* kRandomSpecies = "Random";

int SpeciesCount();
const SeaSpeciesInfo& SpeciesAt(int index);
/// nullptr for an unknown name.
const SeaSpeciesInfo* FindSpecies(const std::string& name);
/// Presets of one kind, after "Random" (which means a random animal of that kind).
std::vector<std::string> SpeciesForKind(SeaKind kind);
/// Names of a species' colour varieties (empty when it has only one).
std::vector<std::string> MorphsOf(const std::string& species);

/// Measured adult size in centimetres: typical and the range most adults fall in.
struct SeaSize {
    double typical = 30.0;
    double low = 20.0;
    double high = 40.0;
};
bool SizeOf(const std::string& species, SeaSize& out);

/// The animal for a species (or "Random") and a seed. `morph` names a colour
/// variety; nullptr lets the seed pick one.
SeaGenome MakeGenome(const std::string& species, u64 seed, const char* morph = nullptr,
                     SeaKind randomKind = SeaKind::Fish, const char* randomArchetype = nullptr);

} // namespace sea
