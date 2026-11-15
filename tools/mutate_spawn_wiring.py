"""Mutation-test tools/check_spawn_wiring.py.

Each mutation rewrites one statement in src/tank.cpp, runs the guard, and
restores the bytes AND the mtime (a bumped mtime makes the sources look newer
than the exe and the next build recompiles for nothing).
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "tank.cpp"
PY = sys.executable

MUTATIONS = [
    ("job species is the bare literal (the real bug)",
     [(b"CreatureJob{ species, salt }", b'CreatureJob{ "Random", salt }')], "fail"),
    ("the stored species is replaced by a literal",
     [(b"const std::string species = added.species;",
       b'const std::string species = std::string("Random");')], "fail"),
    ("Start() hands the worker a literal",
     [(b"CreatureJob{ f.species, f.seed }", b'CreatureJob{ "Random", f.seed }')], "fail"),
    ("GenomeFor stops honouring the pinned kind",
     [(b"return sea::MakeGenome(sea::kRandomSpecies, seed, nullptr, kind);",
       b"return sea::MakeGenome(sea::kRandomSpecies, seed, nullptr);")], "fail"),
    ("renaming the local, declaration and use together, must still pass",
     [(b"const std::string species = added.species;",
       b"const std::string spawned = added.species;"),
      (b"CreatureJob{ species, salt }", b"CreatureJob{ spawned, salt }")], "pass"),
]

original = SRC.read_bytes()
st = SRC.stat()

bad = 0
for name, edits, expect in MUTATIONS:
    mutated = original
    missing = False
    for find, repl in edits:
        if find not in mutated:
            print(f"[SKIP] {name}: pattern not present")
            missing = True
            break
        mutated = mutated.replace(find, repl, 1)
    if missing:
        bad += 1
        continue
    try:
        SRC.write_bytes(mutated)
        r = subprocess.run([PY, str(ROOT / "tools" / "check_spawn_wiring.py")],
                           capture_output=True, text=True)
        got = "pass" if r.returncode == 0 else "fail"
        mark = "ok  " if got == expect else "BAD "
        if got != expect:
            bad += 1
        print(f"[{mark}] {name}: guard {got} (expected {expect})")
        if got != expect:
            print("        " + "\n        ".join(r.stdout.strip().splitlines()[-4:]))
    finally:
        SRC.write_bytes(original)
        os.utime(SRC, ns=(st.st_atime_ns, st.st_mtime_ns))

print()
print("[ok] the guard fails on every mutation that breaks the wiring"
      if bad == 0 else f"[FAIL] {bad} mutation(s) behaved unexpectedly")
sys.exit(1 if bad else 0)
