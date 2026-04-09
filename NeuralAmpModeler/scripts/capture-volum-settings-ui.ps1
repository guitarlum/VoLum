# Capture the VoLum standalone window to PNG (for comparing with ui-mockup/settings-overlay-mockup.html).
# OpenGL UI: uses PrintWindow(..., PW_RENDERFULLCONTENT) per Win 8.1+.
#
# Optional: in config.h set VOLUM_OPEN_SETTINGS_AT_LAUNCH to 1, rebuild Release|x64, then run this script
# immediately so the settings overlay is visible without clicking the gear.
#
# Usage (from repo):
#   .\NeuralAmpModeler\scripts\capture-volum-settings-ui.ps1
#   .\NeuralAmpModeler\scripts\capture-volum-settings-ui.ps1 -ExePath "C:\path\to\VoLum.exe"

param(
  [string] $ExePath = "",
  [string] $WindowTitle = "VoLum",
  [string] $OutPath = "",
  [switch] $Launch
)

$ErrorActionPreference = "Stop"
# Full .NET Framework System.Drawing (needed for PS 7+; -AssemblyName alone is not enough on Core)
$fxDrawing = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\System.Drawing.dll"
if (-not (Test-Path $fxDrawing)) {
  Write-Error "System.Drawing not found at $fxDrawing - use Windows with .NET Framework 4.x."
}
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = Resolve-Path (Join-Path $here "..")
if (-not $OutPath) {
  $capDir = Join-Path $slnDir "ui-mockup\_capture"
  if (-not (Test-Path $capDir)) { New-Item -ItemType Directory -Path $capDir | Out-Null }
  $OutPath = Join-Path $capDir ("volum-{0:yyyyMMdd-HHmmss}.png" -f (Get-Date))
}
if (-not $ExePath) {
  $ExePath = Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe"
}

$capSrc = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class VoLumWinCap
{
  private const uint PW_RENDERFULLCONTENT = 0x00000002;

  [DllImport("user32.dll")]
  private static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);

  [DllImport("user32.dll")]
  private static extern IntPtr GetWindowDC(IntPtr hWnd);

  [DllImport("user32.dll")]
  private static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);

  [DllImport("user32.dll")]
  private static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

  [StructLayout(LayoutKind.Sequential)]
  private struct RECT { public int Left, Top, Right, Bottom; }

  public static void SaveWindowPng(IntPtr hwnd, string path)
  {
    RECT rc;
    if (!GetWindowRect(hwnd, out rc))
      throw new InvalidOperationException("GetWindowRect failed");
    int w = rc.Right - rc.Left;
    int h = rc.Bottom - rc.Top;
    using (var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb))
    {
      using (var g = Graphics.FromImage(bmp))
      {
        IntPtr hdc = g.GetHdc();
        try
        {
          if (!PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT))
            PrintWindow(hwnd, hdc, 0);
        }
        finally { g.ReleaseHdc(hdc); }
      }
      bmp.Save(path, ImageFormat.Png);
    }
  }
}
'@
Add-Type -TypeDefinition $capSrc -ReferencedAssemblies $fxDrawing

if ($Launch) {
  if (-not (Test-Path $ExePath)) { Write-Error "Exe not found: $ExePath" }
  Start-Process -FilePath $ExePath
  Start-Sleep -Seconds 2
}

$findSrc = @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class VoLumFindWin
{
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

  [DllImport("user32.dll")]
  public static extern bool EnumWindows(EnumProc lpEnumFunc, IntPtr lParam);

  [DllImport("user32.dll", CharSet = CharSet.Unicode)]
  public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

  public static IntPtr FindByTitleSubstring(string sub)
  {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, __) => {
      var sb = new StringBuilder(256);
      GetWindowText(h, sb, 256);
      string t = sb.ToString();
      if (!string.IsNullOrEmpty(t) && t.IndexOf(sub, StringComparison.OrdinalIgnoreCase) >= 0)
      {
        found = h;
        return false;
      }
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
'@
Add-Type -TypeDefinition $findSrc

$hwnd = [VoLumFindWin]::FindByTitleSubstring($WindowTitle)
if ($hwnd -eq [IntPtr]::Zero) {
  Write-Error "No window with title containing '$WindowTitle'. Launch VoLum (use -Launch) or open settings if capturing overlay."
}

$outDir = Split-Path -Parent $OutPath
if ($outDir -and -not (Test-Path $outDir)) {
  New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

[VoLumWinCap]::SaveWindowPng($hwnd, $OutPath)
Write-Host "Wrote $OutPath"
