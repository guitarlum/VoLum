# Desktop motion reference sheets for the Android shootout (ticket 01).
# Drives the sandboxed Release standalone through ui-drive.ps1 -Locked and
# grabs frames through VoLum's self-capture (VOLUM_SELF_CAPTURE_DIR), so it
# works with the workstation locked or unlocked. Build the app first with
# run-app-win.ps1; this never touches the real %LOCALAPPDATA%\VoLum library.
#
#   powershell -File capture-desktop-motion.ps1 -Repo C:\dev\VoLum -Out C:\dev\VoLum\.scratch\android-shootout\reference
#
# Writes <motion>.png (4x4 contact sheet, 450x300 tiles, capture time stamped
# on each tile) and frames\<motion>\NNN-<ms>.png for frame-by-frame checks.
param(
  [Parameter(Mandatory = $true)][string] $Repo,
  [Parameter(Mandatory = $true)][string] $Out,
  [int] $IntervalMs = 125
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$scripts = Join-Path $Repo "NeuralAmpModeler\scripts"
$sandbox = Join-Path $env:TEMP "volum-ui-sandbox"
$captureDir = Join-Path $sandbox "capture"
New-Item -ItemType Directory -Force $Out | Out-Null

function Start-Sandbox([string] $artDebug) {
  if ($artDebug) { $env:VOLUM_ART_ANIM_DEBUG = $artDebug } else { Remove-Item Env:\VOLUM_ART_ANIM_DEBUG -ErrorAction SilentlyContinue }
  & powershell -NoProfile -File (Join-Path $scripts ".ui-sandbox-launch.ps1") -Reseed | Out-Host
  Remove-Item Env:\VOLUM_ART_ANIM_DEBUG -ErrorAction SilentlyContinue
  Start-Sleep -Seconds 4
}

function Invoke-Drive([string] $clicks, [string] $keys, [int] $settleMs, [string] $shot) {
  $a = @("-NoProfile", "-File", (Join-Path $scripts "ui-drive.ps1"), "-Locked", "-SettleMs", $settleMs, "-Out", $shot)
  if ($clicks) { $a += @("-Clicks", $clicks) }
  if ($keys) { $a += @("-Keys", $keys) }
  & powershell @a | Out-Host
}

# Grabs frames for $durationMs; $events are @{ at = <ms>; keys = "<SendKeys>" } sent in a second process.
function Get-Frames([string] $name, [int] $durationMs, [object[]] $events = @()) {
  $dir = Join-Path $Out "frames\$name"
  if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
  New-Item -ItemType Directory -Force $dir | Out-Null
  $jobs = @()
  foreach ($e in $events) {
    $jobs += Start-Job -ArgumentList $scripts, $e.at, $e.keys, $captureDir -ScriptBlock {
      param($scripts, $at, $keys, $captureDir)
      Start-Sleep -Milliseconds $at
      $shot = Join-Path $env:TEMP ("volum-ref-event-" + [guid]::NewGuid() + ".png")
      & powershell -NoProfile -File (Join-Path $scripts "ui-drive.ps1") -Locked -SettleMs 0 -Keys $keys -Out $shot -CaptureDir $captureDir | Out-Null
    }
  }
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $i = 0
  $frames = @()
  while ($sw.ElapsedMilliseconds -lt $durationMs) {
    $tick = $sw.ElapsedMilliseconds
    $stem = "ref-$name-$i-" + [DateTime]::UtcNow.Ticks
    $done = Join-Path $captureDir "done.txt"
    [IO.File]::WriteAllText((Join-Path $captureDir "request.txt"), $stem)
    $deadline = $sw.ElapsedMilliseconds + 1500
    while ($sw.ElapsedMilliseconds -lt $deadline) {
      if ((Test-Path $done) -and ((Get-Content $done -Raw -ErrorAction SilentlyContinue) -like "$stem.bmp *")) { break }
      Start-Sleep -Milliseconds 5
    }
    $bmp = Join-Path $captureDir "$stem.bmp"
    if (Test-Path $bmp) {
      $png = Join-Path $dir ("{0:D3}-{1:D5}ms.png" -f $i, $tick)
      $img = [System.Drawing.Image]::FromFile($bmp)
      try { $img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $img.Dispose() }
      Remove-Item $bmp -Force -ErrorAction SilentlyContinue
      $frames += [pscustomobject]@{ path = $png; ms = $tick }
    }
    $i++
    $wait = $IntervalMs - ($sw.ElapsedMilliseconds - $tick)
    if ($wait -gt 0) { Start-Sleep -Milliseconds $wait }
  }
  $jobs | Wait-Job -Timeout 30 | Out-Null
  $jobs | Remove-Job -Force
  return $frames
}

function New-Sheet([string] $name, [object[]] $frames, [string] $title, [string[]] $rowLabels = @()) {
  $cols = 4; $rows = 4; $tw = 450; $th = 300; $pad = 6; $head = 34
  $pick = @()
  if ($frames.Count -le 16) { $pick = $frames } else {
    for ($k = 0; $k -lt 16; $k++) { $pick += $frames[[int][math]::Floor($k * ($frames.Count - 1) / 15)] }
  }
  $bmp = New-Object System.Drawing.Bitmap (($cols * ($tw + $pad)) + $pad), (($rows * ($th + $pad)) + $pad + $head)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.Clear([System.Drawing.Color]::FromArgb(32, 32, 32))
  $font = New-Object System.Drawing.Font "Consolas", 12
  $brush = [System.Drawing.Brushes]::Gold
  $g.DrawString($title, $font, $brush, $pad, 8)
  for ($k = 0; $k -lt $pick.Count; $k++) {
    $x = $pad + ($k % $cols) * ($tw + $pad); $y = $head + $pad + [math]::Floor($k / $cols) * ($th + $pad)
    $img = [System.Drawing.Image]::FromFile($pick[$k].path)
    try { $g.DrawImage($img, $x, $y, $tw, $th) } finally { $img.Dispose() }
    $label = "t=$($pick[$k].ms) ms"
    if ($pick[$k].PSObject.Properties.Name -contains "label") { $label = "$($pick[$k].label)  $label" }
    $g.FillRectangle((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(170, 0, 0, 0))), $x, $y + $th - 22, 240, 22)
    $g.DrawString($label, $font, $brush, $x + 4, $y + $th - 20)
  }
  $g.Dispose()
  $sheet = Join-Path $Out "$name.png"
  $bmp.Save($sheet, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Host "sheet $sheet ($($pick.Count) of $($frames.Count) frames)"
}

$tmp = Join-Path $env:TEMP "volum-ref-nav.png"

# 1. BUILD hero art idle: the seed opens on BUILD with THC Sunset (no art debug, idle hero).
Start-Sandbox ""
Invoke-Drive "10,10" "" 1500 (Join-Path $Out "build_idle-start.png")
$f = Get-Frames "build_idle" 6000
New-Sheet "build_idle" $f "desktop BUILD hero art idle (THC Sunset, seed) - 6 s, every frame stamped"

# 2. PLAY art at energy 0 / 0.5 / 1 (THC Sunset art 14, motion clock running).
$all = @()
foreach ($e in @("0", "0.5", "1")) {
  Start-Sandbox "14:$($e):run"
  Invoke-Drive "10,10;743,22" "" 2500 $tmp
  $fe = Get-Frames "play_energy_$e" 2600
  $fe | ForEach-Object { $_ | Add-Member -NotePropertyName label -NotePropertyValue "energy $e" -Force }
  $step = [math]::Max(1, [math]::Floor($fe.Count / 5))
  $sel = @(); for ($k = 0; $k -lt 5 -and ($k * $step) -lt $fe.Count; $k++) { $sel += $fe[$k * $step] }
  $all += $sel
}
New-Sheet "play_energy" $all "desktop PLAY art (THC Sunset) at energy 0 / 0.5 / 1, VOLUM_ART_ANIM_DEBUG=14:<e>:run"

# 3. Pedal toggle: PRE opens on Compressor in the seed; one Tab selects NAM 1 (Klon, on); Space toggles it twice.
Start-Sandbox ""
Invoke-Drive "10,10" "1;{TAB}" 800 (Join-Path $Out "pedal_toggle-start.png")
$f = Get-Frames "pedal_toggle" 6000 @(@{ at = 1500; keys = "{SPACE}" }, @{ at = 3500; keys = "{SPACE}" })
New-Sheet "pedal_toggle" $f "desktop PRE pedal toggle: Space on the selected NAM 1 card, sent at 1.5 s and 3.5 s (lands ~1.4 s later: ui-drive start-up)"

# 4. Section switch: PRE -> AMP at 1.5 s -> POST at 3.5 s.
Invoke-Drive "" "1" 800 $tmp
$f = Get-Frames "section_switch" 6000 @(@{ at = 1500; keys = "2" }, @{ at = 3500; keys = "3" })
New-Sheet "section_switch" $f "desktop section switch PRE -> AMP -> POST, keys sent at 1.5 s and 3.5 s (land ~1.4 s later: ui-drive start-up)"

Get-Process -Name VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Host "closed the desktop app"
