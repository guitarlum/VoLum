# Seeded real-input stress runner for the VoLum Windows standalone.
#
#   pwsh -File monkey-standalone-win.ps1 -DurationMin 30
#   pwsh -File monkey-standalone-win.ps1 -Seed 1 -DryRun
#   pwsh -File monkey-standalone-win.ps1 -Replay C:\temp\volum-hunt\repro-0042-unexpected-dialog.jsonl
#
# The runner uses a private LOCALAPPDATA library, records each action before
# injecting it, closes native dialogs, captures failure evidence, and leaves WER
# LocalDumps configured exactly as it found them.
[CmdletBinding()]
param(
  [Nullable[int]] $Seed = $null,
  [ValidateRange(1, 1440)][int] $DurationMin = 30,
  [string] $Exe = "",
  [string] $OutDir = "",
  [string] $Sandbox = "",
  [string] $SourceLibrary = "",
  [switch] $DryRun,
  [string] $Replay = "",
  [ValidateSet("default", "keyboard-heavy", "mouse-heavy", "overlay-heavy", "play-heavy")]
  [string] $EventMix = "default",
  [ValidateRange(1, 32768)][int] $PrivateBytesGrowthMB = 512,
  [ValidateRange(1, 1000000)][int] $HandleGrowth = 1000,
  [ValidateRange(1, 1000000)][int] $GdiGrowth = 500,
  [ValidateRange(1, 1000000)][int] $UserGrowth = 500
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $scriptDir "..\..")
if (-not $Exe) { $Exe = Join-Path $repo "NeuralAmpModeler\build-win\app\x64\Release\VoLum.exe" }

if ($null -eq $Seed) {
  $seedBytes = New-Object byte[] 4
  $crypto = [System.Security.Cryptography.RandomNumberGenerator]::Create()
  try { $crypto.GetBytes($seedBytes) } finally { $crypto.Dispose() }
  $Seed = [BitConverter]::ToInt32($seedBytes, 0) -band 0x7fffffff
}
$seedValue = [int]$Seed
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
if (-not $OutDir) { $OutDir = Join-Path $env:TEMP "volum-hunt\monkey\$stamp-$seedValue" }
$sandboxWasDefault = -not $Sandbox
if (-not $Sandbox) { $Sandbox = Join-Path $OutDir "sandbox" }

Write-Host "MONKEY seed=$seedValue duration=${DurationMin}m mix=$EventMix"
Write-Host "MONKEY out=$OutDir"

$rng = New-Object System.Random $seedValue
$script:dryT = 0
$nextProbe = 45 + $rng.Next(11)
$defaultKeyChoices = @(
  "Esc", "Enter", "Tab", "Space", "Left", "Right", "Up", "Down",
  "1", "2", "3", "t", "m", "p", "Ctrl+S", "Ctrl+Z", "Delete",
  "a", "b", "c", "f", "g", "n", "r", "s", "x"
)
$keyboardKeyChoices = @(
  "t", "m", "h", "p", "Space", "b", "Tab", "s", "1", "2", "3",
  "Left", "Right", "Up", "Down", "Esc", "Enter", "Ctrl+S"
)
$playKeyChoices = @(
  "p", "Space", "1", "2", "3", "Left", "Right", "Up", "Down",
  "Enter", "Esc", "Tab"
)

function Get-MonkeyPoint {
  param([int] $Width, [int] $Height)
  $Width = [Math]::Max(40, $Width)
  $Height = [Math]::Max(40, $Height)
  $pick = $rng.Next(100)
  $region = "uniform"
  if ($EventMix -eq "overlay-heavy" -and $pick -lt 30) {
    $region = "top-right"
    $x = $rng.Next([int]($Width * 0.78), [Math]::Max([int]($Width * 0.78) + 1, [int]($Width * 0.98)))
    $y = $rng.Next(3, [Math]::Max(4, [int]($Height * 0.09)))
  }
  elseif ($EventMix -eq "overlay-heavy" -and $pick -lt 60) {
    $region = "overlay"
    $x = $rng.Next([int]($Width * 0.48), [Math]::Max([int]($Width * 0.48) + 1, [int]($Width * 0.98)))
    $y = $rng.Next([int]($Height * 0.08), [Math]::Max([int]($Height * 0.08) + 1, [int]($Height * 0.92)))
  }
  elseif ($EventMix -eq "play-heavy" -and $pick -lt 35) {
    $region = "play-toggle"
    $x = $rng.Next([int]($Width * 0.79), [Math]::Max([int]($Width * 0.79) + 1, [int]($Width * 0.86)))
    $y = $rng.Next(3, [Math]::Max(4, [int]($Height * 0.09)))
  }
  elseif ($EventMix -eq "play-heavy" -and $pick -lt 65) {
    $region = "play-surface"
    $x = $rng.Next([int]($Width * 0.18), [Math]::Max([int]($Width * 0.18) + 1, [int]($Width * 0.98)))
    $y = $rng.Next([int]($Height * 0.10), [Math]::Max([int]($Height * 0.10) + 1, [int]($Height * 0.92)))
  }
  elseif ($pick -lt 20) {
    $region = "top"
    $x = $rng.Next(4, $Width - 4)
    $y = $rng.Next(4, [Math]::Max(5, [int]($Height * 0.11)))
  }
  elseif ($pick -lt 34) {
    $region = "footer"
    $x = $rng.Next(4, $Width - 4)
    $y = $rng.Next([int]($Height * 0.86), $Height - 4)
  }
  elseif ($pick -lt 50) {
    $region = "sidebar"
    $x = $rng.Next(4, [Math]::Max(5, [int]($Width * 0.20)))
    $y = $rng.Next([int]($Height * 0.10), [Math]::Max([int]($Height * 0.10) + 1, [int]($Height * 0.88)))
  }
  elseif ($pick -lt 70) {
    $region = "center"
    $x = $rng.Next([int]($Width * 0.22), [Math]::Max([int]($Width * 0.22) + 1, [int]($Width * 0.88)))
    $y = $rng.Next([int]($Height * 0.14), [Math]::Max([int]($Height * 0.14) + 1, [int]($Height * 0.84)))
  }
  else {
    $x = $rng.Next(4, $Width - 4)
    $y = $rng.Next(4, $Height - 4)
  }
  return @($x, $y, $region)
}

