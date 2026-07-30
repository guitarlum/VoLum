# VoLum REAPER render harness runner (Windows, headless).
#
# Loads VoLum as a track FX in REAPER on a test-tone track, reads the track's
# post-FX output via an audio accessor (volum-harness.lua), and asserts the
# plugin renders finite/bounded/non-silent audio and survives a project
# save/reload round-trip. Version-agnostic (params resolved by name).
#
# NOTE: REAPER instantiates whatever VoLum.vst3 it has scanned (typically
# %COMMONPROGRAMFILES%\VST3). For a true HEAD regression smoke, install the
# freshly built VST3 there first. This runner verifies harness mechanics and
# real-host audio sanity against the currently scanned build.
#
# Usage: pwsh NeuralAmpModeler/scripts/reaper/run-reaper-harness.ps1 [-Reaper C:\REAPER\reaper.exe]

param(
  [string]$Reaper = "C:\REAPER\reaper.exe",
  [int]$TimeoutSec = 120,
  # Point REAPER's VST3 scan path at the freshly built bundle for the duration of
  # the run. Without this the harness exercises whatever VoLum was last installed
  # into Program Files - which needs an elevated build to update, so on a normal
  # dev box it silently tests an older binary. Pass -InstalledVst3 to test what is
  # installed instead.
  [switch]$InstalledVst3
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$harnessLua = Join-Path $here "volum-harness.lua"
if (-not (Test-Path $Reaper)) { Write-Error "REAPER not found: $Reaper"; exit 2 }
if (-not (Test-Path $harnessLua)) { Write-Error "harness lua missing: $harnessLua"; exit 2 }

$work = Join-Path $env:TEMP "volum-reaper-harness"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Path $work | Out-Null

# REAPER records a project it failed to load in reaper.ini as `faultyproject=` and
# then blocks startup scripts behind a "load it anyway?" modal on the next launch.
# Because this harness force-kills REAPER while the round-trip project is open,
# every run poisons the next one: the second run times out with no harness.log and
# looks like a plugin hang. Strip that marker - and any reopen-on-launch reference
# to the work directory - before and after each run. Only harness paths are
# touched, so the user's own project list and settings survive.
function Clear-ReaperHarnessState([string]$iniPath, [string]$workDir) {
  if (-not (Test-Path $iniPath)) { return }
  $leaf = Split-Path -Leaf $workDir
  $kept = Get-Content $iniPath | Where-Object {
    -not ($_ -match '^faultyproject=') -and
    -not ($_ -match '^(lastproject|projecttab\d+)=' -and $_ -match [regex]::Escape($leaf))
  }
  $kept | Set-Content $iniPath -Encoding ASCII
}

# Scan only the freshly built bundle, so "VoLum" can resolve to exactly one binary
# and the run cannot quietly measure an older installed copy. reaper.ini and the
# plugin cache are both restored afterwards, leaving the user's REAPER as it was
# (and its own scan of every other plugin intact).
function Set-ReaperVstPath([string]$iniPath, [string]$path) {
  $updated = $false
  $lines = Get-Content $iniPath | ForEach-Object {
    if ($_ -match '^vstpath64=') { $updated = $true; "vstpath64=$path" } else { $_ }
  }
  if (-not $updated) { throw "no vstpath64= line in $iniPath" }
  $lines | Set-Content $iniPath -Encoding ASCII
}

# RMS of the generated tone, so the bypassed scenario can be checked against the
# signal we know went in rather than against itself.
function Get-WavRms([string]$path) {
  $bytes = [System.IO.File]::ReadAllBytes($path)
  $off = 44
  $n = [int](($bytes.Length - $off) / 2)
  $sumsq = 0.0
  for ($i = 0; $i -lt $n; $i++) {
    $v = [BitConverter]::ToInt16($bytes, $off + $i * 2) / 32768.0
    $sumsq += $v * $v
  }
  return [Math]::Sqrt($sumsq / $n)
}

# --- synthesize a 2 s decaying harmonic tone (48k mono 16-bit PCM) ---
function Write-TestWav([string]$path) {
  $sr = 48000; $secs = 2.0; $n = [int]($sr * $secs)
  $bytes = New-Object byte[] ($n * 2)
  $f0 = 110.0
  for ($i = 0; $i -lt $n; $i++) {
    $t = $i / $sr
    $env = [Math]::Exp(-3.0 * $t)
    $s = 0.6 * [Math]::Sin(2 * [Math]::PI * $f0 * $t) +
         0.3 * [Math]::Sin(2 * [Math]::PI * 2 * $f0 * $t) +
         0.15 * [Math]::Sin(2 * [Math]::PI * 3 * $f0 * $t)
    $v = [int]([Math]::Max(-1.0, [Math]::Min(1.0, $s * $env * 0.8)) * 32767)
    $u = [uint16]([int16]$v -band 0xFFFF)
    $bytes[$i * 2] = [byte]($u -band 0xFF)
    $bytes[$i * 2 + 1] = [byte](($u -shr 8) -band 0xFF)
  }
  $dataLen = $bytes.Length
  $ms = New-Object System.IO.MemoryStream
  $bw = New-Object System.IO.BinaryWriter($ms)
  $bw.Write([byte[]][char[]]"RIFF"); $bw.Write([uint32](36 + $dataLen)); $bw.Write([byte[]][char[]]"WAVE")
  $bw.Write([byte[]][char[]]"fmt "); $bw.Write([uint32]16); $bw.Write([uint16]1); $bw.Write([uint16]1)
  $bw.Write([uint32]$sr); $bw.Write([uint32]($sr * 2)); $bw.Write([uint16]2); $bw.Write([uint16]16)
  $bw.Write([byte[]][char[]]"data"); $bw.Write([uint32]$dataLen); $bw.Write($bytes)
  $bw.Flush()
  [System.IO.File]::WriteAllBytes($path, $ms.ToArray())
  $bw.Dispose(); $ms.Dispose()
}
Write-TestWav (Join-Path $work "input.wav")
$toneRms = Get-WavRms (Join-Path $work "input.wav")
Write-Output ("test tone written: {0} (rms {1:F6})" -f (Join-Path $work "input.wav"), $toneRms)

$reaperDir = Split-Path -Parent $Reaper
$reaperIni = Join-Path $reaperDir "reaper.ini"
Clear-ReaperHarnessState $reaperIni $work

$iniRestore = $null
$cacheRestore = $null
if (-not $InstalledVst3) {
  $bundleDir = (Resolve-Path (Join-Path $here "..\..\build-win")).Path
  if (-not (Test-Path (Join-Path $bundleDir "VoLum.vst3"))) {
    Write-Error "No built VST3 at $bundleDir\VoLum.vst3. Build it first (msbuild /t:NeuralAmpModeler-vst3), or pass -InstalledVst3."
    exit 2
  }
  $iniRestore = Join-Path $work "reaper.ini.bak"
  Copy-Item $reaperIni $iniRestore -Force
  $cache = Join-Path $reaperDir "reaper-vstplugins64.ini"
  if (Test-Path $cache) {
    $cacheRestore = Join-Path $work "reaper-vstplugins64.ini.bak"
    Copy-Item $cache $cacheRestore -Force
  }
  Set-ReaperVstPath $reaperIni $bundleDir
  Write-Output "scanning VST3 from build tree: $bundleDir"
}

# --- install startup hook (backup any existing one) ---
$scriptsDir = "C:\REAPER\Scripts"
if (-not (Test-Path $scriptsDir)) { New-Item -ItemType Directory $scriptsDir | Out-Null }
$startup = Join-Path $scriptsDir "__startup.lua"
$backup = Join-Path $scriptsDir "__startup.volumbak.lua"
if (Test-Path $startup) { Copy-Item $startup $backup -Force }
"dofile([[${harnessLua}]])" | Set-Content -Path $startup -Encoding ASCII

# --- arm sentinel + env, launch REAPER headless ---
# Kill any pre-existing REAPER first: a running instance is single-instance and
# would defer our launch (so __startup.lua never runs), and a crash-recovery
# modal from a prior force-kill would block startup scripts.
Get-Process -Name reaper -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 800
"go" | Set-Content -Path (Join-Path $work "go.txt") -Encoding ASCII
$env:VOLUM_HARNESS_DIR = $work
$results = Join-Path $work "results.json"
$proc = Start-Process -FilePath $Reaper -ArgumentList "-nosplash" -PassThru

$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline -and -not (Test-Path $results)) { Start-Sleep -Milliseconds 500 }
# results.json is written after the 3 core scenarios, then rewritten with the
# round-trip "reloaded" entry. Give the reopen a short grace to land.
if (Test-Path $results) {
  $grace = (Get-Date).AddSeconds(20)
  while ((Get-Date) -lt $grace) {
    try { if ((Get-Content $results -Raw) -match '"reloaded"') { break } } catch {}
    Start-Sleep -Milliseconds 500
  }
}

