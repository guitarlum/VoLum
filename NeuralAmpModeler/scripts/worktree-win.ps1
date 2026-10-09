# Add or remove a VoLum git worktree without the two ways worktrees have bitten.
#
#   -Add     One branch per call (a comma list once became a single branch named
#            "a b"). Runs `git submodule update --init --recursive` inside the new
#            worktree (never junction the main checkout's submodules into one),
#            and copies the local vendor-denylist.txt the commit hooks need.
#   -Remove  `git worktree remove` refuses a worktree with submodules, and its
#            --force follows junctions: on 2026-09-23 that emptied the main
#            checkout. This unlinks every junction and directory symlink first
#            (the link only, never its target), then removes the worktree,
#            retries locked leftovers, and prunes.
#
#   pwsh NeuralAmpModeler/scripts/worktree-win.ps1 -Add feature/hunt-30-foo        # C:\dev\VoLum-hunt-30-foo off dev
#   pwsh NeuralAmpModeler/scripts/worktree-win.ps1 -Add feature/x -Base main -Path C:\dev\VoLum-x -NoSubmodules
#   pwsh NeuralAmpModeler/scripts/worktree-win.ps1 -Remove C:\dev\VoLum-hunt-30-foo -DeleteBranch
#
# -Remove refuses uncommitted tracked or untracked changes unless -Force.
# -DeleteBranch uses `git branch -d`, which keeps a branch that is not merged.
[CmdletBinding()]
param(
  [string] $Add,
  [string] $Remove,
  [string] $Base = "dev",
  [string] $Path,
  [switch] $NoSubmodules,
  [switch] $DeleteBranch,
  [switch] $Force
)

$ErrorActionPreference = "Stop"

function Invoke-Git {
  & git @args
  if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed (exit $LASTEXITCODE)" }
}

function Get-NormalPath([string] $p) {
  return [System.IO.Path]::GetFullPath($p.Replace("/", "\")).TrimEnd("\")
}

function Get-Worktrees([string] $main) {
  $items = @()
  $cur = $null
  foreach ($line in (& git -C $main worktree list --porcelain)) {
    if ($line -like "worktree *") {
      if ($cur) { $items += $cur }
      $cur = [pscustomobject]@{ Path = (Get-NormalPath $line.Substring(9)); Branch = $null; Locked = $false }
    }
    elseif ($line -like "branch refs/heads/*") { $cur.Branch = $line.Substring(18) }
    elseif ($line -like "locked*") { $cur.Locked = $true }
  }
  if ($cur) { $items += $cur }
  return $items
}

# Junctions and directory symlinks under $root, without descending into them.
function Get-DirectoryLinks([string] $root) {
  $links = New-Object System.Collections.Generic.List[string]
  $stack = New-Object System.Collections.Generic.Stack[System.IO.DirectoryInfo]
  $stack.Push([System.IO.DirectoryInfo]::new($root))
  while ($stack.Count -gt 0) {
    $dir = $stack.Pop()
    try { $children = $dir.GetDirectories() } catch { continue }
    foreach ($child in $children) {
      if ($child.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { $links.Add($child.FullName) }
      else { $stack.Push($child) }
    }
  }
  return , $links
}

function Remove-DirectoryLinks([string] $root) {
  foreach ($link in (Get-DirectoryLinks $root)) {
    # rmdir without /s on a reparse point deletes the link and leaves the target alone.
    & cmd /c rmdir "$link"
    if (Test-Path -LiteralPath $link) { throw "could not unlink $link; nothing was removed" }
    Write-Host "Unlinked $link"
  }
  if ((Get-DirectoryLinks $root).Count -gt 0) { throw "links remain under $root; nothing was removed" }
}

if ([bool]$Add -eq [bool]$Remove) { throw "Pass exactly one of -Add <branch> or -Remove <path>." }

$commonDir = (& git rev-parse --path-format=absolute --git-common-dir)
if ($LASTEXITCODE -ne 0) { throw "run this from inside a VoLum checkout" }
$main = Split-Path (Get-NormalPath $commonDir) -Parent

if ($Add) {
  & git check-ref-format --branch $Add | Out-Null
  if ($LASTEXITCODE -ne 0 -or $Add -match '[\s,]') { throw "-Add takes one valid branch name; got '$Add'. Call the script once per branch." }
  if (-not $Path) { $Path = Join-Path (Split-Path $main -Parent) ("VoLum-" + ($Add -split "/")[-1]) }
  $Path = Get-NormalPath $Path
  if (Test-Path -LiteralPath $Path) { throw "$Path already exists" }

  & git -C $main rev-parse --verify --quiet "refs/heads/$Add" | Out-Null
  if ($LASTEXITCODE -eq 0) { Invoke-Git -C $main worktree add $Path $Add }
  else { Invoke-Git -C $main worktree add -b $Add $Path $Base }

  if (-not $NoSubmodules) { Invoke-Git -C $Path submodule update --init --recursive }
  # Local-only and untracked: without it the commit-msg hook refuses every commit.
  $deny = "NeuralAmpModeler\scripts\vendor-denylist.txt"
  if (Test-Path -LiteralPath (Join-Path $main $deny)) { Copy-Item -LiteralPath (Join-Path $main $deny) -Destination (Join-Path $Path $deny) }
  Write-Host "Worktree $Path on $Add" -ForegroundColor Green
  exit 0
}

if (-not (Test-Path -LiteralPath $Remove)) {
  Invoke-Git -C $main worktree prune
  throw "$Remove does not exist (pruned stale worktree entries)"
}
$target = Get-NormalPath $Remove
$all = @(Get-Worktrees $main)
if ($all.Count -gt 0 -and $all[0].Path -ieq $target) { throw "$target is the main checkout; refusing" }
$wt = $all | Where-Object { $_.Path -ieq $target } | Select-Object -First 1
if (-not $wt) { throw "$target is not a worktree of $main" }
if ($wt.Locked) { throw "$target is locked (git worktree unlock it first if that is intended)" }

$dirty = & git -C $target status --porcelain --ignore-submodules=dirty
if ($dirty -and -not $Force) {
  $dirty | Select-Object -First 15 | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
  throw "$target has uncommitted changes; commit them or pass -Force"
}

Get-Process -ErrorAction SilentlyContinue |
  Where-Object { $_.Path -and $_.Path.StartsWith($target + "\", [StringComparison]::OrdinalIgnoreCase) } |
  ForEach-Object { Write-Host "Stopping $($_.ProcessName) ($($_.Id)) running from the worktree"; Stop-Process -Id $_.Id -Force }

Remove-DirectoryLinks $target
# --force skips the submodule refusal; it is safe only because no links are left.
& git -C $main worktree remove --force $target
for ($i = 0; $i -lt 3 -and (Test-Path -LiteralPath $target); $i++) {
  Start-Sleep -Seconds 2
  Remove-DirectoryLinks $target
  Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue
}
Invoke-Git -C $main worktree prune

if ($DeleteBranch -and $wt.Branch) {
  & git -C $main branch -d $wt.Branch
  if ($LASTEXITCODE -ne 0) { Write-Warning "kept branch $($wt.Branch): not merged (delete it by hand if that is intended)" }
}

$lost = & git -C $main submodule status | Where-Object { $_ -match '^-' }
if ($lost) {
  $lost | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
  throw "the main checkout lost submodules; run git submodule update --init in $main"
}
if (Test-Path -LiteralPath $target) {
  Write-Warning "removed the worktree registration, but files are still locked under $target; delete it after closing what holds them"
  exit 1
}
Write-Host "Removed worktree $target" -ForegroundColor Green
exit 0
