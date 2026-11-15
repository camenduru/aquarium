# Water VFX integration plan

How to bring the pixel-water effects from
`C:/Users/PC/Documents/pixel-art-water-vfx-demo` into this CPU-only,
per-pixel-alpha layered window.

Companion reading: the demo's own
[`docs/water-vfx-design.md`](../../pixel-art-water-vfx-demo/docs/water-vfx-design.md)
and its shaders in `demo/shaders/`.

**Locked decisions**

| Question | Choice | Consequence |
| --- | --- | --- |
| Water intensity | **`ghost`** — low-alpha water body, light over the desktop | "no background, no rectangle" survives; `full` tint is deferred, not designed out |
| Seabed | **procedural sand and rock, zero assets** | a small vector spec is rasterised at startup; no PNG decoder, no asset folder, executable stays self-contained |
| Tuning surface | **`water.ini` + hot reload** | no settings window, no JSON reader for v1; IDs stay demo-compatible for later |

---

## 1. What the source project actually is

The demo is **Godot 4 + `canvas_item` shaders**, not an engine feature. The whole
look is five fragment shaders over a 480×320 `SubViewport`, integer-upscaled 2×
with nearest sampling.

| Shader | What it does | Reusable here? |
| --- | --- | --- |
| `water.gdshader` | depth tint, 2-wave refraction, caustics, shoreline foam (distance field), 16-slot ripples, waterfall confluence, bridge reflection, night moon/lamp bands | **yes — the core** |
| `waterfall.gdshader` | two-phase scrolled curtain + impact wobble | partly (see §6.5) |
| `character.gdshader` | curved waterline, underwater tint/alpha/distortion | adapted (fish depth shading) |
| `reflection.gdshader` | flipped, distorted sprite masked by water | optional |
| `occlusion.gdshader` | redraw plate pixels in front of the walker | not needed (no plate) |

The data model is the important half, and it is portable:

| Asset | Encoding | Used for |
| --- | --- | --- |
| `bathymetry.png` | **R** = water, **G** = distance-to-shore ×64, **B** = depth 0..1, **A** = walkable | tint, refraction bounds, foam, caustics fade, submersion |
| `waterfall-mask.png` | R = curtain, G = impact | curtain/impact only |
| `occlusion-mask.png` | R = ground-contact y /320 | foreground overpaint |
| `water-parameters.json` | 158 float/bool params in 9 groups + 16 HSV grade objects + 9 effect toggles | one schema shared by runtime, UI and preset validation |

Three behaviours are worth keeping verbatim because they are what makes it read
as *pixel art* rather than "a shader":

1. **Pixel-centre sampling** — `p = floor(UV * resolution) + 0.5`.
2. **Integer-quantised displacement** — `offset = floor(displacement * strength * (refraction_base + depth) + 0.5)`, and a bent sample that lands outside the water mask falls back to the un-bent sample.
3. **Phase compensation** — every animated term is `clock * speed + offset`; changing a speed at runtime adds `clock * (old - new)` to `offset`, so the pattern never jumps.

Everything else (colours, thresholds, amplitudes) is tuning, not architecture.

---

## 2. The constraints on this side

| Aquarium fact | Consequence |
| --- | --- |
| No GPU, no shaders, no D3D — premultiplied BGRA DIB + `UpdateLayeredWindow` | the shaders must be **ported to C++ arithmetic**; there is no shortcut through a shader cache |
| No texture loading anywhere (`CreatureSheetExport` only *writes* PNG) | masks **and the seabed plate** are computed procedurally at startup (decided); no PNG decoder, no asset folder |
| Per-pixel click-through via `WM_NCHITTEST` + canvas alpha ≥ 8 | a translucent water body would **eat desktop clicks** unless hit-testing moves to its own mask |
| Transparent background, "no background, no rectangle" is the product | water cannot be a full opaque scene; it has to read as **water + light** |
| 1,200×680 @ 60 fps, up to 128 fish (~2.1 M pixel blends/frame) | water must be budgeted: render at **half resolution** and cache slow layers |
| No settings UI, no JSON parser | tuning is a dependency-free **INI with hot reload** (decided) |

