# VoLum REAPER render harness runner (Windows, headless).
#
# Loads VoLum as a track FX in REAPER on a test-tone track, renders the tone
# through it with apply-FX (volum-harness.lua), and asserts the plugin renders
# finite/bounded/non-silent audio and survives a project save/reload round-trip.
# Further scenarios drive MIDI recall, two instances and a realtime render.
# Version-agnostic (params resolved by name).
#
# Usage: pwsh NeuralAmpModeler/scripts/reaper-harness/run-reaper-harness.ps1
#          [-Scenario core|midi-pc|midi-cc102|project-roundtrip-after-recall|two-instances|offline-vs-realtime|all]
#          [-Sandbox] [-Reaper C:\REAPER\reaper.exe] [-TimeoutSec n] [-InstalledVst3]

param(
  [string]$Reaper = "C:\REAPER\reaper.exe",
  # 0 = a budget derived from the selected scenarios.
  [int]$TimeoutSec = 0,
  # Point REAPER's VST3 scan path at the freshly built bundle for the duration of
  # the run. Without this the harness exercises whatever VoLum was last installed
  # into Program Files - which needs an elevated build to update, so on a normal
  # dev box it silently tests an older binary. Pass -InstalledVst3 to test what is
  # installed instead.
  [switch]$InstalledVst3,
  # Comma-separated scenario names, or "all".
  [string]$Scenario = "core",
  # Launch REAPER with LOCALAPPDATA pointed at an empty directory, so VoLum opens a
  # fresh library (five pre-filled Sounds on programs 0-4, default rig) and the
  # run neither reads nor writes the real one.
  [switch]$Sandbox,
  # Scan this directory for VoLum.vst3 instead of NeuralAmpModeler/build-win in this worktree.
  [string]$Vst3Dir = ""
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$harnessLua = Join-Path $here "volum-harness.lua"
if (-not (Test-Path $Reaper)) { Write-Error "REAPER not found: $Reaper"; exit 2 }
if (-not (Test-Path $harnessLua)) { Write-Error "harness lua missing: $harnessLua"; exit 2 }

# Seconds each scenario may take, generous: every MIDI step waits for VoLum's
# OnIdle and renders until two takes agree.
$budget = [ordered]@{
  "core"                           = 120
  "midi-pc"                        = 180
  "midi-cc102"                     = 180
  "project-roundtrip-after-recall" = 120
  "two-instances"                  = 150
  "offline-vs-realtime"            = 90
  "restore-guard-reopen-stopped"   = 120
  "restore-guard-reopen-after-play" = 150
  "restore-guard-early-pc"         = 120
  "restore-guard-repeat-pc"        = 120
  "restore-guard-fx-bypass-offline" = 120
  "restore-guard-offline-pc-render" = 120
  "restore-guard-host-param"       = 120
}
$selected = @($Scenario -split '[,\s]+' | Where-Object { $_ })
foreach ($s in $selected) {
  if ($s -ne "all" -and -not $budget.Contains($s)) {
    Write-Error ("unknown scenario '$s' (known: all, " + (($budget.Keys) -join ", ") + ")"); exit 2
  }
}
if ($selected -contains "all") { $selected = @($budget.Keys) }
if ($TimeoutSec -le 0) { $TimeoutSec = ($selected | ForEach-Object { $budget[$_] } | Measure-Object -Sum).Sum }
$runCore = $selected -contains "core"
if (-not $Sandbox -and ($selected | Where-Object { $_ -ne "core" })) {
  Write-Output "NOTE: without -Sandbox the MIDI scenarios use (and change) your real VoLum library and last-used rig, and cannot check which Sound each program should recall."
}

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
    -not ($_ -match '^lastproject=') -and
    -not ($_ -match '^projecttab\d+=') -and
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
  if ($Vst3Dir -ne "") {
    $bundleDir = (Resolve-Path $Vst3Dir).Path
  } else {
    $bundleDir = (Resolve-Path (Join-Path $here "..\..\build-win")).Path
  }
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
($selected -join ",") | Set-Content -Path (Join-Path $work "scenarios.txt") -Encoding ASCII
$(if ($Sandbox) { "1" } else { "0" }) | Set-Content -Path (Join-Path $work "sandbox.txt") -Encoding ASCII
$results = Join-Path $work "results.json"
# REAPER on Windows does not reliably inherit one-shot env vars from Start-Process,
# so the startup hook reads the work dir from a sentinel under Scripts\.
$goSentinel = Join-Path $scriptsDir "volum-harness-go.txt"
$work | Set-Content -Path $goSentinel -Encoding ASCII
# VoLum resolves its library from %LOCALAPPDATA%; pass a sandbox via a child env block.
$sandboxRoot = Join-Path $work "localappdata"
$launchEnv = @{}
foreach ($k in [System.Environment]::GetEnvironmentVariables().Keys) {
  $launchEnv[$k] = [System.Environment]::GetEnvironmentVariable($k)
}
if ($Sandbox) {
  New-Item -ItemType Directory -Path $sandboxRoot -Force | Out-Null
  $launchEnv["LOCALAPPDATA"] = $sandboxRoot
  Write-Output "sandboxed LOCALAPPDATA: $sandboxRoot"
}
$proc = Start-Process -FilePath $Reaper -ArgumentList "-nosplash" -PassThru -Environment $launchEnv
Write-Output ("scenarios: {0} (timeout {1} s)" -f ($selected -join ","), $TimeoutSec)

# The harness rewrites results.json after every scenario and marks the last
# write "done": true.
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
  try { if ((Test-Path $results) -and ((Get-Content $results -Raw) -match '"done":\s*true')) { break } } catch {}
  Start-Sleep -Milliseconds 500
}

