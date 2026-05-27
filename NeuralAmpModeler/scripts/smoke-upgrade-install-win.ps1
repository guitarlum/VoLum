# Smoke-test upgrading from a prior published Windows release to the freshly built installer.
# Installs the prior release, pre-seeds volum-settings.json, upgrades in-place, and asserts
# version + settings preservation.
param(
  [string] $FromTag = $env:VOLUM_UPGRADE_SMOKE_FROM_TAG,
  [string] $NewSetupPath = "",
  [switch] $ValidateAssetsOnly
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

function Fail-UpgradeSmoke {
  param([string] $Message)

  if ($env:GITHUB_ACTIONS -eq "true") {
    throw $message
  }
  Write-Host "SKIP: $Message"
  exit 0
}

if (-not $FromTag) {
  $FromTag = gh release list --repo guitarlum/VoLum --limit 20 --json tagName,isDraft `
    --jq '.[] | select(.isDraft == false) | .tagName' 2>$null | Select-Object -First 1
}

if (-not $FromTag) {
  $FromTag = "v1.0.0"
}

gh release view $FromTag --repo guitarlum/VoLum *> $null
if ($LASTEXITCODE -ne 0) {
  Fail-UpgradeSmoke "Prior release tag not found: $FromTag"
}

if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

Write-Host "Downloading prior release $FromTag for upgrade smoke..."
Push-Location $workDir
gh release download $FromTag --repo guitarlum/VoLum --pattern "*windows-setup.exe" --pattern "*win-installer.exe"
Pop-Location

$priorSetup = Get-ChildItem $workDir -File -Recurse | Where-Object {
  $_.Name -like "*windows-setup.exe" -or $_.Name -like "*win-installer.exe"
} | Select-Object -First 1

if (-not $priorSetup) {
  Fail-UpgradeSmoke "Prior release $FromTag has no Windows setup executable asset (expected *windows-setup.exe)."
}

Write-Host "Using prior setup: $($priorSetup.Name)"

if ($ValidateAssetsOnly) {
  if (-not (Test-Path $NewSetupPath)) {
    throw "New installer not found: $NewSetupPath"
  }
  Write-Host "Upgrade asset validation OK ($FromTag -> $(Split-Path -Leaf $NewSetupPath))."
  exit 0
}

if (-not (Test-Path $NewSetupPath)) {
  throw "New installer not found: $NewSetupPath"
}

$tempRoot = $env:RUNNER_TEMP
if (-not $tempRoot) {
  $tempRoot = [System.IO.Path]::GetTempPath()
}
$installDir = Join-Path $tempRoot "VoLumUpgradeSmoke"
if (Test-Path $installDir) { Remove-Item $installDir -Recurse -Force }

$settingsBackup = $null
$hadSettings = Test-Path $settingsPath
if ($hadSettings) {
  $settingsBackup = Get-Content $settingsPath -Raw
}

function Install-VoLumSetup {
  param(
    [string] $SetupExe,
    [string] $TargetDir,
    [string] $Label
  )

  $logPath = Join-Path $tempRoot "VoLumUpgradeSmoke-$Label-$([Guid]::NewGuid().ToString('N')).log"
  $args = @(
    "/VERYSILENT",
    "/SUPPRESSMSGBOXES",
    "/NORESTART",
    "/NOICONS",
    "/DIR=$TargetDir",
    "/LOG=$logPath"
  )
  Write-Host "Installing $Label into $TargetDir"
  $proc = Start-Process -FilePath $SetupExe -ArgumentList $args -Wait -PassThru
  if ($proc.ExitCode -ne 0) {
    if (Test-Path $logPath) {
      Write-Host "--- $Label installer log ($logPath) ---"
      Get-Content $logPath | Write-Host
    }
    throw "$Label installer failed with exit code $($proc.ExitCode): $SetupExe"
  }
}

try {
  Install-VoLumSetup -SetupExe $priorSetup.FullName -TargetDir $installDir -Label "prior-$FromTag"

  $priorExe = Join-Path $installDir "VoLum.exe"
  if (-not (Test-Path $priorExe)) {
    throw "Prior release install did not create VoLum.exe at $priorExe"
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

  Install-VoLumSetup -SetupExe $NewSetupPath -TargetDir $installDir -Label "upgrade"

  $newExe = Join-Path $installDir "VoLum.exe"
  if (-not (Test-Path $newExe)) {
    throw "Upgrade install did not create VoLum.exe at $newExe"
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
}
finally {
  $uninstaller = Join-Path $installDir "unins000.exe"
  if (Test-Path $uninstaller) {
    Start-Process -FilePath $uninstaller -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") -Wait -ErrorAction SilentlyContinue | Out-Null
  }
  if (Test-Path $installDir) {
    Remove-Item $installDir -Recurse -Force -ErrorAction SilentlyContinue
  }
  if ($hadSettings -and $null -ne $settingsBackup) {
    Set-Content -Path $settingsPath -Value $settingsBackup -Encoding UTF8 -NoNewline
  }
  elseif (-not $hadSettings -and (Test-Path $settingsPath)) {
    Remove-Item $settingsPath -Force -ErrorAction SilentlyContinue
  }
}
