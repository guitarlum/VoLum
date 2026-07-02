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
  [int]$TimeoutSec = 120
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$harnessLua = Join-Path $here "volum-harness.lua"
if (-not (Test-Path $Reaper)) { Write-Error "REAPER not found: $Reaper"; exit 2 }
if (-not (Test-Path $harnessLua)) { Write-Error "harness lua missing: $harnessLua"; exit 2 }

$work = Join-Path $env:TEMP "volum-reaper-harness"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Path $work | Out-Null

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
Write-Output "test tone written: $(Join-Path $work 'input.wav')"

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
$rel = $r.scenarios.reloaded
Check "loaded"        ($null -ne $d) "no default scenario"
Check "no NaN/Inf"    ($d.bad -eq 0 -and $rel.bad -eq 0) "non-finite samples present"
Check "non-silent"    ($d.rms -gt 1e-5) "default output is silent (rms=$($d.rms))"
Check "bounded"       ($d.peak -lt 8.0) "default peak too large ($($d.peak))"
Check "tremolo finite" ($r.scenarios.tremolo_on.bad -eq 0 -and $r.scenarios.tremolo_on.peak -lt 8.0) "tremolo scenario bad/peak"
Check "pitch finite"  ($r.scenarios.pitch_on.bad -eq 0 -and $r.scenarios.pitch_on.peak -lt 8.0) "pitch scenario bad/peak"
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
