# Verify that a draft release is ready to publish.
param(
  [string] $Tag = "v1.0.1",
  [string] $Branch = "main",
  [string] $Repo = "guitarlum/VoLum",
  [string] $RemoteUrl = "https://github.com/guitarlum/VoLum.git",
  [string] $ExpectedCommit = "",
  [switch] $AllowPublished
)

$ErrorActionPreference = "Stop"

function Fail-ReleaseReady {
  param([string] $Message)
  throw "Release readiness failed: $Message"
}

if (-not $ExpectedCommit) {
  $ExpectedCommit = (git rev-parse HEAD).Trim()
}

$localTag = (git rev-parse $Tag).Trim()
$remoteTagLine = (git ls-remote --tags $RemoteUrl "refs/tags/$Tag").Trim()
if (-not $remoteTagLine) {
  Fail-ReleaseReady "remote tag $Tag was not found at $RemoteUrl"
}
$remoteTag = ($remoteTagLine -split "\s+")[0]

if ($localTag -ne $ExpectedCommit) {
  Fail-ReleaseReady "local tag $Tag points to $localTag, expected $ExpectedCommit"
}
if ($remoteTag -ne $ExpectedCommit) {
  Fail-ReleaseReady "remote tag $Tag points to $remoteTag, expected $ExpectedCommit"
}

$run = gh run list --repo $Repo --branch $Branch --limit 1 --json databaseId,conclusion,status,headSha |
  ConvertFrom-Json |
  Select-Object -First 1
if (-not $run) {
  Fail-ReleaseReady "no CI run found for branch $Branch"
}
if ($run.status -ne "completed" -or $run.conclusion -ne "success") {
  Fail-ReleaseReady "latest $Branch CI is $($run.status)/$($run.conclusion) (run $($run.databaseId))"
}
if ($run.headSha -ne $ExpectedCommit) {
  Fail-ReleaseReady "latest $Branch CI tested $($run.headSha), expected $ExpectedCommit"
}

$release = gh release view $Tag --repo $Repo --json isDraft,isPrerelease,assets,body |
  ConvertFrom-Json
if (-not $release.isDraft -and -not $AllowPublished) {
  Fail-ReleaseReady "$Tag is not a draft release"
}
if ($release.isPrerelease) {
  Fail-ReleaseReady "$Tag is marked prerelease"
}
if (-not $release.body -or $release.body.Trim().Length -lt 80) {
  Fail-ReleaseReady "$Tag release notes look empty or incomplete"
}

$expectedAssets = @(
  "VoLum-$Tag-macos-component.zip",
  "VoLum-$Tag-macos-installer.dmg",
  "VoLum-$Tag-macos-standalone.dmg",
  "VoLum-$Tag-macos-vst3.zip",
  "VoLum-$Tag-windows-portable.zip",
  "VoLum-$Tag-windows-setup.exe"
)

$assetNames = @($release.assets | ForEach-Object { $_.name })
foreach ($asset in $expectedAssets) {
  if ($assetNames -notcontains $asset) {
    Fail-ReleaseReady "missing release asset: $asset"
  }
}
if ($assetNames.Count -ne $expectedAssets.Count) {
  Fail-ReleaseReady "expected $($expectedAssets.Count) assets, found $($assetNames.Count): $($assetNames -join ', ')"
}

if ($release.isDraft) {
  Write-Host "Release $Tag is ready to publish at $ExpectedCommit (CI run $($run.databaseId))."
}
else {
  Write-Host "Release $Tag is published and matches $ExpectedCommit (CI run $($run.databaseId))."
}
