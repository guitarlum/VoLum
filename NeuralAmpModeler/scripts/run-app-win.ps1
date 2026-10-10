# Build VoLum standalone app (Release|x64) and start it for visual UI review.
# Stops an already-running VoLum first so postbuild can replace the exe.
# Must also stop VoLum_x64: postbuild copies VoLum.exe -> build-win\VoLum_x64.exe,
# and a running VoLum_x64 locks that file, silently leaving a stale standalone.
# From repo: VoLum\NeuralAmpModeler\scripts
#
# The app runs on a sandbox library (%TEMP%\volum-app-sandbox\VoLum) unless you pass
# -RealLibrary: an agent clicking through the app once rewrote the owner's real
# %LOCALAPPDATA%\VoLum library. The sandbox persists between runs; when it is
# created, only settings.ini (audio and MIDI devices) is copied from the real one.
#   pwsh NeuralAmpModeler/scripts/run-app-win.ps1                 # sandbox
#   pwsh NeuralAmpModeler/scripts/run-app-win.ps1 -ResetSandbox   # fresh sandbox
#   pwsh NeuralAmpModeler/scripts/run-app-win.ps1 -RealLibrary    # your library
param(
  [switch] $RealLibrary,
  [switch] $ResetSandbox
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = Resolve-Path (Join-Path $here "..")
Set-Location $slnDir

Get-Process -Name VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 200

# Apply our local iPlug2 patches (idempotent). See NeuralAmpModeler/iplug2-patches/README.md.
& (Join-Path $slnDir "iplug2-patches\apply-iplug2-patches.ps1")

$msbuild = $null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
  $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
}
if (-not $msbuild) {
  foreach ($cand in @(
      "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
      "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )) {
    if (Test-Path -LiteralPath $cand) { $msbuild = $cand; break }
  }
}
if (-not $msbuild) {
  Write-Error "MSBuild.exe not found. Install Visual Studio Build Tools."
}
& $msbuild "NeuralAmpModeler.sln" /t:NeuralAmpModeler-app /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) {
  Write-Host "Build failed (exit $LASTEXITCODE)." -ForegroundColor Yellow
  exit $LASTEXITCODE
}

$exe = Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe"
if ($RealLibrary) {
  Start-Process $exe
  Write-Host "Launched with the REAL library $(Join-Path $env:LOCALAPPDATA 'VoLum')" -ForegroundColor Yellow
  exit 0
}

$sandbox = Join-Path $env:TEMP "volum-app-sandbox"
$lib = Join-Path $sandbox "VoLum"
if ($ResetSandbox -and (Test-Path -LiteralPath $lib)) { Remove-Item -LiteralPath $lib -Recurse -Force }
if (-not (Test-Path -LiteralPath $lib)) {
  New-Item -ItemType Directory -Path $lib -Force | Out-Null
  $realIni = Join-Path $env:LOCALAPPDATA "VoLum\settings.ini"
  if (Test-Path -LiteralPath $realIni) { Copy-Item -LiteralPath $realIni -Destination $lib }
}
# Restore afterwards: a caller that runs this script in its own session keeps its env.
$savedLocalAppData = $env:LOCALAPPDATA
try {
  $env:LOCALAPPDATA = $sandbox
  Start-Process $exe
}
finally {
  $env:LOCALAPPDATA = $savedLocalAppData
}
Write-Host "Launched with the sandbox library $lib (-RealLibrary for your own, -ResetSandbox to start fresh)"
