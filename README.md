# Aquarium

> **1.3 sextillion sea creatures, no third-party DLLs, in 884 KB.**

https://github.com/user-attachments/assets/0dfa0eb4-cff2-4cc4-9f15-9afc67cd37de

A transparent desktop fish tank for Windows. A frameless, click-through-per-pixel
window floats above the desktop and swims procedurally generated fish in it: no
background, no title bar, no taskbar button.

The window system is the recipe from **C:/Users/PC/Documents/VRMPet**
(`src/main.cpp`, `LayeredOverlay`). The animals come from the sea creature
generator in **../RoguelikeGame/src/creature/sea**, copied verbatim.

## How the transparency works

A Direct3D swap chain cannot carry per-pixel alpha — DXGI's
`CreateSwapChainForHwnd` takes `DXGI_ALPHA_MODE_IGNORE` — so everything is drawn
into an ordinary CPU canvas and handed to Windows' own layered-window path:

```
WS_POPUP             no caption, border or chrome
WS_EX_LAYERED        required by UpdateLayeredWindow (per-pixel alpha)
WS_EX_TOOLWINDOW     out of the taskbar and Alt+Tab
WS_EX_NOACTIVATE     never steals focus from the app you are working in
WS_EX_TOPMOST        floats above other windows
```

Every frame, with the water off:

1. `Clear()` the `CreateDIBSection` canvas to transparent.
2. `BlendSprite()` each fish cell into it as **premultiplied** BGRA
   (`dst = src + dst*(255-a)/255`). `UpdateLayeredWindow` with `AC_SRC_ALPHA`
   requires premultiplied colour; skipping it leaves a halo.
3. `UpdateLayeredWindow(..., ULW_ALPHA)`, 60 fps.