The single most important consequence: **the desktop behind the window cannot be
sampled**, so there is no real refraction or reflection of the desktop. What *can*
be refracted is our own content — the fish. That is enough to make the water look
real, because the fish are the only thing anyone is looking at.

---

## 3. Core idea: one composite pass over a logical-resolution content layer

Instead of porting the shader per-pixel onto the 1,200×680 canvas, restructure
rendering the way the demo does it:

```
Aquascape::Build  → plate RGBA    (procedural sand + rocks, logical res, cached once)
Tank::Draw        → content RGBA  (plate + fish, straight alpha, 600×340, fish at scale 1)
Water::Composite  → water composite at the same logical resolution, reading:
                        content        (the "seabed" — sand, rock, fish)
                        masks          (water / distance / depth)
                        tuning + clock + ripples + night
                    → premultiplied BGRA, 600×340
UpscaleNearest2x  → the 1,200×680 canvas  (one pass, exact 2× blocks)
UpdateLayeredWindow
```

Why this is the right shape:

- **Refraction, tint and caustics apply to the sand, rock and fish for free**,
  exactly as the demo applies them to its seabed — including the "sample outside
  the water is rejected" rule. The procedural floor is what gives refraction
  something to move; without it, a displaced sample of empty desktop would be
  meaningless.
- **Ripples/foam/light land in front of the fish** without a second layer, because
  they are evaluated after the content sample in the same fragment order as
  `water.gdshader`.
- **Pixel discipline is global**: every sprite pixel becomes an exact 2× block, so
  the 2×-block check from the demo's `check_stone_basin_render.py` applies here
  too (the current per-sprite scaling does not guarantee that alignment).
- **Cost is halved**: 204k logical pixels instead of 816k, and the only
  full-window pass is an integer copy.

This is a refactor of `Tank::Draw` (draw to a logical RGBA buffer) plus one new
composite pass — not a rewrite of the creature subsystem.

---

## 4. Proposed layout

```
src/water/
  WaterTuning.h/.cpp    params, defaults, ranges, INI load/save, presets, phase offsets
  WaterMasks.h/.cpp     logical-size RGBA masks; procedural tank spec + distance transform
  Aquascape.h/.cpp      procedural sand + rock plate from the same spec, fixed palette
  WaterField.h/.cpp     pure functions: waves, caustics, foam, ripples, confluence, night
  WaterLayer.h/.cpp     content buffer, composite pass, 2x upscale, per-frame stats
  RipplePool.h          16 slots + event API (click, turn, fin beat, airstone)
src/composite.h         extend: blit into straight-alpha logical canvas, tinted variant
src/tank.cpp            Draw -> logical canvas; emit ripple/light events; depth query
src/main.cpp            wiring, --water/--night/--clock flags, frame loop, dump path
src/transparent_window  hit mask instead of canvas alpha
```

Both build paths must list the new files: `CMakeLists.txt` (explicit list) and
`build.ps1` (`$Args2`).

---

## 5. Shader → C++ port map

| `water.gdshader` | Port |
| --- | --- |
| `p = floor(UV*resolution)+.5` | the composite loop *is* the pixel grid; no UV |
| `map = texture(bathymetry, uv)` | `Masks::At(x,y)` → `{water, distance(px), depth}` |
| `depth = clamp(B + water_level*tide_influence, 0,1)` | `depth()` in `WaterField` |
| `displacement = vec2(wave(p,false), wave(p.yx+67,true))` | `WaveField::At()` — 2 rotated sine pairs, one evaluated at swapped+offset coords |
| waterfall confluence term | `ConfluenceField` (or the airstone variant, §6.5) |
| 16-ripple loop + `ripple_offsets` | `RipplePool::Contribute(x, y, t, field)` with the same ellipse aspect and lifetime |
| `offset = floor(...)`, bent-sample rejection | `WaterLayer::SampleContent` — keep both rules exactly |
| depth tint (`mix(shallow, deep, depth)`, night variant) | precomputed `Palette` (§7.3) |
| caustics: nested sines + `lattice`/`broken`/depth fade/`smoothstep(1,6,dist)` | `CausticsField::At()` |
| foam: `tide + jagged`, `edge`, `broken` gate, backwash line | `FoamField::At()` |
| night moon/lamp bands | `NightBands::At()` — re-anchored, no plate (§6.4) |
| `grade_rgb` (HSV relative grade) | `GradeRgb()` — port the exact math once |
| `phase_*` macros + `offset_*` | `Tuning::Phase(id)` — same formula, same compensation |
| `COLOR = vec4(color,1)` | `out = premultiply(color, alpha)` — every write premultiplied once |

