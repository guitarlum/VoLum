# Local-only: fail if the machine has no denylist or git hooks.
# CI has neither on purpose and must stay green.
#
#   pwsh NeuralAmpModeler/scripts/check-local-guards.ps1
# Reinstall: pwsh NeuralAmpModeler/scripts/install-local-guards.ps1

$ErrorActionPreference = "Stop"
if ($env:CI -or $env:GITHUB_ACTIONS -eq "true") { exit 0 }

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path
$denyFile = Join-Path $here "vendor-denylist.txt"
$hookDir = Join-Path $repoRoot ".git\hooks"

$problems = @()
if (-not (Test-Path -LiteralPath $denyFile)) {
  $problems += "missing NeuralAmpModeler/scripts/vendor-denylist.txt (local file; not tracked)"
}
else {
  $denied = Get-Content -LiteralPath $denyFile |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -and -not $_.StartsWith("#") }
  if ($denied.Count -eq 0) {
    $problems += "vendor-denylist.txt is empty"
  }
}
foreach ($name in @("commit-msg", "pre-commit")) {
  $hook = Join-Path $hookDir $name
  if (-not (Test-Path -LiteralPath $hook)) {
    $problems += "missing .git/hooks/$name"
  }
}

if ($problems.Count -gt 0) {
  Write-Host "Local guards are not installed (this machine, not CI):" -ForegroundColor Red
  foreach ($p in $problems) { Write-Host ("  {0}" -f $p) -ForegroundColor Yellow }
  Write-Host "Reinstall: pwsh NeuralAmpModeler/scripts/install-local-guards.ps1" -ForegroundColor Yellow
  exit 1
}

Write-Host "Local guards present (denylist + commit-msg + pre-commit)." -ForegroundColor Green
exit 0
