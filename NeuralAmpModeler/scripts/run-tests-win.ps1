# Run after building NeuralAmpModeler-Tests (Release|x64 recommended).
# From repo: VoLum\NeuralAmpModeler\scripts

param(
  [string]$Filter
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = (Resolve-Path (Join-Path $here "..")).Path
Set-Location $slnDir

# A child .ps1 that never runs a native command leaves $LASTEXITCODE unset, and
# PowerShell evaluates `$null -ne 0` as TRUE. Guarding with `-ne 0` therefore exits
# `$null`, which is exit code 0: the suite reports success having built and run
# nothing. Truthiness is false for both $null and 0, so it only exits on a real
# failure.
function Invoke-Check([string]$path) {
  # Cleared first, because a child that succeeds without running a native command
  # never writes $LASTEXITCODE - check-test-source-parity.ps1 falls off the end on
  # success - so the value read below would be whatever the *caller's* shell left
  # there. Running the suite from a session whose last command failed then aborted
  # it with that code, before anything was built, blaming the first check.
  $global:LASTEXITCODE = 0
  & $path
  if ($LASTEXITCODE) { exit $LASTEXITCODE }
}

# Apply our local iPlug2 patches (idempotent). See NeuralAmpModeler/iplug2-patches/README.md.
# Deliberately not Invoke-Check: this script reports failure by Write-Error under
# ErrorActionPreference=Stop, which is a terminating error that propagates out here
# and stops the run with exit 1. It does all its work through
# System.Diagnostics.Process, so it never writes $LASTEXITCODE at all and there is
# nothing for Invoke-Check to read.
& (Join-Path $slnDir "iplug2-patches\apply-iplug2-patches.ps1")

# Fail early if a test source is registered in only one of the two build
# descriptors (Windows vcxproj vs. CMakeLists). Skipping this would silently
# let new tests miss either the Windows or macOS test run.
Invoke-Check (Join-Path $here "check-test-source-parity.ps1")

# Git on Windows stores new files non-executable, which only fails on the macOS
# runner. Catch it here rather than an hour into CI.
Invoke-Check (Join-Path $here "check-shell-exec-bits.ps1")

# Rules and skills go stale silently when a file is renamed, and wrong guidance
# is worse than verbose guidance.
Invoke-Check (Join-Path $here "check-agent-artifact-links.ps1")

# This repo is public and changelog.txt ships inside the installer, so a
# competitor name or a note about how its binary was studied reaches users.
Invoke-Check (Join-Path $here "check-no-vendor-refs.ps1")

$msbuild = $null
if ($env:GITHUB_ACTIONS -eq "true") {
  $msbuild = (Get-Command msbuild -ErrorAction SilentlyContinue | Select-Object -First 1).Source
}
if (-not $msbuild) {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found. Install Visual Studio Build Tools."
  }
  $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
}
if (-not $msbuild) {
  Write-Error "MSBuild.exe not found."
}

& $msbuild "NeuralAmpModeler.sln" /t:NeuralAmpModeler-Tests /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $slnDir "build-win\tests\x64\Release\NeuralAmpModeler-Tests.exe"
if (-not (Test-Path $exe)) {
  Write-Error "Test binary not found: $exe"
}

if ($Filter) {
  & $exe "--test-case=*$Filter*"
} else {
  & $exe
}
exit $LASTEXITCODE
