# Predicate for the linux-support loop (spec.md and loop.md next to this file).
# Exit 0 only when, for the pushed head of feature/linux-support in $Worktree:
#   - clang-format reports no replacements on superproject C++ files changed against origin/dev,
#   - the Windows suite is green (run-tests-win.ps1),
#   - the box gates are green (scripts/linux/verify-linux.sh via scripts/linux/box.ps1),
#   - the ci.yml run for that exact head sha is green on every job.
# -Final adds: every ticket resolved, and the run has the Linux tests, plugin, e2e and package jobs.
# Exit 3 when the box is busy with localai-platform work (BOX_BUSY_LOCALAI): not met, retry later.
param(
  [string] $Worktree = "C:\dev\VoLum-linux",
  [string] $Branch = "feature/linux-support",
  [switch] $Final
)

$ErrorActionPreference = "Stop"
$effort = Join-Path $Worktree ".scratch\linux-support"
function Fail([string] $msg, [int] $code = 1) { Write-Output "FAIL: $msg"; Write-Output "predicate: NOT MET"; exit $code }

Push-Location $Worktree
try {
  $head = (& git rev-parse HEAD).Trim()
  & git fetch -q https://github.com/guitarlum/VoLum.git "${Branch}:refs/remotes/origin/${Branch}" 2>$null
  $remote = (& git rev-parse "refs/remotes/origin/$Branch").Trim()
  if ($head -ne $remote) { Fail "HEAD $head is not the pushed head of $Branch ($remote)" }
  if ((& git status --porcelain --untracked-files=no) -ne $null) { Fail "tracked changes not committed" }
  Write-Output "head: $head"

  if ($Final) {
    $open = @(Get-ChildItem (Join-Path $effort "issues") -Filter *.md | Where-Object {
        (Select-String -Path $_.FullName -Pattern '^Status:\s*(\S+)' | Select-Object -First 1).Matches[0].Groups[1].Value -ne "resolved" })
    if ($open.Count -gt 0) { Fail ("unresolved tickets: " + (($open | ForEach-Object Name) -join ", ")) }
  }

  $clang = Get-Command clang-format -ErrorAction SilentlyContinue
  $clangPath = if ($clang) { $clang.Source } else {
    @(Get-ChildItem "C:\Program Files*\Microsoft Visual Studio\2022\*\VC\Tools\Llvm\x64\bin\clang-format.exe" -ErrorAction SilentlyContinue | ForEach-Object FullName) | Select-Object -First 1
  }
  $files = @(& git diff --name-only --diff-filter=ACMRT origin/dev -- "*.h" "*.cpp" | Where-Object { $_ -and $_ -notlike "NeuralAmpModeler/tests/third_party/*" })
  if ($files.Count -gt 0) {
    if (-not $clangPath) { Fail "clang-format not found" }
    $dirty = @($files | Where-Object { (& $clangPath --output-replacements-xml $_ 2>&1 | Out-String) -match "<replacement " })
    if ($dirty.Count -gt 0) { Fail ("clang-format replacements: " + ($dirty -join ", ")) }
    Write-Output "clang-format: clean ($($files.Count) files)"
  }

  Write-Output "== run-tests-win.ps1"
  $global:LASTEXITCODE = 0
  & pwsh -NoProfile -File (Join-Path $Worktree "NeuralAmpModeler\scripts\run-tests-win.ps1")
  if ($LASTEXITCODE) { Fail "run-tests-win.ps1 exit $LASTEXITCODE" }

  $box = Join-Path $Worktree "NeuralAmpModeler\scripts\linux\box.ps1"
  if (-not (Test-Path $box)) { Fail "scripts/linux/box.ps1 missing (ticket 01)" }
  $linuxCmd = if ($Final) { "bash NeuralAmpModeler/scripts/linux/verify-linux.sh --final" } else { "bash NeuralAmpModeler/scripts/linux/verify-linux.sh" }
  Write-Output "== box: $linuxCmd"
  $global:LASTEXITCODE = 0
  & pwsh -NoProfile -File $box -Branch $Branch -Sha $head -Command $linuxCmd
  if ($LASTEXITCODE -eq 3) { Fail "BOX_BUSY_LOCALAI: box gates deferred" 3 }
  if ($LASTEXITCODE) { Fail "box verify-linux.sh exit $LASTEXITCODE" }

  Write-Output "== CI for $head"
  $runs = & gh run list --repo guitarlum/VoLum --workflow ci.yml --branch $Branch --limit 20 --json databaseId,headSha,status,conclusion | ConvertFrom-Json
  $run = @($runs | Where-Object { $_.headSha -eq $head }) | Select-Object -First 1
  $watch = Join-Path $Worktree "NeuralAmpModeler\scripts\ci-watch.ps1"
  $global:LASTEXITCODE = 0
  if (-not $run) { & pwsh -NoProfile -File $watch -Ref $Branch -Dispatch }
  elseif ($run.status -ne "completed") { & pwsh -NoProfile -File $watch -RunId $run.databaseId }
  elseif ($run.conclusion -ne "success") { Fail "CI run $($run.databaseId) on $head concluded $($run.conclusion)" }
  if ($LASTEXITCODE) { Fail "ci-watch exit $LASTEXITCODE" }

  $runs = & gh run list --repo guitarlum/VoLum --workflow ci.yml --branch $Branch --limit 20 --json databaseId,headSha,status,conclusion | ConvertFrom-Json
  $run = @($runs | Where-Object { $_.headSha -eq $head -and $_.status -eq "completed" }) | Select-Object -First 1
  if (-not $run -or $run.conclusion -ne "success") { Fail "no green CI run for $head" }
  $jobs = (& gh run view $run.databaseId --repo guitarlum/VoLum --json jobs | ConvertFrom-Json).jobs
  $bad = @($jobs | Where-Object { $_.conclusion -notin @("success", "skipped") })
  if ($bad.Count -gt 0) { Fail ("CI jobs not green: " + (($bad | ForEach-Object name) -join ", ")) }
  $skippedMac = @($jobs | Where-Object { $_.name -match '^macOS' -and $_.conclusion -eq "skipped" })
  if ($skippedMac.Count -gt 0) { Fail "macOS jobs skipped (Windows-only run does not count)" }
  if ($Final) {
    foreach ($need in @("Linux tests", "Linux plugin", "Linux standalone e2e", "Linux package")) {
      if (-not @($jobs | Where-Object { $_.name -like "$need*" -and $_.conclusion -eq "success" })) { Fail "CI job '$need' missing or not green" }
    }
  }
  Write-Output "CI: run $($run.databaseId) green ($($jobs.Count) jobs)"
}
finally { Pop-Location }

Write-Output "predicate: MET"
exit 0