`character.gdshader` maps to **fish depth shading**, not sprite-local submersion:
see §6.3.

---

## 6. Adaptation decisions

### 6.1 Two water modes (ghost ships)

`--water` picks how the water body is drawn:

| Mode | What is drawn | Alpha | Product impact |
| --- | --- | --- | --- |
| `off` | nothing (today's aquarium) | — | unchanged |
| `ghost` **(default, v1)** | sand/rock, caustics, light bands, rim foam, ripples, and a light depth tint (~10–20 %) | low | desktop stays readable; reads as water in front of it; keeps "no rectangle" |

Both share one composite pass; the mode is a palette/alpha multiplier, not a
different renderer. The strong tint of a "real tank" (`full`, up to ~60 %, plus a
glass rim) is **deferred** — the palette split exists, but v1 does not ship it.

### 6.2 Side-view reinterpretation of the masks

The demo's masks are top-down (shore distance, basin depth). This window is
side-view, so:

| Demo channel | Here |
| --- | --- |
| R water | tank interior: rounded rectangle inset a few pixels from the window edge, minus the rock silhouettes that rise above the sand |
| G distance-to-shore ×64 | distance to that **tank edge or to the sand**, fed to the same foam + caustic-fade code (in ghost mode it is a surface/rim band, not a painted shoreline) |
| B depth 0..1 | **vertical** depth `pow(y / waterHeight, 1.05)`: 0 at the surface, 1 at the sand. Tint darkens downward and caustics fade downward, which is what the demo's distance fit communicates anyway |
| A walkable | unused for water; reused as the **hit mask** (§6.6) |

Computed once at startup by rasterising a small vector spec (rounded rect + sand
line + optional blobs) and running a distance transform — ~5 ms for 600×340, no
asset files, no PNG decoder. This keeps the "one self-contained executable, no
assets" property (836 KB with the water layer and the reef).

### 6.2a Procedural sand, rock and corals (zero assets)

The floor is generated from the **same spec**, in code, once at startup, and cached
as a logical-resolution RGBA plate. No PNG, no embedded blob, no asset folder.

The look is built from a handful of pixel-art constraints rather than detail:

- **Silhouette first** — a sand horizon drawn as a polyline quantised to the
  logical grid, plus 3–6 rock silhouettes from seeded ellipses/polygons. A 1-pixel
  darker outline on the rocks, the demo's shared-outline trick.
- **A fixed palette** — 6–8 entries (sand light/mid/shadow, rock light/mid/shadow,
  plus the coral palette). Every pixel is one of those entries, so no
  gradient banding appears when the water tint is applied on top.
- **No anti-aliasing** — edges are hard; the 2× upscale and the water's own
  refraction supply all the softness that is wanted.
- **Deterministic** — seeded from the tank seed, so a new tank is a new aquascape
  (matching how fish already work), and `--dump-frame` stays reproducible.
- **Placement** — the sand sits in the bottom band of the window (the depth mask's
  `depth → 1` region) and is the densest element in the scene (up to ~45 % alpha);
  rocks stay lighter and semi-transparent, so the desktop still reads through the
  scene. Only the fish and the front rock ever reach full opacity.

Two depth cues fall out of the same spec with no extra art:

1. **Back rock vs front rock.** Rocks behind the fish are part of the plate; one
   optional "front rock" is drawn after the fish, gated by the demo's
   contact rule — a *footer* value per rock compared against the fish centre.
   That is `occlusion.gdshader` reused for a fish instead of a walker, ~20 lines,
   and it sells the tank's depth more than any shader.
2. **Sand texture under water.** Because the plate is the composite's `seabed`,
   caustics and refraction land on it directly; the sand grain is just palette
   dithering plus the existing caustic light.

Corals were added after v1, as the same kind of generated art (see §13.2):
a genome per index, five species (branch, fan, brain, kelp, polyp), a shared
1 px outline and a five-tone palette per coral. They are drawn into a scratch
tone mask first, then blitted through `Masks::Inside`, so a coral can never
leave the tank or reach the fish hit mask. Placement walks the width by the
golden angle from the coral's index, which makes "add one more" a pure append:
no existing coral moves or changes shape.

The plate is editable with a `[aquascape]` section in `water.ini`
(`seed`, `sand_y`, `sand_curve`, `rock_count`, `rock_scale`, `front_rock`,
`coral_count`) and a reload (Ctrl+Shift+R, or the automatic half-second poll)
rebuilds it, so the scene can be tuned without recompiling.

### 6.3 Fish

Fish stay fully submerged; no sprite-local waterline in v1. Instead:

- **depth tint + alpha** per fish, from the mask at its centre — a fish near the
  sand is dimmer and slightly transparent, one near the surface is clearer;
- **caustic modulation** of the sprite during the composite: the caustic value at
  each pixel is already computed, so fish catch the light bands;
- optional later: a per-fish `breach` bit that stores sprite-local height for
  `character.gdshader`'s curved waterline, when a fish bobs through the top.

The demo's `immersion`, `waterline_height`, `under_alpha`, etc. therefore remain
in the schema and drive fish shading, not a walker.

### 6.4 Day / night

The demo keeps two ImageGen plates and a `night` scalar. Here the plate is
procedural, so `night` re-palettes it instead of swapping it: tint, caustic colour,
sand/rock palette, foam colour and a moon band anchored to the top edge, plus a fish
grade. Expose `--night 0|1` and an optional slow cycle (`--day-cycle <minutes>`),
and keep the demo's semantics: one shared set of adjustments, two base palettes.

### 6.5 Waterfall → airstone (recommended substitution)

A waterfall needs a tall, dedicated curtain region; the procedural plate is a
seabed, not a cliff. Instead, reuse the **impact + confluence** math as an air
stone / outflow in a corner: an upward bubble column drawn as staggered pixel
beads (20 by default, `bubble_count` in `water.ini`, one column per configured
`bubble_column_N` outlet), a radial disturbance at the surface, and ripples
emitted from the columns.
Identical math to `flow_*` and `impact_*`, and it explains the bubbles an aquarium
is expected to have. The `waterfall_enabled` toggle stays in the schema as the
airstone switch; the curtain shader stays unported.

### 6.6 Click-through must survive

Water tint alpha (≥8) would make `OpaqueAt` answer `HTCLIENT` everywhere. Add a
**hit buffer** (one byte per canvas pixel) written only by fish — plus bubbles once
M8 lands, and the front rock if it is interactive — while sand, light and water
never mark it. `TransparentWindow::OpaqueAt` reads that buffer instead of the alpha
channel. Water then never eats a desktop click, and fish keep their right-click
menu. Cover with a probe that samples a water-only pixel and asserts
`HTTRANSPARENT` (§10).

---

## 7. Parameter system

### 7.1 Schema

Port the demo's **IDs verbatim** (`clarity`, `caustic_strength`, `foam_distance`,
`ripple_speed`, …) into `WaterTuning` with the same defaults and ranges, so a
preset written for one side is meaningful on the other. Do not port the 158 at
once — start with the ~70 that this pipeline actually reads, keep the rest as
declared-but-inert so the schema stays complete, and mark each in a header
comment.

