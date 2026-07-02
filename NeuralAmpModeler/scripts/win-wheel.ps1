# win-wheel.ps1 - inject a vertical mouse-wheel scroll at window-relative coords.
#   pwsh -File win-wheel.ps1 -X 100 -Y 400 -Delta -360
param(
  [string]$ProcessName = "VoLum",
  [Parameter(Mandatory=$true)][int]$X,
  [Parameter(Mandatory=$true)][int]$Y,
  [int]$Delta = -120,
  [int]$SettleMs = 250
)
$ErrorActionPreference = "Stop"
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WinWheel {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public const uint WHEEL = 0x0800;
}
'@
$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) { Write-Output "NO_WINDOW"; exit 2 }
$hwnd = $proc.MainWindowHandle
[WinWheel]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 150
$rect = New-Object WinWheel+RECT
[WinWheel]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
[WinWheel]::SetCursorPos($rect.Left + $X, $rect.Top + $Y) | Out-Null
Start-Sleep -Milliseconds 60
$wheelData = if ($Delta -lt 0) { [uint32]([int64]4294967296 + $Delta) } else { [uint32]$Delta }
[WinWheel]::mouse_event([WinWheel]::WHEEL, 0, 0, $wheelData, [UIntPtr]::Zero)
Start-Sleep -Milliseconds $SettleMs
Write-Output "WHEEL $Delta at ($X,$Y)"
