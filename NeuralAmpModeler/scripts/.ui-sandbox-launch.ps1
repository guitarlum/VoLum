# Screenshot / UI-drive sandbox (docs/screenshot-recipes.md).
# Launches the already-built standalone with LOCALAPPDATA redirected to a scratch
# sandbox, so a capture pass cannot touch the real %LOCALAPPDATA%\VoLum library.
param(
  [switch] $Reseed,
  # Seed the library with no MIDI assignments at all, for the PLAY empty state.
  [switch] $EmptyMap,
  # Healthy three-Sound map for the docs shots: no deliberately broken slot.
  [switch] $DocMap
)
$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..\..")
$sandbox = Join-Path $env:TEMP "volum-ui-sandbox"
$lib = Join-Path $sandbox "VoLum"

Get-Process -Name VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

if ($Reseed) {
  if (Test-Path $lib) { Remove-Item $lib -Recurse -Force }
  New-Item -ItemType Directory -Path $lib -Force | Out-Null
  Copy-Item (Join-Path $repo "docs\screenshot-seed\volum-settings.json") "$lib\" -Force
  Copy-Item (Join-Path $repo "docs\screenshot-seed\volum-dual-amp-settings.json") "$lib\" -Force
  Copy-Item (Join-Path $repo "docs\screenshot-seed\content") "$lib\" -Recurse -Force

  # Seed the MIDI Sound map: four assignments a player would recognise plus one
  # slot whose Sound is gone, so the MIDI tab and the PLAY rail both show the red
  # "Invalid slot" state without hand-editing the UI. All five sit on the MIDI
  # tab's first footswitch bank (programs 0-7). Also point one preset at the
  # custom IR and the custom pedal so Pack export has a closure to show.
  $jsonPath = Join-Path $lib "content\volum-content.json"
  $j = Get-Content $jsonPath -Raw | ConvertFrom-Json
  $map = if ($EmptyMap) { @() } elseif ($DocMap) { @(
    [pscustomobject]@{ slot = 0; ampId = "factory:13"; presetId = "preset_402e30dc" },
    [pscustomobject]@{ slot = 1; ampId = "factory:13"; presetId = "preset_b36c739e" },
    [pscustomobject]@{ slot = 2; ampId = "factory:14"; presetId = "factory:14:v1" }
  ) } else { @(
    [pscustomobject]@{ slot = 0; ampId = "factory:13"; presetId = "preset_402e30dc" },
    [pscustomobject]@{ slot = 1; ampId = "factory:13"; presetId = "preset_17a37691" },
    [pscustomobject]@{ slot = 2; ampId = "factory:0";  presetId = "factory:0:v1" },
    [pscustomobject]@{ slot = 4; ampId = "factory:13"; presetId = "preset_b36c739e" },
    [pscustomobject]@{ slot = 6; ampId = "factory:9";  presetId = "preset_gone_forever" }
  ) }
  $j | Add-Member -NotePropertyName midiSoundMap -NotePropertyValue $map -Force
  foreach ($pr in $j.presetBanks.'factory:13') {
    if ($pr.name -eq "Crunch Rhythm") {
      $pr.settings.activeIrId = "ir_271f4894"
      $pr.settings.preNam1Capture = 64
    }
  }
  $j | ConvertTo-Json -Depth 100 | Set-Content $jsonPath -Encoding UTF8
  Write-Host "Reseeded $lib"
}

$env:LOCALAPPDATA = $sandbox
# Answers ui-drive.ps1 -Locked capture requests (VoLumSelfCapture.h).
$env:VOLUM_SELF_CAPTURE_DIR = Join-Path $sandbox "capture"
New-Item -ItemType Directory -Path $env:VOLUM_SELF_CAPTURE_DIR -Force | Out-Null
$proc = Start-Process (Join-Path $repo "NeuralAmpModeler\build-win\app\x64\Release\VoLum.exe") -PassThru
# Pin the pid: another agent on this machine launches VoLum against the real
# %LOCALAPPDATA% library, and driving that one by clicking would edit the user's
# actual content store.
Set-Content (Join-Path $sandbox "pid.txt") $proc.Id -Encoding ASCII
Write-Host "Launched pid $($proc.Id) with LOCALAPPDATA=$sandbox"
