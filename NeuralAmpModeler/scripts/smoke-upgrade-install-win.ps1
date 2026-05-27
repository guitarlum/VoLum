# Smoke-test upgrading from a prior published Windows release to the freshly built installer.
param(
  [string] $FromTag = $env:VOLUM_UPGRADE_SMOKE_FROM_TAG,
  [string] $NewSetupPath = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $NewSetupPath) {
  $NewSetupPath = Join-Path $repoRoot "NeuralAmpModeler\build-win\installer\VoLum-Setup.exe"
}

$workDir = Join-Path $repoRoot "NeuralAmpModeler\build-win\upgrade-smoke"
$settingsDir = Join-Path $env:LOCALAPPDATA "VoLum"
$settingsPath = Join-Path $settingsDir "volum-settings.json"
$sentinelValue = "0.777777"
$sentinelAmp = "Ampete One"

if (-not $FromTag) {
  $FromTag = gh release list --repo guitarlum/VoLum --limit 20 --json tagName,isDraft `
    --jq '.[] | select(.isDraft == false) | .tagName' 2>$null | Select-Object -First 1
}

if (-not $FromTag) {
  $FromTag = "v1.0.0"
}

gh release view $FromTag --repo guitarlum/VoLum *> $null
if ($LASTEXITCODE -ne 0) {
  Write-Host "SKIP: prior release tag not found: $FromTag"
  exit 0
}

if (-not (Test-Path $NewSetupPath)) {
  throw "New installer not found: $NewSetupPath"
}

if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

Write-Host "Downloading prior release $FromTag for upgrade smoke..."
Push-Location $workDir
gh release download $FromTag --repo guitarlum/VoLum --pattern "*win-installer.exe"
Pop-Location

$priorSetup = Get-ChildItem $workDir -Filter "*win-installer.exe" -File | Select-Object -First 1
if (-not $priorSetup) {
  Write-Host "SKIP: prior release $FromTag has no win-installer.exe asset."
  exit 0
}

$priorInstallDir = Join-Path $workDir "prior-install"
$newInstallDir = Join-Path $workDir "new-install"
foreach ($dir in @($priorInstallDir, $newInstallDir)) {
  if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
}

function Install-VoLumSetup {
  param(
    [string] $SetupExe,
    [string] $TargetDir
  )

  $logPath = Join-Path $env:TEMP "VoLumUpgradeSmoke-$([Guid]::NewGuid().ToString('N')).log"
  $args = @(
    "/VERYSILENT",
    "/SUPPRESSMSGBOXES",
    "/NORESTART",
    "/NOICONS",
    "/DIR=$TargetDir",
    "/LOG=$logPath"
  )
  $proc = Start-Process -FilePath $SetupExe -ArgumentList $args -Wait -PassThru
  if ($proc.ExitCode -ne 0) {
    if (Test-Path $logPath) { Get-Content $logPath | Write-Host }
    throw "Installer failed with exit code $($proc.ExitCode): $SetupExe"
  }
}

Install-VoLumSetup -SetupExe $priorSetup.FullName -TargetDir $priorInstallDir
$priorExe = Join-Path $priorInstallDir "VoLum.exe"
if (-not (Test-Path $priorExe)) {
  throw "Prior release install did not create VoLum.exe"
}

New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
@{
  version = 6
  lastAmpIdx = 0
  amps = @{
    $sentinelAmp = @{
      postDelayMix = [double]$sentinelValue
    }
  }
} | ConvertTo-Json -Depth 5 | Set-Content -Path $settingsPath -Encoding UTF8

Install-VoLumSetup -SetupExe $NewSetupPath -TargetDir $newInstallDir
$newExe = Join-Path $newInstallDir "VoLum.exe"
if (-not (Test-Path $newExe)) {
  throw "Upgrade install did not create VoLum.exe"
}

$expectedVersion = (Select-String -Path (Join-Path $repoRoot "NeuralAmpModeler\config.h") `
  -Pattern '#define PLUG_VERSION_STR' | ForEach-Object {
    if ($_.Line -match '"([^"]+)"') { $Matches[1] }
  })
$actualVersion = (Get-Item $newExe).VersionInfo.ProductVersion
if ($actualVersion -ne $expectedVersion) {
  throw "Upgraded exe version is '$actualVersion', expected '$expectedVersion'."
}

if (-not (Test-Path $settingsPath)) {
  throw "volum-settings.json missing after upgrade."
}
$settingsText = Get-Content $settingsPath -Raw
if ($settingsText -notmatch [regex]::Escape($sentinelValue)) {
  throw "Upgrade did not preserve seeded volum-settings.json sentinel."
}

Write-Host "Windows upgrade smoke OK ($FromTag -> $expectedVersion)."
