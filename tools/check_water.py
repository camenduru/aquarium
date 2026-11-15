#!/usr/bin/env python3
"""AQUARIUM - water layer checks.

Mirrors the demo's evidence discipline: every claim about the water is a
measurement from a dump, not a claim. Runs the headless dump a few times and
checks the plan's list (§10):

  1  determinism      two runs with the same seed/clock are byte-identical
  2  2x pixel grid    every 2x2 block of the window is one logical pixel
  3  premultiplied    no BGRA channel exceeds alpha in the raw canvas
  4  containment      nothing is drawn outside the water mask
  6  frozen time      (covered by 1: the clock is explicit in both runs)
 12  ghost budget     mean/max alpha of the water body stays readable

Usage:
    python tools/check_water.py [--exe bin/aquarium.exe] [--out out/water-check]
                               [--fish 6] [--seed 99] [--seconds 3] [--clock 12]

Exit code 0 when every check passes, 1 otherwise. The PNGs and the raw
premultiplied canvas are left in --out for inspection.
"""

import argparse
import hashlib
import os
import struct
import subprocess
import sys
import zlib

try:
    from PIL import Image, ImageFilter
except ImportError:
    Image = None
    ImageFilter = None


# ---- PNG reading without Pillow -------------------------------------------
# The dump is written by our own writer (stored deflate, RGBA8), but Pillow is
# present on most machines; fall back to a tiny reader when it is not.

def read_png(path):
    """Return (width, height, rgba bytes)."""
    if Image is not None:
        im = Image.open(path).convert("RGBA")
        return im.width, im.height, im.tobytes()

    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG: " + path)
    pos = 8
    idat = b""
    w = h = 0
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            w, h, depth, colour = struct.unpack(">IIBB", body[:10])
            if depth != 8 or colour != 6:
                raise ValueError("expected RGBA8: " + path)
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = w * 4
    out = bytearray(stride * h)
    for y in range(h):
        line = raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)]
        out[y * stride:(y + 1) * stride] = line
    return w, h, bytes(out)


# ---- checks ---------------------------------------------------------------

def check_blocks(w, h, rgba):
    """Every 2x2 window block must carry one logical pixel."""
    bad = 0
    for y in range(0, h - 1, 2):
        row0 = y * w * 4
        row1 = (y + 1) * w * 4
        for x in range(0, w - 1, 2):
            i = x * 4
            if (rgba[row0 + i:row0 + i + 4] != rgba[row0 + i + 4:row0 + i + 8]
                    or rgba[row0 + i:row0 + i + 4] != rgba[row1 + i:row1 + i + 4]
                    or rgba[row0 + i:row0 + i + 4] != rgba[row1 + i + 4:row1 + i + 8]):
                bad += 1
    return bad


def check_premultiplied(path, w, h):
    with open(path, "rb") as f:
        raw = f.read()
    if len(raw) != w * h * 4:
        return -1
    bad = 0
    for i in range(0, len(raw), 4):
        b, g, r, a = raw[i], raw[i + 1], raw[i + 2], raw[i + 3]
        if b > a or g > a or r > a:
            bad += 1
    return bad


def check_containment(lw, lh, scale, fw, frame, content, masks):
    """Nothing outside the water mask may carry alpha.

    `masks` and `content` are logical resolution, `frame` is the window canvas
    at `scale` times that.
    """
    outside = 0
    bad_frame = 0
    bad_content = 0
    for y in range(lh):
        for x in range(lw):
            if masks[(y * lw + x) * 4] >= 128:   # R channel of the mask PNG
                continue
            outside += 1
            if content[(y * lw + x) * 4 + 3] != 0:
                bad_content += 1
            for by in range(scale):
                row = (y * scale + by) * fw
                for bx in range(scale):
                    if frame[(row + x * scale + bx) * 4 + 3] != 0:
                        bad_frame += 1
    return outside, bad_frame, bad_content


def ghost_budget(lw, lh, scale, fw, frame, art):
    """Mean water alpha inside the tank, and inside it excluding the art.

    `art` is the content alpha, dilated a few pixels: a water pixel can
    legitimately sample a fish through refraction, so the "open water" set
    must stay clear of the art it may refract.
    """
    total = 0
    water = 0
    water_n = 0
    sand = 0
    sand_n = 0
    peak = 0
    bright = 0
    for y in range(lh):
        for x in range(lw):
            li = (y * lw + x) * 4
            fi = ((y * scale) * fw + x * scale) * 4
            a = frame[fi + 3]
            if a == 0:
                continue
            total += a
            if art[li + 3] > 8:
                sand += a
                sand_n += 1
            else:
                water += a
                water_n += 1
                peak = max(peak, a)
                if a > 102:            # above ~40 %: a highlight, not a veil
                    bright += 1
    mean_all = total / max(1, water_n + sand_n)
    mean_water = water / max(1, water_n)
    mean_sand = sand / max(1, sand_n)
    bright_fraction = bright / max(1, water_n)
    return mean_all, mean_water, mean_sand, peak, bright_fraction