Effects toggles: keep all nine (`depth`, `refraction`, `caustics`, `foam`,
`ripples`, `waterfall`, `environment`, `reflection`, `submersion`).

### 7.2 Format and presets

Aquarium has no JSON parser and no settings window, by decision. Use a strict
`key = value` INI with sections:

```
; aquarium/water.ini
[water]
clarity = 0.65
caustic_strength = 0.27
[falls]
bubble_count = 20
bubble_column_1 = 0.16,0.90
[night]
night = 0
[aquascape]
seed = 7
sand_y = 0.86
rock_count = 4
front_rock = 1
coral_count = 9
```

- `--water-preset <path>` loads at startup; **Ctrl+Shift+R hot-reloads** (mtime
  check, no file watcher needed — the file is also picked up automatically within
  half a second) so tuning is still live.
- `--water-set key=value` overrides for checks; `--water-restore-defaults`.
- One live file (`water.ini`, next to `aquarium.exe`); `--water-preset <path>`
  points at an alternative file for a check or a second look, and the
  right-click menu can rewrite the file from the current defaults. No
  named-preset browser in v1, so there is no auto-save to get wrong.
- Unknown keys, non-numeric values and out-of-range values are rejected with a
  console message and the previous value stays — same rule as
  `parameter_store.gd`, no partial application.
