# Headless screenshot of a PLAY prototype HTML (Edge).
# Usage: powershell -NoProfile -File shot.ps1 -Html "...\index.html" -Variant A -Freeze 0.85 -Out "...\shots\x.png"
param(
  [Parameter(Mandatory = $true)][string] $Html,
  [Parameter(Mandatory = $true)][string] $Out,
  [string] $Variant = "A",
  [string] $Freeze = "0.75",
  [string] $ExtraQuery = ""
)
$ErrorActionPreference = "Stop"
$browser = @(
  "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
  "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
  "$env:ProgramFiles\Google\Chrome\Application\chrome.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $browser) { Write-Error "Edge/Chrome not found" }

$htmlFull = (Resolve-Path $Html).Path
$outDir = Split-Path -Parent $Out
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force -Path $outDir | Out-Null }
$outFull = [System.IO.Path]::GetFullPath($Out)
$uri = ([Uri]$htmlFull).AbsoluteUri
$q = @()
if ($Variant) { $q += "variant=$Variant" }
if ($Freeze -ne "") { $q += "freeze=$Freeze" }
if ($ExtraQuery) { $q += $ExtraQuery.TrimStart("&") }
$uri = "$uri`?" + ($q -join "&")

$userData = Join-Path $env:TEMP ("volum-play-proto-edge-" + [guid]::NewGuid().ToString("n"))
New-Item -ItemType Directory -Force -Path $userData | Out-Null
$shotName = [System.IO.Path]::GetFileName($outFull)
$workDir = Split-Path -Parent $outFull

Push-Location $workDir
try {
  & $browser --headless=new --disable-gpu --allow-file-access-from-files --hide-scrollbars `
    --user-data-dir=$userData --window-size=1100,820 --screenshot=$shotName $uri
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Pop-Location
  Start-Sleep -Milliseconds 400
  Remove-Item -Recurse -Force $userData -ErrorAction SilentlyContinue
}

if (-not (Test-Path $outFull)) { Write-Error "Screenshot was not written: $outFull" }
Write-Output $outFull