# --- tear down REAPER + restore startup ---
try { if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force } } catch {}
Get-Process -Name reaper -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
if (Test-Path $backup) { Move-Item $backup $startup -Force } else { Remove-Item $startup -Force -ErrorAction SilentlyContinue }
if ($iniRestore) { Copy-Item $iniRestore $reaperIni -Force }
if ($cacheRestore) { Copy-Item $cacheRestore (Join-Path $reaperDir "reaper-vstplugins64.ini") -Force }
Clear-ReaperHarnessState $reaperIni $work

if (Test-Path (Join-Path $work "harness.log")) {
  Write-Output "--- harness.log (tail) ---"
  Get-Content (Join-Path $work "harness.log") -Tail 30
}
if (-not (Test-Path $results)) { Write-Error "TIMEOUT: no results.json (REAPER did not finish the harness)"; exit 1 }

$r = Get-Content $results -Raw | ConvertFrom-Json
if (-not $r.ok) { Write-Error "harness reported failure: $($r.error)"; exit 1 }

$fail = 0
function Check([string]$name, [bool]$cond, [string]$msg) {
  if ($cond) { Write-Output "PASS  $name" } else { Write-Output "FAIL  $name -- $msg"; $script:fail++ }
}
$d = $r.scenarios.default
$byp = $r.scenarios.bypassed
$rel = $r.scenarios.reloaded
Check "loaded"        ($null -ne $d) "no default scenario"