function New-MonkeyAction {
  param([int] $Width = 900, [int] $Height = 600, [switch] $Probe)
  if ($Probe) {
    $extra = [ordered]@{ keyCount = 3; pauseMs = $rng.Next(90, 221); region = "neutral" }
    $a = [ordered]@{ t = $script:dryT; kind = "probe"; x = 10; y = 10; extra = $extra }
    $script:dryT += $extra.pauseMs
    return [pscustomobject]$a
  }

  $roll = $rng.Next(100)
  $p = Get-MonkeyPoint $Width $Height
  $kind = "left"
  $extra = [ordered]@{ region = $p[2] }
  $thresholds = switch ($EventMix) {
    "keyboard-heavy" { @(18, 23, 26, 32, 38) }
    "mouse-heavy"    { @(35, 47, 55, 82, 100) }
    "overlay-heavy"  { @(48, 58, 62, 74, 82) }
    "play-heavy"     { @(42, 50, 53, 63, 70) }
    default          { @(38, 46, 52, 73, 84) }
  }
  if ($roll -lt $thresholds[0]) {
    $kind = "left"
  }
  elseif ($roll -lt $thresholds[1]) {
    $kind = "double"
  }
  elseif ($roll -lt $thresholds[2]) {
    $kind = "right"
  }
  elseif ($roll -lt $thresholds[3]) {
    $kind = "drag"
    $dx = $rng.Next(-80, 81)
    $dy = $rng.Next(-70, 71)
    if ([Math]::Abs($dx) + [Math]::Abs($dy) -lt 16) { $dy = 24 }
    $extra.x2 = [Math]::Max(2, [Math]::Min($Width - 3, $p[0] + $dx))
    $extra.y2 = [Math]::Max(2, [Math]::Min($Height - 3, $p[1] + $dy))
    $extra.steps = $rng.Next(3, 10)
  }
  elseif ($roll -lt $thresholds[4]) {
    $kind = "wheel"
    $extra.delta = @(120, -120, 240, -240, 360, -360)[$rng.Next(6)]
  }
  else {
    $kind = "key"
    $choices = switch ($EventMix) {
      "keyboard-heavy" { $keyboardKeyChoices }
      "play-heavy" { $playKeyChoices }
      default { $defaultKeyChoices }
    }
    $extra.key = $choices[$rng.Next($choices.Count)]
  }
  $extra.pauseMs = $rng.Next(70, 251)
  $a = [ordered]@{ t = $script:dryT; kind = $kind; x = [int]$p[0]; y = [int]$p[1]; extra = $extra }
  $script:dryT += $extra.pauseMs
  return [pscustomobject]$a
}

if ($DryRun) {
  if ($Replay) {
    if (-not (Test-Path -LiteralPath $Replay)) { throw "Replay file not found: $Replay" }
    $planned = @(Get-Content -LiteralPath $Replay | Where-Object { $_.Trim() } | ForEach-Object { $_ | ConvertFrom-Json })
  }
  else {
    $planned = @()
    for ($i = 0; $i -lt 25; $i++) { $planned += New-MonkeyAction }
  }
  $n = 0
  foreach ($a in $planned) {
    $extraText = $a.extra | ConvertTo-Json -Compress -Depth 8
    Write-Host ("{0:D4} t={1,5} {2,-7} x={3,3} y={4,3} extra={5}" -f $n, $a.t, $a.kind, $a.x, $a.y, $extraText)
    $n++
  }
  Write-Host "MONKEY DRY RUN actions=$n seed=$seedValue"
}

if (-not $DryRun -and -not (Test-Path -LiteralPath $Exe)) { throw "VoLum executable not found: $Exe" }
if ($Replay -and -not (Test-Path -LiteralPath $Replay)) { throw "Replay file not found: $Replay" }

