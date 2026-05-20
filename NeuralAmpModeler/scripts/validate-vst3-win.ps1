# Validate the built Windows VST3 bundle with pluginval.

param(
  [string] $Vst3Path = "",
  [string] $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

$ErrorActionPreference = "Stop"

if (-not $Vst3Path) {
  $Vst3Path = Join-Path $RepoRoot "NeuralAmpModeler\build-win\VoLum.vst3"
}

if (-not (Test-Path $Vst3Path)) {
  throw "VST3 bundle not found: $Vst3Path"
}

$toolDir = Join-Path $RepoRoot "NeuralAmpModeler\build-win\pluginval"
$zipPath = Join-Path $toolDir "pluginval.zip"
$outputDir = Join-Path $RepoRoot "NeuralAmpModeler\build-win\pluginval-output"

if (Test-Path $toolDir) { Remove-Item $toolDir -Recurse -Force }
New-Item -ItemType Directory -Path $toolDir | Out-Null
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile $zipPath
Expand-Archive $zipPath -DestinationPath $toolDir -Force

$pluginval = Get-ChildItem $toolDir -Recurse -Filter "pluginval.exe" | Select-Object -First 1
if (-not $pluginval) {
  throw "pluginval.exe not found after extracting $zipPath"
}

Write-Host "Validating VST3 with pluginval: $Vst3Path"
& $pluginval.FullName --validate-in-process --strictness-level 10 --output-dir $outputDir $Vst3Path
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$sdkRoot = Join-Path $RepoRoot "iPlug2\Dependencies\IPlug\VST3_SDK"
$steinbergValidator = $null
if (Test-Path $sdkRoot) {
  $steinbergValidator = Get-ChildItem $sdkRoot -Recurse -File |
    Where-Object { $_.Name -eq "validator" -or $_.Name -eq "validator.exe" } |
    Select-Object -First 1
}

if ($steinbergValidator) {
  Write-Host "Validating VST3 with Steinberg validator: $Vst3Path"
  & $steinbergValidator.FullName $Vst3Path
  exit $LASTEXITCODE
}

Write-Host "Steinberg validator not found under $sdkRoot; pluginval validation completed."
