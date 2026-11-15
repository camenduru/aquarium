# ================================================================
# AQUARIUM - does clicking a fish startle it?
# ----------------------------------------------------------------
# Drives the real window with one fish: save the tank, post a real
# WM_LBUTTONUP on that fish's own position (from the save, so no hit
# test is needed to find it), wait, save again, and compare the travel
# with the fish's cruise speed.
#
#   powershell -File tools\poke_check.ps1
#
# The alarm lasts 1.6 s at up to 2.8x cruise speed, so over a 0.8 s gap
# the poked fish should cover ~2.3x its normal distance, away from the
# click.
# ================================================================
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class W
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
'@

function Save-Tank($name) {
    $ws.SendKeys('^+s')
    Start-Sleep -Milliseconds 400
    Copy-Item bin\tank.ini (Join-Path $dir $name) -Force
}

function OnlyFish($name) {
    $line = (Get-Content (Join-Path $dir $name)) -match '^fish_1 = '
    $f = ($line -split ' = ')[1].Split('|')
    return [pscustomobject]@{
        species = $f[0]; x = [double]$f[2]; baseY = [double]$f[3]
        speed = [double]$f[4]; dir = [int]$f[5]
    }
}

function Startles { return @(Select-String -Path $log -Pattern 'startled').Count }

function Poke($x, $y) {
    $lp = [IntPtr]((($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF))
    [void][W]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp)   # WM_LBUTTONUP
}

Stop-Process -Name aquarium -ErrorAction SilentlyContinue
Remove-Item bin\tank.ini -ErrorAction SilentlyContinue
$dir = (New-Item -ItemType Directory -Force out\poke).FullName
$log = Join-Path $dir 'log.txt'
$exe = (Resolve-Path bin\aquarium.exe).Path
$p = Start-Process -FilePath $exe -ArgumentList '--seed', '99', '--fish', '1' -PassThru -RedirectStandardOutput $log
Start-Sleep -Seconds 10

$hwnd = [W]::FindWindow('AquariumOverlay', 'Aquarium')
if ($hwnd -eq [IntPtr]::Zero) { throw 'window not found' }

$ws = New-Object -ComObject WScript.Shell
Save-Tank 'a.ini'
$before = OnlyFish 'a.ini'
Write-Output ("fish: {0}, x {1:N1}, baseY {2:N1}, speed {3:N1}, dir {4}" -f `
    $before.species, $before.x, $before.baseY, $before.speed, $before.dir)

# Click 20 px left of the fish, i.e. on its body from the left side, so the
# escape direction is unambiguous: it should bolt to the right.
$clickX = [int]($before.x - 20)
$clickY = [int]$before.baseY
$n0 = Startles
$t0 = Get-Date
Poke $clickX $clickY
Start-Sleep -Milliseconds 800
$t1 = Get-Date
Save-Tank 'b.ini'
$gap = ($t1 - $t0).TotalSeconds

$ws.SendKeys('^+q')
Start-Sleep -Seconds 3
if (-not $p.HasExited) { Stop-Process -Name aquarium -Force }

$hit = (Startles) -gt $n0
$after = OnlyFish 'b.ini'
$dx = $after.x - $before.x
$ratio = [math]::Abs($dx) / ($before.speed * $gap)

Write-Output ("click {0},{1}  gap {2:N2} s" -f $clickX, $clickY, $gap)
Write-Output ("hit: {0}   dx {1:N1} px   dir {2} -> {3}   travel/speed {4:N2}x" -f `
    $hit, $dx, $before.dir, $after.dir, $ratio)

$ok = $hit -and ($ratio -gt 1.5) -and ($dx -gt 0)
Write-Output ''
if ($ok) { Write-Output '[ok] the clicked fish bolted away from the pointer' }
else { Write-Output '[FAIL] the click did not startle the fish as expected' }
exit ([int](-not $ok))