Add-Type -AssemblyName System.Drawing
if (-not ([System.Management.Automation.PSTypeName]'VoLumMonkeyWin').Type) {
  Add-Type -ReferencedAssemblies System.Drawing @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public sealed class VoLumMonkeyWindow {
  public IntPtr Handle;
  public string Title;
  public string ClassName;
  public string StaticText;
}

public static class VoLumMonkeyWin {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] static extern void mouse_event(uint f, uint x, uint y, uint data, UIntPtr extra);
  [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern bool AttachThreadInput(uint from, uint to, bool attach);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll", SetLastError=true)] static extern IntPtr SendMessageTimeout(
    IntPtr h, uint msg, IntPtr w, IntPtr l, uint flags, uint timeout, out UIntPtr result);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] static extern uint GetGuiResources(IntPtr process, uint flags);

  const uint LEFTDOWN=0x0002, LEFTUP=0x0004, RIGHTDOWN=0x0008, RIGHTUP=0x0010, WHEEL=0x0800;
  const uint KEYUP=0x0002, WM_NULL=0x0000, WM_CLOSE=0x0010, BM_CLICK=0x00F5;
  const uint SMTO_ABORTIFHUNG=0x0002;
  const int SW_RESTORE=9;
  static readonly IntPtr TOPMOST=new IntPtr(-1), NOTOPMOST=new IntPtr(-2);
  const uint NOSIZE=0x0001, NOMOVE=0x0002, SHOWWINDOW=0x0040;

  static string Text(IntPtr h) {
    var s=new StringBuilder(1024); GetWindowText(h,s,s.Capacity); return s.ToString();
  }
  static string Class(IntPtr h) {
    var s=new StringBuilder(128); GetClassName(h,s,s.Capacity); return s.ToString();
  }
  static string StaticText(IntPtr h) {
    var b=new StringBuilder();
    EnumChildWindows(h,(c,l)=>{
      if (Class(c)=="Static") {
        string t=Text(c).Trim();
        if (t.Length>0) { if (b.Length>0) b.Append(" | "); b.Append(t); }
      }
      return true;
    },IntPtr.Zero);
    return b.ToString();
  }
  public static VoLumMonkeyWindow[] ExtraWindows(int pid, IntPtr main) {
    var a=new List<VoLumMonkeyWindow>();
    EnumWindows((h,l)=>{
      uint p; GetWindowThreadProcessId(h,out p);
      if (p==(uint)pid && h!=main && IsWindowVisible(h))
        a.Add(new VoLumMonkeyWindow { Handle=h, Title=Text(h), ClassName=Class(h), StaticText=StaticText(h) });
      return true;
    },IntPtr.Zero);
    return a.ToArray();
  }
  public static bool WindowExists(IntPtr h) { return IsWindow(h); }
  public static void CloseDialog(IntPtr h) {
    IntPtr preferred=IntPtr.Zero, fallback=IntPtr.Zero;
    EnumChildWindows(h,(c,l)=>{
      if (Class(c)!="Button") return true;
      if (fallback==IntPtr.Zero) fallback=c;
      string t=Text(c).Replace("&","").Trim();
      int id=GetDlgCtrlID(c);
      if (id==2 || t.Equals("Cancel",StringComparison.OrdinalIgnoreCase) ||
          t.Equals("Abbrechen",StringComparison.OrdinalIgnoreCase)) preferred=c;
      return true;
    },IntPtr.Zero);
    IntPtr button=preferred!=IntPtr.Zero ? preferred : fallback;
    if (button!=IntPtr.Zero) SendMessage(button,BM_CLICK,IntPtr.Zero,IntPtr.Zero);
    Thread.Sleep(100);
    if (IsWindow(h)) {
      KeyStroke("Esc");
      Thread.Sleep(100);
    }
    if (IsWindow(h)) PostMessage(h,WM_CLOSE,IntPtr.Zero,IntPtr.Zero);
  }
  public static void ClientSize(IntPtr h, out int w, out int height) {
    RECT r; GetClientRect(h,out r); w=Math.Max(1,r.Right-r.Left); height=Math.Max(1,r.Bottom-r.Top);
  }
  static POINT ScreenPoint(IntPtr h, int x, int y) {
    RECT r; GetClientRect(h,out r);
    x=Math.Max(0,Math.Min(r.Right-r.Left-1,x)); y=Math.Max(0,Math.Min(r.Bottom-r.Top-1,y));
    POINT p=new POINT { X=x, Y=y }; ClientToScreen(h,ref p); return p;
  }
  public static void ForceFront(IntPtr h) {
    if (IsIconic(h)) { ShowWindow(h,SW_RESTORE); Thread.Sleep(120); }
    uint ignored; uint fg=GetWindowThreadProcessId(GetForegroundWindow(),out ignored);
    uint self=GetCurrentThreadId();
    bool attached=fg!=0 && fg!=self && AttachThreadInput(self,fg,true);
    SetWindowPos(h,TOPMOST,0,0,0,0,NOSIZE|NOMOVE|SHOWWINDOW);
    BringWindowToTop(h); SetForegroundWindow(h);
    SetWindowPos(h,NOTOPMOST,0,0,0,0,NOSIZE|NOMOVE);
    if (attached) AttachThreadInput(self,fg,false);
    Thread.Sleep(80);
  }
  public static void Click(IntPtr h,int x,int y,bool right,bool twice) {
    POINT p=ScreenPoint(h,x,y); SetCursorPos(p.X,p.Y); Thread.Sleep(35);
    uint down=right?RIGHTDOWN:LEFTDOWN, up=right?RIGHTUP:LEFTUP;
    mouse_event(down,0,0,0,UIntPtr.Zero); Thread.Sleep(35); mouse_event(up,0,0,0,UIntPtr.Zero);
    if (twice) { Thread.Sleep(70); mouse_event(down,0,0,0,UIntPtr.Zero); Thread.Sleep(30); mouse_event(up,0,0,0,UIntPtr.Zero); }
  }
  public static void Drag(IntPtr h,int x1,int y1,int x2,int y2,int steps) {
    POINT a=ScreenPoint(h,x1,y1), b=ScreenPoint(h,x2,y2);
    SetCursorPos(a.X,a.Y); Thread.Sleep(35); mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    steps=Math.Max(1,steps);
    for(int i=1;i<=steps;i++) { SetCursorPos(a.X+(b.X-a.X)*i/steps,a.Y+(b.Y-a.Y)*i/steps); Thread.Sleep(15); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Wheel(IntPtr h,int x,int y,int delta) {
    POINT p=ScreenPoint(h,x,y); SetCursorPos(p.X,p.Y); Thread.Sleep(25);
    mouse_event(WHEEL,0,0,unchecked((uint)delta),UIntPtr.Zero);
  }
  static byte Vk(string key) {
    switch(key.ToUpperInvariant()) {
      case "ESC": return 0x1B; case "ENTER": return 0x0D; case "TAB": return 0x09;
      case "SPACE": return 0x20; case "LEFT": return 0x25; case "UP": return 0x26;
      case "RIGHT": return 0x27; case "DOWN": return 0x28; case "DELETE": return 0x2E;
      default:
        if (key.Length==1) {
          char c=Char.ToUpperInvariant(key[0]);
          if ((c>='A'&&c<='Z')||(c>='0'&&c<='9')) return (byte)c;
        }
        throw new ArgumentException("Unsupported key "+key);
    }
  }
  public static void KeyStroke(string spec) {
    bool ctrl=spec.StartsWith("Ctrl+",StringComparison.OrdinalIgnoreCase);
    string key=ctrl?spec.Substring(5):spec;
    byte vk=Vk(key);
    if(ctrl) keybd_event(0x11,0,0,UIntPtr.Zero);
    keybd_event(vk,0,0,UIntPtr.Zero); Thread.Sleep(20); keybd_event(vk,0,KEYUP,UIntPtr.Zero);
    if(ctrl) keybd_event(0x11,0,KEYUP,UIntPtr.Zero);
  }
  public static void ReleaseInput() {
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero); mouse_event(RIGHTUP,0,0,0,UIntPtr.Zero);
    keybd_event(0x10,0,KEYUP,UIntPtr.Zero); keybd_event(0x11,0,KEYUP,UIntPtr.Zero);
    keybd_event(0x12,0,KEYUP,UIntPtr.Zero); keybd_event(0x5B,0,KEYUP,UIntPtr.Zero);
    keybd_event(0x5C,0,KEYUP,UIntPtr.Zero);
  }
  public static bool Responds(IntPtr h,int timeoutMs) {
    UIntPtr result;
    return SendMessageTimeout(h,WM_NULL,IntPtr.Zero,IntPtr.Zero,SMTO_ABORTIFHUNG,(uint)timeoutMs,out result)!=IntPtr.Zero;
  }
  static double Dominant(Bitmap b) {
    var counts=new Dictionary<int,int>(); int max=0,total=0;
    for(int y=0;y<b.Height;y+=3) for(int x=0;x<b.Width;x+=3) {
      Color c=b.GetPixel(x,y); int k=((c.R&0xF8)<<16)|((c.G&0xF8)<<8)|(c.B&0xF8);
      int n; counts.TryGetValue(k,out n); n++; counts[k]=n; if(n>max)max=n; total++;
    }
    return total==0?100.0:100.0*max/total;
  }
  public static double SaveClient(IntPtr h,string path) {
    RECT wr,cr; GetWindowRect(h,out wr); GetClientRect(h,out cr);
    POINT o=new POINT(); ClientToScreen(h,ref o);
    int fw=Math.Max(1,wr.Right-wr.Left), fh=Math.Max(1,wr.Bottom-wr.Top);
    int w=Math.Max(1,cr.Right-cr.Left), height=Math.Max(1,cr.Bottom-cr.Top);
    using(var full=new Bitmap(fw,fh,PixelFormat.Format32bppArgb)) {
      using(var g=Graphics.FromImage(full)) { IntPtr dc=g.GetHdc(); try { PrintWindow(h,dc,2); } finally { g.ReleaseHdc(dc); } }
      var crop=new Rectangle(Math.Max(0,o.X-wr.Left),Math.Max(0,o.Y-wr.Top),Math.Min(w,fw),Math.Min(height,fh));
      using(var client=full.Clone(crop,PixelFormat.Format32bppArgb)) { client.Save(path,ImageFormat.Png); return Dominant(client); }
    }
  }
  public static void SaveWindow(IntPtr h,string path) {
    RECT r; GetWindowRect(h,out r); int w=Math.Max(1,r.Right-r.Left), height=Math.Max(1,r.Bottom-r.Top);
    using(var b=new Bitmap(w,height,PixelFormat.Format32bppArgb)) {
      using(var g=Graphics.FromImage(b)) { IntPtr dc=g.GetHdc(); try { PrintWindow(h,dc,2); } finally { g.ReleaseHdc(dc); } }
      b.Save(path,ImageFormat.Png);
    }
  }
  public static uint GdiObjects(Process p) { return GetGuiResources(p.Handle,0); }
  public static uint UserObjects(Process p) { return GetGuiResources(p.Handle,1); }
}
'@
}

