# Drive the sandboxed VoLum canvas: one process per shot so foreground is not
# lost between click and capture. Coordinates are client-canvas pixels (the
# same space as capture-volum-canvas.ps1 / docs/user-guide-*.png), not
# window-relative win-click.ps1 pixels.
#
#   powershell -File ui-drive.ps1 -Clicks "743,22" -Out shot.png
#   powershell -File ui-drive.ps1 -Clicks "718,346" -PackOpen "$env:TEMP\volum-docs\seed.volumpack" -Out docs\user-guide-pack-import.png
#
# -PackOpen: after Clicks, complete the native Open dialog with the clipboard
# (never type an 8.3 path: ~ is SendKeys ALT). Does not ForceFront VoLum while
# the dialog is up — that cancels PromptForFile and leaves "No Pack opened."
param(
  [string] $Clicks = "",
  [string] $Keys = "",
  [Parameter(Mandatory = $true)][string] $Out,
  [int] $SettleMs = 600,
  [switch] $NoForceFront,
  [string] $PackOpen = ""
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
$fxDrawing = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\System.Drawing.dll"
Add-Type -ReferencedAssemblies $fxDrawing @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;
public static class UiDrive {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc lp, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint from, uint to, bool attach);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] private static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] private static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public const uint LEFTDOWN = 0x0002, LEFTUP = 0x0004;
  public const int SW_RESTORE = 9;
  public static readonly IntPtr TOPMOST = new IntPtr(-1), NOTOPMOST = new IntPtr(-2);
  public const uint NOSIZE = 0x0001, NOMOVE = 0x0002, SHOWWINDOW = 0x0040;

  public static void ForceFront(IntPtr hWnd) {
    if (IsIconic(hWnd)) { ShowWindow(hWnd, SW_RESTORE); System.Threading.Thread.Sleep(200); }
    uint fgPid; uint fgThread = GetWindowThreadProcessId(GetForegroundWindow(), out fgPid);
    uint self = GetCurrentThreadId();
    AttachThreadInput(self, fgThread, true);
    SetWindowPos(hWnd, TOPMOST, 0, 0, 0, 0, NOSIZE | NOMOVE | SHOWWINDOW);
    BringWindowToTop(hWnd);
    SetForegroundWindow(hWnd);
    AttachThreadInput(self, fgThread, false);
    System.Threading.Thread.Sleep(250);
  }
  public static void Drop(IntPtr hWnd) {
    SetWindowPos(hWnd, NOTOPMOST, 0, 0, 0, 0, NOSIZE | NOMOVE);
  }
  public static void ClickAt(IntPtr hWnd, int x, int y) {
    POINT o = new POINT(); ClientToScreen(hWnd, ref o);
    SetCursorPos(o.X + x, o.Y + y);
    System.Threading.Thread.Sleep(70);
    mouse_event(LEFTDOWN, 0, 0, 0, UIntPtr.Zero);
    System.Threading.Thread.Sleep(45);
    mouse_event(LEFTUP, 0, 0, 0, UIntPtr.Zero);
  }
  public static IntPtr FindOpenDialog(uint want) {
    IntPtr found = IntPtr.Zero;
    EnumWindows(delegate(IntPtr h, IntPtr l) {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != want || !IsWindowVisible(h)) return true;
      var c = new StringBuilder(128);
      GetClassName(h, c, 128);
      if (c.ToString() != "#32770") return true;
      var t = new StringBuilder(256);
      GetWindowText(h, t, 256);
      var title = t.ToString();
      if (title != "Open") return true;
      found = h;
      return false;
    }, IntPtr.Zero);
    return found;
  }
  public static void FrontDialog(IntPtr h) {
    uint pid; uint fg = GetWindowThreadProcessId(GetForegroundWindow(), out pid);
    uint self = GetCurrentThreadId();
    AttachThreadInput(self, fg, true);
    BringWindowToTop(h);
    SetForegroundWindow(h);
    AttachThreadInput(self, fg, false);
    System.Threading.Thread.Sleep(200);
  }
  public static void Poke(IntPtr hWnd) {
    RECT cr; GetClientRect(hWnd, out cr);
    POINT o = new POINT(); ClientToScreen(hWnd, ref o);
    int w = cr.Right - cr.Left, h = cr.Bottom - cr.Top;
    for (int i = 1; i <= 6; ++i) {
      SetCursorPos(o.X + w * i / 7, o.Y + h / 2);
      System.Threading.Thread.Sleep(45);
    }
    SetCursorPos(o.X + 3, o.Y + h - 3);
    System.Threading.Thread.Sleep(300);
  }
  static bool AnyDark(Bitmap b) {
    int dark = 0;
    for (int y = 0; y < b.Height; y += 8)
      for (int x = 0; x < b.Width; x += 8) {
        Color c = b.GetPixel(x, y);
        if (c.R < 40 && c.G < 45 && c.B < 55 && ++dark > 40) return true;
      }
    return false;
  }
  public static double SaveCanvas(IntPtr hWnd, string path) {
    RECT wr; GetWindowRect(hWnd, out wr);
    RECT cr; GetClientRect(hWnd, out cr);
    POINT o = new POINT(); ClientToScreen(hWnd, ref o);
    int w = cr.Right - cr.Left, h = cr.Bottom - cr.Top;
    long dark = 0, total = 0;
    using (var full = new Bitmap(wr.Right - wr.Left, wr.Bottom - wr.Top, PixelFormat.Format32bppArgb)) {
      using (var g = Graphics.FromImage(full)) {
        IntPtr hdc = g.GetHdc();
        try { if (!PrintWindow(hWnd, hdc, 2)) PrintWindow(hWnd, hdc, 0); }
        finally { g.ReleaseHdc(hdc); }
      }
      var crop = new Rectangle(o.X - wr.Left, o.Y - wr.Top, w, h);
      using (var canvas = full.Clone(crop, PixelFormat.Format32bppArgb)) {
        if (!AnyDark(canvas))
          using (var g2 = Graphics.FromImage(canvas))
            g2.CopyFromScreen(o.X, o.Y, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
        long lit = 0;
        for (int y = 0; y < h; y += 4)
          for (int x = 0; x < w; x += 4) {
            Color c = canvas.GetPixel(x, y);
            ++total;
            if (c.R < 40 && c.G < 45 && c.B < 55) ++dark;
            if (c.R + c.G + c.B > 60) ++lit;
          }
        canvas.Save(path, ImageFormat.Png);
        if (lit * 200 < total) return 0.0;
      }
    }
    return total == 0 ? 0.0 : (100.0 * dark / total);
  }
}
'@

