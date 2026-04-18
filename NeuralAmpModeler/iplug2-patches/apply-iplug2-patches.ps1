# Apply VoLum's iPlug2 patches to the iPlug2 submodule working tree.
# Idempotent: a second invocation is a no-op.
# Run from anywhere; paths are resolved relative to this script.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path
$iplugDir = Join-Path $repoRoot "iPlug2"

if (-not (Test-Path $iplugDir)) {
  Write-Error "iPlug2 submodule not found at $iplugDir. Run 'git submodule update --init --recursive' first."
}

$patches = Get-ChildItem -Path $here -Filter "*.patch" | Sort-Object Name
if ($patches.Count -eq 0) {
  Write-Host "No patches in $here; nothing to apply."
  exit 0
}

function Invoke-GitApply {
  # Returns the git exit code. Suppresses both stdout and stderr because git
  # writes informational lines to stderr ("Checking patch ...") that
  # PowerShell would otherwise treat as errors under EAP=Stop.
  param(
    [Parameter(Mandatory=$true)] [string] $RepoDir,
    [Parameter(Mandatory=$true)] [string[]] $GitArgs
  )
  $allArgs = @("-C", $RepoDir, "apply") + $GitArgs
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = "git"
  $psi.Arguments = ($allArgs | ForEach-Object {
    if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
  }) -join ' '
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.UseShellExecute = $false
  $psi.CreateNoWindow = $true
  $proc = [System.Diagnostics.Process]::Start($psi)
  $null = $proc.StandardOutput.ReadToEnd()
  $null = $proc.StandardError.ReadToEnd()
  $proc.WaitForExit()
  return $proc.ExitCode
}

foreach ($patch in $patches) {
  $patchPath = $patch.FullName
  Write-Host "Checking $($patch.Name)..."

  # If the patch is already applied, --reverse --check succeeds.
  $reverseExit = Invoke-GitApply -RepoDir $iplugDir -GitArgs @("--reverse", "--check", $patchPath)
  if ($reverseExit -eq 0) {
    Write-Host "  already applied, skipping."
    continue
  }

  # Otherwise it should apply cleanly forward.
  $checkExit = Invoke-GitApply -RepoDir $iplugDir -GitArgs @("--check", $patchPath)
  if ($checkExit -ne 0) {
    Write-Error "Patch $($patch.Name) does not apply cleanly to iPlug2 (and is not already applied). Aborting."
  }

  $applyExit = Invoke-GitApply -RepoDir $iplugDir -GitArgs @("--whitespace=nowarn", $patchPath)
  if ($applyExit -ne 0) {
    Write-Error "Failed to apply $($patch.Name)."
  }
  Write-Host "  applied."
}
