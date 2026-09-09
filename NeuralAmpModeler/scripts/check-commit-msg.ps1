# Scan a commit message file against the local denylist. Fail closed if the
# denylist is missing — a machine without it must not commit.
#
# Called from .git/hooks/commit-msg. Not used in CI.

param(
  [Parameter(Mandatory = $true)]
  [string]$MessageFile
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$denyFile = Join-Path $here "vendor-denylist.txt"

if (-not (Test-Path -LiteralPath $denyFile)) {
  Write-Host "Refuse to commit: vendor-denylist.txt is missing." -ForegroundColor Red
  Write-Host "pwsh NeuralAmpModeler/scripts/install-local-guards.ps1" -ForegroundColor Yellow
  exit 1
}

$denied = Get-Content -LiteralPath $denyFile |
  ForEach-Object { $_.Trim() } |
  Where-Object { $_ -and -not $_.StartsWith("#") }

if ($denied.Count -eq 0) {
  Write-Host "Refuse to commit: vendor-denylist.txt is empty." -ForegroundColor Red
  exit 1
}

$lineNo = 0
$hits = @()
foreach ($line in (Get-Content -LiteralPath $MessageFile)) {
  $lineNo++
  foreach ($pattern in $denied) {
    if ($line -match $pattern) {
      $hits += "{0}: {1}" -f $lineNo, $line.Trim()
    }
  }
}

if ($hits.Count -gt 0) {
  Write-Host "Commit message matches a local denylist pattern:" -ForegroundColor Red
  foreach ($h in $hits) { Write-Host ("  {0}" -f $h) -ForegroundColor Yellow }
  Write-Host "Describe the design. Do not name third-party products or methods." -ForegroundColor Red
  exit 1
}

exit 0