$pidFile = Join-Path (Join-Path $env:TEMP "volum-ui-sandbox") "pid.txt"
$want = if (Test-Path $pidFile) { [int](Get-Content $pidFile -Raw).Trim() } else { 0 }
$proc = Get-Process -Name VoLum -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } |
  Where-Object { $want -eq 0 -or $_.Id -eq $want } | Select-Object -First 1
if (-not $proc) {
  Write-Error "Sandboxed VoLum (pid $want) is not running. Relaunch with .ui-sandbox-launch.ps1."
}
$hwnd = $proc.MainWindowHandle
$pidNum = [uint32]$proc.Id
$usePackOpen = -not [string]::IsNullOrWhiteSpace($PackOpen)

if (-not $NoForceFront) {
  [UiDrive]::ForceFront($hwnd)
}

if ($Clicks) {
  foreach ($pair in $Clicks.Split(';')) {
    if (-not $pair.Trim()) { continue }
    $xy = $pair.Split(',')
    [UiDrive]::ClickAt($hwnd, [int]$xy[0], [int]$xy[1])
    Start-Sleep -Milliseconds 450
  }
}
if ($Keys -and -not $usePackOpen) {
  if (-not $NoForceFront) { [UiDrive]::ForceFront($hwnd) }
  foreach ($chunk in $Keys.Split(';')) {
    if (-not $chunk) { continue }
    [System.Windows.Forms.SendKeys]::SendWait($chunk)
    Start-Sleep -Milliseconds 350
  }
}

if ($usePackOpen) {
  if (-not (Test-Path -LiteralPath $PackOpen)) {
    Write-Error "PackOpen path does not exist: $PackOpen"
  }
  $full = (Resolve-Path -LiteralPath $PackOpen).Path
  $dlg = [IntPtr]::Zero
  for ($i = 0; $i -lt 40; $i++) {
    $dlg = [UiDrive]::FindOpenDialog($pidNum)
    if ($dlg -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 250
  }
  if ($dlg -eq [IntPtr]::Zero) {
    Write-Error "Open dialog did not appear. Click Import Pack... in -Clicks and do not ForceFront another process."
  }
  [UiDrive]::FrontDialog($dlg)
  [System.Windows.Forms.Clipboard]::SetText($full)
  Start-Sleep -Milliseconds 150
  [System.Windows.Forms.SendKeys]::SendWait("%n")
  Start-Sleep -Milliseconds 200
  [System.Windows.Forms.SendKeys]::SendWait("^a")
  Start-Sleep -Milliseconds 80
  [System.Windows.Forms.SendKeys]::SendWait("^v")
  Start-Sleep -Milliseconds 200
  [System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
  for ($i = 0; $i -lt 40; $i++) {
    if ([UiDrive]::FindOpenDialog($pidNum) -eq [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 250
  }
  Start-Sleep -Milliseconds 400
  if (-not $NoForceFront) { [UiDrive]::ForceFront($hwnd) }
}

Start-Sleep -Milliseconds $SettleMs
[UiDrive]::Poke($hwnd)

$outDir = Split-Path -Parent $Out
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }
$stage = [System.IO.Path]::Combine($env:TEMP, "volum-ui-stage.png")
$dark = [UiDrive]::SaveCanvas($hwnd, $stage)
[UiDrive]::Drop($hwnd)
if ($dark -ge 5) {
  Move-Item $stage $Out -Force
  Write-Host ("Wrote {0} (darkPct={1:N1})" -f $Out, $dark)
}
if ($dark -lt 5) {
  if (Get-Process LogonUI, LockApp -ErrorAction SilentlyContinue) {
    Write-Host "LOCKED: LogonUI/LockApp is running - unlock the session, then re-run."
  }
  Write-Error "Capture is blank (darkPct=$dark). The GL surface was not composited."
}
