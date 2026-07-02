# win-key.ps1 - send keystrokes to VoLum's main window for self-verification/screenshots.
# Complements win-click.ps1 / win-screenshot.ps1 / capture-volum-canvas.ps1.
# Keys use SendKeys syntax: letters/digits verbatim, specials as {UP} {DOWN}
# {LEFT} {RIGHT} {TAB} {ENTER} {ESC}. Example:
#   pwsh -File win-key.ps1 -Keys "3"        # switch to POST
#   pwsh -File win-key.ps1 -Keys "{TAB}{TAB}" -SettleMs 400
param(
  [string]$ProcessName = "VoLum",
  [Parameter(Mandatory=$true)][string]$Keys,
  [int]$SettleMs = 300
)
$ErrorActionPreference = "Stop"
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WinKey {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  public const int SW_RESTORE = 9;
}
'@
$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) { Write-Output "NO_WINDOW"; exit 2 }
$hwnd = $proc.MainWindowHandle
if ([WinKey]::IsIconic($hwnd)) { [WinKey]::ShowWindow($hwnd, [WinKey]::SW_RESTORE) | Out-Null; Start-Sleep -Milliseconds 200 }
[WinKey]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 250
$wsh = New-Object -ComObject WScript.Shell
$wsh.SendKeys($Keys)
Start-Sleep -Milliseconds $SettleMs
Write-Output "KEYS '$Keys' -> $ProcessName"