- Changing an `[aquascape]` key rebuilds the plate and the masks together, because
  they must stay in agreement.
- Bubble outlets are a **list**: every `bubble_column_N = x,y` line is one
  rising column, and the list is rebuilt from the file on each load, so adding
  or deleting a line adds or removes that column without leaving stale state.
- No JSON reader in v1. Inert-but-declared IDs stay in the table so a future
  JSON/preset bridge is a loader, not a schema change.

### 7.3 Precompute the colours

`grade_rgb` is evaluated per pixel per layer in the demo. Here, resolve the 16 HSV
grades into ~16 RGB constants whenever tuning or `night` changes, then the
composite only mixes. Same maths, one-time cost.

---

## 8. Milestones

Sizes are rough working days, not commitments.

| # | Milestone | Deliverable | Acceptance | Size |
| --- | --- | --- | --- | --- |
| M0 | Baseline harness | `--water off`, `--clock`, `--night` flags; deterministic `--dump-frame`; `out/baseline/fish-only.png` + hash | `--water off` output byte-identical to today at a fixed seed/clock | 0.5 |
| M1 | Logical-res scene pipeline | `RgbaCanvas` at w/2×h/2; `Tank::Draw` into it; `UpscaleNearest2x`; old path behind `--water off` | perfect 2× blocks in a dump; fish size and positions unchanged on screen; frame time ≤ today | 1.0 |
| M2 | Masks + procedural aquascape | `WaterMasks` + `TankSpec` + EDT; `Aquascape` sand/rock plate from the same spec with a fixed palette; dump both | monotonic depth, distance 0 at rim, dry pixels exactly 0, plate uses only palette entries, same seed → same plate, < 20 ms startup | 1.5 |
| M3 | Water body | depth tint + 2-wave refraction + caustics via the port map | water pixels animate, sand/rock refract, dry/outside pixels unchanged, every pixel `rgb ≤ a` (no premultiplied halo), `strength = 0` still animates caustics | 1.5 |
| M4 | Rim foam + tide | `FoamField` + `water_level` (tide moves the band, never the rim) | band follows distance, gate breaks it up, `foam_strength = 0` clean | 0.5 |
| M5 | Ripples + events | `RipplePool` (16 slots), emitters: click, turn, fin beat, airstone; speed-change compensation | 16-slot cycle, ring expands/fades, max-rate stream does not blow out, speed edit never jumps | 1.0 |
| M6 | Fish shading + front rock | depth tint/alpha, caustic modulation, night grade; contact-gated front rock | deep fish visibly dimmer, caustic band crosses a fish without shimmering, front rock covers exactly the fish pixels behind its footer | 0.75 |
| M7 | Day / night | `night` from flag or slow cycle; repaletted sand/rock, water, fish | switching night changes palette but not geometry; same seed gives the same scene | 0.5 |
| M8 | Airstone (optional) | bubble column + impact disturbance + ripple emission | bubbles rise, surface ripples, off switch restores today's look | 1.0 |
| M9 | INI + hot reload | `water.ini` with `[water] [night] [aquascape]`, Ctrl+Shift+R reload, CLI overrides, save/load menu, restore defaults | live edit changes the picture; invalid key/value rejected and the old value kept; `[aquascape]` edit rebuilds plate + masks | 0.75 |
| M10 | Verification + docs | `tools/check_water.py`, probe extension, README section, `docs/water.md` | full suite green; perf table recorded | 1.0 |
| — | **v1 total** | ghost + procedural aquascape + INI | | **~9** |

