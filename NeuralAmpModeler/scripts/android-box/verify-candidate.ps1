# Mechanical gate for one Android shootout unit at the branch head. Writes
# evidence/<x>/<sha>/verify-candidate.json in the main checkout's .scratch/android-shootout/
# so reviewers in any worktree read the same file.
#
#   pwsh NeuralAmpModeler/scripts/android-box/verify-candidate.ps1 -Candidate a|b|c|design
#
# Exit 0 only when every gate passes; 1 when a gate fails; 2 when the box pipeline
# itself could not run. The harness gate is the harness self-test, so a harness that
# cannot tell a wrong UI from a right one fails the gate. The design system has no
# feature surfaces: for -Candidate design the harness, coverage, reachability and
# touch gates are reported as skipped.
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("a", "b", "c", "design")]
  [string] $Candidate,
  [string] $Branch
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$box = Join-Path $here "..\android-box.ps1"
$repo = (& git rev-parse --show-toplevel).Trim()
if (-not $Branch) { $Branch = (& git -C $repo rev-parse --abbrev-ref HEAD).Trim() }
$sha = (& git -C $repo rev-parse "refs/heads/$Branch").Trim()

$dirty = & git -C $repo status --porcelain -- NeuralAmpModeler/projects/android NeuralAmpModeler/scripts/android-box
if ($dirty) {
  Write-Host "warning: uncommitted changes under projects/android or scripts/android-box are NOT part of $($sha.Substring(0, 8)):" -ForegroundColor Yellow
  $dirty | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
}

function Invoke-Box([string[]] $steps) {
  & $box -Step $steps -Candidate $Candidate -Branch $Branch -Sha $sha
  return $LASTEXITCODE
}

$started = Get-Date
if ((Invoke-Box @("Sync")) -ne 0) {
  Write-Host "verify-candidate: Sync failed; nothing was verified" -ForegroundColor Red
  exit 2
}
$buildCode = Invoke-Box @("Build")
[void](Invoke-Box @("Unit"))
$isDesign = $Candidate -eq "design"
if ($buildCode -eq 0) {
  if (-not $isDesign) { [void](Invoke-Box @("Test")) }
  [void](Invoke-Box @("Capture"))
  [void](Invoke-Box @("Motion"))
}
if ((Invoke-Box @("Fetch")) -ne 0) {
  Write-Host "verify-candidate: Fetch failed; no evidence on the laptop" -ForegroundColor Red
  exit 2
}

$common = (& git -C $repo rev-parse --path-format=absolute --git-common-dir).Trim()
$evidence = Join-Path (Split-Path -Parent $common) ".scratch\android-shootout\evidence\$Candidate\$sha"
$results = Join-Path $evidence "results"

function Read-Result([string] $name, [string] $whenMissing) {
  $path = Join-Path $results $name
  if (Test-Path -LiteralPath $path) { return (Get-Content -LiteralPath $path -Raw | ConvertFrom-Json) }
  return [pscustomobject]@{ pass = $false; error = $whenMissing }
}

$skipped = [pscustomobject]@{ pass = $true; skipped = "the design system has no feature surfaces" }
$notRun = "not run: the build failed"
$build = Read-Result "build.json" "no build result"
$unit = Read-Result "unit.json" "no unit result"
if ($isDesign) {
  $coverage = [pscustomobject]@{ total = 0; passed = 0; missing = @(); pass = $true; skipped = $skipped.skipped }
  $reach = $skipped
  $touch = $skipped
  $harness = $skipped
} else {
  $cov = Read-Result "coverage.json" $notRun
  $missing = @()
  if ($cov.PSObject.Properties.Name -contains "missing") { $missing = @($cov.missing) }
  $total = if ($cov.PSObject.Properties.Name -contains "total") { [int]$cov.total } else { 0 }
  $passed = if ($cov.PSObject.Properties.Name -contains "passed") { [int]$cov.passed } else { 0 }
  $coverage = [pscustomobject]@{
    total = $total; passed = $passed; missing = $missing
    pass = ($total -gt 0 -and $missing.Count -eq 0 -and $passed -eq $total)
    report = "results/coverage.json"
  }
  if ($cov.PSObject.Properties.Name -contains "error") { $coverage | Add-Member -NotePropertyName error -NotePropertyValue $cov.error }
  $reach = Read-Result "reachability.json" $notRun
  $touch = Read-Result "touch.json" $notRun
  $harness = Read-Result "harness.json" $notRun
}
$crash = Read-Result "crash_anr.json" "no crash scan"
$captures = Read-Result "captures.json" $notRun
$motion = Read-Result "motion.json" $notRun

$gates = [ordered]@{
  build = [bool]$build.pass
  unit = [bool]$unit.pass
  harness = [bool]$harness.pass
  coverage = [bool]$coverage.pass
  reachability = [bool]$reach.pass
  touch = [bool]$touch.pass
  crash_anr = [bool]$crash.pass
  captures = [bool]$captures.pass
  motion = [bool]$motion.pass
}
$pass = -not ($gates.Values -contains $false)

$report = [ordered]@{
  commit = $sha
  pass = $pass
  candidate = $Candidate
  branch = $Branch
  generated = (Get-Date).ToString("yyyy-MM-ddTHH:mm:sszzz")
  minutes = [math]::Round(((Get-Date) - $started).TotalMinutes, 1)
  gates = $gates
  build = $build
  unit = $unit
  harness = $harness
  coverage = $coverage
  reachability = $reach
  touch = $touch
  crash_anr = $crash
  captures = $captures
  motion = $motion
}
New-Item -ItemType Directory -Force $evidence | Out-Null
$out = Join-Path $evidence "verify-candidate.json"
[IO.File]::WriteAllText($out, ($report | ConvertTo-Json -Depth 12))

Write-Host ""
Write-Host "verify-candidate $Candidate @ $($sha.Substring(0, 8)): $(if ($pass) { 'PASS' } else { 'FAIL' })" -ForegroundColor $(if ($pass) { "Green" } else { "Red" })
foreach ($g in $gates.GetEnumerator()) {
  Write-Host ("  {0,-13} {1}" -f $g.Key, $(if ($g.Value) { "pass" } else { "FAIL" }))
}
if (-not $isDesign) { Write-Host "  coverage      $($coverage.passed)/$($coverage.total)" }
Write-Host "  evidence      $evidence"
if ($pass) { exit 0 } else { exit 1 }
