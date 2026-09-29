# Regenerate the golden sound references in NeuralAmpModeler/tests/golden/.
# This is the only way they are written: a plain test run compares and never
# writes. A group file is rewritten only when its fingerprints moved AND it is
# listed in -Groups; a group that moved but is not listed FAILS, so an
# unintended sound change cannot ride along. Each rewritten file records
# -Reason. Add a changelog.txt line containing that exact reason afterwards;
# until then run-tests-win.ps1 (check-golden-changelog.ps1) and the changelog
# doctest fail.
#
# Usage: pwsh NeuralAmpModeler/scripts/regen-golden-renders.ps1 -Reason "chorus redesign" -Groups chorus
# Groups are the file names in tests/golden/ without .json (amps, chorus, nam-files, ...).

param(
  [Parameter(Mandatory = $true)]
  [string]$Reason,
  [Parameter(Mandatory = $true)]
  [string[]]$Groups
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = (Resolve-Path (Join-Path $here "..")).Path
$goldenDir = Join-Path $projectDir "tests\golden"
$changelogPath = Join-Path $projectDir "installer\changelog.txt"
$minReasonLength = 12

if ($env:CI -or $env:GITHUB_ACTIONS) {
  Write-Host "Golden references are never regenerated on CI (CI / GITHUB_ACTIONS is set)." -ForegroundColor Red
  exit 1
}

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

# -Groups a,b arrives as one array; "a,b" in quotes as one string. Accept both.
$groupList = @($Groups | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($groupList.Count -eq 0) {
  Write-Host "-Groups must list at least one group." -ForegroundColor Red
  exit 1
}
foreach ($g in $groupList) {
  if ($g -notmatch '^[a-z0-9-]+$') {
    Write-Host "-Groups: ""$g"" is not a group name (tests/golden/<name>.json)." -ForegroundColor Red
    exit 1
  }
  if (-not (Test-Path -LiteralPath (Join-Path $goldenDir "$g.json"))) {
    Write-Host "-Groups: $g.json does not exist yet; it is created only if a test case renders group ""$g""." -ForegroundColor Yellow
  }
}

$before = @{}
foreach ($g in $groupList) {
  $p = Join-Path $goldenDir "$g.json"
  $before[$g] = if (Test-Path -LiteralPath $p) { (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash } else { "" }
}

$env:VOLUM_GOLDEN_REGEN = "1"
$env:VOLUM_GOLDEN_REASON = $Reason
$env:VOLUM_GOLDEN_GROUPS = ($groupList -join ",")
try {
  $global:LASTEXITCODE = 0
  & (Join-Path $here "run-tests-win.ps1") -Filter "Golden renders:"
  $rc = $LASTEXITCODE
} finally {
  Remove-Item Env:VOLUM_GOLDEN_REGEN -ErrorAction SilentlyContinue
  Remove-Item Env:VOLUM_GOLDEN_REASON -ErrorAction SilentlyContinue
  Remove-Item Env:VOLUM_GOLDEN_GROUPS -ErrorAction SilentlyContinue
}
if ($rc) {
  Write-Host "Regeneration FAILED (a group outside -Groups moved, or a guard refused). Listed groups may be partly rewritten; check git status." -ForegroundColor Red
  exit $rc
}

$missing = @()
foreach ($g in $groupList) {
  $p = Join-Path $goldenDir "$g.json"
  if (-not (Test-Path -LiteralPath $p)) { $missing += $g; continue }
  $after = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
  if ($after -eq $before[$g]) {
    Write-Host "  $g : unchanged (listed, nothing moved)"
  } else {
    Write-Host "  $g : REWRITTEN"
  }
}
if ($missing.Count -gt 0) {
  Write-Host "-Groups listed $($missing -join ', ') but no test case wrote it (typo?)." -ForegroundColor Red
  exit 1
}

Write-Host ""
Write-Host "Regenerated for: $Reason"
& git -C $projectDir status --short -- tests/golden
Write-Host "Next: add a changelog.txt line containing ""$Reason"" and run run-tests-win.ps1." -ForegroundColor Yellow
exit 0
