# win-screenshot.ps1 - capture a window's own pixels via PrintWindow(PW_RENDERFULLCONTENT).
#
# Works for the VoLum standalone (NanoVG/GL surface) even when the window is
# occluded or in the background, which plain GDI CopyFromScreen cannot do.
# Used as the standard self-verify step for VoLum UI iteration.
#
# Usage:
#   pwsh -NoProfile -File NeuralAmpModeler/scripts/win-screenshot.ps1
#   pwsh -NoProfile -File NeuralAmpModeler/scripts/win-screenshot.ps1 -OutFile C:\tmp\shot.png
#   pwsh -NoProfile -File NeuralAmpModeler/scripts/win-screenshot.ps1 -ProcessName VoLum -SettleMs 800

param(
  [string]$ProcessName = "VoLum",
  [string]$OutFile = "",
  [int]$SettleMs = 500
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

if (-not $OutFile -or $OutFile.Trim() -eq "") {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  $dir = Join-Path $env:TEMP "volum-shots"
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  $OutFile = Join-Path $dir "$ProcessName-$stamp.png"
}

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WinShot {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public const uint PW_RENDERFULLCONTENT = 0x00000002;
  public const int SW_RESTORE = 9;
}
'@

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
  Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1

if (-not $proc) {
  Write-Output "NO_WINDOW: no '$ProcessName' process with a top-level window."
  exit 2
}

$hwnd = $proc.MainWindowHandle

if ([WinShot]::IsIconic($hwnd)) {
  [WinShot]::ShowWindow($hwnd, [WinShot]::SW_RESTORE) | Out-Null
  Start-Sleep -Milliseconds 300
}

# Let the UI settle (animations, first paint) before capture.
Start-Sleep -Milliseconds $SettleMs

$rect = New-Object WinShot+RECT
[WinShot]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top

if ($w -le 0 -or $h -le 0) {
  Write-Output "BAD_RECT: ${w}x${h}"
  exit 3
}

$bmp = New-Object System.Drawing.Bitmap $w, $h
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $gfx.GetHdc()
try {
  $ok = [WinShot]::PrintWindow($hwnd, $hdc, [WinShot]::PW_RENDERFULLCONTENT)
} finally {
  $gfx.ReleaseHdc($hdc)
}

if (-not $ok) {
  Write-Output "PRINTWINDOW_FAILED (hwnd=$hwnd)"
  exit 4
}

# Quality heuristic. VoLum's UI is predominantly dark Art-Deco-Noir. A capture
# whose client area is almost entirely near-white (Win32 default brush) or
# all-black means the GL child surface was NOT composited into the bitmap -
# common when the standalone window isn't truly visible on the desktop. Count
# how many sampled client pixels read as "VoLum dark" so the caller can tell a
# real render from an unrendered/uncapturable frame.
$step = [Math]::Max(1, [int]($w / 60))
$clientTop = [Math]::Min($h - 1, 60) # skip native title bar + menu strip
$samples = 0
$dark = 0
$black = 0
for ($x = 0; $x -lt $w; $x += $step) {
  for ($y = $clientTop; $y -lt $h; $y += $step) {
    $px = $bmp.GetPixel($x, $y)
    $samples++
    if ($px.R -lt 70 -and $px.G -lt 70 -and $px.B -lt 90 -and ($px.R + $px.G + $px.B) -gt 6) { $dark++ }
    if ($px.R -le 6 -and $px.G -le 6 -and $px.B -le 6) { $black++ }
  }
}
$darkPct = if ($samples -gt 0) { [int](100 * $dark / $samples) } else { 0 }

$bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
$gfx.Dispose()
$bmp.Dispose()

Write-Output "SAVED $OutFile (${w}x${h}, darkClientPct=$darkPct)"
if ($darkPct -lt 8) {
  Write-Output "WARN: client area is not VoLum-dark (darkClientPct=$darkPct). The GL surface was likely not composited into the capture (window not visible / DWM not caching it). Bring the VoLum window to the foreground on a real display and retry."
  exit 5
}
