# Regenerate the golden sound references in NeuralAmpModeler/tests/golden/.
# This is the only way they are written: a plain test run compares and never
# writes. Only groups whose fingerprints moved are rewritten, and each rewritten
# file records -Reason. Add a changelog.txt line containing that exact reason
# afterwards; until then run-tests-win.ps1 (check-golden-changelog.ps1) and the
# changelog doctest fail.
#
# Usage: pwsh NeuralAmpModeler/scripts/regen-golden-renders.ps1 -Reason "chorus redesign"

param(
  [Parameter(Mandatory = $true)]
  [string]$Reason
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = (Resolve-Path (Join-Path $here "..")).Path
$changelogPath = Join-Path $projectDir "installer\changelog.txt"
$minReasonLength = 12

$Reason = $Reason.Trim()
if ($Reason.Length -lt $minReasonLength) {
  Write-Host "-Reason must name the sound change (at least $minReasonLength characters)." -ForegroundColor Red
  exit 1
}

# A reason that is already in the changelog would let a new sound hide behind an
# old line. Regenerate first, then write the changelog line.
$changelog = [System.IO.File]::ReadAllText($changelogPath, (New-Object System.Text.UTF8Encoding($false)))
if ($changelog.Contains($Reason)) {
  Write-Host "changelog.txt already contains ""$Reason"". Name this sound change with a reason that is not in the changelog yet, regenerate, then add the line." -ForegroundColor Red
  exit 1
}

$env:VOLUM_GOLDEN_REGEN = "1"
$env:VOLUM_GOLDEN_REASON = $Reason
try {
  $global:LASTEXITCODE = 0
  & (Join-Path $here "run-tests-win.ps1") -Filter "Golden renders:"
  $rc = $LASTEXITCODE
} finally {
  Remove-Item Env:VOLUM_GOLDEN_REGEN -ErrorAction SilentlyContinue
  Remove-Item Env:VOLUM_GOLDEN_REASON -ErrorAction SilentlyContinue
}
if ($rc) { exit $rc }

Write-Host ""
Write-Host "Regenerated for: $Reason"
& git -C $projectDir status --short -- tests/golden
Write-Host "Next: add a changelog.txt line containing ""$Reason"" and run run-tests-win.ps1." -ForegroundColor Yellow
exit 0
