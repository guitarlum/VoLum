# Verify the Windows installer exists and optionally smoke-test a silent install.

param(
  [string] $SetupPath = "",
  [string] $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path,
  [switch] $RunSilentInstall
)

$ErrorActionPreference = "Stop"

if (-not $SetupPath) {
  $SetupPath = Join-Path $RepoRoot "NeuralAmpModeler\build-win\installer\VoLum-Setup.exe"
}

Write-Host "Verifying installer: $SetupPath"
if (-not (Test-Path $SetupPath)) {
  throw "Installer not found: $SetupPath"
}

$prePedalsSource = Join-Path $RepoRoot "rigs\PrePedals"
$prePedalFiles = @()
if (Test-Path $prePedalsSource) {
  $prePedalFiles = Get-ChildItem $prePedalsSource -Filter "*.nam" -File | Sort-Object Name
}

if (-not $RunSilentInstall) {
  Write-Host "Windows installer artifact OK."
  return
}

$tempRoot = $env:RUNNER_TEMP
if (-not $tempRoot) {
  $tempRoot = [System.IO.Path]::GetTempPath()
}
$installDir = Join-Path $tempRoot "VoLumInstallerSmoke"
if (Test-Path $installDir) { Remove-Item $installDir -Recurse -Force }

$logPath = Join-Path ([System.IO.Path]::GetTempPath()) "VoLumInstallerSmoke.log"
$args = @(
  "/VERYSILENT",
  "/SUPPRESSMSGBOXES",
  "/NORESTART",
  "/NOICONS",
  "/DIR=$installDir",
  "/LOG=$logPath"
)

Write-Host "Running silent install into $installDir"
$install = Start-Process -FilePath $SetupPath -ArgumentList $args -Wait -PassThru
if ($install.ExitCode -ne 0) {
  if (Test-Path $logPath) { Get-Content $logPath | Write-Host }
  throw "Installer failed with exit code $($install.ExitCode)"
}

$exePath = Join-Path $installDir "VoLum.exe"
$rigsPath = Join-Path $installDir "VoLumRigs"
$prePedalsPath = Join-Path $rigsPath "PrePedals"
$sampleRig = Join-Path $rigsPath "Ampete One\AMP-Ampt-1.nam"
$vst3Path = Join-Path ${env:ProgramFiles} "Common Files\VST3\VoLum.vst3"

if (-not (Test-Path $exePath)) { throw "Missing installed standalone: $exePath" }
if (-not (Test-Path $vst3Path)) { throw "Missing installed VST3: $vst3Path" }
if (-not (Test-Path $rigsPath)) { throw "Missing installed VoLumRigs: $rigsPath" }
if (-not (Test-Path $prePedalsPath)) { throw "Missing installed PrePedals folder: $prePedalsPath" }
if (-not (Test-Path $sampleRig)) { throw "Missing installed sample rig: $sampleRig" }

foreach ($file in $prePedalFiles) {
  $installed = Join-Path $prePedalsPath $file.Name
  if (-not (Test-Path $installed)) {
    throw "Missing installed PRE pedal capture: $installed"
  }
}

$regPath = "HKLM:\Software\VoLum\NeuralAmpModeler"
$rigsRoot = (Get-ItemProperty -Path $regPath -Name "VoLumRigsRoot").VoLumRigsRoot
if ($rigsRoot -ne $rigsPath) {
  throw "Unexpected VoLumRigsRoot registry value: '$rigsRoot' (expected '$rigsPath')"
}

$uninstaller = Join-Path $installDir "unins000.exe"
if (Test-Path $uninstaller) {
  Write-Host "Running silent uninstall"
  $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") -Wait -PassThru
  if ($uninstall.ExitCode -ne 0) {
    throw "Uninstaller failed with exit code $($uninstall.ExitCode)"
  }
}

Write-Host "Windows installer smoke test OK."
