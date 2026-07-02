# win-click.ps1 - inject a left click at window-relative pixel coords into a process's main window.
# Used together with win-screenshot.ps1 to drive VoLum overlays for self-verification.
#   pwsh -File win-click.ps1 -X 660 -Y 76
param(
  [string]$ProcessName = "VoLum",
  [Parameter(Mandatory=$true)][int]$X,
  [Parameter(Mandatory=$true)][int]$Y,
  [int]$SettleMs = 250
)
$ErrorActionPreference = "Stop"
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WinClick {
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
if ([WinClick]::IsIconic($hwnd)) { [WinClick]::ShowWindow($hwnd, [WinClick]::SW_RESTORE) | Out-Null; Start-Sleep -Milliseconds 200 }
[WinClick]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 200
$rect = New-Object WinClick+RECT
[WinClick]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$sx = $rect.Left + $X
$sy = $rect.Top + $Y
[WinClick]::SetCursorPos($sx, $sy) | Out-Null
Start-Sleep -Milliseconds 60
[WinClick]::mouse_event([WinClick]::LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 40
[WinClick]::mouse_event([WinClick]::LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds $SettleMs
Write-Output "CLICK ($sx,$sy) in $ProcessName"