Sequencing notes: M1 is the only risky refactor and must land before anything
else; M2/M3 can be developed against a synthetic mask while M1 settles; M9 can
start early because the INI loader is independent.

---

## 9. Performance budget

Target: **≤ 4 ms/frame** for the water composite at 1,200×680 (600×340 logical),
≤ 12 ms total, 128 fish unchanged.

| Cost centre | Naive | Plan |
| --- | --- | --- |
| ~13 `sin` per pixel | 2.65 M `sin`/frame ≈ 10 ms+ | **4096-entry sine LUT with lerp** (offsets are integer-quantised anyway, so the error is invisible) → ~1.5 ms |
| 16-ripple loop per pixel | 3.2 M distance tests | iterate **ripples outside**, accumulate into a bounding-rect field; only 1–3 are live normally |
| caustics/foam everywhere | full frame | skip when the mask says dry (cheap), and cache the slow light field at 15 Hz across worker threads, composite the 60 Hz ripple overlay on top |
| procedural plate | — | built once at startup, cached; only re-built on Ctrl+Shift+R with an `[aquascape]` change. Zero per-frame cost beyond the composite |
| upscale | — | one integer pass, premultiplied, no filtering |
| fullscreen 4K | 960×540 logical | still within budget with the LUT; record a measured row in the perf table |
| optional | — | 2–4 row-split worker threads for the composite; fish blending stays as is |

Memory: content 816 KB + plate 816 KB + water 816 KB + masks 816 KB + hit mask
400 KB ≈ 3.7 MB.

---

## 10. Verification

Reuse the demo's evidence discipline; every claim gets a number.

