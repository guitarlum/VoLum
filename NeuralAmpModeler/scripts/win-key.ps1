param(
  [Parameter(Mandatory = $true)][int]$VK,   # virtual-key code, e.g. 0x27 (Right)
  [string]$ProcessName = "VoLum",
  [int]$SettleMs = 120
)

# Inject a single key press/release into the focused VoLum window. Used to verify
# keyboard handling (e.g. arrow-key art navigation in the custom-amp builder).

$sig = @'
using System;
using System.Runtime.InteropServices;
public static class Key {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
}
'@
Add-Type -TypeDefinition $sig

$p = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { Write-Error "No $ProcessName window found"; exit 1 }

[Key]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 80

$KEYUP = 0x2
[Key]::keybd_event([byte]$VK, 0, 0, [UIntPtr]::Zero)        # down
Start-Sleep -Milliseconds 20
[Key]::keybd_event([byte]$VK, 0, $KEYUP, [UIntPtr]::Zero)   # up
Start-Sleep -Milliseconds $SettleMs
Write-Output ("KEY 0x{0:X} -> {1}" -f $VK, $ProcessName)
