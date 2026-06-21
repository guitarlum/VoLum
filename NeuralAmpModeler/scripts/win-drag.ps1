# win-drag.ps1 - inject a left-button drag (down -> stepped move -> up) at
# window-relative pixel coords into a process's main window. Built to drive the
# VoLum standalone corner-resizer grip for repeatable resize profiling alongside
# win-screenshot.ps1 (darkClientPct) and the VOLUM_SHOW_FPS overlay.
#
#   pwsh -File win-drag.ps1 -X1 900 -Y1 700 -X2 1180 -Y2 900 -Steps 24 -StepMs 16
#
# X1/Y1 = drag start (window-relative), X2/Y2 = drag end (window-relative).
# Steps  = number of intermediate SetCursorPos moves (simulates drag speed).
# StepMs = delay between moves in ms (16 ~ one 60Hz frame; lower = faster drag).
param(
  [string]$ProcessName = "VoLum",
  [Parameter(Mandatory=$true)][int]$X1,
  [Parameter(Mandatory=$true)][int]$Y1,
  [Parameter(Mandatory=$true)][int]$X2,
  [Parameter(Mandatory=$true)][int]$Y2,
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
Write-Output "DRAG ($($rect.Left + $X1),$($rect.Top + $Y1)) -> ($($rect.Left + $X2),$($rect.Top + $Y2)) steps=$Steps stepMs=$StepMs in $ProcessName"