if ($DryRun) { exit 0 }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$dumpsDir = Join-Path $OutDir "dumps"
$shotsDir = Join-Path $OutDir "screenshots"
New-Item -ItemType Directory -Path $dumpsDir, $shotsDir -Force | Out-Null
$actionsPath = Join-Path $OutDir "actions.jsonl"
$flagsPath = Join-Path $OutDir "flags.jsonl"
$dialogsPath = Join-Path $OutDir "dialogs.jsonl"
$metricsPath = Join-Path $OutDir "metrics.jsonl"
[IO.File]::WriteAllText($actionsPath, "", (New-Object Text.UTF8Encoding $false))
[IO.File]::WriteAllText($flagsPath, "", (New-Object Text.UTF8Encoding $false))
[IO.File]::WriteAllText($dialogsPath, "", (New-Object Text.UTF8Encoding $false))
[IO.File]::WriteAllText($metricsPath, "", (New-Object Text.UTF8Encoding $false))
$runInfo = [ordered]@{
  seed = $seedValue
  durationMin = $DurationMin
  exe = $Exe
  outDir = $OutDir
  sandbox = $Sandbox
  sourceLibrary = $SourceLibrary
  replay = $Replay
  eventMix = $EventMix
  startedUtc = [DateTime]::UtcNow.ToString("o")
}
$runInfo | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutDir "run.json") -Encoding UTF8

$library = Join-Path $Sandbox "VoLum"
if ($sandboxWasDefault -or $SourceLibrary -or -not (Test-Path -LiteralPath $library)) {
  if (Test-Path -LiteralPath $library) { Remove-Item -LiteralPath $library -Recurse -Force }
  New-Item -ItemType Directory -Path $library -Force | Out-Null
  $source = if ($SourceLibrary) { $SourceLibrary } else { Join-Path $repo "docs\screenshot-seed" }
  if (-not (Test-Path -LiteralPath $source)) { throw "Source library not found: $source" }
  Copy-Item (Join-Path $source "*") $library -Recurse -Force
}
New-Item -ItemType Directory -Path $Sandbox -Force | Out-Null

