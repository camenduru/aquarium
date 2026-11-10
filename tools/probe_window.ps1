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

if ($layered -and $topmost -and $visible -and (-not $clickthru) -and $w -gt 0 -and $h -gt 0) {
    Write-Output 'OK: layered, topmost, visible, per-pixel click-through'
    exit 0
}

Write-Output 'FAIL: one of the transparency invariants is wrong'
exit 1
