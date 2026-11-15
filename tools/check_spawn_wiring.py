#!/usr/bin/env python3
"""Does every spawned fish carry the kind that was actually drawn?

The bug this guards against: `Tank::AddFish` drew a kind from the `[fish]`
weights, stored it on the fish as `Random:Fish`, and then handed the worker the
bare literal `"Random"` - which falls into `GenomeFor`'s legacy-save branch and
picks a kind from the seed at EVEN odds. Every added fish was then 1 in 7 an
exotic instead of ~1 in 50, and the sheet disagreed with the species the tank
had recorded (and so with the save).

Nothing about that is visible in the save, the console or the API: the recorded
species was always right, only the pixels were wrong. So it is checked here, at
the wiring, by statement rather than by name.

    python tools/check_spawn_wiring.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "tank.cpp"

FAILURES: list[str] = []


def ok(msg: str) -> None:
    print(f"  ok    {msg}")


def fail(msg: str) -> None:
    FAILURES.append(msg)
    print(f"  FAIL  {msg}")


def strip_comments(s: str) -> str:
    out = []
    i, n = 0, len(s)
    while i < n:
        if s.startswith("//", i):
            j = s.find("\n", i)
            i = n if j < 0 else j
        elif s.startswith("/*", i):
            j = s.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def strip_literals(s: str) -> str:
    """Remove string/char literal bodies so braces inside them cannot confuse
    the body extraction. Literals become "" / '' placeholders."""
    out = []
    i, n = 0, len(s)
    while i < n:
        c = s[i]
        if c in "\"'":
            quote = c
            i += 1
            while i < n:
                if s[i] == "\\":
                    i += 2
                    continue
                if s[i] == quote:
                    i += 1
                    break
                i += 1
            out.append(quote * 2)
        else:
            out.append(c)
            i += 1
    return "".join(out)


def body_of(code: str, signature: str) -> str:
    """The brace-balanced body of the first definition of `signature`.

    Balanced, not "up to a column-0 brace": a member body closes on an
    INDENTED brace, and stopping at the first column-0 one swallows the next
    function - which is how a guard ends up matching a name that lives
    somewhere else entirely.
    """
    m = re.search(re.escape(signature) + r"\s*\([^)]*\)\s*(?:const\s*)?\{", code)
    if m is None:
        raise SystemExit(f"cannot find the definition of {signature} in {SRC}")
    start = m.end() - 1
    depth = 0
    for i in range(start, len(code)):
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return code[start + 1 : i]
    raise SystemExit(f"unbalanced braces in {signature}")


def first_field(code: str, pos: int) -> str:
    """The first initialiser of a `CreatureJob{...}` starting at `pos` (just
    after the opening brace), up to the first depth-0 comma."""
    depth = 0
    out = []
    for i in range(pos, len(code)):
        c = code[i]
        if c in "({[":
            depth += 1
        elif c in ")}]":
            if depth == 0:
                break
            depth -= 1
        elif c == "," and depth == 0:
            break
        out.append(c)
    return "".join(out).strip()


def main() -> int:
    raw = SRC.read_text(encoding="utf-8", errors="replace")
    code = strip_literals(strip_comments(raw))

    print(f"checking {SRC.name}")

    # 1. No job may name its species with a literal. A literal bypasses the
    #    draw entirely and means the sheet's kind is not the tank's species.
    jobs = [(m.start(), first_field(code, m.end())) for m in re.finditer(r"CreatureJob\s*\{", code)]
    if not jobs:
        fail("no CreatureJob initialiser found - the spawn path moved")
    for pos, field in jobs:
        line = code.count("\n", 0, pos) + 1
        if field.startswith(('"', "'")) or field == '""':
            fail(f"tank.cpp:{line}: a job's species is the literal {field} - "
                 f"that picks a kind from the seed at even odds, not from the weights")
    if jobs and not FAILURES:
        ok(f"{len(jobs)} job initialiser(s), none with a literal species")

    # 2. AddFish must hand the worker the species it stored on the fish.
    add = body_of(code, "bool Tank::AddFish")
    if re.search(r"added\.species\s*=", add) is None:
        fail("Tank::AddFish no longer assigns added.species - the draw moved")
    m = re.search(r"CreatureJob\s*\{", add)
    if m is None:
        fail("Tank::AddFish builds no CreatureJob")
    else:
        field = first_field(add, m.end())
        if field == "added.species":
            ok("AddFish's job carries added.species directly")
        elif re.search(rf"\b(?:const\s+)?(?:std::string|auto)\s+{re.escape(field)}\s*=\s*added\.species\b", add):
            ok(f"AddFish's job carries `{field}`, assigned from added.species")
        else:
            fail(f"Tank::AddFish's job carries `{field}`, which is not added.species "
                 f"(the drawn kind) - the sheet will not match the stored species")

    # 3. The opening school and a restored tank must both carry f.species.
    for sig, label in (("void Tank::Start", "Start"), ("bool Tank::Restore", "Restore")):
        body = body_of(code, sig)
        m = re.search(r"CreatureJob\s*\{", body)
        if m is None:
            fail(f"Tank::{label} builds no CreatureJob")
            continue
        field = first_field(body, m.end())
        if re.search(r"\.species$", field):
            ok(f"{label}'s job carries {field}")
        else:
            fail(f"Tank::{label}'s job carries `{field}`, not a .species")

    # 4. The fix only works because a pinned kind is honoured. If GenomeFor's
    #    ParseRandomSpecies branch stopped passing `kind`, every spawn would
    #    fall back to the seed-drawn kind and the wiring above would be moot.
    genome_for = body_of(code, "sea::SeaGenome GenomeFor")
    if re.search(r"ParseRandomSpecies\s*\([^)]*\)[^{};]*MakeGenome\s*\([^;]*\bkind\b", genome_for, re.S):
        ok("GenomeFor honours the pinned kind for Random:<Kind>")
    else:
        fail("GenomeFor no longer passes the parsed kind to MakeGenome - "
             "a Random:<Kind> species would be rebuilt at the seed's own kind")

    print()
    if FAILURES:
        print(f"[FAIL] {len(FAILURES)} wiring problem(s)")
        return 1
    print("[ok] every spawned fish carries the kind that was drawn")
    return 0


if __name__ == "__main__":
    sys.exit(main())