function Enable-WerDumps {
  param([string] $Folder)
  $path = "Software\Microsoft\Windows\Windows Error Reporting\LocalDumps"
  $base = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($path)
  $old = $base.OpenSubKey("VoLum.exe", $false)
  $snapshot = [ordered]@{ existed = ($null -ne $old); values = @() }
  if ($old) {
    foreach ($name in $old.GetValueNames()) {
      $snapshot.values += [pscustomobject]@{
        name = $name
        value = $old.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        kind = $old.GetValueKind($name).ToString()
      }
    }
    $old.Dispose()
  }
  $app = $base.CreateSubKey("VoLum.exe")
  $app.SetValue("DumpFolder", $Folder, [Microsoft.Win32.RegistryValueKind]::ExpandString)
  $app.SetValue("DumpType", 2, [Microsoft.Win32.RegistryValueKind]::DWord)
  $app.Dispose()
  $base.Dispose()
  return $snapshot
}

function Restore-WerDumps {
  param($Snapshot)
  if ($null -eq $Snapshot) { return }
  $path = "Software\Microsoft\Windows\Windows Error Reporting\LocalDumps"
  $base = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($path)
  try { $base.DeleteSubKeyTree("VoLum.exe", $false) } catch {}
  if ($Snapshot.existed) {
    $app = $base.CreateSubKey("VoLum.exe")
    foreach ($v in $Snapshot.values) {
      $kind = [Microsoft.Win32.RegistryValueKind][Enum]::Parse([Microsoft.Win32.RegistryValueKind], [string]$v.kind)
      $app.SetValue($v.name, $v.value, $kind)
    }
    $app.Dispose()
  }
  $base.Dispose()
}

$script:proc = $null
$script:main = [IntPtr]::Zero
$script:actionIndex = -1
$script:currentAction = $null
$script:flagCount = 0
$script:flagKinds = @{}
$script:restarts = 0
$script:blankStreak = 0
$script:logLineCount = 0
$script:dialogPoints = @{}
$script:dialogRegionCounts = @{}
$script:lastDialogIndex = -100
$script:lastDialogPoint = $null
$script:dialogStreak = 0
$script:metricSamples = @()
$script:leakFlagged = @{}
$script:werSnapshot = $null
$script:runWatch = [Diagnostics.Stopwatch]::StartNew()
$script:runDeadline = (Get-Date).AddMinutes($DurationMin)
$logPath = Join-Path $library "volum.log"

function ConvertTo-JsonLine {
  param($Object)
  return ($Object | ConvertTo-Json -Compress -Depth 12)
}
function Add-JsonLine {
  param([string] $Path, $Object)
  [IO.File]::AppendAllText($Path, (ConvertTo-JsonLine $Object) + [Environment]::NewLine, (New-Object Text.UTF8Encoding $false))
}
function Get-LastActions {
  if (-not (Test-Path $actionsPath)) { return @() }
  return @(Get-Content -LiteralPath $actionsPath | Select-Object -Last 30 | ForEach-Object { $_ | ConvertFrom-Json })
}
function Get-LogTail {
  if (-not (Test-Path $logPath)) { return @() }
  return @(Get-Content -LiteralPath $logPath | Select-Object -Last 80)
}
function Test-ProcessAlive {
  if ($null -eq $script:proc) { return $false }
  try { $script:proc.Refresh(); return -not $script:proc.HasExited } catch { return $false }
}

function Save-MonkeyScreenshot {
  param([string] $Stem, [switch] $CheckBlank)
  if (-not (Test-ProcessAlive) -or $script:main -eq [IntPtr]::Zero) { return $null }
  $safe = $Stem -replace '[^A-Za-z0-9_.-]', '_'
  $path = Join-Path $shotsDir "$safe.png"
  try {
    $dominant = [VoLumMonkeyWin]::SaveClient($script:main, $path)
    if ($CheckBlank) {
      if ($dominant -gt 98.0) { $script:blankStreak++ } else { $script:blankStreak = 0 }
      if ($script:blankStreak -ge 3) {
        $script:blankStreak = 0
        Add-MonkeyFlag "blank-frame" ([ordered]@{ dominantPct = $dominant; screenshot = $path })
      }
    }
    return $path
  }
  catch {
    Write-Warning "Screenshot failed: $($_.Exception.Message)"
    return $null
  }
}

function Add-MonkeyFlag {
  param([string] $Kind, $Context)
  $script:flagCount++
  if (-not $script:flagKinds.ContainsKey($Kind)) { $script:flagKinds[$Kind] = 0 }
  $script:flagKinds[$Kind]++
  $entry = [ordered]@{
    t = $script:runWatch.ElapsedMilliseconds
    actionIndex = $script:actionIndex
    kind = $Kind
    action = $script:currentAction
    context = $Context
  }
  Add-JsonLine $flagsPath $entry
  $repro = Join-Path $OutDir ("repro-{0:D6}-{1}.jsonl" -f ([Math]::Max(0, $script:actionIndex)), $Kind)
  Copy-Item -LiteralPath $actionsPath -Destination $repro -Force
  [void](Save-MonkeyScreenshot ("flag-{0:D6}-{1}" -f ([Math]::Max(0, $script:actionIndex)), $Kind))
  Write-Warning ("MONKEY flag {0} at action {1}" -f $Kind, $script:actionIndex)
}

