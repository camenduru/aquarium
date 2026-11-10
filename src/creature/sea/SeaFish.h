#pragma once
// ================================================================
// SEA CREATURES - a self-contained procedural fish/sea-life system
// ----------------------------------------------------------------
// Port of C:/Users/PC/Documents/content/fish-gen (MIT) to engine-free
// C++20. Nothing here includes WickedEngine, so the standalone harness
// compiles and renders exactly what the game does.
//
//   genome     SeaGenome / SeaSpecies   - DNA, kind generators, 47 presets
//   palette    SeaPalette               - fixed colour slots from the genes
//   geometry   SeaModel / SeaPose       - lofted body, fins, wings, arms,
//                                         spines and the swim/turn spine
//   raster     SeaRaster                - z-buffered triangle rasterizer
//   render     SeaRenderer              - supersampled shading + markings
//   pixel      SeaPixelate              - OKLab k-means, outline, tidy-up
//   animation  SeaClipPoses             - swim / turn / puff clips
//   sprite     BuildSeaSheet            - packed PNG sheet + JSON + bundle
//
// Quick start:
//
//   sea::SeaGenome fish = sea::RandomFish(1234);
//   sea::SeaSpriteOptions opt;
//   opt.size = 64;
//   sea::SeaSheet sheet;
//   if (sea::BuildSeaSheet(fish, opt, sheet))
//       sea::WriteSeaSheetPng(sheet, "fish.png");
//
//   // or a named species:
//   sea::SeaGenome koi = sea::MakeGenome("Koi", 7);
//
// SeaRandom reproduces CPython's random.Random (MT19937, SHA-512 string
// seeding), so a seed/species pair draws the same individual as the
// Python tool, for fish presets and for all seven kinds.
// ================================================================

#include "creature/sea/SeaAnimation.h"
#include "creature/sea/SeaGenome.h"
#include "creature/sea/SeaGeometry.h"
#include "creature/sea/SeaGif.h"
#include "creature/sea/SeaMath.h"
#include "creature/sea/SeaPalette.h"
#include "creature/sea/SeaPixel.h"
#include "creature/sea/SeaRaster.h"
#include "creature/sea/SeaRender.h"
#include "creature/sea/SeaSpecies.h"
#include "creature/sea/SeaSprite.h"
