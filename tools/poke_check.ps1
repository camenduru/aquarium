# ================================================================
# AQUARIUM - does clicking a fish startle it, at escape_speed?
# ----------------------------------------------------------------
# Drives the real window with one fish: save the tank, post a real
# WM_LBUTTONUP beside that fish, wait, save again, and compare the
# travel with the speed the save reports (`escape_speed`).
#
#   powershell -File tools\poke_check.ps1
#   powershell -File tools\poke_check.ps1 -ExtraArgs '--water-set','escape_speed=5'
#
# The click lands on the side the fish is already facing when that side
# has room, so it runs without reversing and the two saves bracket one
# steady escape. The alarm lasts 1.6 s and eases from S to 1, so over a
# gap T under 1.6 s the distance is speed*(T + (S-1)*(T - T^2/3.2)).
#
# Whoever else is clicking the tank can add startled fish and spoil the
# measurement; the check prints what it saw, so a rerun is enough.
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

# Where it is now: the save is a moment old and it keeps swimming.
$now = Get-Date
$xNow = $before.x + $before.dir * $before.speed * ($now - $tA).TotalSeconds
$xNow = [math]::Min([math]::Max($xNow, 90), $winW - 90)

# Run the way it already faces when that side has room; otherwise turn it.
$roomAhead = if ($before.dir -gt 0) { $winW - 78 - $xNow } else { $xNow - 78 }
$away = if ($roomAhead -gt 200) { $before.dir } else { -$before.dir }
$clickX = [int]($xNow - $away * 6)
$clickY = [int]$before.baseY

Write-Output ("fish: {0}, x {1:N1}, speed {2:N1}, escape_speed {3:N1}, room ahead {4:N0}" -f `
    $before.species, $xNow, $before.speed, $before.escape, $roomAhead)

$n0 = Startles
$t0 = Get-Date
Poke $clickX $clickY
Start-Sleep -Milliseconds 800
$tB = Save-Tank 'b.ini'

$ws.SendKeys('^+q')
Start-Sleep -Seconds 3
if (-not $p.HasExited) { Stop-Process -Name aquarium -Force }

$after = Read-Fish 'b.ini'
$T = ($tB - $tA).TotalSeconds
$s = $before.escape
$expected = $away * $before.speed * ($T + ($s - 1.0) * ($T - $T * $T / 3.2))
$dx = $after.x - $before.x
$err = [math]::Abs($dx - $expected) / [math]::Abs($expected)
$newStartle = (Startles) -gt $n0

Write-Output ("click {0},{1}  span {2:N2} s  startled: {3}" -f $clickX, $clickY, $T, $newStartle)
Write-Output ("dx {0:N1} px (expected {1:N1}), facing {2} (expected {3}), error {4:P0}" -f `
    $dx, $expected, $after.dir, $away, $err)

$ok = $newStartle -and ($err -lt 0.30) -and ($after.dir -eq $away)
Write-Output ''
if ($ok) { Write-Output '[ok] the clicked fish bolted away at the configured speed' }
else { Write-Output '[FAIL] click and escape did not line up' }
exit ([int](-not $ok))
