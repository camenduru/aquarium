# Aquarium

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

Every frame:

1. `Clear()` the `CreateDIBSection` canvas to transparent.
2. `BlendSprite()` each fish cell into it as **premultiplied** BGRA
   (`dst = src + dst*(255-a)/255`). `UpdateLayeredWindow` with `AC_SRC_ALPHA`
   requires premultiplied colour; skipping it leaves a halo.
3. `UpdateLayeredWindow(..., ULW_ALPHA)`, 60 fps.

`WS_EX_TRANSPARENT` is deliberately **not** set: that would make the whole
window click-through, fish included. `WM_NCHITTEST` answers `HTTRANSPARENT` for
a transparent canvas pixel and `HTCLIENT` for a fish pixel, so the desktop keeps
its clicks and a fish can still be right-clicked.

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

### 3. The screen: once per frame, by Windows

`Push()` hands the finished bitmap to `UpdateLayeredWindow(..., ULW_ALPHA)` at
60 fps and the Desktop Window Manager composites it over the desktop. Because
transparent pixels carry no colour at all, the desktop shows through them; the
same alpha channel decides clicks, since `WM_NCHITTEST` answers `HTTRANSPARENT`
below an alpha of 8 and `HTCLIENT` on anything more solid.

Cost is CPU cycles per pixel touched, not GPU time: one fish at 2x covers
128x128 = 16,384 pixels, so a 128-fish tank is about **2.1 million pixel blends
per frame**. That is the budget this design spends in exchange for per-pixel
alpha, which a Direct3D swap chain on an `HWND` cannot provide at all.

## Build

```
cmake -S . -B build -A x64
cmake --build build --config Release
build/run/Release/aquarium.exe      # multi-config generator appends the config
```

or, without CMake (MSVC only):

```
./build.sh
bin/aquarium.exe
```

## Run

```
aquarium.exe                      # 1200x680, bottom-right of the work area, 8 fish
aquarium.exe --fish 12 --seed 99
aquarium.exe --width 1600 --height 900
aquarium.exe --seconds 20         # auto-close (for checks)
aquarium.exe --dump-frame out/frame.png --fish 6   # headless: no window, writes a PNG
```

Hotkeys (global, work while another app has focus):

| Keys | Action |
| --- | --- |
| `Ctrl+Shift+Q` | quit |
| `Ctrl+Shift+N` | new tank (new seed) |

Right-clicking a fish opens the same menu, plus **Add a random fish**: one extra
animal is generated on a worker thread and appears in place, while the rest of
the tank keeps swimming. It stops at 16 fish, where the entry greys out.

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
Windows, so the transparency setup can be verified rather than assumed.

## Credits

- Fish generator: [fish-gen](https://github.com/JacobKulberg/fish-gen) (MIT) as ported to `RoguelikeGame/src/creature/sea`.
- Transparent-window recipe: [VRMPet](https://github.com/camenduru/VRMPet).
