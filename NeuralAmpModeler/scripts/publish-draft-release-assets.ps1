# Refresh a draft release from CI artifacts.
param(
  [Parameter(Mandatory = $true)]
  [long] $RunId,
  [string] $ReleaseTag = "v1.0.1",
  [string] $Repo = "guitarlum/VoLum",
  [string] $WorkDir = "",
  [switch] $IncludePdbs
)

$ErrorActionPreference = "Stop"

if (-not $WorkDir) {
  $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
  $WorkDir = Join-Path $repoRoot "NeuralAmpModeler\build-release-assets\$RunId"
}

function Fail-AssetPublish {
  param([string] $Message)
  throw "Release asset publish failed: $Message"
}

function Get-GitHubToken {
  $token = gh auth token
  if ($LASTEXITCODE -ne 0 -or -not $token) {
    Fail-AssetPublish "gh auth token failed"
  }
  return $token.Trim()
}

function Get-Artifact {
  param(
    [object[]] $Artifacts,
    [string] $Name
  )

  $artifact = $Artifacts | Where-Object { $_.name -eq $Name -and -not $_.expired } | Select-Object -First 1
  if (-not $artifact) {
    Fail-AssetPublish "artifact '$Name' was not found or has expired"
  }
  return $artifact
}

function Download-ArtifactZip {
  param(
    [object] $Artifact,
    [string] $OutFile,
    [hashtable] $Headers
  )

  $uri = "https://api.github.com/repos/$Repo/actions/artifacts/$($Artifact.id)/zip"
  Write-Host "Downloading artifact $($Artifact.name) ($($Artifact.size_in_bytes) bytes)..."
  Invoke-WebRequest -Uri $uri -Headers $Headers -OutFile $OutFile
  if (-not (Test-Path $OutFile) -or (Get-Item $OutFile).Length -le 0) {
    Fail-AssetPublish "downloaded artifact is empty: $OutFile"
  }
}

$version = $ReleaseTag.TrimStart("v")
$uploadDir = Join-Path $WorkDir "release-upload"
$winZip = Join-Path $WorkDir "VoLum-win.zip"
$macZip = Join-Path $WorkDir "VoLum-mac.zip"
$winDir = Join-Path $WorkDir "VoLum-win"
$macDir = Join-Path $WorkDir "VoLum-mac"

if (Test-Path $WorkDir) { Remove-Item $WorkDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $uploadDir | Out-Null

$release = gh release view $ReleaseTag --repo $Repo --json isDraft,tagName | ConvertFrom-Json
if (-not $release.isDraft) {
  Fail-AssetPublish "$ReleaseTag is not a draft release"
}

$run = gh run view $RunId --repo $Repo --json conclusion,status,headSha | ConvertFrom-Json
if ($run.status -ne "completed" -or $run.conclusion -ne "success") {
  Fail-AssetPublish "CI run $RunId is $($run.status)/$($run.conclusion), not completed/success"
}

$artifactResponse = gh api "repos/$Repo/actions/runs/$RunId/artifacts" | ConvertFrom-Json
$artifacts = @($artifactResponse.artifacts)
$winArtifact = Get-Artifact $artifacts "VoLum-win"
$macArtifact = Get-Artifact $artifacts "VoLum-mac"

$headers = @{
  Authorization = "Bearer $(Get-GitHubToken)"
  Accept = "application/vnd.github+json"
  "X-GitHub-Api-Version" = "2022-11-28"
}

Download-ArtifactZip $winArtifact $winZip $headers
Download-ArtifactZip $macArtifact $macZip $headers

Expand-Archive -Force $winZip $winDir
Expand-Archive -Force $macZip $macDir

$expectedCopies = @(
  @{ From = Join-Path $winDir "installer\VoLum-Setup.exe"; To = Join-Path $uploadDir "VoLum-v$version-windows-setup.exe" },
  @{ From = Join-Path $winDir "out\VoLum-v$version-win.zip"; To = Join-Path $uploadDir "VoLum-v$version-windows-portable.zip" },
  @{ From = Join-Path $macDir "VoLum-v$version-mac-app.dmg"; To = Join-Path $uploadDir "VoLum-v$version-macos-standalone.dmg" },
  @{ From = Join-Path $macDir "VoLum-v$version-mac.dmg"; To = Join-Path $uploadDir "VoLum-v$version-macos-installer.dmg" },
  @{ From = Join-Path $macDir "VoLum-v$version-mac-vst3.zip"; To = Join-Path $uploadDir "VoLum-v$version-macos-vst3.zip" },
  @{ From = Join-Path $macDir "VoLum-v$version-mac-component.zip"; To = Join-Path $uploadDir "VoLum-v$version-macos-component.zip" }
)

if ($IncludePdbs) {
  $expectedCopies += @{ From = Join-Path $winDir "out\VoLum-v$version-win-pdbs.zip"; To = Join-Path $uploadDir "VoLum-v$version-win-pdbs.zip" }
}

foreach ($copy in $expectedCopies) {
  if (-not (Test-Path $copy.From)) {
    Fail-AssetPublish "expected artifact file missing: $($copy.From)"
  }
  Copy-Item $copy.From $copy.To
}

$uploadFiles = @(Get-ChildItem $uploadDir -File | Sort-Object Name | ForEach-Object { $_.FullName })
Write-Host "Uploading $($uploadFiles.Count) files to draft $ReleaseTag..."
& gh release upload $ReleaseTag @uploadFiles --repo $Repo --clobber
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$uploaded = gh release view $ReleaseTag --repo $Repo --json assets | ConvertFrom-Json
$uploadedNames = @($uploaded.assets | ForEach-Object { $_.name })
foreach ($file in $uploadFiles) {
  $name = Split-Path -Leaf $file
  if ($uploadedNames -notcontains $name) {
    Fail-AssetPublish "uploaded asset missing after release refresh: $name"
  }
}

Write-Host "Draft $ReleaseTag assets refreshed from CI run $RunId."
