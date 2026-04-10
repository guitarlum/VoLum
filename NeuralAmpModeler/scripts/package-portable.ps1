# Package VoLum as a portable zip (no installer needed)
# Usage: .\scripts\package-portable.ps1

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$buildDir = "$repoRoot\NeuralAmpModeler\build-win"
$exe = "$buildDir\app\x64\Release\VoLum.exe"
$rigsDir = "$repoRoot\rigs"
$outDir = "$buildDir\portable"
$zipName = "VoLum-Portable.zip"

if (-not (Test-Path $exe)) {
    Write-Error "Release exe not found at $exe. Build Release x64 first."
    exit 1
}

if (-not (Test-Path $rigsDir)) {
    Write-Error "Rigs directory not found at $rigsDir."
    exit 1
}

# Clean and create staging directory
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory -Path "$outDir\VoLum" | Out-Null

# Copy exe (renamed to VoLum.exe)
Copy-Item $exe "$outDir\VoLum\VoLum.exe"

# Copy amp .nam trees as VoLumRigs (matches installer / CI portable layout)
New-Item -ItemType Directory -Path "$outDir\VoLum\VoLumRigs" | Out-Null
Get-ChildItem $rigsDir -Directory | ForEach-Object {
    $dest = Join-Path "$outDir\VoLum\VoLumRigs" $_.Name
    New-Item -ItemType Directory -Path $dest -Force | Out-Null
    Get-ChildItem $_.FullName -Filter *.nam -File | Copy-Item -Destination $dest
}

# Copy changelog
Copy-Item "$repoRoot\NeuralAmpModeler\installer\changelog.txt" "$outDir\VoLum\"

# Create zip
$zipPath = "$buildDir\$zipName"
if (Test-Path $zipPath) { Remove-Item $zipPath }
Compress-Archive -Path "$outDir\VoLum" -DestinationPath $zipPath

Write-Host "Portable package created: $zipPath"
Write-Host "Contents:"
Get-ChildItem "$outDir\VoLum" -Recurse | Select-Object FullName | Format-Table -AutoSize

# Cleanup staging
Remove-Item $outDir -Recurse -Force
