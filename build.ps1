# ================================================================
# AQUARIUM - direct MSVC build (PowerShell twin of build.sh)
# ----------------------------------------------------------------
# Same as build.sh but with no bash dependency: toolchain paths are
# set explicitly, no vcvars64.
#
#   ./build.ps1              release build -> bin/aquarium.exe
#   ./build.ps1 debug        /Od + symbols, for a debugger
# ================================================================
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

$VSW  = 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC'
$KITS = 'C:/Program Files (x86)/Windows Kits/10'
if (-not (Test-Path $VSW)) { Write-Error "Visual Studio not found at $VSW" }

$Toolset = (Get-ChildItem $VSW | Sort-Object -Property Name | Select-Object -Last 1).Name
$Sdk     = (Get-ChildItem "$KITS/Include" | Sort-Object -Property Name | Select-Object -Last 1).Name

$env:INCLUDE = "$VSW/$Toolset/include;$KITS/Include/$Sdk/ucrt;$KITS/Include/$Sdk/shared;$KITS/Include/$Sdk/um"
$env:LIB     = "$VSW/$Toolset/lib/x64;$KITS/Lib/$Sdk/ucrt/x64;$KITS/Lib/$Sdk/um/x64"
$env:Path    = "$VSW/$Toolset/bin/Hostx64/x64;$env:Path"

# The tank is usually left running while it is being worked on, and Windows
# refuses to relink an executable that is still loaded (LNK1104).
Stop-Process -Name aquarium -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$Opt = '/O2'
$Defs = ''
if ($args[0] -eq 'debug') { $Opt = '/Od /Zi'; $Defs = '/D_DEBUG' }

New-Item -ItemType Directory -Force -Path bin\obj | Out-Null
New-Item -ItemType Directory -Force -Path out | Out-Null

$Sea = Get-ChildItem src/creature/sea/*.cpp | ForEach-Object { $_.FullName }

Write-Host '--- compiling aquarium'
$Args2 = @(
    '/nologo','/std:c++20','/EHsc',$Opt,$Defs,
    '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DUNICODE','/D_UNICODE',
    '/D_CRT_SECURE_NO_WARNINGS','/utf-8','/Isrc',
    "/Fo:bin/obj/","/Fe:bin/aquarium.exe",
    (Resolve-Path src/main.cpp).Path,
    (Resolve-Path src/tank.cpp).Path,
    (Resolve-Path src/transparent_window.cpp).Path,
    (Resolve-Path src/creature/CreatureSheetExport.cpp).Path
) + $Sea + @(
    '/link','/SUBSYSTEM:WINDOWS','user32.lib','gdi32.lib','shell32.lib'
)
& cl @Args2
if ($LASTEXITCODE -ne 0) { Write-Error "[FAIL] cl exit $LASTEXITCODE" }
Write-Host '[ok] bin/aquarium.exe'