With the water on the same canvas is filled by one composite over a
half-resolution scene layer instead ([Water](#water)).

`WS_EX_TRANSPARENT` is deliberately **not** set: that would make the whole
window click-through, fish included. `WM_NCHITTEST` answers `HTTRANSPARENT` for
a transparent pixel and `HTCLIENT` for a fish pixel, so the desktop keeps its
clicks and a fish can still be right-clicked. A separate **hit mask** decides
that, not the canvas alpha, because the water layer deliberately draws
translucent pixels over most of the window ([Water](#water)).

## Fish

Each animal is one sheet from the sea generator: a row per clip
(`swim_right`, `turn_right_to_left`, `swim_left`, `turn_left_to_right`, plus the
two puff rows for a pufferfish), a column per frame. Agents pick the clip that
matches their heading — the sheets already contain both facings, so nothing is
mirrored at runtime. At each end of the tank the fish plays the matching turn
clip and swaps direction when it finishes.

Sheets are rendered on a worker thread (a 64 px sheet is seconds of
supersampled rendering), so the window appears immediately and fish arrive one
by one.

## How a fish is drawn, in detail

There is no GPU code in this project and there are no shaders. A fish is
produced in three stages, and only the last one happens per frame.

### 1. The sheet: once per fish, on a worker thread

1. **Genome.** `MakeGenome(species, seed)` fills roughly 118 genes - body
   profile, thickness, peak, nose, taper, all four fins, wings, arms, spines,
   eyes, mouth, plus a colour block in HSV. `RandomGenome` invents the same
   structure for the species that are not in the fixed roster.
2. **Mesh.** `SeaGeometry` lofts a triangle mesh from those genes, and
   `SeaAnimation` poses it per frame: a travelling swim wave down the body,
   breathing, fin flap, tail beat, and the turn lag that makes a turning fish
   bend nose-first.
3. **Rasterize.** `SeaRaster` is an orthographic, z-buffered triangle
   rasterizer. Every triangle is filled with barycentric weights and the nearest
   depth wins, so overlapping geometry resolves correctly - a pectoral fin in
   front of the flank, a ray's wing over its own body.
4. **Shade.** `SeaRender` computes each pixel in C++: key light plus hemisphere
   fill, Blinn specular (`pow(dot(N, H), shininess) * gloss`), rim light,
   countershading, fin translucency, all 26 markings, scars, gill slits, the
   mouth line and teeth. Eyes and urchin spines are camera-space overlays
   stamped after the geometry pass, exactly where the reference does it.
   The frame is rendered **4x supersampled** at this size and averaged down,
   which is where the smooth silhouette comes from.
5. **Quantize.** `SeaPixel` learns **one palette for the whole sheet**, so a
   swimming fish cannot shimmer between two nearly identical blues frame to
   frame, maps every frame onto it, and stamps a shared outline (one shade set
   for the entire sheet) so the inner shading never bleeds off the silhouette.
6. **Pack.** One cell per frame, `size x size` (64 here). Row = clip:
   `swim_right`, `turn_right_to_left`, `swim_left`, `turn_left_to_right`
   (+ two `puff` rows for a pufferfish). Column = frame. Frame count and fps
   scale together with `smoothness`, so at 2 the base 8 swim frames become 16 at
   20 fps. A 4-clip sheet is `maxFrames*64` by `clips*64` - 1280x256 for this
   tank, about **1.2 MB of RGBA**, and takes **~1.7 s** to generate at 64 px on
   this machine.

### 2. The frame: every frame, on the main thread

`Tank::Draw` walks the fish and blits one cell each:

- the cell is `col = frame`, `row = clip row`, clipped to the sheet bounds;
- the destination is `centre - (64*2)/2`, so a fish is positioned by its middle,
  not its corner;
- scaling is **integer nearest-neighbour** (`srcX = ox / 2`). There is no
  sampler and no filtering - that division is the whole reason the art stays
  crisp;
- the blend is `BlendSprite`: straight-alpha RGBA in, premultiplied BGRA out,
  integer src-over per channel,
  `dst = src*a/255 + dst*(255-a)/255`, with the whole sprite scaled by its depth
  alpha (230-255).

Premultiplying matters: `UpdateLayeredWindow` with `AC_SRC_ALPHA` expects
premultiplied colour, and feeding it straight alpha is what puts a bright halo
around every sprite.

With the water on, the same walk goes through `Tank::DrawLogical`: one cell per
fish, unscaled, into the 600x340 straight-alpha content buffer (destination
`lround(x/2) - 32`), and the water composite premultiplies and upscales it once.

### 3. The screen: once per frame, by Windows

`Push()` hands the finished bitmap to `UpdateLayeredWindow(..., ULW_ALPHA)` at
60 fps and the Desktop Window Manager composites it over the desktop. Because
transparent pixels carry no colour at all, the desktop shows through them; the
hit mask (not the alpha channel) decides clicks, since `WM_NCHITTEST` answers
`HTTRANSPARENT` over water and `HTCLIENT` on a fish.

Cost is CPU cycles per pixel touched, not GPU time: one fish at 2x covers
128x128 = 16,384 pixels, so a 128-fish tank is about **2.1 million pixel blends
per frame**. That is the budget this design spends in exchange for per-pixel
alpha, which a Direct3D swap chain on an `HWND` cannot provide at all.

## Water

The tank can be read as a tank instead of as fish on a desktop: a low-alpha
**ghost** water body, a procedural sand bed with rock silhouettes and a reef of
corals, plants and polyps, caustics on the sand, a foam line at the surface,
ripple rings, and airstone bubble columns. It is on by default; `--water off`
is today's aquarium, and the dump at a fixed seed is byte-identical to the
pre-water build.

Everything is still generated in code - there is no PNG loader and no asset
folder. The sand, gravel dither, rock silhouettes, the front rock and every
coral are rasterised from one seeded `AquascapeSpec`, so the plate and the water
masks cannot drift apart:

```
window canvas   1200x680, premultiplied BGRA, UpdateLayeredWindow
  logical        600x340 straight RGBA content (sand + rock + corals + fish)
  water          600x340 composite: refraction, tint, caustics, foam,
                 ripples, bubbles, then one exact 2x integer upscale
```

Pixel-art discipline is enforced, not implied: the water is evaluated at
**half resolution** on pixel centres, refraction is quantised to whole logical
pixels, a bent sample that leaves the water falls back to the un-bent one, and
every logical pixel lands as an exact 2x2 block. `tools/check_water.py` asserts
that block grid, the premultiplied invariant, containment inside the water mask,
determinism, and the ghost alpha budget (measured 18 % mean over the tank, 18.7 %
with all 24 corals).

Cost at 1200x680: the slow light field (caustics, foam, depth tint) is rebuilt
at 15 Hz across worker threads (~6.5-7.5 ms per build with the reef, ~1.8 ms
amortised), and the 60 Hz pass overlays ripples, bubbles and the content
composite (~4.6 ms), so the water is about **6.4 ms/frame**.

Click-through survives because the click test uses a **hit mask** written only
by fish, not the canvas alpha: water pixels are `HTTRANSPARENT` and the desktop
keeps its clicks. `tools/probe_window.ps1` samples a 48x27 grid with
`WM_NCHITTEST` and asserts that only a fish-sized fraction is `HTCLIENT`.

### Tuning

Every knob is a `key = value` line in `water.ini`, next to `aquarium.exe`
(`bin\water.ini` in a source build), which is written with comments on first
run. The keys are the [pixel-art-water-vfx](#credits) demo's own IDs, so its
documentation applies here. Edit the file and the running tank picks it up
within half a second (or press `Ctrl+Shift+R`); there is no settings window.

```
[water]
clarity = 0.65
ghost_alpha = 0.5
[aquascape]
sand_y = 0.86
rock_count = 4
coral_count = 9
```

Invalid or out-of-range values are reported and the previous value is kept.
Changing an `[aquascape]` key rebuilds the sand, rock, coral and masks together.

### Corals

The reef is procedural, not a texture atlas. Each coral is a small **genome**
- kind, size, lean, curl, branchiness, hue and a detail seed - drawn into a
tone mask and then blitted with one shared 1 px outline, exactly like the rocks
share their palette. There are five kinds, weighted so branching and fans
dominate:

| kind | reads as |
| --- | --- |
| `Branch` | staghorn: a thick trunk splitting three levels into bright tips |
| `Fan` | a filled sea fan with radial ribs and a scissored rim |
| `Brain` | a low wavy-groove dome sitting on the sand |
| `Kelp` | two to four ribbons, dark on one edge, light on the other |
| `Polyp` | a mound with a crown of fingers in the complementary hue |

Corals are **plate art**: they paint only inside the water mask, never touch the
fish hit mask, and live under the same ghost tint as the sand. Placement comes
from the coral's index and the aquascape seed, walking the width by the golden
angle, so **adding a coral never moves the ones already there** - the same rule
the fish use for a new individual. Colours walk a curated reef palette (pink,
orange, yellow, green, teal, blue, violet) and each coral's tips are the
complement of its body, which is what makes a polyp or a branch end read at
600x340.

`coral_count` (0-24) seeds the reef; the default is 9. There are three ways to
change it:

- **Right-click a fish -> Add a random coral** appends one (session only, like
  adding a fish; the menu entry greys out at 24).
- `--corals N` overrides the count for this run, including dumps.
- Edit `coral_count` in `water.ini`; a reload rebuilds the plate and masks.

### Bubbles

The airstone is a bubble column, and `water.ini` owns both how many beads rise
and where the columns leave the tank:

```
[falls]
bubble_count = 20
bubble_column_1 = 0.48
```

`bubble_count` (0-64) is the number of beads in **each** column. Every
`bubble_column_N` line is one outlet, written as `x` in fractions of the tank
width; the column always leaves the **sand** at that x and rises to the
surface, so an outlet never starts in mid-water. Add a line to add a column,
delete it to remove that column: the list is rebuilt from the file on every
load, so nothing stale survives a reload. Up to six outlets are accepted, and
at most 128 beads are drawn in total.

With no `bubble_column_N` line the single outlet stays on the stone's airstone,
which is the default, so an untouched `water.ini` looks like the original
column. The airstone ripple rings cycle through the configured outlets.
`bubble_count` and `bubble_column_N` are aquarium-only keys; everything else in
the file keeps the demo's own IDs.

### Saving and loading a tank

One text file brings the whole aquarium back. **Ctrl+Shift+S** (or right-click
-> **Save tank**) writes `tank.ini` next to `aquarium.exe`; **Ctrl+Shift+L** (or
**Load tank**) puts it back: sand, rock, corals, every water setting, the night
mood, the bubble outlets, and each fish with its species, genome seed and place
in the tank.

```
; aquarium tank save - water settings, aquascape and every fish
[water]
clarity = 0.65
...
[aquascape]
seed = 7
coral_count = 9

[creatures]
count = 6
fish_1 = Giant squid|100|483.89903635519175|225.24207069717119|34.433290651259547|1|3.8441412565612127|2.6779769898602939|0.59682459754103612|253
```

The settings half is exactly a `water.ini`, so a save also works as a preset
(`--water-preset tank.ini`); `[creatures]` is the only addition, one line per
fish: `species|seed|x|base_y|speed|dir|bob_phase|bob_amp|bob_speed|alpha`. A
saved fish is identified by **species + genome seed**, and the sheet is
regenerated from that pair on a worker, so the same animal comes back - not a
similar one - and it arrives one at a time like every other fish. Numbers are
written with 17 significant digits, which is enough to read every `double` back
bit-for-bit: load -> save reproduces the file byte for byte. A fish saved
mid-swim reappears exactly where it was; only its swim cycle restarts, because
the regenerated sheet begins at its first frame.

`count = 0` is a valid save: an emptied tank keeps its sand, reef and settings,
and loads back with no fish.

Loading a save replaces the water settings as well (a `--water-set` flag still
wins, like on a hot reload), rebuilds the plate from the saved aquascape seed,
and clears the old ripples. A missing or malformed file is reported line by
line on the console and the running tank is left alone; the **Load tank** menu
entry greys out until a `tank.ini` exists. **New tank** starts a fresh school,
so a load never lingers as the default.

`--load-tank <path>` and `--save-tank <path>` do the same headlessly, like
`--dump-frame`. A save-only run stores the tank **as the run starts**, which is
what makes the round trip checkable:

```
aquarium.exe --seed 99 --fish 6 --save-tank out/tank.ini
aquarium.exe --load-tank out/tank.ini --dump-frame out/after.png
```

That dump is byte-identical to the same run started from the seed, in water-off
and in ghost mode.

### Clicking and removing fish

A **left click on a fish startles it**: it turns its back on the pointer and
bolts away, then eases back to its cruise speed. The alarm lasts 1.6 s, and the
flee speed is a multiple of that fish's own cruise speed:

```
[fish]
; how fast a clicked fish flees (multiple of cruise speed)
escape_speed = 2.8
```

`escape_speed` is 1-6 (1 = no hurry, 6 = a blur), read on load and on
**Ctrl+Shift+R** like every other key. The fish picks the side with room, so a
fish near a wall leaves the other way instead of bouncing off it. This works
with the water off too - the fish still darts, there is just no ring behind it.
Startle is an event, not part of the tank: it is deliberately absent from
`tank.ini`.

A **right click on a fish** opens the menu with **Remove this fish**, which takes
that one animal out and leaves the rest swimming (the others renumber, so a
saved `fish_N` afterwards is not the same fish as before). Like **Add a random
fish**, the change lives in the session - **Ctrl+Shift+S** keeps it. The last
fish can go too: the water, sand and reef are still an aquarium, and it saves and
loads as one (`count = 0`). With no fish there is no solid pixel, so the window
is fully click-through and its menu cannot be reached - **Ctrl+Shift+A** adds a
fish back from the keyboard, which is why it exists alongside the menu entry.

## How many creatures can be generated

The generator itself has a finite roster, but the individuals it can invent are
effectively unlimited.

### Named species: 47 + "Random" = 48 entries

| Kind | Presets |
| --- | --- |
| Fish | 18 - Goldfish, Clownfish, Angelfish, Tuna, Eel, Pufferfish, Betta, Neon tetra, Koi, Discus, Mackerel, Piranha, Yellow tang, Blue tang, Rainbow trout, Atlantic salmon, Great barracuda, Ocean sunfish |
| Shark | 8 - Tiger, Great white, Hammerhead, Whale, Mako, Blacktip reef, Blue, Nurse |
| Whale / dolphin | 6 - Orca, Bottlenose dolphin, Common dolphin, Beluga, Humpback, Sperm whale |
| Ray | 5 - Cownose, Manta, Spotted eagle, Stingray, Bluespotted stingray |
| Squid / cuttlefish | 3 - Squid, Giant squid, Cuttlefish |
| Octopus | 3 - Octopus, Blue-ringed, Dumbo |
| Urchin | 4 - Purple, Long-spined, Red pencil, Green |

Eight of those have colour-morph tables (Goldfish 4, Clownfish 2, Angelfish 3,
Betta 5, Koi 5, Discus 3, Manta 2, Cuttlefish 2), so there are **65 distinct
named species + morph looks**.

### Individuals: effectively unlimited

Every animal is a ~118-gene genome and most genes are continuous doubles drawn
from the seed, so a seed does not select one of a few variants - it produces a
unique individual:

- **7 kinds**, each with seed-selectable sub-variants: shark (requiem /
  hammerhead), whale (whale / dolphin), ray (eagle / stingray), squid (squid /
  cuttlefish), octopus (octopus / dumbo), urchin (short / long), and fish with 5
  body archetypes (slender, torpedo, classic, deep, round).
- **2^64 seeds per kind** - about **1.8 x 10^19 individuals per kind**, and
  **~1.3 x 10^20 across all seven** - before counting that presets vary too
  ("Koi, seed 3" and "Koi, seed 4" are recognisably koi but not twins).
- The discrete building blocks alone already multiply past counting: 5 body
  archetypes x 9 tail shapes x 6 fin shapes x 3 eye styles x 3 wing shapes x 26
  markings x 2 lobe types, plus booleans (teeth, pleats, vertical swim, ...) and
  continuous hues, proportions and fin placements.

In practice you will never see the same fish twice. The tank's opening fish and
every **Add a random fish** pick are both drawn fresh, so no two launches repeat
the same animal.

**Total generatable creatures: 1,328,165,573,307,087,716,352** - over
**1.3 sextillion** unique animals, each one reachable from a single seed.

The count, in one line:

```
N = S x (P + K) = 2^64 x (65 + 7) = 72 x 2^64

  = 1,328,165,573,307,087,716,352

S = 2^64 seeds            P = 65 named species + morph looks
K = 7 random kinds        (17 if a sub-variant is pinned: 1,512,633,014,044,183,232,512)
```

The genome itself has far more room than a seed can reach - ~78 continuous
genes at 53 bits each times the discrete choices is roughly `10^1256`
combinations - so `2^64` is the limit of what the generator can actually be
asked for, and the total above is what it can produce.

## Build

```
cmake -S . -B build -A x64
cmake --build build --config Release
build/run/Release/aquarium.exe      # multi-config generator appends the config
```

or, without CMake (MSVC only):

```
./build.sh          # Git Bash / MSYS
./build.ps1         # PowerShell, no bash needed
bin/aquarium.exe
```

The release build is one self-contained **884 KB** executable (`905,216 bytes`)
with no third-party DLLs, no runtime install and no assets folder - the 1.3
sextillion creatures, the sand, the rocks and the reef are all generated inside
it.

## Run

```
aquarium.exe                      # 1200x680, bottom-right of the work area, 1 fish
aquarium.exe --fish 12 --seed 99
aquarium.exe --corals 16          # seed the reef with 16 corals, plants and polyps
aquarium.exe --width 1600 --height 900
aquarium.exe --seconds 20         # auto-close (for checks)
aquarium.exe --dump-frame out/frame.png --fish 6   # headless: no window, writes a PNG

aquarium.exe --water off          # fish on a bare desktop, no water, no sand
aquarium.exe --water-preset my.ini --water-set caustic_strength=0.4
aquarium.exe --water-restore-defaults             # rewrite water.ini from defaults
aquarium.exe --night 1 --clock 12 # pin the light instead of following the hour
aquarium.exe --dump-frame f.png --dump-layers out/layer   # also dump content/masks/raw

aquarium.exe --save-tank out/tank.ini             # headless: save the tank as it starts
aquarium.exe --load-tank out/tank.ini             # open the saved sand, corals and fish
aquarium.exe --load-tank out/tank.ini --dump-frame f.png  # check a save without a window
```

Hotkeys (global, work while another app has focus):

| Keys | Action |
| --- | --- |
| `Ctrl+Shift+Q` | quit |
| `Ctrl+Shift+N` | new tank (new seed) |
| `Ctrl+Shift+R` | reload `water.ini` |
| `Ctrl+Shift+W` | water on / off |
| `Ctrl+Shift+T` | night on / off |
| `Ctrl+Shift+S` | save the tank to `tank.ini` |
| `Ctrl+Shift+L` | load `tank.ini` |
| `Ctrl+Shift+A` | add a random fish |

Right-clicking a fish opens the same menu, plus **Add a random fish**: one extra
animal is generated on a worker thread and appears in place, while the rest of
the tank keeps swimming. It stops at 128 fish (`kMaxTankFish`), where the entry
greys out. Every added fish is a fresh random individual, and **Ctrl+Shift+A**
does the same thing without the mouse. The menu also carries **Remove this
fish** - see [Clicking and removing fish](#clicking-and-removing-fish).

**Add a random coral** grows the reef the same way: one new seeded coral,
plant or polyp appears on the sand and the ones already there keep their place.
It stops at 24 (`kMaxCorals`). Like an added fish it lives in the session - use
`coral_count` in `water.ini`, or **Ctrl+Shift+S**, to keep the reef.

**Full screen on this monitor** resizes the layered window to the monitor's full
bounds. Because the aquarium is chrome-less and click-through per pixel, the
taskbar underneath still gets its clicks wherever no fish is swimming. The menu
entry stays checked while it applies, and unchecking it returns to the size the
window had before.

**Swap monitor** moves the tank to the next screen and cycles: monitors are
sorted left-to-right (then top-to-bottom), so the order is predictable rather
than whatever order the display driver reports. While full screen it fills the
new monitor; otherwise it re-centres at the same windowed size, clamped to fit.
With a single monitor the entry is greyed out.

`tools/probe_window.ps1` reads the layered/topmost/visible styles back out of
Windows and samples a `WM_NCHITTEST` grid, so the transparency and hit-testing
setup can be verified rather than assumed. `tools/check_water.py` runs the
headless dump and checks the water layer's pixel invariants and alpha budget.

## Credits

- Fish generator: [fish-gen](https://github.com/JacobKulberg/fish-gen) (MIT) as ported to `RoguelikeGame/src/creature/sea`.
- Transparent-window recipe: [VRMPet](https://github.com/camenduru/VRMPet).
- Water effect: reimplemented from **pixel-art-water-vfx** (Godot 4, no licence
  shipped) - its design document and parameter semantics, not its shader text.
