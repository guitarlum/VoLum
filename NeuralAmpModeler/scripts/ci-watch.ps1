# Dispatch and/or follow a GitHub Actions CI run, reporting job and step state.
#
# `gh run watch` tails a single run but says little about *which* step failed,
# and CI only auto-triggers on `dev`/`main` - a release or feature branch needs
# an explicit `workflow_dispatch`. This wraps both, and on failure prints the
# failing job/step plus the tail of its log, which is the only part worth
# reading first.
#
# Examples:
#   pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -Dispatch -Ref release/1.2.1
#   pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -RunId 30286593044
#   pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -Ref dev -NoWait

[CmdletBinding()]
param(
  [string]$Ref,
  [string]$RunId,
  [string]$Repo = "guitarlum/VoLum",
  [string]$Workflow = "ci.yml",
  [switch]$Dispatch,
  [switch]$NoWait,
  [int]$PollSeconds = 60,
  [int]$TimeoutMinutes = 120
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command gh -ErrorAction SilentlyContinue))
{
  Write-Error "gh CLI not found. Install GitHub CLI to use this script."
}

function Get-Run([string]$id)
{
  $json = & gh run view $id --repo $Repo --json status,conclusion,url,headSha,jobs
  if ($LASTEXITCODE -ne 0) { Write-Error "gh run view failed for run $id" }
  return $json | ConvertFrom-Json
}

if (-not $Ref -and -not $RunId)
{
  $Ref = (& git rev-parse --abbrev-ref HEAD).Trim()
  Write-Host "No -Ref given, using current branch: $Ref"
}

if ($Dispatch)
{
  & gh workflow run $Workflow --repo $Repo --ref $Ref
  if ($LASTEXITCODE -ne 0) { Write-Error "workflow dispatch failed" }
  # The run does not appear in the list immediately.
  Start-Sleep -Seconds 10
}

if (-not $RunId)
{
  $list = & gh run list --repo $Repo --branch $Ref --limit 1 --json databaseId | ConvertFrom-Json
  if (-not $list -or $list.Count -eq 0) { Write-Error "No CI runs found for ref '$Ref'." }
  $RunId = $list[0].databaseId
}

$run = Get-Run $RunId
Write-Host "Run $RunId  $($run.url)"
Write-Host "Commit $($run.headSha)"

$deadline = (Get-Date).AddMinutes($TimeoutMinutes)
while ($true)
{
  $run = Get-Run $RunId
  $stamp = (Get-Date).ToString("HH:mm:ss")
  Write-Host ""
  Write-Host "[$stamp] run: $($run.status) $($run.conclusion)"

  foreach ($job in $run.jobs)
  {
    $line = "  {0,-62} {1} {2}" -f $job.name, $job.status, $job.conclusion
    $color = switch ($job.conclusion)
    {
      "success" { "Green" }
      "failure" { "Red" }
      default { "Gray" }
    }
    Write-Host $line -ForegroundColor $color

    foreach ($step in $job.steps | Where-Object { $_.conclusion -eq "failure" })
    {
      Write-Host "      FAILED STEP: $($step.name)" -ForegroundColor Red
    }
    if ($job.status -ne "completed")
    {
      $current = $job.steps | Where-Object { $_.status -ne "completed" } | Select-Object -First 1
      if ($current) { Write-Host "      running: $($current.name)" -ForegroundColor DarkGray }
    }
  }

  if ($run.status -eq "completed") { break }
  if ($NoWait) { return }
  if ((Get-Date) -gt $deadline)
  {
    Write-Warning "Timed out after $TimeoutMinutes minutes; run is still in progress."
    return
  }
  Start-Sleep -Seconds $PollSeconds
}

if ($run.conclusion -eq "success")
{
  Write-Host ""
  Write-Host "CI green. Artifacts:" -ForegroundColor Green
  & gh run view $RunId --repo $Repo --json artifacts `
    --jq '.artifacts[]? | "  " + .name' 2>$null
  exit 0
}

Write-Host ""
Write-Host "CI failed. Log tail of failing steps:" -ForegroundColor Red
& gh run view $RunId --repo $Repo --log-failed 2>&1 | Select-Object -Last 40
exit 1
