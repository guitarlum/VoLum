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

function Test-ZipHeader {
  param([string] $Path)

  if (-not (Test-Path $Path)) { return $false }
  $bytes = [System.IO.File]::ReadAllBytes($Path)
  return $bytes.Length -ge 4 -and $bytes[0] -eq 0x50 -and $bytes[1] -eq 0x4b
}

function Invoke-DownloadWithRetry {
  param(
    [string] $Uri,
    [string] $OutFile,
    [int] $Attempts = 4
  )

  for ($attempt = 1; $attempt -le $Attempts; ++$attempt) {
    if (Test-Path $OutFile) { Remove-Item $OutFile -Force }
    try {
      Write-Host "Downloading pluginval (attempt $attempt/$Attempts)..."
      Invoke-WebRequest $Uri -OutFile $OutFile -Headers @{ "Accept" = "application/octet-stream" } -UserAgent "VoLum-CI"
      if (Test-ZipHeader $OutFile) { return }
      throw "Downloaded file is not a zip archive. GitHub may have returned an HTML error page."
    }
    catch {
      if ($attempt -eq $Attempts) { throw }
      $delay = [Math]::Min(30, 2 * $attempt * $attempt)
      Write-Host "WARN: pluginval download failed: $($_.Exception.Message). Retrying in $delay seconds..."
      Start-Sleep -Seconds $delay
    }
  }
}

Invoke-DownloadWithRetry "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" $zipPath
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