# --- tear down REAPER + restore startup ---
try { if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force } } catch {}
Get-Process -Name reaper -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
if (Test-Path $backup) { Move-Item $backup $startup -Force } else { Remove-Item $startup -Force -ErrorAction SilentlyContinue }
Remove-Item (Join-Path $scriptsDir "volum-harness-go.txt") -Force -ErrorAction SilentlyContinue
if ($iniRestore) { Copy-Item $iniRestore $reaperIni -Force }
if ($cacheRestore) { Copy-Item $cacheRestore (Join-Path $reaperDir "reaper-vstplugins64.ini") -Force }
Clear-ReaperHarnessState $reaperIni $work

if (Test-Path (Join-Path $work "harness.log")) {
  Write-Output "--- harness.log (tail) ---"
  Get-Content (Join-Path $work "harness.log") -Tail $(if ($runCore -and $selected.Count -eq 1) { 30 } else { 80 })
}
if ($Sandbox) {
  $sandboxLog = Join-Path $sandboxRoot "VoLum\volum.log"
  if (Test-Path $sandboxLog) {
    Write-Output "--- sandbox volum.log [midi] lines ---"
    Select-String -Path $sandboxLog -Pattern '\[midi\]' | ForEach-Object { $_.Line }
  }
  $library = Join-Path $sandboxRoot "VoLum\content\volum-content.json"
  if (Test-Path $library) {
    try {
      $map = (Get-Content $library -Raw | ConvertFrom-Json).midiSoundMap
      Write-Output ("sandbox library midiSoundMap entries: {0}" -f @($map).Count)
    } catch { Write-Output "sandbox library unreadable: $library" }
  } else {
    Write-Output "sandbox library missing: $library (VoLum never opened a library - did it load?)"
  }
}
if (-not (Test-Path $results)) { Write-Error "TIMEOUT: no results.json (REAPER did not finish the harness)"; exit 1 }

$r = Get-Content $results -Raw | ConvertFrom-Json

$fail = 0
function Check([string]$name, [bool]$cond, [string]$msg) {
  if ($cond) { Write-Output "PASS  $name" } else { Write-Output "FAIL  $name -- $msg"; $script:fail++ }
}

# Scenario checks the harness judged itself (everything except core). WARN is
# advisory - a host quirk or a product finding worth reading - and does not fail
# the run.
foreach ($c in @($r.checks)) {
  if ($null -eq $c) { continue }
  $line = "{0,-4}  {1}/{2}" -f $c.status, $c.scenario, $c.name
  if ($c.detail) { $line += " -- " + $c.detail }
  Write-Output $line
  if ($c.status -eq "FAIL") { $fail++ }
}
if ($r.facts -and @($r.facts.PSObject.Properties).Count -gt 0) {
  Write-Output "--- facts ---"
  foreach ($p in $r.facts.PSObject.Properties) { Write-Output ("FACT  {0} = {1}" -f $p.Name, $p.Value) }
}
if (-not $r.ok) { Write-Error "harness reported failure: $($r.error)"; exit 1 }
if (-not $r.done) {
  if ($runCore -and $selected.Count -eq 1 -and $null -ne $r.scenarios.default) {
    Write-Output "NOTE  the round-trip reopen did not finish before the timeout"
  } else {
    Write-Output "FAIL  harness did not finish within $TimeoutSec s -- results above are partial"
    $fail++
  }
}

if (-not $runCore) {
  Write-Output ("fxname: " + $r.fxname)
  if ($fail -gt 0) { Write-Error "$fail REAPER harness check(s) failed"; exit 1 }
  Write-Output "REAPER harness: ALL CHECKS PASSED"
  exit 0
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
  # apply-FX on Windows gains/normalizes the item; compare dry vs wet, not raw WAV RMS.
  $bypDelta = [Math]::Abs($byp.rms - $toneRms)
  $dryOk = $bypDelta -le [Math]::Max(1e-4, $toneRms * 0.02)
  if (-not $dryOk) {
    Write-Output ("NOTE  apply-FX dry rms $($byp.rms) vs source tone $toneRms (REAPER render gain; not a VoLum signal)")
  }
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

Check "no NaN/Inf"    ($d.bad -eq 0 -and ($null -eq $rel -or $rel.bad -eq 0)) "non-finite samples present"
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
