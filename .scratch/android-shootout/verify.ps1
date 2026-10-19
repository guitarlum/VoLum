# Predicate for the android-shootout conductor loop. Exit 0 only when every
# unit is accepted for its current branch head:
#   design, a, b, c  verdicts/<x>.md "verdict: pass" + evidence/<x>/<sha>/verify-candidate.json "pass": true
#   a, b, c          branch contains the accepted feature/android-design commit
#   spike            evidence/spike/<sha>/spike.json "pass": true + verdicts/spike.md "verdict: pass"
#   gallery          gallery/index.html names every candidate head
#   tickets          no issue left ready-for-agent, claimed or blocked
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$effort = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = (Resolve-Path (Join-Path $effort "..\..")).Path
$failures = New-Object System.Collections.Generic.List[string]

function Get-Head([string]$branch) {
  $sha = & git -C $repo rev-parse --verify --quiet "refs/heads/$branch" 2>$null
  if ($LASTEXITCODE -ne 0 -or -not $sha) { return $null }
  return $sha.Trim()
}

function Get-Field([string]$path, [string]$name) {
  if (-not (Test-Path -LiteralPath $path)) { return $null }
  $m = Select-String -LiteralPath $path -Pattern "^\s*$name\s*:\s*(\S+)" | Select-Object -First 1
  if ($m) { return $m.Matches[0].Groups[1].Value.Trim() }
  return $null
}

function Test-JsonPass([string]$path, [string]$sha) {
  if (-not (Test-Path -LiteralPath $path)) { return "missing $path" }
  $j = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
  if ($j.commit -ne $sha) { return "evidence names $($j.commit), head is $sha" }
  if ($j.pass -ne $true) { return "evidence pass=false" }
  return $null
}

function Test-Unit([string]$id, [string]$branch, [string]$evidenceName) {
  $sha = Get-Head $branch
  if (-not $sha) { $failures.Add("${id}: branch $branch missing"); return $null }
  $err = Test-JsonPass (Join-Path $effort "evidence\$id\$sha\$evidenceName") $sha
  if ($err) { $failures.Add("${id}: $err") }
  $verdictFile = Join-Path $effort "verdicts\$id.md"
  $vCommit = Get-Field $verdictFile "commit"
  $verdict = Get-Field $verdictFile "verdict"
  if ($vCommit -ne $sha) { $failures.Add("${id}: verdict commit '$vCommit' is not head $sha") }
  elseif ($verdict -ne "pass") { $failures.Add("${id}: verdict '$verdict'") }
  return $sha
}

$designSha = Test-Unit "design" "feature/android-design" "verify-candidate.json"
$heads = @{}
foreach ($x in "a", "b", "c") {
  $heads[$x] = Test-Unit $x "feature/android-ui-$x" "verify-candidate.json"
  if ($heads[$x] -and $designSha) {
    & git -C $repo merge-base --is-ancestor $designSha $heads[$x] 2>$null
    if ($LASTEXITCODE -ne 0) { $failures.Add("${x}: does not contain accepted design $designSha") }
  }
}
[void](Test-Unit "spike" "feature/android-engine-spike" "spike.json")

$gallery = Join-Path $effort "gallery\index.html"
if (-not (Test-Path -LiteralPath $gallery)) { $failures.Add("gallery: gallery/index.html missing") }
else {
  $html = Get-Content -LiteralPath $gallery -Raw
  foreach ($x in "a", "b", "c") {
    if ($heads[$x] -and -not $html.Contains($heads[$x].Substring(0, 8))) {
      $failures.Add("gallery: does not reference candidate $x head $($heads[$x].Substring(0, 8))")
    }
  }
}

foreach ($t in Get-ChildItem -LiteralPath (Join-Path $effort "issues") -Filter "*.md") {
  $st = Get-Field $t.FullName "Status"
  if ($st -ne "resolved") { $failures.Add("ticket $($t.Name): Status $st") }
}

if ($failures.Count -gt 0) {
  Write-Output "android-shootout predicate: FAIL ($($failures.Count))"
  $failures | ForEach-Object { Write-Output "  $_" }
  exit 1
}
Write-Output "android-shootout predicate: PASS"
exit 0
