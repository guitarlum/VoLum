# Merge a CI-verified branch into dev (or -Into <target>) and push it, in one call.
#
# Refuses unless the branch head is pushed and has a successful ci.yml run whose
# macOS job is green (-AllowWindowsOnly accepts a `ci-watch.ps1 -WindowsOnly` run),
# the local target matches origin, and the merge has no conflicts. Then it makes a
# --no-ff "Merge <branch> into <target>" commit, pushes over HTTPS (SSH to GitHub
# is blocked on this machine), and refreshes origin/<target>.
#
# The push starts the target's CI by itself; follow it with
# `ci-watch.ps1 -Ref dev`, never -Dispatch it (a second ~93 min matrix).
#
# Where the target is checked out in a worktree, the merge runs there (git refuses
# if it would overwrite local edits). Otherwise it is written with git merge-tree
# and no checkout is touched.
#
#   pwsh NeuralAmpModeler/scripts/merge-verified-branch.ps1 -Branch feature/hunt-30-foo -Summary "Fix X (F-112)."
#   pwsh NeuralAmpModeler/scripts/merge-verified-branch.ps1 -Branch feature/hunt-30-foo -DryRun
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)] [string] $Branch,
  [string] $Into = "dev",
  [string] $Summary = "",
  [string] $Repo = "guitarlum/VoLum",
  [switch] $AllowWindowsOnly,
  [switch] $DryRun
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Invoke-Git {
  & git @args
  if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed (exit $LASTEXITCODE)" }
}

function Get-Sha([string] $ref) {
  $sha = & git -C $main rev-parse --verify --quiet "$ref^{commit}"
  if ($LASTEXITCODE -ne 0) { return $null }
  return $sha.Trim()
}

function Stop-Refused([string] $why) {
  Write-Host "REFUSED: $why" -ForegroundColor Red
  exit 1
}

