# Run after building NeuralAmpModeler-Tests (Release|x64 recommended).
# From repo: VoLum\NeuralAmpModeler\scripts

param(
  [string]$Filter
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = (Resolve-Path (Join-Path $here "..")).Path
Set-Location $slnDir

# Apply our local iPlug2 patches (idempotent). See NeuralAmpModeler/iplug2-patches/README.md.
& (Join-Path $slnDir "iplug2-patches\apply-iplug2-patches.ps1")

# Fail early if a test source is registered in only one of the two build
# descriptors (Windows vcxproj vs. CMakeLists). Skipping this would silently
# let new tests miss either the Windows or macOS test run.
& (Join-Path $here "check-test-source-parity.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Git on Windows stores new files non-executable, which only fails on the macOS
# runner. Catch it here rather than an hour into CI.
& (Join-Path $here "check-shell-exec-bits.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

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
