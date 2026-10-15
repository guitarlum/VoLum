# Predicate for .scratch/1.3.0-ci-e2e (see loop.md). Read-only: never builds or launches.
$ErrorActionPreference = "Continue"
$repo = "guitarlum/VoLum"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$fail = New-Object System.Collections.Generic.List[string]
$jobs = @("Windows installer upgrade", "macOS installer upgrade", "macOS standalone e2e")

# The owner's real library must never change.
$baseline = "2026-10-09 20:24:21.703"
$lib = Join-Path $env:LOCALAPPDATA "VoLum"
if (Test-Path $lib) {
  $lwt = (Get-ChildItem $lib -Recurse -File | Measure-Object LastWriteTime -Maximum).Maximum.ToString("yyyy-MM-dd HH:mm:ss.fff")
  if ($lwt -ne $baseline) { $fail.Add("real library changed: newest file $lwt, baseline $baseline") }
}

# gh times out on a slow link; an empty answer must not read as a wrong setting.
function Invoke-GhApi([string[]]$GhArgs) {
  for ($i = 0; $i -lt 3; $i++) {
    $out = & gh api @GhArgs 2>$null
    if ($LASTEXITCODE -eq 0) { return $out }
    Start-Sleep -Seconds 5
  }
  throw "gh api $($GhArgs -join ' ') failed three times"
}

# GitHub Pages: workflow source, and the github-pages environment admits release tags.
$pages = Invoke-GhApi @("repos/$repo/pages", "--jq", ".build_type")
if ($pages -ne "workflow") { $fail.Add("GitHub Pages source is '$pages', expected 'workflow'") }
$tagRules = Invoke-GhApi @("repos/$repo/environments/github-pages/deployment-branch-policies", "--jq", '.branch_policies[] | select(.type == "tag") | .name')
if (-not ($tagRules -match '^v')) { $fail.Add("github-pages environment has no tag rule like 'v*' (owner setting): a release deploy would be rejected") }

# The three jobs exist on origin/dev and are green on the newest dev CI run.
git -C $root fetch -q https://github.com/guitarlum/VoLum.git dev:refs/remotes/origin/dev 2>$null
$ci = git -C $root show refs/remotes/origin/dev:.github/workflows/ci.yml
foreach ($j in $jobs) { if (-not ($ci -match [regex]::Escape("name: $j"))) { $fail.Add("ci.yml on origin/dev has no job '$j'") } }
$devSha = (git -C $root rev-parse refs/remotes/origin/dev).Trim()
$run = gh run list --repo $repo --branch dev --workflow ci.yml --limit 1 --json databaseId,headSha,status,conclusion | ConvertFrom-Json
if (-not $run -or $run[0].headSha -ne $devSha) { $fail.Add("no ci.yml run yet for origin/dev $devSha") }
elseif ($run[0].status -ne "completed" -or $run[0].conclusion -ne "success") { $fail.Add("dev CI $($run[0].databaseId) is $($run[0].status)/$($run[0].conclusion)") }
else {
  $id = $run[0].databaseId
  $view = gh run view $id --repo $repo --json jobs | ConvertFrom-Json
  foreach ($j in $jobs) {
    $job = $view.jobs | Where-Object { $_.name -eq $j }
    if (-not $job) { $fail.Add("dev CI $id has no job '$j'"); continue }
    if ($job.conclusion -ne "success") { $fail.Add("dev CI $id job '$j' is $($job.conclusion)"); continue }
    $log = gh api "repos/$repo/actions/jobs/$($job.databaseId)/logs" 2>$null | Out-String
    if ($log -notmatch "NEGATIVE CONTROL OK") { $fail.Add("job '$j' log lacks NEGATIVE CONTROL OK") }
    if ($log -notmatch "ALL PASS \d+ checks") { $fail.Add("job '$j' log lacks ALL PASS <n> checks") }
  }
  $arts = gh api "repos/$repo/actions/runs/$id/artifacts" --jq '.artifacts[].name' 2>$null
  if (-not ($arts -contains "mac-e2e-shots")) { $fail.Add("dev CI $id has no mac-e2e-shots artifact") }
}

# No ticket left open.
Get-ChildItem (Join-Path $PSScriptRoot "issues") -Filter *.md | ForEach-Object {
  if ((Get-Content $_.FullName -Raw) -match 'Status:\s*(ready-for-agent|claimed)') { $fail.Add("ticket $($_.Name) still open") }
}

if ($fail.Count) { $fail | ForEach-Object { Write-Host "FAIL  $_" -ForegroundColor Red }; exit 1 }
Write-Host "VERIFY OK (dev $devSha)" -ForegroundColor Green
exit 0