function Start-VoLum {
  $psi = New-Object Diagnostics.ProcessStartInfo
  $psi.FileName = (Resolve-Path -LiteralPath $Exe).Path
  $psi.UseShellExecute = $false
  $psi.EnvironmentVariables["LOCALAPPDATA"] = $Sandbox
  $psi.EnvironmentVariables["VOLUM_PACK_SAVE_PATH"] = (Join-Path $OutDir "monkey.volumpack")
  $psi.EnvironmentVariables["VOLUM_PACK_OPEN_PATH"] = (Join-Path $OutDir "monkey.volumpack")
  $script:proc = [Diagnostics.Process]::Start($psi)
  [IO.File]::WriteAllText((Join-Path $OutDir "pid.txt"), [string]$script:proc.Id, (New-Object Text.ASCIIEncoding))
  $deadline = (Get-Date).AddSeconds(25)
  while ((Get-Date) -lt $deadline) {
    $script:proc.Refresh()
    if ($script:proc.HasExited) { break }
    if ($script:proc.MainWindowHandle -ne 0) {
      $script:main = $script:proc.MainWindowHandle
      return $true
    }
    foreach ($d in [VoLumMonkeyWin]::ExtraWindows($script:proc.Id, [IntPtr]::Zero)) {
      [VoLumMonkeyWin]::CloseDialog($d.Handle)
    }
    Start-Sleep -Milliseconds 200
  }
  $script:main = [IntPtr]::Zero
  if (-not $script:proc.HasExited) {
    try { $script:proc.Kill(); $script:proc.WaitForExit(5000) | Out-Null } catch {}
  }
  $script:proc = $null
  return $false
}

function Stop-VoLum {
  if (Test-ProcessAlive) {
    try { $script:proc.Kill(); $script:proc.WaitForExit(5000) | Out-Null } catch {}
  }
  $script:proc = $null
  $script:main = [IntPtr]::Zero
}

function Restart-VoLum {
  Stop-VoLum
  $script:restarts++
  $script:metricSamples = @()
  $script:leakFlagged = @{}
  Start-Sleep -Milliseconds 500
  if (-not (Start-VoLum)) {
    Add-MonkeyFlag "restart-failed" ([ordered]@{ restart = $script:restarts })
    return $false
  }
  return $true
}

function Close-PreexistingDialogs {
  if (-not (Test-ProcessAlive)) { return }
  foreach ($d in [VoLumMonkeyWin]::ExtraWindows($script:proc.Id, $script:main)) {
    [VoLumMonkeyWin]::CloseDialog($d.Handle)
  }
}

function Get-PointDistance {
  param($A, $B)
  if ($null -eq $A -or $null -eq $B) { return [double]::PositiveInfinity }
  $dx = [double]$A[0] - [double]$B[0]
  $dy = [double]$A[1] - [double]$B[1]
  return [Math]::Sqrt($dx * $dx + $dy * $dy)
}

function Handle-NewDialogs {
  param($Action)
  if (-not (Test-ProcessAlive)) { return 0 }
  $dialogs = @()
  for ($poll = 0; $poll -lt 4; $poll++) {
    $dialogs = @([VoLumMonkeyWin]::ExtraWindows($script:proc.Id, $script:main))
    if ($dialogs.Count -gt 0) { break }
    Start-Sleep -Milliseconds 50
  }
  if ($dialogs.Count -eq 0) {
    $script:dialogStreak = 0
    return 0
  }

  $clickish = @("left", "double", "right", "drag", "probe") -contains [string]$Action.kind
  $point = if ($clickish) { @([int]$Action.x, [int]$Action.y) } else { $null }
  $farFromPreviousAction = (Get-PointDistance $point $script:lastDialogPoint) -gt 80.0
  if ($clickish -and $script:lastDialogIndex -eq ($script:actionIndex - 1) -and $farFromPreviousAction) {
    $script:dialogStreak++
  }
  else {
    $script:dialogStreak = 1
  }
  $script:lastDialogIndex = $script:actionIndex
  $script:lastDialogPoint = $point

  foreach ($d in $dialogs) {
    $titleKey = if ($d.Title) { $d.Title } else { "<$($d.ClassName)>" }
    $safeTitle = $titleKey -replace '[^A-Za-z0-9_.-]', '_'
    $dialogShot = Join-Path $shotsDir ("dialog-{0:D6}-{1}.png" -f $script:actionIndex, $safeTitle)
    try { [VoLumMonkeyWin]::SaveWindow($d.Handle, $dialogShot) } catch { $dialogShot = $null }
    Add-JsonLine $dialogsPath ([ordered]@{
      t = $script:runWatch.ElapsedMilliseconds
      actionIndex = $script:actionIndex
      action = $Action
      title = $d.Title
      class = $d.ClassName
      text = $d.StaticText
      screenshot = $dialogShot
    })

    if ($clickish) {
      $region = if ($Action.extra -and $Action.extra.region) { [string]$Action.extra.region } else { "unknown" }
      $regionKey = "$titleKey|$region"
      if (-not $script:dialogRegionCounts.ContainsKey($regionKey)) { $script:dialogRegionCounts[$regionKey] = 0 }
      $script:dialogRegionCounts[$regionKey]++
      if (-not $script:dialogPoints.ContainsKey($titleKey)) {
        $script:dialogPoints[$titleKey] = New-Object Collections.ArrayList
      }
      $previous = @($script:dialogPoints[$titleKey])
      if ($previous.Count -gt 0) {
        $near = $false
        foreach ($old in $previous) {
          if ((Get-PointDistance $point $old) -le 80.0) { $near = $true; break }
        }
        if (-not $near) {
          Add-MonkeyFlag "unexpected-dialog" ([ordered]@{
            reason = "same-title-far-from-known-opener"
            title = $titleKey
            text = $d.StaticText
            point = $point
            previousPoints = $previous
          })
        }
      }
      [void]$script:dialogPoints[$titleKey].Add($point)
    }
    [VoLumMonkeyWin]::CloseDialog($d.Handle)
  }

  if ($script:dialogStreak -ge 3) {
    Add-MonkeyFlag "unexpected-dialog" ([ordered]@{
      reason = "three-consecutive-unrelated-clicks"
      streak = $script:dialogStreak
      point = $point
    })
    $script:dialogStreak = 0
  }
  return $dialogs.Count
}

