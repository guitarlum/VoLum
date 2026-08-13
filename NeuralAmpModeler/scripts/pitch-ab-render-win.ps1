# pitch-ab-render-win.ps1 - build and run the pitch A/B WAV renderer.
#
# LOCAL ONLY, and deliberately not wired into CI: the output is meant for your ears.
# The automated ceilings in tests/test_volum_pitch_artifacts.cpp only mean something
# because they were calibrated against renders like these. Whenever you touch the
# splice logic, listen before you retune a threshold - the POLY upshift crackle
# shipped twice under a fully green suite because the asserted metric did not match
# what a player hears.
#
# Usage:
#   pwsh -File NeuralAmpModeler/scripts/pitch-ab-render-win.ps1
#   pwsh -File NeuralAmpModeler/scripts/pitch-ab-render-win.ps1 -Baseline v1.2.1
#   pwsh -File NeuralAmpModeler/scripts/pitch-ab-render-win.ps1 -Baseline dev -InputWav take.wav
#
# -Baseline compiles VoLumPitchShifter.h from that git revision alongside the working
# tree and emits matched _A_baseline / _B_current pairs, so a change can be judged as
# a difference rather than in isolation.
#
# -InputWav takes a 16-bit mono 48 kHz WAV (a real DI is the most revealing source).

param(
  [string]$Baseline = "",
  [string]$InputWav = "",
  [string]$OutDir = "",
  [switch]$Rebuild
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$src = Join-Path $repo "NeuralAmpModeler\tools\pitch-ab\volum_pitch_ab.cpp"
$buildDir = Join-Path $repo "NeuralAmpModeler\build-win\pitch-ab"
$exe = Join-Path $buildDir "volum_pitch_ab.exe"
if ([string]::IsNullOrWhiteSpace($OutDir)) { $OutDir = Join-Path $buildDir "renders" }

if (-not (Test-Path $src)) { throw "Missing renderer source: $src" }
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Find-VsDevCmd {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $vswhere) {
    $root = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($root) {
      $cand = Join-Path $root "VC\Auxiliary\Build\vcvars64.bat"
      if (Test-Path $cand) { return $cand }
    }
  }
  foreach ($r in @("${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022", "${env:ProgramFiles}\Microsoft Visual Studio\2022")) {
    foreach ($ed in @("BuildTools", "Community", "Professional", "Enterprise")) {
      $cand = Join-Path $r "$ed\VC\Auxiliary\Build\vcvars64.bat"
      if (Test-Path $cand) { return $cand }
    }
  }
  throw "Could not find vcvars64.bat - install the VS 2022 C++ build tools."
}

# Compile the baseline revision's header into a parallel namespace so both engines can
# coexist in one binary. Rewriting the namespace is the whole trick: the file is
# otherwise byte-identical to what that revision shipped.
$extraDefines = ""
if (-not [string]::IsNullOrWhiteSpace($Baseline)) {
  $baselineHeader = Join-Path (Join-Path $repo "NeuralAmpModeler\tools\pitch-ab") "baseline_pitch.h"
  Push-Location $repo
  try {
    $text = & git show "${Baseline}:NeuralAmpModeler/VoLumPitchShifter.h" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { throw "git show ${Baseline}:NeuralAmpModeler/VoLumPitchShifter.h failed: $text" }
  }
  finally { Pop-Location }
  if ($text -notmatch 'namespace\s+effect') { throw "Baseline header has no 'namespace effect' - refusing to emit a bogus variant." }
  $text = $text -replace 'namespace\s+effect', 'namespace effect_baseline'
  Set-Content -Path $baselineHeader -Value $text -NoNewline
  $extraDefines = "/DVOLUM_PITCH_AB_BASELINE"
  Write-Host "Baseline '$Baseline' -> tools/pitch-ab/baseline_pitch.h (namespace effect_baseline)"
}

if ($Rebuild -and (Test-Path $exe)) { Remove-Item $exe -Force }

if (-not (Test-Path $exe) -or $Rebuild -or $extraDefines -ne "") {
  $vcvars = Find-VsDevCmd
  Write-Host "Building $exe"
  # Forward slash on the /Fo directory: a trailing backslash before the closing quote
  # would escape it and cmd would lose the rest of the command line.
  $objDir = $buildDir.Replace("\", "/")
  # The generated baseline header lives in tools/pitch-ab/, so its own relative
  # includes (../AudioDSPTools/...) no longer resolve from its location. Putting
  # NeuralAmpModeler/ on the include path makes them resolve as they do in the plugin.
  $incDir = (Join-Path $repo "NeuralAmpModeler")
  $cmd = "`"$vcvars`" >nul 2>&1 && cl /nologo /O2 /EHsc /std:c++17 $extraDefines /I `"$incDir`" /Fe:`"$exe`" /Fo:`"$objDir/`" `"$src`""
  & cmd /c $cmd
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$args = @($OutDir)
if (-not [string]::IsNullOrWhiteSpace($InputWav)) {
  if (-not (Test-Path $InputWav)) { throw "Input WAV not found: $InputWav" }
  $args += (Resolve-Path $InputWav).Path
}

& $exe @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Listen to the renders in: $OutDir"
