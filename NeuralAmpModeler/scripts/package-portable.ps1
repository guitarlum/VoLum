# Package VoLum as a portable zip (no installer needed)
# Usage: .\scripts\package-portable.ps1

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$buildDir = "$repoRoot\NeuralAmpModeler\build-win"
$exe = "$buildDir\app\x64\Release\NeuralAmpModeler.exe"
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

# Copy all rigs
Copy-Item $rigsDir "$outDir\VoLum\rigs" -Recurse

# Remove non-.nam files from rigs (e.g. ampPictures, settings)
Get-ChildItem "$outDir\VoLum\rigs" -Recurse -File | Where-Object { $_.Extension -ne ".nam" } | Remove-Item -Force
Get-ChildItem "$outDir\VoLum\rigs" -Directory | Where-Object { $_.Name -eq "ampPictures" } | Remove-Item -Recurse -Force

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