def run_dump(exe, out_dir, layer_prefix, args):
    cmd = [exe, "--dump-frame", os.path.join(out_dir, "frame.png"),
           "--dump-layers", layer_prefix] + args
    result = subprocess.run(cmd, capture_output=True, text=True)
    lines = (result.stdout or "").strip().splitlines()
    return result.returncode, lines


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=os.path.join("bin", "aquarium.exe"))
    ap.add_argument("--out", default=os.path.join("out", "water-check"))
    ap.add_argument("--fish", default="6")
    ap.add_argument("--seed", default="99")
    ap.add_argument("--seconds", default="3")
    ap.add_argument("--clock", default="12")
    ap.add_argument("--water", default="ghost", choices=["ghost", "off"])
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    preset = os.path.join(args.out, "water.ini")
    if os.path.exists(preset):
        os.remove(preset)

    common = ["--fish", args.fish, "--seed", args.seed,
              "--seconds", args.seconds, "--clock", args.clock,
              "--water", args.water, "--water-preset", preset]

    failures = []
    print("aquarium water checks")
    print("  exe   ", args.exe)
    print("  out   ", args.out)

    # Run 1 also writes the layers the other checks read.
    prefix1 = os.path.join(args.out, "layer")
    code, log = run_dump(args.exe, args.out, prefix1, common + ["--water-restore-defaults"])
    for line in log:
        print("  |", line)
    if code != 0:
        failures.append("dump exited with %d" % code)

    frame_png = os.path.join(args.out, "frame.png")
    content_png = prefix1 + "-content.png"
    masks_png = prefix1 + "-masks.png"
    raw_path = prefix1 + "-raw.bgra"

    for path in (frame_png, content_png, masks_png, raw_path):
        if not os.path.exists(path):
            print("  [FAIL] missing", path)
            return 1

    # 1 + 6: determinism and frozen time.
    first_hash = sha(frame_png) if os.path.exists(frame_png) else ""
    prefix2 = os.path.join(args.out, "again")
    code2, _ = run_dump(args.exe, args.out, prefix2, common)
    same = code2 == 0 and first_hash != "" and sha(frame_png) == first_hash
    if same:
        print("  [ok] determinism: identical bytes at the same seed/clock")
    else:
        failures.append("determinism: two runs differ")
        print("  [FAIL] determinism: two runs differ")

    w, h, frame = read_png(frame_png)
    cw, ch, content = read_png(content_png)
    mw, mh, masks = read_png(masks_png)
    print("  frame %dx%d, content %dx%d (logical %dx%d)" % (w, h, cw, ch, w // 2, h // 2))

    # 2: the logical canvas is an exact 2x integer upscale.
    bad_blocks = check_blocks(w, h, frame)
    if bad_blocks == 0:
        print("  [ok] 2x blocks: every 2x2 block is one logical pixel")
    else:
        failures.append("2x blocks: %d uneven blocks" % bad_blocks)
        print("  [FAIL] 2x blocks: %d uneven blocks" % bad_blocks)

    # 3: premultiplied invariant on the canvas handed to the window.
    bad_premul = check_premultiplied(raw_path, w, h)
    if bad_premul == 0:
        print("  [ok] premultiplied: no channel exceeds alpha")
    else:
        failures.append("premultiplied: %d bad pixels" % bad_premul)
        print("  [FAIL] premultiplied: %d bad pixels" % bad_premul)

    # 4: containment.
    outside, bad_frame, bad_content = check_containment(
        cw, ch, w // cw, w, frame, content, masks)
    if bad_frame == 0 and bad_content == 0:
        print("  [ok] containment: %d pixels outside the mask, all empty" % outside)
    else:
        failures.append("containment: %d frame / %d content pixels outside the mask"
                        % (bad_frame, bad_content))
        print("  [FAIL] containment: %d frame / %d content pixels outside the mask"
              % (bad_frame, bad_content))

    # 12: ghost budget.
    art = content
    if Image is not None:
        alpha = Image.open(content_png).convert("RGBA").getchannel("A")
        alpha = alpha.filter(ImageFilter.MaxFilter(9)).tobytes()
        widened = bytearray(len(alpha) * 4)
        widened[3::4] = alpha
        art = bytes(widened)
    mean_all, mean_water, mean_sand, peak, bright = ghost_budget(
        cw, ch, w // cw, w, frame, art)
    print("  ghost alpha: mean %.1f%% overall, %.1f%% over open water, "
          "%.1f%% over art; peak water %.1f%%, %.2f%% of open water above 40%%"
          % (mean_all * 100 / 255, mean_water * 100 / 255,
             mean_sand * 100 / 255, peak * 100 / 255, bright * 100))
    if mean_all <= 0.20 * 255 and mean_water <= 0.20 * 255 and bright <= 0.01:
        print("  [ok] ghost budget: mean under 20%, highlights under 1% of the water")
    else:
        failures.append("ghost budget over cap (mean %.1f%%, water %.1f%%, highlights %.2f%%)"
                        % (mean_all * 100 / 255, mean_water * 100 / 255, bright * 100))
        print("  [FAIL] ghost budget over cap")

    print()
    if failures:
        for f in failures:
            print("  FAILED:", f)
        return 1
    print("all water checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