function Invoke-MonkeyAction {
  param($Action)
  Close-PreexistingDialogs
  [VoLumMonkeyWin]::ForceFront($script:main)
  switch ([string]$Action.kind) {
    "left"   { [VoLumMonkeyWin]::Click($script:main, [int]$Action.x, [int]$Action.y, $false, $false) }
    "double" { [VoLumMonkeyWin]::Click($script:main, [int]$Action.x, [int]$Action.y, $false, $true) }
    "right"  { [VoLumMonkeyWin]::Click($script:main, [int]$Action.x, [int]$Action.y, $true, $false) }
    "drag"   { [VoLumMonkeyWin]::Drag($script:main, [int]$Action.x, [int]$Action.y,
        [int]$Action.extra.x2, [int]$Action.extra.y2, [int]$Action.extra.steps) }
    "wheel"  { [VoLumMonkeyWin]::Wheel($script:main, [int]$Action.x, [int]$Action.y, [int]$Action.extra.delta) }
    "key"    { [VoLumMonkeyWin]::KeyStroke([string]$Action.extra.key) }
    "probe"  {
      [VoLumMonkeyWin]::KeyStroke("Esc"); [VoLumMonkeyWin]::KeyStroke("Esc"); [VoLumMonkeyWin]::KeyStroke("Esc")
      [VoLumMonkeyWin]::Click($script:main, [int]$Action.x, [int]$Action.y, $false, $false)
    }
    default { throw "Unknown replay action kind '$($Action.kind)'" }
  }
}

function Check-NewLogErrors {
  if (-not (Test-Path -LiteralPath $logPath)) { return }
  $all = @(Get-Content -LiteralPath $logPath)
  if ($all.Count -lt $script:logLineCount) { $script:logLineCount = 0 }
  $new = @()
  for ($i = $script:logLineCount; $i -lt $all.Count; $i++) {
    if ($all[$i] -match '(?i)exception|assert|error') { $new += $all[$i] }
  }
  $script:logLineCount = $all.Count
  if ($new.Count -gt 0) { Add-MonkeyFlag "log-error" ([ordered]@{ lines = $new }) }
}

function Save-FailureEvidence {
  param([string] $Kind)
  $last = Get-LastActions
  Add-JsonLine (Join-Path $OutDir "$Kind-last-actions.json") $last
  [IO.File]::WriteAllLines((Join-Path $OutDir "$Kind-volum-log-tail.txt"), [string[]](Get-LogTail))
}

function Save-HangDump {
  if (-not (Test-ProcessAlive)) { return }
  $procdump = Get-Command procdump.exe -ErrorAction SilentlyContinue
  if (-not $procdump) { $procdump = Get-Command procdump64.exe -ErrorAction SilentlyContinue }
  if ($procdump) {
    try { & $procdump.Source -accepteula -ma $script:proc.Id (Join-Path $dumpsDir "hang-$($script:actionIndex).dmp") | Out-Null } catch {}
  }
  else {
    try {
      $script:proc.Refresh()
      $info = [ordered]@{
        id = $script:proc.Id
        responding = $script:proc.Responding
        privateBytes = $script:proc.PrivateMemorySize64
        handles = $script:proc.HandleCount
        threads = $script:proc.Threads.Count
        startTime = $script:proc.StartTime
      }
      $info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutDir "hang-$($script:actionIndex)-process.json") -Encoding UTF8
    } catch {}
  }
}

function Sample-Metrics {
  if (-not (Test-ProcessAlive)) { return }
  try {
    $script:proc.Refresh()
    $sample = [pscustomobject][ordered]@{
      t = $script:runWatch.ElapsedMilliseconds
      privateBytes = [int64]$script:proc.PrivateMemorySize64
      workingSet = [int64]$script:proc.WorkingSet64
      handles = [int]$script:proc.HandleCount
      gdi = [int][VoLumMonkeyWin]::GdiObjects($script:proc)
      user = [int][VoLumMonkeyWin]::UserObjects($script:proc)
    }
    $script:metricSamples += $sample
    Add-JsonLine $metricsPath $sample
    if ($script:metricSamples.Count -lt 4) { return }
    $recent = @($script:metricSamples | Select-Object -Last 4)
    $checks = @(
      [pscustomobject]@{ name = "privateBytes"; threshold = ([int64]$PrivateBytesGrowthMB * 1MB) },
      [pscustomobject]@{ name = "handles"; threshold = [int64]$HandleGrowth },
      [pscustomobject]@{ name = "gdi"; threshold = [int64]$GdiGrowth },
      [pscustomobject]@{ name = "user"; threshold = [int64]$UserGrowth }
    )
    foreach ($check in $checks) {
      $name = $check.name; $threshold = [int64]$check.threshold
      if ($script:leakFlagged.ContainsKey($name)) { continue }
      $steady = $true
      for ($i = 1; $i -lt $recent.Count; $i++) {
        if ([int64]$recent[$i].$name -lt [int64]$recent[$i - 1].$name) { $steady = $false; break }
      }
      $growth = [int64]$recent[-1].$name - [int64]$script:metricSamples[0].$name
      if ($steady -and $growth -gt $threshold) {
        $script:leakFlagged[$name] = $true
        Add-MonkeyFlag "leak" ([ordered]@{ metric = $name; growth = $growth; threshold = $threshold; recent = $recent })
      }
    }
  }
  catch { Write-Warning "Metric sample failed: $($_.Exception.Message)" }
}

function Test-CheapInvariants {
  param($Action)
  if (-not (Test-ProcessAlive)) {
    Save-FailureEvidence "crash-$($script:actionIndex)"
    Add-MonkeyFlag "crash" ([ordered]@{ lastActions = Get-LastActions; logTail = Get-LogTail })
    [void](Restart-VoLum)
    return
  }
  if (-not [VoLumMonkeyWin]::Responds($script:main, 2000)) {
    [void](Save-MonkeyScreenshot ("hang-pending-{0:D6}" -f $script:actionIndex))
    Start-Sleep -Seconds 10
    if (-not [VoLumMonkeyWin]::Responds($script:main, 2000)) {
      Save-HangDump
      Save-FailureEvidence "hang-$($script:actionIndex)"
      Add-MonkeyFlag "hang" ([ordered]@{ waitedSeconds = 10; lastActions = Get-LastActions; logTail = Get-LogTail })
      [void](Restart-VoLum)
      return
    }
  }
  Check-NewLogErrors
}

