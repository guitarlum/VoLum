# Laptop side of the Android shootout box pipeline (box: lum@192.168.178.91,
# Ubuntu under WSL2, provisioned by android-box/provision.sh). Each step runs
# android-box/box.sh from the box worktree of the commit under test.
#
#   pwsh NeuralAmpModeler/scripts/android-box.ps1 -Step Sync,Build,Unit -Candidate a
#   pwsh NeuralAmpModeler/scripts/android-box.ps1 -Step Test,Capture,Motion,Fetch -Candidate a
#
# Steps:
#   Sync     push the commit to ~/volum-android/VoLum.git and check it out in ~/volum-android/wt/<branch-slug>
#   Build    assemble the flavor's app + test APKs and dump the registries (build lock)
#   Unit     :core unit tests (build lock)
#   Test     coverage, reachability and touch-target instrumented tests (emu lock; boots the AVD if needed)
#   Capture  every screen id x fixture at font scale 1.0 and 1.3 (emu lock)
#   Motion   screenrecord of the motion scripts -> ffmpeg contact sheets (emu lock)
#   Fetch    pack the box results and unpack them into <main checkout>/.scratch/android-shootout/evidence/<x>/<sha>/
#
# Never changes git config: the box remote and key go in through GIT_SSH_COMMAND for this process only.
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("Sync", "Build", "Unit", "Test", "Capture", "Motion", "Fetch")]
  [string[]] $Step,
  [ValidateSet("a", "b", "c", "design")]
  [string] $Candidate = "a",
  [string] $Branch,
  [string] $Sha
)

$ErrorActionPreference = "Stop"
$BoxHost = "lum@192.168.178.91"
$BoxKey = "C:/Users/SteffenDangmann/.ssh/volum_a2_pc"
$BoxRepo = "ssh://lum@192.168.178.91/home/lum/volum-android/VoLum.git"
$SshOpts = @("-i", $BoxKey, "-o", "BatchMode=yes", "-o", "ConnectTimeout=15", "-o", "ServerAliveInterval=30")

$repo = (& git rev-parse --show-toplevel).Trim()
if (-not $Branch) { $Branch = (& git -C $repo rev-parse --abbrev-ref HEAD).Trim() }
if (-not $Sha) { $Sha = (& git -C $repo rev-parse HEAD).Trim() }
$slug = $Branch -replace "[^A-Za-z0-9._-]", "-"
$boxScript = "~/volum-android/wt/$slug/NeuralAmpModeler/scripts/android-box/box.sh"

function Get-EvidenceRoot {
  $common = (& git -C $repo rev-parse --path-format=absolute --git-common-dir).Trim()
  $main = Split-Path -Parent $common
  return Join-Path $main ".scratch\android-shootout\evidence"
}

# Pipes a bash script to the box. CRs are stripped and a sacrificial comment
# line absorbs the CR PowerShell appends to the last piped line.
function Invoke-BoxBash([string] $script) {
  $clean = ($script -replace "`r", "") + "`n#"
  $clean | & ssh @SshOpts $BoxHost "bash -s" | Out-Host
  return $LASTEXITCODE
}

function Invoke-BoxStep([string] $name) {
  $flavor = $Candidate
  & ssh @SshOpts $BoxHost "bash $boxScript $name $flavor $Candidate $Sha" | Out-Host
  return $LASTEXITCODE
}

function Invoke-Sync {
  $env:GIT_SSH_COMMAND = "ssh -i $BoxKey -o BatchMode=yes -o ConnectTimeout=15"
  try {
    # The box repo is a private mirror for builds; force-updating its branch ref is how a rewritten branch reaches it.
    & git -C $repo push --quiet $BoxRepo "+$($Sha):refs/heads/$Branch" | Out-Host
    if ($LASTEXITCODE -ne 0) { return $LASTEXITCODE }
  } finally {
    Remove-Item Env:\GIT_SSH_COMMAND -ErrorAction SilentlyContinue
  }
  $script = @'
set -euo pipefail
R="$HOME/volum-android"
WT="$R/wt/__SLUG__"
SHA="__SHA__"
exec 7>"$R/locks/git.lock"
flock -w 600 7
if [ -e "$WT/.git" ]; then
  git -C "$WT" checkout -q --detach -f "$SHA"
  git -C "$WT" clean -q -fd
else
  git --git-dir="$R/VoLum.git" worktree prune
  git --git-dir="$R/VoLum.git" worktree add -q --detach "$WT" "$SHA"
fi
echo "box worktree $WT at $(git -C "$WT" rev-parse --short HEAD)"
'@
  return Invoke-BoxBash ($script.Replace("__SLUG__", $slug).Replace("__SHA__", $Sha))
}

function Invoke-Fetch {
  $code = Invoke-BoxStep "pack"
  if ($code -ne 0) { return $code }
  $dest = Join-Path (Get-EvidenceRoot) $Candidate
  New-Item -ItemType Directory -Force $dest | Out-Null
  $tmp = Join-Path $env:TEMP "volum-box-$Candidate-$Sha.tgz"
  & scp -q @SshOpts "$($BoxHost):volum-android/out/$Candidate/$Sha.tgz" $tmp | Out-Host
  if ($LASTEXITCODE -ne 0) { return $LASTEXITCODE }
  $target = Join-Path $dest $Sha
  if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
  & tar -xzf $tmp -C $dest | Out-Host
  $code = $LASTEXITCODE
  Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
  if ($code -eq 0) { Write-Host "evidence: $target" }
  return $code
}

$failed = 0
foreach ($s in $Step) {
  Write-Host "== $s ($Candidate, $Branch @ $($Sha.Substring(0, 8)))" -ForegroundColor Cyan
  $code = switch ($s) {
    "Sync" { Invoke-Sync }
    "Fetch" { Invoke-Fetch }
    default { Invoke-BoxStep $s.ToLowerInvariant() }
  }
  if ($code -ne 0) {
    Write-Host "$s exited $code" -ForegroundColor Yellow
    $failed = $code
    if ($s -in @("Sync", "Build")) { break }
  }
}
exit $failed
