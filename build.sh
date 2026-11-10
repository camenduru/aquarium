#!/usr/bin/env bash
# ================================================================
# AQUARIUM - direct MSVC build
# ----------------------------------------------------------------
# cmd.exe is blocked in the agent sandbox, so vcvars64.bat cannot run;
# this exports the same INCLUDE/LIB/PATH by hand (the same trap list as
# RoguelikeGame/tools/build_sea.sh):
#   1. toolchain paths are set explicitly, no vcvars
#   2. MSYS rewrites "/nologo"-style flags, so MSYS2_ARG_CONV_EXCL='*'
#   3. MSVC/SDK include dirs contain spaces, so they are passed as quoted
#      Windows paths with no space after /I
#
#   ./build.sh          release build -> bin/aquarium.exe
#   ./build.sh debug    /Od + symbols, for a debugger
# ================================================================
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

VS_VC_UNIX='/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC'
KITS_UNIX='/c/Program Files (x86)/Windows Kits/10'
VSW='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC'
KITSW='C:/Program Files (x86)/Windows Kits/10'

if [ ! -d "$VS_VC_UNIX" ]; then
    echo "ERROR: Visual Studio not found at $VSW" >&2
    exit 1
fi
TOOLSET="$(ls -1 "$VS_VC_UNIX" | sort -V | tail -1)"
SDK="$(ls -1 "$KITS_UNIX/Include" | sort -V | tail -1)"

export INCLUDE="$VSW/$TOOLSET/include;$KITSW/Include/$SDK/ucrt;$KITSW/Include/$SDK/shared;$KITSW/Include/$SDK/um"
export LIB="$VSW/$TOOLSET/lib/x64;$KITSW/Lib/$SDK/ucrt/x64;$KITSW/Lib/$SDK/um/x64"
export PATH="$VS_VC_UNIX/$TOOLSET/bin/Hostx64/x64:$PATH"
export MSYS2_ARG_CONV_EXCL='*'

# The tank is usually left running while it is being worked on, and Windows
# refuses to relink an executable that is still loaded (cl reports LNK1104),
# so close ours first. Any other app called aquarium.exe goes too - the build
# always produces this one, so that is the right trade.
if taskkill /F /IM aquarium.exe >/dev/null 2>&1; then
    echo "--- closed the running aquarium.exe so the link can replace it"
    sleep 1
fi

OPT=/O2
DEFS=''
if [ "${1:-}" = "debug" ]; then
    OPT='/Od /Zi'
    DEFS='/D_DEBUG'
fi

mkdir -p bin out
OBJ=bin/_obj
mkdir -p "$OBJ"

SEA=$(ls src/creature/sea/*.cpp)

echo "--- compiling aquarium"
# /Fo and /Fe are COMPILER options: anything after /link goes to the linker,
# which ignores them (LNK4044) and quietly writes the default name instead.
cl /nologo /std:c++20 /EHsc $OPT $DEFS \
    /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /utf-8 /Isrc \
    "/Fo:$OBJ/" "/Fe:bin/aquarium.exe" \
    src/main.cpp src/tank.cpp src/transparent_window.cpp \
    src/creature/CreatureSheetExport.cpp \
    $SEA \
    /link /SUBSYSTEM:WINDOWS \
    user32.lib gdi32.lib shell32.lib

rc=$?
if [ "$rc" -ne 0 ]; then
    echo "[FAIL] cl exit $rc" >&2
    exit "$rc"
fi
echo "[ok] bin/aquarium.exe"
