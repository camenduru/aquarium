# Reads the aquarium's window styles back out of Windows, so "it is a
# transparent window system" is a measurement and not a claim.
#
#   powershell -ExecutionPolicy Bypass -File tools/probe_window.ps1
#
# The window is located by enumerating top-level windows and comparing class
# names: FindWindowW's marshalling of the class name is unreliable from
# PowerShell, and a silent miss there reads like "the app never made a window".
#
# Checks: the window exists, is visible, carries WS_EX_LAYERED
# (UpdateLayeredWindow) and WS_EX_TOPMOST, does NOT carry WS_EX_TRANSPARENT
# (the click-through is per pixel, so fish stay clickable), and reports its size.

$ErrorActionPreference = 'Stop'

$sig = @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public struct RECT { public int Left, Top, Right, Bottom; }

public static class AquariumWindow
{
    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumProc callback, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr hWnd, StringBuilder buffer, int max);

    [DllImport("user32.dll")]
    public static extern int GetWindowLongW(IntPtr hWnd, int index);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    public struct POINT { public int X, Y; }

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(IntPtr hWnd, int msg, IntPtr wParam, IntPtr lParam);

    // WM_NCHITTEST wants SCREEN coordinates in lParam.
    public static int HitTest(IntPtr hWnd, int clientX, int clientY)
    {
        POINT p = new POINT();
        p.X = clientX;
        p.Y = clientY;
        ClientToScreen(hWnd, ref p);
        IntPtr lParam = (IntPtr)((p.Y << 16) | (p.X & 0xFFFF));
        return SendMessageW(hWnd, 0x0084, IntPtr.Zero, lParam).ToInt32();
    }

    public static IntPtr FindByClass(string wanted)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam)
        {
            StringBuilder sb = new StringBuilder(256);
            GetClassNameW(hWnd, sb, 256);
            if (sb.ToString() == wanted)
            {
                found = hWnd;
                return false;   // stop enumerating
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

Add-Type -TypeDefinition $sig

$hwnd = [AquariumWindow]::FindByClass('AquariumOverlay')
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Output 'FAIL: no top-level window with class AquariumOverlay (is the aquarium running?)'
    exit 1
}

$GWL_EXSTYLE = -20
$ex = [AquariumWindow]::GetWindowLongW($hwnd, $GWL_EXSTYLE)

$layered     = ($ex -band 0x00080000) -ne 0   # WS_EX_LAYERED
$topmost     = ($ex -band 0x00000008) -ne 0   # WS_EX_TOPMOST
$clickthru   = ($ex -band 0x00000020) -ne 0   # WS_EX_TRANSPARENT
$toolwindow  = ($ex -band 0x00000080) -ne 0   # WS_EX_TOOLWINDOW
$noactivate  = ($ex -band 0x08000000) -ne 0   # WS_EX_NOACTIVATE
$visible     = [AquariumWindow]::IsWindowVisible($hwnd)

$rect = New-Object RECT
[AquariumWindow]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top

Write-Output ("window     0x{0:X8}  {1}x{2} at {3},{4}" -f $hwnd.ToInt64(), $w, $h, $rect.Left, $rect.Top)
Write-Output ("exstyle    0x{0:X8}" -f $ex)
Write-Output ("layered    {0}" -f $layered)
Write-Output ("topmost    {0}" -f $topmost)
Write-Output ("toolwindow {0}" -f $toolwindow)
Write-Output ("noactivate {0}" -f $noactivate)
Write-Output ("visible    {0}" -f $visible)
Write-Output ("ex_transparent (must be False: per-pixel hit test) {0}" -f $clickthru)

# ---- per-pixel hit test -----------------------------------------------------
# The whole point of the water layer is that the desktop stays usable, so the
# claim to measure is: water-only pixels answer HTTRANSPARENT (the click falls
# through to whatever is underneath) while the fish answer HTCLIENT. WM_NCHITTEST
# is the same message the desktop sends when the mouse moves, so asking it
# directly is the real behaviour rather than an approximation of it.
#
# A grid is sampled instead of the centre alone: the centre can land on a fish.
# The water covers nearly the whole tank, so a working hit mask must keep the
# HTCLIENT fraction low (measured ~6 %, all of it fish) - that is the assertion.
$HTCLIENT       = 1
$HTTRANSPARENT  = -1

$cols = 48
$rows = 27
$client = 0
$transparent = 0
$other = @{}
$hits = New-Object 'System.Collections.Generic.List[string]'

for ($j = 0; $j -lt $rows; $j++) {
    $py = [int](($j + 0.5) * $h / $rows)
    for ($i = 0; $i -lt $cols; $i++) {
        $px = [int](($i + 0.5) * $w / $cols)
        $r = [AquariumWindow]::HitTest($hwnd, $px, $py)
        if ($r -eq $HTCLIENT) {
            $client++
            $hits.Add("$px,$py")
        }
        elseif ($r -eq $HTTRANSPARENT) {
            $transparent++
        }
        else {
            $key = "0x{0:X}" -f ($r -band 0xFFFF)
            if (-not $other.ContainsKey($key)) { $other[$key] = 0 }
            $other[$key]++
        }
    }
}

$total = $cols * $rows
$fraction = if ($total -gt 0) { $client / $total } else { 0 }

Write-Output ("hittest    {0} samples: {1} HTCLIENT ({2:P1}), {3} HTTRANSPARENT" -f `
    $total, $client, $fraction, $transparent)
if ($other.Count -gt 0) {
    foreach ($key in $other.Keys) {
        Write-Output ("           unexpected {0} x{1}" -f $key, $other[$key])
    }
}

# The fish are small moving targets, so a run can legitimately catch zero of
# them; only the click-through population is asserted, plus "not everything".
$hittestOk = ($transparent -gt 0) -and ($fraction -lt 0.25) -and ($other.Count -eq 0)

if ($layered -and $topmost -and $visible -and (-not $clickthru) -and $w -gt 0 -and $h -gt 0 -and $hittestOk) {
    Write-Output 'OK: layered, topmost, visible, per-pixel click-through'
    exit 0
}

if (-not $hittestOk) {
    Write-Output 'FAIL: hit test did not behave like a per-pixel mask'
}
Write-Output 'FAIL: one of the transparency invariants is wrong'
exit 1