$commonDir = (& git rev-parse --path-format=absolute --git-common-dir)
if ($LASTEXITCODE -ne 0) { throw "run this from inside a VoLum checkout" }
$main = Split-Path ([System.IO.Path]::GetFullPath($commonDir.Replace("/", "\")).TrimEnd("\")) -Parent
$url = "https://github.com/$Repo.git"

& git -C $main fetch --quiet $url "+refs/heads/${Branch}:refs/remotes/origin/$Branch" "+refs/heads/${Into}:refs/remotes/origin/$Into"
if ($LASTEXITCODE -ne 0) { Stop-Refused "could not fetch $Branch and $Into from $url (is $Branch pushed?)" }

$head = Get-Sha "refs/remotes/origin/$Branch"
$base = Get-Sha "refs/remotes/origin/$Into"
$localHead = Get-Sha "refs/heads/$Branch"
$localBase = Get-Sha "refs/heads/$Into"
if ($localHead -and $localHead -ne $head) { Stop-Refused "local $Branch ($localHead) differs from origin ($head); push or pull it first" }
if ($localBase -and $localBase -ne $base) { Stop-Refused "local $Into ($localBase) differs from origin ($base); push or pull it first" }

& git -C $main merge-base --is-ancestor $head $base
if ($LASTEXITCODE -eq 0) {
  Write-Host "$Branch ($($head.Substring(0, 8))) is already in $Into; nothing to do." -ForegroundColor Green
  exit 0
}

$runs = & gh run list --repo $Repo --workflow ci.yml --branch $Branch --limit 30 --json databaseId,headSha,status,conclusion,url | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { Stop-Refused "gh run list failed" }
$run = @($runs | Where-Object { $_.headSha -eq $head }) | Select-Object -First 1
if (-not $run) {
  Stop-Refused "no CI run on $Branch head $($head.Substring(0, 8)). Start one: pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -Ref $Branch -Dispatch"
}
if ($run.status -ne "completed") {
  Stop-Refused "CI run $($run.databaseId) on $($head.Substring(0, 8)) is still $($run.status). Wait: pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -RunId $($run.databaseId)"
}
if ($run.conclusion -ne "success") { Stop-Refused "CI run $($run.databaseId) concluded '$($run.conclusion)': $($run.url)" }
$jobs = (& gh run view $run.databaseId --repo $Repo --json jobs | ConvertFrom-Json).jobs
$mac = @($jobs | Where-Object { $_.name -like "macOS*" }) | Select-Object -First 1
if (-not $AllowWindowsOnly -and (-not $mac -or $mac.conclusion -ne "success")) {
  Stop-Refused "CI run $($run.databaseId) has no green macOS job (Windows-only run?). Dispatch the full matrix, or pass -AllowWindowsOnly if macOS cannot be affected."
}
Write-Host "CI green on $($head.Substring(0, 8)): $($run.url)"

$tree = & git -C $main merge-tree --write-tree --name-only $base $head
if ($LASTEXITCODE -ne 0) {
  # --name-only output: tree oid, conflicted paths, a blank line, then messages.
  foreach ($line in @($tree | Select-Object -Skip 1)) {
    if (-not $line) { break }
    Write-Host "  conflict: $line" -ForegroundColor Yellow
  }
  Stop-Refused "$Branch conflicts with $Into; merge $Into into the branch, push, and let CI go green again"
}
$tree = @($tree)[0].Trim()

$message = "Merge $Branch into $Into"
$body = "$message`n`n"
if ($Summary) { $body += "$Summary`n`n" }
$body += "CI $($run.databaseId) green on $($head.Substring(0, 8)).`n"
$msgFile = Join-Path ([System.IO.Path]::GetTempPath()) "volum-merge-msg.txt"
[System.IO.File]::WriteAllText($msgFile, $body)
# commit-tree skips the commit-msg hook, so run its check here.
& (Join-Path $here "check-commit-msg.ps1") -MessageFile $msgFile
if ($LASTEXITCODE -ne 0) { Stop-Refused "commit message check failed" }

if ($DryRun) {
  Write-Host "DRY RUN: would commit '$message' onto $($base.Substring(0, 8)) and push $Into to $url" -ForegroundColor Green
  exit 0
}

$checkout = $null
$cur = $null
foreach ($line in (& git -C $main worktree list --porcelain)) {
  if ($line -like "worktree *") { $cur = $line.Substring(9) }
  elseif ($line -eq "branch refs/heads/$Into") { $checkout = $cur }
}

if ($checkout) {
  & git -C $checkout merge --no-ff -F $msgFile $head
  if ($LASTEXITCODE -ne 0) {
    if (Test-Path -LiteralPath (& git -C $checkout rev-parse --path-format=absolute --git-path MERGE_HEAD)) { & git -C $checkout merge --abort }
    Stop-Refused "git merge in $checkout failed (local edits in the way?); the checkout was left as it was"
  }
  $merge = Get-Sha "refs/heads/$Into"
}
else {
  $merge = (& git -C $main commit-tree $tree -p $base -p $head -F $msgFile)
  if ($LASTEXITCODE -ne 0) { throw "git commit-tree failed" }
  $merge = $merge.Trim()
  if ($localBase) { Invoke-Git -C $main update-ref "refs/heads/$Into" $merge $base }
}

& git -C $main push $url "${merge}:refs/heads/$Into"
if ($LASTEXITCODE -ne 0) {
  if (-not $checkout -and $localBase) { & git -C $main update-ref "refs/heads/$Into" $base $merge }
  Stop-Refused "push of $Into was rejected (did $Into move on origin?). Rerun the script."
}
Invoke-Git -C $main fetch --quiet $url "+refs/heads/${Into}:refs/remotes/origin/$Into"

Write-Host "Merged $Branch into $Into as $($merge.Substring(0, 8)) and pushed." -ForegroundColor Green
Write-Host "The push starts $Into CI: pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -Ref $Into (do not -Dispatch)."
exit 0