$summary = $null
$fatal = $null
$actionsDone = 0
$nextMetricAt = [int64]0
$replayActions = $null
$replayFirstT = [int64]0
$replayStartedAt = $null
if ($Replay) {
  $replayActions = @(Get-Content -LiteralPath $Replay | Where-Object { $_.Trim() } | ForEach-Object { $_ | ConvertFrom-Json })
  if ($replayActions.Count -gt 0) { $replayFirstT = [int64]$replayActions[0].t }
}

try {
  $script:werSnapshot = Enable-WerDumps $dumpsDir
  if (-not (Start-VoLum)) {
    Add-MonkeyFlag "launch-failed" ([ordered]@{ exe = $Exe })
  }
  else {
    Sample-Metrics
    $nextMetricAt = $script:runWatch.ElapsedMilliseconds + 30000
  }

  while ($true) {
    if ($null -ne $replayActions) {
      if ($actionsDone -ge $replayActions.Count) { break }
    }
    elseif ((Get-Date) -ge $script:runDeadline) {
      break
    }

    if (-not (Test-ProcessAlive)) {
      if (Restart-VoLum) {
        Sample-Metrics
        $nextMetricAt = $script:runWatch.ElapsedMilliseconds + 30000
      }
      else {
        Start-Sleep -Seconds 2
        continue
      }
    }

    if ($null -ne $replayActions) {
      $action = $replayActions[$actionsDone]
      if ($null -eq $replayStartedAt) { $replayStartedAt = $script:runWatch.ElapsedMilliseconds }
      $target = [int64]$replayStartedAt + ([int64]$action.t - $replayFirstT)
      $wait = $target - $script:runWatch.ElapsedMilliseconds
      if ($wait -gt 0) { Start-Sleep -Milliseconds ([int][Math]::Min($wait, [int]::MaxValue)) }
    }
    else {
      $w = 900; $h = 600
      [VoLumMonkeyWin]::ClientSize($script:main, [ref]$w, [ref]$h)
      $isProbe = ($actionsDone -ge $nextProbe)
      $action = New-MonkeyAction -Width $w -Height $h -Probe:$isProbe
      if ($isProbe) { $nextProbe += 45 + $rng.Next(11) }
      $action.t = $script:runWatch.ElapsedMilliseconds
    }

    $script:actionIndex = $actionsDone
    $script:currentAction = $action
    Add-JsonLine $actionsPath $action
    Invoke-MonkeyAction $action
    Start-Sleep -Milliseconds 80
    $dialogCount = Handle-NewDialogs $action
    if ([string]$action.kind -eq "probe") {
      $alive = Test-ProcessAlive
      $responsive = $alive -and [VoLumMonkeyWin]::Responds($script:main, 2000)
      if ($alive -and ($dialogCount -gt 0 -or -not $responsive)) {
        Add-MonkeyFlag "stuck-input" ([ordered]@{ dialogs = $dialogCount; responsive = $responsive })
      }
    }
    Test-CheapInvariants $action
    $actionsDone++

    if (($actionsDone % 100) -eq 0) {
      [void](Save-MonkeyScreenshot ("periodic-{0:D6}" -f $actionsDone) -CheckBlank)
    }
    if ($script:runWatch.ElapsedMilliseconds -ge $nextMetricAt) {
      Sample-Metrics
      $nextMetricAt = $script:runWatch.ElapsedMilliseconds + 30000
    }
    if ($null -eq $replayActions) {
      $pause = 100
      if ($null -ne $action.extra -and $null -ne $action.extra.pauseMs) { $pause = [int]$action.extra.pauseMs }
      if ($pause -gt 0) { Start-Sleep -Milliseconds $pause }
    }

    if (-not (Test-ProcessAlive) -and ($null -eq $replayActions -and (Get-Date) -lt $script:runDeadline)) {
      Save-FailureEvidence "crash-$($script:actionIndex)"
      Add-MonkeyFlag "crash" ([ordered]@{ lastActions = Get-LastActions; logTail = Get-LogTail })
      [void](Restart-VoLum)
    }
  }
}
catch {
  $fatal = $_
  try { Add-MonkeyFlag "harness-error" ([ordered]@{ message = $_.Exception.Message; position = $_.InvocationInfo.PositionMessage }) } catch {}
}
finally {
  try { [VoLumMonkeyWin]::ReleaseInput() } catch {}
  Stop-VoLum
  try { Restore-WerDumps $script:werSnapshot } catch { Write-Warning "Could not restore WER LocalDumps: $($_.Exception.Message)" }
}

$kindSummary = [ordered]@{}
foreach ($k in ($script:flagKinds.Keys | Sort-Object)) { $kindSummary[$k] = $script:flagKinds[$k] }
$dialogSummary = [ordered]@{}
foreach ($k in ($script:dialogRegionCounts.Keys | Sort-Object)) { $dialogSummary[$k] = $script:dialogRegionCounts[$k] }
$summary = [ordered]@{
  seed = $seedValue
  requestedDurationMin = $DurationMin
  elapsedSeconds = [Math]::Round($script:runWatch.Elapsed.TotalSeconds, 3)
  replay = $Replay
  eventMix = $EventMix
  actions = $actionsDone
  restarts = $script:restarts
  flags = $script:flagCount
  flagsByKind = $kindSummary
  dialogsByTitleAndRegion = $dialogSummary
  outDir = $OutDir
  sandbox = $Sandbox
}
$summary | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutDir "summary.json") -Encoding UTF8
if ($fatal) { Write-Warning $fatal.Exception.Message }
if ($script:flagCount -eq 0) {
  Write-Host "MONKEY OK"
  exit 0
}
Write-Host "MONKEY FLAGS $($script:flagCount)"
exit 1
