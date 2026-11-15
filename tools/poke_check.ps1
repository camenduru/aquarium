# ================================================================
# AQUARIUM - does clicking a fish startle it, and by how much?
# ----------------------------------------------------------------
# Drives the real window with one fish. It saves the tank, posts a real
# WM_LBUTTONUP just beside that fish, waits, saves again, and compares
# the travel against the speed the save itself says the tank is tuned to
# (`escape_speed`). No hit test is needed to find the fish: the save
# gives its position, and PokeAt's own alpha test decides the click.
#
#   powershell -File tools\poke_check.ps1
#   powershell -File tools\poke_check.ps1 -ExtraArgs '--water-set','escape_speed=5'
#
# The alarm lasts 1.6 s and eases from escape_speed back to 1, so over a
# gap T the fish travels speed*(T + (S-1)*(T - T^2/3.2)) in the escape
# direction.
# ================================================================
param([string[]] $ExtraArgs = @())
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
    $ws.SendKeys('^+s')          # the snapshot is taken as the hotkey arrives
    $t = Get-Date
    Start-Sleep -Milliseconds 400
    Copy-Item bin\tank.ini (Join-Path $dir $name) -Force
    return $t
}

function Read-Fish($name) {
    $lines = Get-Content (Join-Path $dir $name)
    $f = (($lines -match '^fish_1 = ')[0] -split ' = ')[1].Split('|')
    $esc = ($lines -match '^escape_speed = ')[0]
    return [pscustomobject]@{
        species = $f[0]; x = [double]$f[2]; baseY = [double]$f[3]
        speed = [double]$f[4]; dir = [int]$f[5]
        escape = if ($esc) { [double](($esc -split ' = ')[1]) } else { 2.8 }
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
$p = Start-Process -FilePath $exe -ArgumentList (@('--seed', '99', '--fish', '1') + $ExtraArgs) -PassThru -RedirectStandardOutput $log
Start-Sleep -Seconds 10

$hwnd = [W]::FindWindow('AquariumOverlay', 'Aquarium')
if ($hwnd -eq [IntPtr]::Zero) { throw 'window not found' }
$size = (Select-String -Path $log -Pattern '(\d+)x(\d+) at').Matches[0]
$winW = [int]$size.Groups[1].Value

$ws = New-Object -ComObject WScript.Shell
$tA = Save-Tank 'a.ini'
$before = Read-Fish 'a.ini'

# Where the fish is now: the save is a few hundred ms old, and it keeps
# swimming while we get here. Poke just beside the centre, on the side with
# room to run.
$now = Get-Date
$xNow = $before.x + $before.dir * $before.speed * ($now - $tA).TotalSeconds
$xNow = [math]::Min([math]::Max($xNow, 90), $winW - 90)
$roomRight = $winW - 78 - $xNow
$roomLeft  = $xNow - 78
if ($roomRight -ge $roomLeft) { $away = 1;  $clickX = [int]($xNow - 6) }
else                          { $away = -1; $clickX = [int]($xNow + 6) }
$clickY = [int]$before.baseY

Write-Output ("fish: {0}, x {1:N1}, speed {2:N1}, escape_speed {3:N1}, room {4:N0}/{5:N0}" -f `
    $before.species, $xNow, $before.speed, $before.escape, $roomLeft, $roomRight)

$n0 = Startles
$t0 = Get-Date
Poke $clickX $clickY
Start-Sleep -Milliseconds 800
$tB = Save-Tank 'b.ini'
$gap = ($tB - $t0).TotalSeconds

$ws.SendKeys('^+q')
Start-Sleep -Seconds 3
if (-not $p.HasExited) { Stop-Process -Name aquarium -Force }

$after = Read-Fish 'b.ini'
$dx = $after.x - $before.x
$s  = $before.escape
$expected = $away * $before.speed * ($gap + ($s - 1.0) * ($gap - $gap * $gap / 3.2))
$err = if ($expected -ne 0) { [math]::Abs($dx - $expected) / [math]::Abs($expected) } else { 1 }

Write-Output ("click {0},{1}  gap {2:N2} s  startled: {3}" -f $clickX, $clickY, $gap, ((Startles) -gt $n0))
Write-Output ("dx {0:N1} px (expected {1:N1}), facing {2} (expected {3}), error {4:P0}" -f `
    $dx, $expected, $after.dir, $away, $err)

$ok = ((Startles) -gt $n0) -and ($err -lt 0.30) -and ($after.dir -eq $away)
Write-Output ''
if ($ok) { Write-Output '[ok] the clicked fish bolted away at the configured speed' }
else { Write-Output '[FAIL] the click did not startle the fish as expected' }
exit ([int](-not $ok))
