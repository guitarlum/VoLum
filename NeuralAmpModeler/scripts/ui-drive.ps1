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
#
# -Locked: drive VoLum with window messages and capture through its own
# framebuffer, so it works while LogonUI owns the desktop (and the same way when
# it does not). VoLum must run with VOLUM_SELF_CAPTURE_DIR = -CaptureDir, which
# .ui-sandbox-launch.ps1 sets. -Keys uses the SendKeys subset: ^ Ctrl, + Shift,
# % Alt, {ESC} {ENTER} {TAB} {UP} {DOWN} {LEFT} {RIGHT} {HOME} {END} {BS} {DEL}
# {SPACE} {F1}..{F12}, {X n} repeats, anything else is typed. -PackOpen needs
# the real desktop and is refused. -Drags "x1,y1>x2,y2" drags with the button
# held (add -HoldLastDrag to capture mid-drag).
param(
  [string] $Clicks = "",
  [string] $Keys = "",
  # -Locked only: "x1,y1>x2,y2[>x3,y3...];..." presses at the first point, drags
  # through the rest and releases at the last. Runs after -Clicks, before -Keys.
  [string] $Drags = "",
  # -Locked only: keep the button down on the last -Drags entry until after the
  # capture, so the shot shows the drag in flight; it is released afterwards.
  [switch] $HoldLastDrag,
  [Parameter(Mandatory = $true)][string] $Out,
  [int] $SettleMs = 600,
  [switch] $NoForceFront,
  [string] $PackOpen = "",
  [Alias("Messages")][switch] $Locked,
  [string] $CaptureDir = (Join-Path $env:TEMP "volum-ui-sandbox\capture"),
  [int] $CaptureTimeoutMs = 8000
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

if ($Locked) {
  if ($usePackOpen) { Write-Error "-PackOpen drives a native dialog and cannot run with -Locked." }
  Add-Type -ReferencedAssemblies $fxDrawing @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class UiDriveMsg {
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint type);
  [DllImport("user32.dll")] static extern short VkKeyScan(char c);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool AttachThreadInput(uint from, uint to, bool attach);
  [DllImport("user32.dll")] static extern bool GetKeyboardState(byte[] s);
  [DllImport("user32.dll")] static extern bool SetKeyboardState(byte[] s);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [StructLayout(LayoutKind.Sequential)] struct RECT { public int Left, Top, Right, Bottom; }
  const uint WM_MOUSEMOVE = 0x0200, WM_LBUTTONDOWN = 0x0201, WM_LBUTTONUP = 0x0202;
  const uint WM_KEYDOWN = 0x0100, WM_KEYUP = 0x0101;
  public static bool ModifiersShared = true;

  public static IntPtr PlugWindow(IntPtr main) {
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(main, (h, l) => {
      var c = new StringBuilder(64);
      GetClassName(h, c, 64);
      if (c.ToString() != "IPlugWndClass") return true;
      found = h;
      return false;
    }, IntPtr.Zero);
    return found;
  }
  public static double Scale(IntPtr plug) {
    RECT r; GetClientRect(plug, out r);
    return (r.Right - r.Left) / 900.0;
  }
  static IntPtr At(IntPtr plug, int cx, int cy) {
    double s = Scale(plug);
    int x = (int)Math.Round(cx * s), y = (int)Math.Round(cy * s);
    return (IntPtr)((y << 16) | (x & 0xFFFF));
  }
  // Canvas pixels (the 900x600 layout), scaled to the plug window's client size.
  public static void Click(IntPtr plug, int cx, int cy) {
    IntPtr at = At(plug, cx, cy);
    SendMessage(plug, WM_MOUSEMOVE, IntPtr.Zero, at);
    SendMessage(plug, WM_LBUTTONDOWN, (IntPtr)1, at);
    SendMessage(plug, WM_LBUTTONUP, IntPtr.Zero, at);
  }
  public static void Hover(IntPtr plug, int cx, int cy) {
    SendMessage(plug, WM_MOUSEMOVE, IntPtr.Zero, At(plug, cx, cy));
  }
  // Press at the first point, move through the rest with the button held
  // (MK_LBUTTON in wParam, which iPlug routes to OnMouseDrag), release at the last.
  public static void Drag(IntPtr plug, int[] xs, int[] ys, bool release) {
    SendMessage(plug, WM_MOUSEMOVE, IntPtr.Zero, At(plug, xs[0], ys[0]));
    SendMessage(plug, WM_LBUTTONDOWN, (IntPtr)1, At(plug, xs[0], ys[0]));
    for (int p = 1; p < xs.Length; p++) {
      const int steps = 12;
      for (int i = 1; i <= steps; i++) {
        int x = xs[p - 1] + (xs[p] - xs[p - 1]) * i / steps;
        int y = ys[p - 1] + (ys[p] - ys[p - 1]) * i / steps;
        SendMessage(plug, WM_MOUSEMOVE, (IntPtr)1, At(plug, x, y));
        System.Threading.Thread.Sleep(15);
      }
      System.Threading.Thread.Sleep(120);
    }
    if (release) Release(plug, xs[xs.Length - 1], ys[ys.Length - 1]);
  }
  public static void Release(IntPtr plug, int cx, int cy) {
    SendMessage(plug, WM_LBUTTONUP, IntPtr.Zero, At(plug, cx, cy));
  }

  // iPlug reads modifiers with GetKeyState and the character with ToAscii on its
  // own thread. A message cannot carry either, so the key state is shared for the
  // duration of the stroke.
  static void Stroke(IntPtr plug, int vk, bool shift, bool ctrl, bool alt) {
    uint pid;
    uint target = GetWindowThreadProcessId(plug, out pid);
    uint self = GetCurrentThreadId();
    bool attached = (shift || ctrl || alt) && AttachThreadInput(self, target, true);
    if ((shift || ctrl || alt) && !attached) ModifiersShared = false;
    byte[] saved = new byte[256];
    if (attached) {
      GetKeyboardState(saved);
      byte[] state = (byte[])saved.Clone();
      if (shift) { state[0x10] = 0x80; state[0xA0] = 0x80; }
      if (ctrl) { state[0x11] = 0x80; state[0xA2] = 0x80; }
      if (alt) { state[0x12] = 0x80; state[0xA4] = 0x80; }
      SetKeyboardState(state);
    }
    long scan = MapVirtualKey((uint)vk, 0);
    SendMessage(plug, WM_KEYDOWN, (IntPtr)vk, (IntPtr)(1 | (scan << 16)));
    SendMessage(plug, WM_KEYUP, (IntPtr)vk, (IntPtr)(1 | (scan << 16) | 0xC0000000L));
    if (attached) {
      SetKeyboardState(saved);
      AttachThreadInput(self, target, false);
    }
  }

  static readonly Dictionary<string, int> Named = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase) {
    {"ESC", 0x1B}, {"ESCAPE", 0x1B}, {"ENTER", 0x0D}, {"TAB", 0x09}, {"UP", 0x26}, {"DOWN", 0x28},
    {"LEFT", 0x25}, {"RIGHT", 0x27}, {"HOME", 0x24}, {"END", 0x23}, {"BS", 0x08}, {"BKSP", 0x08},
    {"BACKSPACE", 0x08}, {"DEL", 0x2E}, {"DELETE", 0x2E}, {"SPACE", 0x20}, {"PGUP", 0x21}, {"PGDN", 0x22},
    {"F1", 0x70}, {"F2", 0x71}, {"F3", 0x72}, {"F4", 0x73}, {"F5", 0x74}, {"F6", 0x75},
    {"F7", 0x76}, {"F8", 0x77}, {"F9", 0x78}, {"F10", 0x79}, {"F11", 0x7A}, {"F12", 0x7B}
  };

  // SendKeys subset: ^ + % prefix the next key; {NAME} or {NAME n}; ~ is Enter.
  public static void Keys(IntPtr plug, string spec) {
    bool shift = false, ctrl = false, alt = false;
    for (int i = 0; i < spec.Length; i++) {
      char c = spec[i];
      if (c == '^') { ctrl = true; continue; }
      if (c == '+') { shift = true; continue; }
      if (c == '%') { alt = true; continue; }
      int vk; bool needShift = false; int repeat = 1;
      if (c == '{') {
        int close = spec.IndexOf('}', i + 2);
        if (close < 0) throw new ArgumentException("unclosed { in " + spec);
        string body = spec.Substring(i + 1, close - i - 1);
        i = close;
        string[] parts = body.Split(' ');
        if (parts.Length == 2) repeat = int.Parse(parts[1]);
        if (!Named.TryGetValue(parts[0], out vk)) {
          if (parts[0].Length != 1) throw new ArgumentException("unknown key {" + body + "}");
          short scan = VkKeyScan(parts[0][0]);
          vk = scan & 0xFF; needShift = (scan & 0x100) != 0;
        }
      } else if (c == '~') {
        vk = 0x0D;
      } else {
        short scan = VkKeyScan(c);
        if (scan == -1) throw new ArgumentException("cannot type '" + c + "'");
        vk = scan & 0xFF; needShift = (scan & 0x100) != 0;
      }
      for (int r = 0; r < repeat; r++) Stroke(plug, vk, shift || needShift, ctrl, alt);
      shift = ctrl = alt = false;
    }
  }
}
'@

  $plug = [UiDriveMsg]::PlugWindow($hwnd)
  if ($plug -eq [IntPtr]::Zero) { Write-Error "No IPlugWndClass child under VoLum's main window." }
  if (-not (Test-Path $CaptureDir)) { Write-Error "Capture dir $CaptureDir does not exist. Launch VoLum with VOLUM_SELF_CAPTURE_DIR=$CaptureDir (.ui-sandbox-launch.ps1 does)." }

  if ($Clicks) {
    foreach ($pair in $Clicks.Split(';')) {
      if (-not $pair.Trim()) { continue }
      $xy = $pair.Split(',')
      [UiDriveMsg]::Click($plug, [int]$xy[0], [int]$xy[1])
      Start-Sleep -Milliseconds 450
    }
  }
  $held = $null
  if ($Drags) {
    $dragList = @($Drags.Split(';') | Where-Object { $_.Trim() })
    for ($d = 0; $d -lt $dragList.Count; $d++) {
      $points = @($dragList[$d].Split('>') | ForEach-Object { , ($_.Split(',') | ForEach-Object { [int]$_ }) })
      if ($points.Count -lt 2) { Write-Error "-Drags entry '$($dragList[$d])' needs at least two points (x1,y1>x2,y2)." }
      $xs = [int[]]@($points | ForEach-Object { $_[0] })
      $ys = [int[]]@($points | ForEach-Object { $_[1] })
      $hold = $HoldLastDrag -and $d -eq $dragList.Count - 1
      [UiDriveMsg]::Drag($plug, $xs, $ys, -not $hold)
      if ($hold) { $held = @($xs[-1], $ys[-1]) }
      Start-Sleep -Milliseconds 450
    }
  }
  if ($Keys) {
    foreach ($chunk in $Keys.Split(';')) {
      if (-not $chunk) { continue }
      [UiDriveMsg]::Keys($plug, $chunk)
      Start-Sleep -Milliseconds 350
    }
    if (-not [UiDriveMsg]::ModifiersShared) {
      Write-Warning "AttachThreadInput failed: Ctrl/Shift/Alt did not reach VoLum for at least one key."
    }
  }
  # Park the pointer where nothing hovers, like the unlocked Poke. A held drag
  # stays where it is so the capture shows it in flight.
  if (-not $held) { [UiDriveMsg]::Hover($plug, 3, 597) }
  Start-Sleep -Milliseconds $SettleMs

  $stem = "shot-" + [DateTime]::UtcNow.Ticks
  $done = Join-Path $CaptureDir "done.txt"
  Remove-Item $done -Force -ErrorAction SilentlyContinue
  [IO.File]::WriteAllText((Join-Path $CaptureDir "request.txt"), $stem)
  $bmp = Join-Path $CaptureDir "$stem.bmp"
  $deadline = (Get-Date).AddMilliseconds($CaptureTimeoutMs)
  $landed = $false
  while ((Get-Date) -lt $deadline) {
    if ((Test-Path $done) -and ((Get-Content $done -Raw) -like "$stem.bmp *")) { $landed = $true; break }
    Start-Sleep -Milliseconds 100
  }
  if (-not $landed) {
    Write-Error "VoLum did not answer the capture request in $CaptureTimeoutMs ms. Is VOLUM_SELF_CAPTURE_DIR=$CaptureDir set for pid $($proc.Id)?"
  }
  $outDir = Split-Path -Parent $Out
  if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }
  Add-Type -AssemblyName System.Drawing
  $img = [System.Drawing.Image]::FromFile($bmp)
  try { $img.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png) }
  finally { $img.Dispose() }
  Remove-Item $bmp -Force -ErrorAction SilentlyContinue
  if ($held) {
    [UiDriveMsg]::Release($plug, $held[0], $held[1])
    [UiDriveMsg]::Hover($plug, 3, 597)
  }
  $locked = [bool](Get-Process LogonUI -ErrorAction SilentlyContinue)
  Write-Host ("Wrote {0} ({1}; self-capture, workstation {2})" -f $Out, ((Get-Content $done -Raw).Trim()),
    $(if ($locked) { "LOCKED" } else { "unlocked" }))
  exit 0
}

if ($Drags) { Write-Error "-Drags needs -Locked; on an unlocked desktop use win-drag.ps1." }
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
