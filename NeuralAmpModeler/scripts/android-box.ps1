# Laptop side of the Android shootout box pipeline (box provisioned by android-box/provision.sh).
#
#   pwsh NeuralAmpModeler/scripts/android-box.ps1 -Step Sync,Build,Unit,Test,Capture,Motion,Fetch -Candidate a
#
# Never changes git config: the box remote and key go in through GIT_SSH_COMMAND for this process only.
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string[]] $Step,
  [ValidateSet("a", "b", "c", "design")]
  [string] $Candidate = "a",
  [string] $Branch,
  [string] $Sha
)

$ErrorActionPreference = "Stop"
$validSteps = @("Sync", "Build", "Unit", "Test", "Capture", "Motion", "Fetch")
$Step = @($Step | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
foreach ($s in $Step) { if ($s -notin $validSteps) { throw "unknown step '$s' (use $($validSteps -join ', '))" } }
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

# PowerShell appends a CR to the last piped line; the sacrificial comment line absorbs it.
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
