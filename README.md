# Aquarium

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

`tools/probe_window.ps1` reads the layered/topmost/visible styles back out of
Windows, so the transparency setup can be verified rather than assumed.

## Credits

- Fish generator: `fish-gen` (MIT) as ported to `RoguelikeGame/src/creature/sea`.
- Transparent-window recipe: VRMPet.
