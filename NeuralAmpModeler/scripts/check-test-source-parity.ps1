# Fail if any tests/*.cpp is listed in only one of the two build descriptors:
#   - NeuralAmpModeler/tests/CMakeLists.txt   (CMake / macOS test build)
#   - NeuralAmpModeler/projects/NeuralAmpModeler-Tests.vcxproj (Windows test build)
#
# This catches the "added a test but only wired it into one build" drift
# class. Run it from run-tests-win.ps1 before invoking MSBuild so a missing
# Windows entry surfaces in seconds instead of "next code review".

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = (Resolve-Path (Join-Path $here "..")).Path

$testsDir = Join-Path $projectDir "tests"
$cmakeListPath = Join-Path $testsDir "CMakeLists.txt"
$vcxprojPath = Join-Path $projectDir "projects\NeuralAmpModeler-Tests.vcxproj"

if (-not (Test-Path $cmakeListPath)) { Write-Error "Missing $cmakeListPath" }
if (-not (Test-Path $vcxprojPath))   { Write-Error "Missing $vcxprojPath" }

# Source of truth: every *.cpp directly under tests/, minus the third_party
# folder. main.cpp is the doctest entry point and is also expected in both.
$onDisk = Get-ChildItem -LiteralPath $testsDir -Filter *.cpp -File |
  Where-Object { $_.FullName -notmatch '\\third_party\\' } |
  ForEach-Object { $_.Name } |
  Sort-Object

$cmakeText = Get-Content -LiteralPath $cmakeListPath -Raw
$vcxText   = Get-Content -LiteralPath $vcxprojPath   -Raw

$missing = New-Object System.Collections.Generic.List[string]
foreach ($name in $onDisk) {
  # CMake side: bare filename appears on its own line in the source list.
  $inCmake = $cmakeText -match ("(?m)^\s*" + [Regex]::Escape($name) + "\s*$")
  # Vcxproj side: <ClCompile Include="..\tests\<name>" />
  $inVcx   = $vcxText -match ('ClCompile Include="\.\.\\tests\\' + [Regex]::Escape($name) + '"')

  if ($inCmake -and -not $inVcx) {
    $missing.Add("$name : present in CMakeLists.txt, missing from NeuralAmpModeler-Tests.vcxproj")
  }
  elseif ($inVcx -and -not $inCmake) {
    $missing.Add("$name : present in NeuralAmpModeler-Tests.vcxproj, missing from CMakeLists.txt")
  }
  elseif (-not $inCmake -and -not $inVcx) {
    $missing.Add("$name : present on disk, missing from BOTH build descriptors")
  }
}

if ($missing.Count -gt 0) {
  Write-Host "Test source parity check FAILED:" -ForegroundColor Red
  foreach ($line in $missing) { Write-Host "  $line" -ForegroundColor Red }
  Write-Host ""
  Write-Host "Fix: add the missing entry to the listed build descriptor so" -ForegroundColor Yellow
  Write-Host "both Windows (vcxproj) and macOS/CI (CMake) compile every test." -ForegroundColor Yellow
  exit 1
}

Write-Host "Test source parity OK ($($onDisk.Count) tests / both descriptors agree)."
