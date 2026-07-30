# Fail if a tracked *.sh is stored in git without the executable bit.
#
# Git on Windows records new files as mode 100644, so a shell script authored
# here looks fine locally (nothing on Windows runs it) and then dies on the
# macOS/Linux runner with "Permission denied" - up to an hour into a CI run.
# Checking the index catches it in milliseconds on the machine that made the
# mistake.
#
# Fix a reported file with:
#   git update-index --chmod=+x <path>

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path

Push-Location $repoRoot
try
{
  # Only this repo's own scripts: submodules carry their own modes and history.
  $entries = & git ls-files -s -- "NeuralAmpModeler/*.sh" "*.sh" ":(exclude)iPlug2/**" `
    ":(exclude)NeuralAmpModelerCore/**" ":(exclude)AudioDSPTools/**" ":(exclude)eigen/**"
  if ($LASTEXITCODE -ne 0) { Write-Error "git ls-files failed" }

  $offenders = @()
  foreach ($entry in $entries)
  {
    # Format: "<mode> <sha> <stage>\t<path>"
    if ($entry -match "^(\d{6})\s+\S+\s+\d+\s+(.+)$")
    {
      if ($matches[1] -ne "100755") { $offenders += $matches[2] }
    }
  }

  if ($offenders.Count -gt 0)
  {
    Write-Host "Shell scripts missing the executable bit in git:" -ForegroundColor Red
    foreach ($path in $offenders) { Write-Host "  $path" -ForegroundColor Red }
    Write-Host ""
    Write-Host "Fix with: git update-index --chmod=+x <path>" -ForegroundColor Yellow
    exit 1
  }

  Write-Host "Shell exec bits OK ($($entries.Count) tracked .sh files)."
  exit 0
}
finally
{
  Pop-Location
}
