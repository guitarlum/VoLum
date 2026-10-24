# Run a command on the VoLum Linux box from the laptop.
#
#   pwsh -NoProfile -File NeuralAmpModeler\scripts\linux\box.ps1 -Branch <branch> -Sha <sha> -Command "<bash command>"
#
# The command runs from ~/volum-linux/wt/<branch-with-slashes-replaced>, after
# that worktree is checked out at the exact sha and its submodules are updated.
# Exit codes: the command's code; 2 when ssh is unreachable; 3 when
# localai-guard.sh reports BOX_BUSY_LOCALAI. -NoGuard skips the guard.
#
# PowerShell appends a CR to the last line piped into ssh. Scripts are sent to
# `bash -s` with CRs stripped and a sacrificial trailing comment line. The
# remote command is base64 so a `$` in it is never expanded by PowerShell or
# by the local ssh argument parser.
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string] $Branch,
  [Parameter(Mandatory = $true)]
  [string] $Sha,
  [Parameter(Mandatory = $true)]
  [string] $Command,
  [switch] $NoGuard
)

$ErrorActionPreference = "Stop"
# pwsh 7 can turn a native non-zero exit into a terminating error. The exit
# code below has to stay visible so callers can tell busy (3) from unreachable (2).
$PSNativeCommandUseErrorActionPreference = $false

$BoxHost = "lum@192.168.178.91"
$BoxKey = "C:/Users/SteffenDangmann/.ssh/volum_a2_pc"
$SshOpts = @(
  "-i", $BoxKey,
  "-o", "BatchMode=yes",
  "-o", "ConnectTimeout=10",
  "-o", "ServerAliveInterval=30",
  "-o", "ServerAliveCountMax=120",
  "-o", "IdentitiesOnly=yes"
)

if ($Branch -notmatch '^[A-Za-z0-9._/-]+$' -or $Branch -match '\.\.') {
  Write-Host "box: branch must be a simple git ref (letters, digits, . _ / -)"
  exit 1
}
if ($Sha -notmatch '^[0-9a-fA-F]{7,40}$') {
  Write-Host "box: sha must be 7-40 hex characters"
  exit 1
}
if ([string]::IsNullOrWhiteSpace($Command)) {
  Write-Host "box: -Command is empty"
  exit 1
}

$slug = $Branch -replace '[\\/]', '-'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$guardPath = Join-Path $here "localai-guard.sh"

function Invoke-BoxBash([string] $script) {
  $clean = ($script -replace "`r", "") + "`n#"
  $clean | & ssh @SshOpts $BoxHost 'bash -s'
  $script:BoxCode = $LASTEXITCODE
}

try {
  & ssh @SshOpts $BoxHost "true"
  $preflight = $LASTEXITCODE
}
catch {
  Write-Host "ssh unreachable: $BoxHost ($($_.Exception.Message))"
  exit 2
}
if ($preflight -ne 0) {
  Write-Host "ssh unreachable: $BoxHost (exit $preflight)"
  exit 2
}

if (-not $NoGuard) {
  if (-not (Test-Path -LiteralPath $guardPath)) {
    Write-Host "box: missing $guardPath"
    exit 1
  }
  $guard = [IO.File]::ReadAllText($guardPath)
  Invoke-BoxBash $guard
  if ($script:BoxCode -eq 255) {
    Write-Host "ssh unreachable: $BoxHost (connection lost during localai-guard)"
    exit 2
  }
  if ($script:BoxCode -ne 0) { exit $script:BoxCode }
}

$b64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($Command))
$runner = @'
set -euo pipefail
export GIT_TERMINAL_PROMPT=0
ROOT="$HOME/volum-linux"
REPO="$ROOT/VoLum"
WT="$ROOT/wt/@@SLUG@@"
BRANCH="@@BRANCH@@"
SHA="@@SHA@@"
URL="https://github.com/guitarlum/VoLum.git"
log() { printf 'box: %s\n' "$*" >&2; }
mkdir -p "$ROOT/wt" "$ROOT/locks" "$ROOT/out" "$ROOT/logs" "$ROOT/tools/bin"
for lock in build1.lock build2.lock git.lock gui.lock; do
  [ -e "$ROOT/locks/$lock" ] || : > "$ROOT/locks/$lock"
done
exec 7>>"$ROOT/locks/git.lock"
if ! flock -w 1800 7; then
  log "timed out waiting for git.lock"
  exit 1
fi
if [ ! -d "$REPO/.git" ]; then
  log "cloning $URL"
  git clone "$URL" "$REPO"
fi
log "fetch $BRANCH"
git -C "$REPO" fetch --prune "$URL" "+refs/heads/${BRANCH}:refs/remotes/origin/${BRANCH}"
if ! git -C "$REPO" cat-file -e "${SHA}^{commit}" 2>/dev/null; then
  log "fetch commit $SHA"
  git -C "$REPO" fetch "$URL" "$SHA"
fi
git -C "$REPO" cat-file -e "${SHA}^{commit}" >/dev/null
if [ -e "$WT/.git" ]; then
  git -C "$WT" -c submodule.recurse=false checkout -q --detach -f "$SHA"
else
  if [ -e "$WT" ]; then
    log "$WT exists and is not a git worktree"
    exit 1
  fi
  git -C "$REPO" worktree prune
  git -C "$REPO" -c submodule.recurse=false worktree add -q --detach "$WT" "$SHA"
fi
log "submodules $WT"
git -C "$WT" submodule update --init --recursive --force
log "worktree $WT @ $(git -C "$WT" rev-parse HEAD)"
exec 7>&-
# fds stay open until this shell exits, including when the command fails,
# so the kernel drops the flock on the way out.
exec 8>>"$ROOT/locks/build1.lock"
exec 9>>"$ROOT/locks/build2.lock"
if flock -n 8; then
  log "lock build1"
elif flock -n 9; then
  log "lock build2"
else
  log "both build locks held; waiting for build1"
  flock 8
  log "lock build1"
fi
cd "$WT"
export VOLUM_LINUX_ROOT="$ROOT"
export PATH="$ROOT/tools/bin:${PATH}"
cmd=$(printf '%s' '@@B64@@' | base64 -d)
set +e
bash --noprofile --norc -c "$cmd"
rc=$?
exit "$rc"
'@
$runner = $runner.Replace("@@SLUG@@", $slug).Replace("@@BRANCH@@", $Branch).Replace("@@SHA@@", $Sha).Replace("@@B64@@", $b64)
Invoke-BoxBash $runner
if ($script:BoxCode -eq 255) {
  Write-Host "ssh unreachable: $BoxHost (connection lost during the command)"
  exit 2
}
exit $script:BoxCode
