# win-drag.ps1 - inject a left-button drag (down -> stepped move -> up) into a
# process's main window. Built to drive the VoLum standalone corner-resizer grip
# for repeatable resize profiling alongside win-screenshot.ps1 (darkClientPct) and
# the VOLUM_SHOW_FPS overlay.
#
# Two modes:
#
# 1) Explicit window-relative coords:
#    pwsh -File win-drag.ps1 -X1 900 -Y1 700 -X2 1180 -Y2 900 -Steps 24 -StepMs 16
#
# 2) -Grip (recommended): auto-start from the LIVE bottom-right corner so the
#    coordinates can never go stale after a previous resize. Give a target size
#    or a corner delta:
#    pwsh -File win-drag.ps1 -Grip -TargetW 1400 -TargetH 980 -Steps 24 -StepMs 8
#    pwsh -File win-drag.ps1 -Grip -DeltaX 300 -DeltaY 220
#
# Steps  = number of intermediate SetCursorPos moves (simulates drag speed).
# StepMs = delay between moves in ms (16 ~ one 60Hz frame; lower = faster drag).
# GripMargin = inset (px) from the window corner to land on the resizer grip.
param(
  [string]$ProcessName = "VoLum",
  [int]$X1 = -1,
  [int]$Y1 = -1,
  [int]$X2 = -1,
  [int]$Y2 = -1,
  [switch]$Grip,
  [int]$GripMargin = 12,
  [int]$TargetW = 0,
  [int]$TargetH = 0,
  [int]$DeltaX = 0,
  [int]$DeltaY = 0,
  [int]$Steps = 24,
  [int]$StepMs = 16,
  [int]$SettleMs = 400
)
$ErrorActionPreference = "Stop"
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WinDrag {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public const uint LEFTDOWN = 0x0002, LEFTUP = 0x0004;
  public const int SW_RESTORE = 9;
}
'@
$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) { Write-Output "NO_WINDOW"; exit 2 }
$hwnd = $proc.MainWindowHandle
if ([WinDrag]::IsIconic($hwnd)) { [WinDrag]::ShowWindow($hwnd, [WinDrag]::SW_RESTORE) | Out-Null; Start-Sleep -Milliseconds 200 }
[WinDrag]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 200
$rect = New-Object WinDrag+RECT
[WinDrag]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top

if ($Grip) {
  # Start on the live corner grip; never trust a hardcoded coordinate.
  $X1 = $w - $GripMargin
  $Y1 = $h - $GripMargin
  if ($TargetW -gt 0 -or $TargetH -gt 0) {
    if ($TargetW -le 0) { $TargetW = $w }
    if ($TargetH -le 0) { $TargetH = $h }
    $X2 = $X1 + ($TargetW - $w)
    $Y2 = $Y1 + ($TargetH - $h)
  }
  else {
    $X2 = $X1 + $DeltaX
    $Y2 = $Y1 + $DeltaY
  }
}
elseif ($X1 -lt 0 -or $Y1 -lt 0 -or $X2 -lt 0 -or $Y2 -lt 0) {
  Write-Output "NEED_COORDS: pass -Grip (+ -TargetW/-TargetH or -DeltaX/-DeltaY) or explicit -X1 -Y1 -X2 -Y2"
  exit 3
}

$sx = $rect.Left + $X1
$sy = $rect.Top + $Y1
[WinDrag]::SetCursorPos($sx, $sy) | Out-Null
Start-Sleep -Milliseconds 80
[WinDrag]::mouse_event([WinDrag]::LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 40
if ($Steps -lt 1) { $Steps = 1 }
for ($i = 1; $i -le $Steps; $i++) {
  $t = [double]$i / [double]$Steps
  $cx = [int]($rect.Left + $X1 + ($X2 - $X1) * $t)
  $cy = [int]($rect.Top + $Y1 + ($Y2 - $Y1) * $t)
  [WinDrag]::SetCursorPos($cx, $cy) | Out-Null
  Start-Sleep -Milliseconds $StepMs
}
Start-Sleep -Milliseconds 40
[WinDrag]::mouse_event([WinDrag]::LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds $SettleMs
[WinDrag]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$nw = $rect.Right - $rect.Left
$nh = $rect.Bottom - $rect.Top
Write-Output "DRAG win-rel ($X1,$Y1)->($X2,$Y2) steps=$Steps stepMs=$StepMs : size ${w}x${h} -> ${nw}x${nh}"
