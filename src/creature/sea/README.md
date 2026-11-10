# Sea creatures

A self-contained, engine-free C++20 port of the procedural fish generator in
`C:/Users/PC/Documents/content/fish-gen` (MIT). It generates an animal from a
seed, builds a 3D mesh, rasterizes and shades it, turns the render into pixel
art, animates it, and packs a PNG sprite sheet plus JSON metadata.

Nothing in this folder includes WickedEngine, so `tools/sea_fish_test.cpp`
compiles and renders exactly the same code the game does. `CMakeLists.txt`
uses `file(GLOB_RECURSE ... src/*.cpp)`, so the modules are part of the game
target. (A plain `GLOB` with `src/**/*.cpp` is *not* recursive in CMake and
silently skipped this folder; `tools/check_sea_wiring.py` now checks that
every source under `src/` reaches the target.)

## Pipeline

| Module | What it does |
| --- | --- |
| `SeaMath.h` | Vec2/3/4, easing, splitmix64 `Rng`, value noise/FBM, HSV/hex helpers, Bezier, polyline resampling |
| `SeaNumpyRandom.h` | numpy `default_rng()` (PCG64 + SeedSequence) for the arm/spike jitter |
| `SeaGenome.h/.cpp` | `SeaGenome`, seven kinds, every gene, random fish/kind generators, `Individualize` |
| `SeaPalette.h/.cpp` | Fixed colour slots derived from the colour genes |
| `SeaSpecies.h/.cpp` | 47 named presets + `Random`, eight colour-morph tables, measured sizes, `MakeGenome` |
| `SeaGeometry.h/.cpp` | Lofted body, median/paired/caudal fins, wings, arms, lobes, spines, eyes; `SeaPose` and the swim/turn/vertical spine |
| `SeaRaster.h/.cpp` | Orthographic z-buffered triangle rasterizer with barycentrics |
| `SeaRender.h/.cpp` | Supersampled shading: key + hemisphere light, Blinn gloss, rim, countershading, fin translucency, all 26 markings, eyes, spines |
| `SeaPixel.h/.cpp` | OKLab k-means palette, hard alpha, despeckle, tidy, silhouette outline |
| `SeaAnimation.h/.cpp` | `swim_left/right`, `turn_left_to_right/right_to_left`, `puff_left/right`; smoothness scales frames and fps |
| `SeaSprite.h/.cpp` | One shared palette per sheet, one row per clip, PNG + JSON + bundle writer |
| `SeaFish.h` | Umbrella header |

Sheet rows match the reference. A non-puffer sheet is four rows: `swim_right`,
`turn_right_to_left`, `swim_left`, `turn_left_to_right`. A pufferfish sheet adds
the two puff clips in fish-gen's `SHOWCASE_ORDER`: `swim_right`, `puff_right`,
`turn_right_to_left`, `swim_left`, `puff_left`, `turn_left_to_right`.