# Tripwire first: everything below is only meaningful if these renders came out of
# the plugin. The bypassed render must be the input tone, and the default render
# must not be. The previous harness failed exactly here - silently - by reading a
# track audio accessor, which serves source audio rather than post-FX output.
if ($null -ne $byp) {
  $bypDelta = [Math]::Abs($byp.rms - $toneRms)
  Check "bypassed render is the input tone" ($bypDelta -le [Math]::Max(1e-4, $toneRms * 0.02)) `
    "bypassed rms $($byp.rms) != tone rms $toneRms - the harness is not rendering the item it generated"
  # Compared on peak as well as RMS: a guitar amp render lands near the input's
  # level by design, so RMS alone can legitimately sit within a few percent of the
  # dry tone. What can never happen is both figures matching - that is the
  # signature of measuring the input.
  $rmsMoved = [Math]::Abs($d.rms - $byp.rms) -gt ($byp.rms * 0.01)
  $peakMoved = [Math]::Abs($d.peak - $byp.peak) -gt ($byp.peak * 0.01)
  Check "harness measures VoLum, not its input" ($rmsMoved -or $peakMoved) `
    "default (peak $($d.peak), rms $($d.rms)) matches bypassed (peak $($byp.peak), rms $($byp.rms)) - the render is not going through the plugin"
}
else {
  Write-Output "FAIL  harness measures VoLum, not its input -- no bypassed scenario"
  $fail++
}

Check "no NaN/Inf"    ($d.bad -eq 0 -and $rel.bad -eq 0) "non-finite samples present"
Check "non-silent"    ($d.rms -gt 1e-5) "default output is silent (rms=$($d.rms))"
Check "bounded"       ($d.peak -lt 8.0) "default peak too large ($($d.peak))"

# Enabling an effect has to change the audio. A "finite and bounded" check on a
# render identical to the default one costs a REAPER launch and proves nothing.
$trem = $r.scenarios.tremolo_on
Check "tremolo finite" ($trem.bad -eq 0 -and $trem.peak -lt 8.0) "tremolo scenario bad/peak"
Check "tremolo changes the audio" ([Math]::Abs($trem.rms - $d.rms) -gt $d.rms * 0.01) `
  "tremolo rms $($trem.rms) matches default $($d.rms) - the parameter did not reach the audio path"
$pitch = $r.scenarios.pitch_on
Check "pitch finite"  ($pitch.bad -eq 0 -and $pitch.peak -lt 8.0) "pitch scenario bad/peak"
Check "pitch changes the audio" ([Math]::Abs($pitch.rms - $d.rms) -gt $d.rms * 0.01) `
  "pitch rms $($pitch.rms) matches default $($d.rms) - the parameter did not reach the audio path"

if ($null -ne $rel) {
  $tol = [Math]::Max(1e-4, $d.rms * 0.02)
  Check "roundtrip rms" ([Math]::Abs($d.rms - $rel.rms) -le $tol) "reloaded rms $($rel.rms) != default $($d.rms) (tol $tol)"
} else {
  Write-Output "SKIP  roundtrip rms -- reopen did not complete (soft); covered by pluginval state round-trip"
}

Write-Output ("fxname: " + $r.fxname)
if ($fail -gt 0) { Write-Error "$fail REAPER harness check(s) failed"; exit 1 }
Write-Output "REAPER harness: ALL CHECKS PASSED"
exit 0
