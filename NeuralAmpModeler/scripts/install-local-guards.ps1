# Copy local git hooks into this clone. Does not create vendor-denylist.txt
# (that file stays local and untracked).
#
#   pwsh NeuralAmpModeler/scripts/install-local-guards.ps1

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path
$srcDir = Join-Path $here "git-hooks"
# In a git worktree .git is a file; ask git where the shared hooks live.
$dstDir = (& git -C $repoRoot rev-parse --path-format=absolute --git-path hooks 2>$null)
if (-not $dstDir) { $dstDir = Join-Path $repoRoot ".git\hooks" }
$denyFile = Join-Path $here "vendor-denylist.txt"

if (-not (Test-Path -LiteralPath $srcDir)) {
  throw "Missing hook source: $srcDir"
}
New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
$copied = @()
foreach ($name in @("commit-msg", "pre-commit")) {
  $src = Join-Path $srcDir $name
  if (-not (Test-Path -LiteralPath $src)) {
    throw "Missing hook source: $src"
  }
  Copy-Item -LiteralPath $src -Destination (Join-Path $dstDir $name) -Force
  $copied += $name
}

if (-not (Test-Path -LiteralPath $denyFile)) {
  Write-Host ("Hooks installed ({0})." -f ($copied -join ", ")) -ForegroundColor Green
  Write-Host "vendor-denylist.txt is still missing — commits will be refused until you restore that local file." -ForegroundColor Yellow
  exit 1
}

Write-Host ("Hooks installed ({0}). Denylist present. Local commits and staged files will be scanned." -f ($copied -join ", ")) -ForegroundColor Green
exit 0