Every frame shares one camera scale and centre, computed from the whole pose set
before rendering (the reference's `fit_poses`), so a turn foreshortens instead of
zooming. The orca marking follows fish-gen's curved white chin/flank, oval eye
patch and streaky grey saddle, and keeps its white paint bright in shadow.

## Usage

```cpp
#include "creature/sea/SeaFish.h"

sea::SeaGenome fish = sea::RandomFish(1234);          // or MakeGenome("Koi", 7)
sea::SeaSpriteOptions options;
options.size = 64;          // cell size
options.smoothness = 3;     // 8 swim frames -> 24, 10 fps -> 30 fps

sea::SeaSheet sheet;
if (sea::BuildSeaSheet(fish, options, sheet))
    sea::WriteSeaSheetPng(sheet, "koi.png");

std::string error;
sea::WriteSeaBundle("assets/sea", fish, options, error);   // <id>.png + <id>.json
```

`SeaSpriteId` produces a stable stem such as `sea_koi_7` (or
`sea_great_white_shark_7`); a random animal falls back to `sea_<kind>_<seed>`.

### Species and morphs

```cpp
const int n = sea::SpeciesCount();                    // 48 entries: 47 presets + "Random"
for (const std::string& name : sea::SpeciesForKind(sea::SeaKind::Shark)) { ... }
for (const std::string& morph : sea::MorphsOf("Koi")) { ... }

sea::SeaSize size;
sea::SizeOf("Manta ray", size);                       // typical/low/high cm
sea::SeaGenome g = sea::MakeGenome("Koi", 3, "Kohaku");
```

### Posing directly

```cpp
sea::SeaModel model(genome, 96);          // detail controls tessellation
sea::SeaPose pose;
pose.phase = 0.25;                        // swim cycle 0..1
pose.yaw = 0.0;                           // 0 = right, pi = left
sea::SeaPosed posed;
model.Pose(pose, posed);                  // camera-space vertices, normals, eyes, spines
```

## Building and testing

```bat
tools\build_sea_test.bat
bin\sea_fish_test.exe
```

The harness checks the 47 presets, all seven kinds, sheet structure, per-frame
coverage and margins, seeded determinism, animation timing/pose sweeps, palette
limits and hard alpha, outline growth, PNG chunk CRCs, bundle JSON, and the
special genomes. It also guards the genome-driven camera tilt / turn lag and
multi-pass despeckle, so those wiring bugs cannot come back silently.

`python tools/check_sea_wiring.py` is the build-side guard: it evaluates the
real `file(GLOB...)` expressions from `CMakeLists.txt` with CMake and fails if
any `.cpp` under `src/` is missing from the game target.

The harness writes previews to `bin/sea_preview/`:

- `contact_sheet.png` – twelve distinctive species at 64 px
- `koi_128.png` – one 128 px portrait
- `sheet_test.png` – a small six-row animation sheet
- `bundle/` – a full PNG + JSON bundle

## Comparing against the reference

The bundle JSON carries the genome under `"genome"`, using the Python
generator's own field names, so a same-seed run can be diffed field by field
instead of eyeballed:

```bat
python -m fishgen generate --species Clownfish --seed 7 --size 128 --out out\py --formats sheet --flat
bin\sea_generate.exe Clownfish 7 128 3 out\cpp
python tools\compare_sea_sheets.py --py-sheet out\py\clownfish_128px_7.png ^
    --py-json out\py\clownfish_128px_7.json ^
    --cpp-sheet out\cpp\sea_clownfish_7.png --cpp-json out\cpp\sea_clownfish_7.json ^
    --out out\cmp
```

It writes `metrics.json` (per-clip silhouette IoU raw and aligned, colour error,
coverage, genome diff) plus a side-by-side strip, an eye crop, a diff heatmap
and the swim row. Clownfish seed 7 at 128 px, measured:

| | reference | port |
|---|---|---|
| sheet | 3840×512, 24/30 frames | identical |
| genome | — | **0 of 118 genes differ** |
| silhouette IoU | — | **0.999** (0.999 on every clip) |
| mean colour error | — | **15.9 / 255** |
| coverage | 26.3 % | 26.3 % |
| distinct colours | 27 | 27 |
| mean RGB | 152, 91, 56 | 149, 90, 57 |

The silhouette, coverage and palette count now match to within a pixel. The
black rims around the clownfish bars, the translucent fin membranes and rays,
the shadow map and SSAO, and the 2 px pixel-art fit margin are all ported; the
remaining 15.9/255 colour error is tone-map/palette-cluster rounding.

Same-seed checks at 128 px, seed 7: pufferfish IoU **0.986** / colour
**14.3**, octopus IoU **0.975** / colour **17.1**.

## Differences from the Python original

- **RNG**: `SeaRandom` reproduces CPython's `random.Random` bit for bit
  (MT19937 seeded through `init_by_array`, `genrand_res53`, `_randbelow`,
  `bisect_right` for `choices`), including SHA-512 string seeding. A seed gives
  the same individual the Python tool draws. `Individualize` and the morph and
  size picks use the same draw order, so the whole 118-field genome matches.
  Anything that draws two randoms inside one call must hoist them into named
  locals first — MSVC evaluates arguments right to left.
- **Lighting**: linear-light sun and hemisphere (`SUN = (1.0,0.96,0.88)*0.95`,
  `SKY`, `DEEP`, `RIM`), per-kind `shininess` and `wrap`, body countershading
  with the belly threshold, and the highlight shoulder (`ToneMap`, knee at 0.8).
  Overlays work in linear light too: the eye crop is converted in and back out,
  because multiplying or adding sRGB is not the same operation.
- **Pixel art**: the reference's whole pre-quantisation pass is ported —
  `_see_through`, the two-pass bilateral `_smooth`, `_punch`, and `_despeckle`
  with its four-neighbour + Oklab-distance rule. The old eight-neighbour
  majority vote erased the eye's one-pixel catch-light, which is why the port's
  eye read as a bare gold ring. `_outline` is ported as well: three hue-shifted
  rim shades chosen from the colours each rim pixel borders, not one flat dark
  colour.
- **Supersampling** follows `_supersample()` exactly (4x at ≤128 px). The old
  rule dropped to 2x at 128, softening the silhouette and averaging the
  catch-light away.
- **Simplified geometry**: the mouth and teeth are shaded procedurally on the
  front underside instead of a full 3D jaw cavity (the `_mouth` lip line and
  gape are ported, but there is no cavity behind them); fins are single sheets
  (no shark/whale thickness lobes). Body loft, all fin/tail shapes, wings, arms,
  lobes, spines, eyes and every marking are present. The shadow map and
  screen-space AO are now ported.
- **Detail caps**: tessellation and arm sample counts are capped lower than the
  Python tool's maximum for predictable sprite-generation time. `detail`
  remains a parameter.
- **PNG**: reuses `creature::EncodePng` from `CreatureSheetExport.h` instead of
  duplicating zlib/PNG writing, so the output is the same straight-alpha RGBA
  the game's other creature sheets use.
- **Palette learning**: the reference's Oklab bucketing (`quant = 0.006`),
  `sqrt(count)` cluster weights and 40 Lloyd iterations are ported. Seeding is
  still deterministic rather than numpy's `default_rng(0)`, so a sheet stays
  byte-reproducible; the outline shades are clustered from the rim colours as in
  `pixel.py`. This closed most of the old 24-vs-27 colour gap.
- **Turn pose**: `SeaPose::phase` drives both the tail beat and the heading
  progress. The reference keeps a separate `turn` field, so its tail beats at
  the swim rate through a turn; here the tail beats once per turn.
- **Not ported**: the designer GUI (`gui.py`), the live aquarium preview
  (`preview.py`) and the Tk theme (`theme.py`). These are interactive desktop
  tools that need Tkinter/pygame; the engine-free module has no windowing
  dependency. `cli.py` is ported as `tools/sea_generate.cpp`.
