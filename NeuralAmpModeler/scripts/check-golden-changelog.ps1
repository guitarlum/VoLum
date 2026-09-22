# Every golden sound reference (NeuralAmpModeler/tests/golden/*.json) records the
# reason it was last regenerated. That reason has to appear verbatim in
# installer/changelog.txt, so a changed sound always ships with a line naming it.
# Independent of git history, so it holds on shallow CI clones. The same rule is
# asserted by test_volum_golden_render.cpp, which is how macOS CI enforces it.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = (Resolve-Path (Join-Path $here "..")).Path
$goldenDir = Join-Path $projectDir "tests\golden"
$changelogPath = Join-Path $projectDir "installer\changelog.txt"
$minReasonLength = 12
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Fail([string[]]$lines) {
  Write-Host "Golden changelog check FAILED:" -ForegroundColor Red
  foreach ($line in $lines) { Write-Host "  $line" -ForegroundColor Red }
  Write-Host ""
  Write-Host "A golden reference changes only through regen-golden-renders.ps1 -Reason ""<sound change>""," -ForegroundColor Yellow
  Write-Host "and changelog.txt needs a line containing that exact reason." -ForegroundColor Yellow
  exit 1
}

if (-not (Test-Path -LiteralPath $goldenDir)) { Fail @("$goldenDir is missing") }
$files = @(Get-ChildItem -LiteralPath $goldenDir -Filter *.json -File | Sort-Object Name)
if ($files.Count -eq 0) { Fail @("no *.json references in $goldenDir") }

$changelog = [System.IO.File]::ReadAllText($changelogPath, $utf8)
$problems = New-Object System.Collections.Generic.List[string]
foreach ($file in $files) {
  try {
    $json = [System.IO.File]::ReadAllText($file.FullName, $utf8) | ConvertFrom-Json
  } catch {
    $problems.Add("$($file.Name): not valid JSON")
    continue
  }
  $reason = [string]$json.reason
  if ($reason.Length -lt $minReasonLength) {
    $problems.Add("$($file.Name): no regeneration reason (or one under $minReasonLength characters)")
    continue
  }
  if (-not $changelog.Contains($reason)) {
    $problems.Add("$($file.Name): regenerated for ""$reason"", but no changelog.txt line contains that text")
  }
}

if ($problems.Count -gt 0) { Fail $problems.ToArray() }

Write-Host "Golden changelog OK ($($files.Count) reference files, every regeneration reason is in changelog.txt)."
exit 0