1. **Determinism** — fixed `--seed`, `--clock`, `--night` dump twice → identical bytes.
2. **2× pixel grid** — reshape the dumped window into 2×2 blocks, assert every block is uniform (the demo's `perfect_2x_art_blocks`).
3. **Premultiplied invariant** — no pixel with a channel > alpha (catches the halo bug).
4. **Containment** — changed pixels outside the water mask must be 0 (the demo's `dry_pixels_changed_outside_waterfall`).
5. **Per-effect visibility** — toggling each of the nine effects changes > 0 pixels inside its region.
6. **Frozen time** — two frames at the same clock are identical.
7. **Ripple response** — inject a ripple at (x,y), assert the ring appears within its radius and expires after `ripple_lifetime`.
8. **Click-through** — extend `tools/probe_window.ps1`: sample a water-only client pixel and a fish pixel, assert `HTTRANSPARENT` and `HTCLIENT` respectively.
9. **Window invariants** — the existing layered/topmost/no-`WS_EX_TRANSPARENT` probe still passes.
10. **Perf** — record ms/frame for 1, 12, 128 fish at 1,200×680 and at full screen.
11. **Aquascape** — same seed → identical plate bytes; every plate pixel is a palette entry; `sand_y`/`rock_count`/`coral_count` visibly move the scene; the front rock never covers a fish whose centre is in front of its footer. Corals are plate-only: they change content, never the hit mask.
12. **Ghost budget** — mean water alpha over the tank stays under the agreed cap (≈20 %), so a screenshot of a text document behind the aquarium is still legible.

`tools/check_water.py` mirrors `tools/check_stone_basin_render.py` for 1–7 and 11–12.

---

## 11. Risks and limits

| Risk | Mitigation |
| --- | --- |
| **Licensing**: the demo has no `LICENSE`; copying shader code verbatim is risky | reimplement from the published design doc and equations (they are fully documented), credit the project in README; ask the author if verbatim reuse is wanted |
| Water dims the desktop and weakens the product | `ghost` default, alpha budget with a measured cap (§10.12), `--water off` |
| Water eats clicks | separate hit mask (§6.6), probe test |
| Per-pixel cost at 4K/fullscreen | LUT, 30 Hz light cache, row threads, measured budget |
| Procedural sand/rock reads as programmer art | silhouette-first, 6–8 entry palette, no AA, one outline tone; review `--dump-frame` stills at 1× and 2× before shader work proceeds; the spec is editable in INI, so the scene is tuned without recompiles |
| Refraction has nothing to move | fixed by the plate: sand dithering and rock edges give the integer displacement something to shift, so refraction is provable in a still dump |
| Parameters drift from the demo | identical IDs, defaults and ranges; a parity test that reads the demo JSON and asserts this side's defaults match |
| Refactor breaks fish rendering | M0 byte-identical baseline, M1 diff check before water lands |
| Masks and plate disagree after a spec edit | they are generated from one `TankSpec` in one call; an `[aquascape]` reload rebuilds both |

Stated limits to write down, not to discover later: no true desktop refraction or
reflection; no flow conservation; no volumetric lighting; fixed tank geometry (tide
moves the band, not the rim); `ghost` mode is an artistic overlay, not simulated
water.

---

## 12. Decisions

### Locked for v1

| Decision | Choice |
| --- | --- |
| Water intensity | `ghost` only; `off` remains the escape hatch |
| Seabed | procedural sand + rock, generated from one `TankSpec`, zero assets |
| Tuning | `water.ini` + Ctrl+Shift+R hot reload + CLI overrides + named presets; no UI window, no JSON reader |

Consequences that are now scope, not options: the plate is a deliverable (M2) and
carries its own palette and review step; `[aquascape]` keys rebuild plate and masks
together; the ghost alpha cap is a measured acceptance criterion, not a taste
call.

### Deferred (designed for, not built)

- **`full` mode** — strong tint and glass rim. Needs the palette split only.
- **ImageGen / embedded plate** — a loader plus a bigger executable.
- **Slider panel** — a second Win32 window ported from `parameter_panel.gd`.
- **Demo preset compatibility** — a ~250-line JSON reader so the demo's
  `water-parameters.json` and presets load directly.
- **Breaching fish** — per-fish local height for `character.gdshader`'s curved
  waterline when a fish crosses the surface.
- **Curtain waterfall shader** — unported; the airstone uses the impact math.

---

## 13. As built (v1)

This section records where the implementation departs from the plan above. The
plan is the design authority; this is the diff, kept short so the next reader can
tell deliberate choices from accidental ones.

### 13.1 Shape of the code

`WaterField` ended up header-only (pure inline arithmetic rather than a
translation unit), so the shipped sources are:

```
src/water/WaterTuning.h/.cpp   parameter table, INI read/write, palette
src/water/WaterMasks.h/.cpp    rounded-rect interior, chamfer distance, depth
src/water/Aquascape.h/.cpp     procedural sand + rock + coral plate (zero assets)
src/water/RipplePool.h         16 slots, phase compensation
src/water/WaterLayer.h/.cpp    static light cache, composite, upscale
```

`src/composite.h` gained `BlendSpriteLogical` (straight-alpha source-over plus
hit-mask marking); `src/transparent_window.*` gained the hit mask and
`WM_LBUTTONUP` dispatch; `src/tank.*` gained `DrawLogical` and ripple emission;
`src/main.cpp` owns the flags, the INI poll and the hotkeys.

### 13.2 Deviations from the plan

| Plan | As built | Why |
| --- | --- | --- |
| F5 reloads `water.ini` | **Ctrl+Shift+R** (`RegisterHotKey`) | the window is `WS_EX_NOACTIVATE`, so it never takes keyboard focus and an in-window key handler would never fire |
| light cache at 30 Hz | **15 Hz**, split into 4 row bands on worker threads | one build is 6–7 ms; at 15 Hz with row threads it amortises to ~1.7 ms/frame and the light barely changes in 66 ms |
| caustic palette from the demo | lighter day palette (`0.52, 0.93, 0.87`), weighted `0.06 + 0.94·depth²` toward the sand | the demo's tint is built for an opaque seabed; over a desktop it read as noise until the light was pushed down onto the sand |
| foam defaults | `foam_strength 0.28`, `foam_distance 3.0` | 0.22/1.8 was invisible at ghost alpha |
| edge handling | one taper over the last **14 logical px** of the finished pixel | tint-only fading still left a rectangle of sand and caustics on the desktop |
| click ripples | only on solid pixels (fish), plus airstone and fish turns | open water is `HTTRANSPARENT`, so Windows never delivers those clicks; pretending otherwise would need a click-through override |
| `--day-cycle <minutes>` | not built; `night` follows the local hour unless `--night`/Ctrl+Shift+T pins it | dumps must stay deterministic, and a slow cycle needs a story about resume-from-sleep for little gain |
| `[aquascape]` keys (`sand_y`, `rock_count`, …) | present in the INI; defaults ship tuned | the panel is honest: editing them rebuilds plate + masks together |
| no coral in the plan | five seeded species (`Branch/Fan/Brain/Kelp/Polyp`) plus `coral_count` and **Add a random coral** | the plate read as empty sand; corals are the cheapest way to make it a tank, and they reuse the rock rules (mask-clipped, shared outline, palette-only) |
| 8–16 airstone bubbles | **20** beads per column, each with its own size/alpha wobble, 0–64 per column and up to six `bubble_column_N` outlets (128 beads total) | one density read as a white smudge at 600×340, but the wobble makes 20 read as separate beads, and the count and outlet positions belong to the INI: add or delete a line to add or remove a column |
| named presets in a `water\` folder | one `water.ini` beside the executable, `--water-preset <path>` for alternates | no preset browser means nothing to auto-save; the menu can rewrite the file from defaults |

Other decisions worth knowing: ripple radius edits compensate existing rings
(the demo's phase-offset trick); bubble and ripple contributions are capped
before compositing so a crossing never lights the desktop to full opacity;
`stats_.compositeMs` excludes the static build so the two costs stay separable.

### 13.3 Measured (1,200×680, ghost, 6 fish, this machine)

| Metric | Value |
| --- | --- |
| static light build (15 Hz, 4 threads) | 6.5–7.5 ms → ~1.8 ms/frame amortised |
| 60 Hz composite | 4.5–4.7 ms |
| frame cost, water only | ~6.4 ms |
| ghost alpha, mean over the tank | 18.0 % (18.7 % with all 24 corals) |
| ghost alpha, mean over open water | 9.8 % |
| highlights above 40 % of open water | 0.01 % |
| `--water off` at seed 99, clock frozen | `51E5EF7C…` — byte-identical to the pre-water build |

`python tools/check_water.py` covers determinism, the 2× block grid, the
premultiplied invariant, containment, and the ghost budget. `tools/probe_window.ps1`
now samples a 48×27 grid with `WM_NCHITTEST`: water pixels answer
`HTTRANSPARENT`, fish answer `HTCLIENT`, and only ~2 % of the client area is
interactive.

### 13.4 Attribution

The effect is a reimplementation of the equations in the local
`pixel-art-water-vfx-demo` (Godot 4). The demo ships no licence, so no shader
text was copied: the port works from its `docs/water-vfx-design.md` and the
published parameter semantics, and README credits the project.
