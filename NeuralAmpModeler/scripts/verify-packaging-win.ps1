# Verify Windows portable zip layout (VoLum_x64.exe, VoLum.vst3, VoLumRigs with .nam).
# Usage:
#   .\verify-packaging-win.ps1 -ZipPath "C:\path\to\VoLum-vX.Y.Z-win.zip"
# Or omit -ZipPath to resolve archive name from config.h via get_archive_name.py (repo root as cwd).

param(
  [string] $ZipPath = "",
  [string] $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

$ErrorActionPreference = "Stop"

if (-not $ZipPath) {
  $archiveName = python (Join-Path $RepoRoot "iPlug2\Scripts\get_archive_name.py") NeuralAmpModeler win full
  if (-not $archiveName) { throw "get_archive_name.py returned empty" }
  $ZipPath = Join-Path $RepoRoot "NeuralAmpModeler\build-win\out\$archiveName.zip"
}

Write-Host "Verifying portable zip: $ZipPath"
if (-not (Test-Path $ZipPath)) { throw "Zip not found: $ZipPath" }

$verifyDir = Join-Path $RepoRoot "NeuralAmpModeler\build-win\verify-packaging-ci"
if (Test-Path $verifyDir) { Remove-Item $verifyDir -Recurse -Force }
New-Item -ItemType Directory -Path $verifyDir | Out-Null
Expand-Archive -Path $ZipPath -DestinationPath $verifyDir -Force

$exePath = Join-Path $verifyDir "VoLum_x64.exe"
$vst3Path = Join-Path $verifyDir "VoLum.vst3"
$rigsPath = Join-Path $verifyDir "VoLumRigs"
$sampleRig = Join-Path $rigsPath "Ampete One\AMP-Ampt-1.nam"

if (-not (Test-Path $exePath)) { throw "Missing standalone exe: $exePath" }
if (-not (Test-Path $vst3Path)) { throw "Missing VST3 bundle: $vst3Path" }
if (-not (Test-Path $rigsPath)) { throw "Missing VoLumRigs folder: $rigsPath" }
if (-not (Test-Path $sampleRig)) { throw "Missing sample rig: $sampleRig" }

Write-Host "Windows portable package OK."
