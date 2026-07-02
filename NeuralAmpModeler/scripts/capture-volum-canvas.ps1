# Capture only the VoLum plugin CANVAS (client area) to PNG - no OS title bar / menu / borders.
# Matches the framing of the docs/user-guide-*.png screenshots.
# Uses PrintWindow(PW_RENDERFULLCONTENT) for the OpenGL surface, then crops to the client rect.
# Run under Windows PowerShell 5.x (not pwsh 7).
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File capture-volum-canvas.ps1 -OutPath "C:\path\out.png"
param(
  [string] $WindowTitle = "VoLum",
  [Parameter(Mandatory=$true)][string] $OutPath
)
$ErrorActionPreference = "Stop"
$fxDrawing = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\System.Drawing.dll"
if (-not (Test-Path $fxDrawing)) { Write-Error "System.Drawing not found at $fxDrawing" }

$src = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class VoLumCanvasCap
{
  private const uint PW_RENDERFULLCONTENT = 0x00000002;
  [DllImport("user32.dll")] private static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);
  [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] private static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc lpEnumFunc, IntPtr lParam);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
  [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] private struct POINT { public int X, Y; }

  public static IntPtr FindByTitleSubstring(string sub)
  {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, __) => {
      var sb = new StringBuilder(256);
      GetWindowText(h, sb, 256);
      string t = sb.ToString();
      if (!string.IsNullOrEmpty(t) && t.IndexOf(sub, StringComparison.OrdinalIgnoreCase) >= 0) { found = h; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }

  public static void SaveCanvasPng(IntPtr hwnd, string path)
  {
    RECT wr; GetWindowRect(hwnd, out wr);
    RECT cr; GetClientRect(hwnd, out cr);
    POINT origin = new POINT(); ClientToScreen(hwnd, ref origin);
    int ww = wr.Right - wr.Left;
    int wh = wr.Bottom - wr.Top;
    int cw = cr.Right - cr.Left;
    int ch = cr.Bottom - cr.Top;
    int offX = origin.X - wr.Left;
    int offY = origin.Y - wr.Top;
    using (var full = new Bitmap(ww, wh, PixelFormat.Format32bppArgb))
    {
      using (var g = Graphics.FromImage(full))
      {
        IntPtr hdc = g.GetHdc();
        try { if (!PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)) PrintWindow(hwnd, hdc, 0); }
        finally { g.ReleaseHdc(hdc); }
      }
      var crop = new Rectangle(offX, offY, cw, ch);
      using (var canvas = full.Clone(crop, PixelFormat.Format32bppArgb))
      {
        canvas.Save(path, ImageFormat.Png);
      }
    }
  }
}
'@
Add-Type -TypeDefinition $src -ReferencedAssemblies $fxDrawing

$hwnd = [VoLumCanvasCap]::FindByTitleSubstring($WindowTitle)
if ($hwnd -eq [IntPtr]::Zero) { Write-Error "No window with title containing '$WindowTitle'." }
$outDir = Split-Path -Parent $OutPath
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }
[VoLumCanvasCap]::SaveCanvasPng($hwnd, $OutPath)
Write-Host "Wrote $OutPath"
