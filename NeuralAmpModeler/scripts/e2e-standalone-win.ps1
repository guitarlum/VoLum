# End-to-end scenarios for the VoLum standalone, driven from a sandboxed
# LOCALAPPDATA so real user state is never touched.
#
# Every VoLum state path (volum-settings.json, content/volum-content.json,
# settings.ini, volum.log) is derived from %LOCALAPPDATA% - see VoLumPaths.h - so
# pointing that one variable at a temp directory gives a fully isolated instance.
# Each scenario seeds a starting state, launches the app, closes it gracefully so
# the shutdown save runs, then asserts on what landed on disk.
#
# These cover the class of bug this release was about: state that is correct in
# memory but lost, reset, or silently rewritten across a restart. Local-only -
# CI runners have no audio device, so the app cannot open a stream there.
#
#   pwsh NeuralAmpModeler/scripts/e2e-standalone-win.ps1
#   pwsh NeuralAmpModeler/scripts/e2e-standalone-win.ps1 -Scenario upgrade -KeepSandbox

[CmdletBinding()]
param(
  [ValidateSet("all", "fresh", "roundtrip", "custom", "brokenrefs", "future", "upgrade", "presets", "corrupt",
    "samplerate", "savedialog", "pack")]
  [string]$Scenario = "all",
  [string]$Exe,
  # Seed state for the round-trip and upgrade scenarios. Defaults to a copy of the
  # live library, which is the only place real .nam/.wav payloads exist on a dev box.
  [string]$SeedFrom,
  [int]$LaunchTimeoutSec = 60,
  [switch]$KeepSandbox,
  # pack scenario: also save the export/import overlays as <ShotsDir>\06-*.png.
  [string]$ShotsDir
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = (Resolve-Path (Join-Path $here "..")).Path

if (-not $Exe) { $Exe = Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe" }
if (-not (Test-Path $Exe)) {
  Write-Error "VoLum.exe not found at $Exe. Build it first: pwsh $here\run-app-win.ps1"
}
if (-not $SeedFrom) { $SeedFrom = Join-Path $env:LOCALAPPDATA "VoLum" }

# The schema this build writes, so an upgrade is checked against the real target
# rather than a number that goes stale on the next bump.
$storeHeader = Get-Content (Join-Path $slnDir "VoLumContentStore.h") -Raw
if ($storeHeader -notmatch 'kContentSchemaVersion\s*=\s*(\d+)\s*;') {
  Write-Error "kContentSchemaVersion not found in VoLumContentStore.h"
}
$script:ContentSchemaVersion = [int]$Matches[1]

$script:Failures = @()
$script:Checks = 0

function Assert-True {
  param([string]$What, [bool]$Ok, [string]$Detail = "")
  $script:Checks++
  if ($Ok) {
    Write-Host ("  PASS  {0}" -f $What) -ForegroundColor Green
  }
  else {
    $script:Failures += $What
    Write-Host ("  FAIL  {0}{1}" -f $What, $(if ($Detail) { " - $Detail" } else { "" })) -ForegroundColor Red
  }
}

function Assert-Equal {
  param([string]$What, $Expected, $Actual)
  Assert-True $What ($Expected -eq $Actual) ("expected '{0}', got '{1}'" -f $Expected, $Actual)
}

function New-Sandbox {
  param([string]$Name)
  $dir = Join-Path ([IO.Path]::GetTempPath()) ("volum-e2e\" + $Name)
  if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
  New-Item -ItemType Directory -Path (Join-Path $dir "VoLum") -Force | Out-Null
  return $dir
}

# Copy the seed library into a sandbox. The seed's settings.ini is replaced: see
# Write-SandboxAudioConfig.
function Copy-SeedState {
  param([string]$SandboxRoot)
  if (-not (Test-Path $SeedFrom)) {
    throw "Seed state not found at $SeedFrom. Pass -SeedFrom, or launch VoLum once to create it."
  }
  Copy-Item (Join-Path $SeedFrom "*") (Join-Path $SandboxRoot "VoLum") -Recurse -Force
  # A log from the seed would make the fresh-log assertions meaningless.
  Remove-Item (Join-Path $SandboxRoot "VoLum\volum.log") -Force -ErrorAction SilentlyContinue
  Write-SandboxAudioConfig $SandboxRoot
}

# DirectSound on the default devices. The seed's own interface (an ASIO driver on a
# dev box) is often absent - unplugged, or unavailable while the session is locked -
# and any failed open puts a modal "Audio Error" box in front of the window, which
# blocks the graceful close every scenario asserts on.
function Write-SandboxAudioConfig {
  param([string]$SandboxRoot)
  @("[audio]", "driver=0", "indev=Default Device", "outdev=Default Device", "in1=1", "out1=1", "out2=2",
    "buffer=512", "sr=48000") | Set-Content (Join-Path $SandboxRoot "VoLum\settings.ini") -Encoding ASCII
}

# Launch VoLum against a sandboxed LOCALAPPDATA and close it the way a user would.
# CloseMainWindow posts WM_CLOSE, which runs the normal shutdown path - the one that
# saves settings. Killing the process instead would skip the save and make every
# persistence assertion below vacuous, so a hard kill is reported as a failure - and
# so is any non-zero exit, including the watchdog's own.
function Invoke-VoLumRun {
  # -Drive runs against the live window after the settle, before the graceful close.
  # -AcknowledgeNotice presses OK, as a user would, on a startup message box whose
  # "caption: text" matches it; the text lands in the result's `notices`. Any other
  # box is left up, so a launch it blocks still fails to open.
  # -Environment adds variables for this launch only (the Pack dialog hooks, self-capture).
  param([string]$SandboxRoot, [int]$SettleSec = 6, [scriptblock]$Drive, [string]$AcknowledgeNotice,
    [hashtable]$Environment = @{})

  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $Exe
  $psi.UseShellExecute = $false
  $psi.EnvironmentVariables["LOCALAPPDATA"] = $SandboxRoot
  foreach ($k in $Environment.Keys) { $psi.EnvironmentVariables[$k] = [string]$Environment[$k] }
  $proc = [System.Diagnostics.Process]::Start($psi)

  $result = [ordered]@{ started = $false; graceful = $false; exitCode = $null; drive = $null; notices = @() }
  $blocking = @{}
  $acknowledged = @{}
  $deadline = (Get-Date).AddSeconds($LaunchTimeoutSec)
  while ((Get-Date) -lt $deadline) {
    $proc.Refresh()
    if ($proc.HasExited) { break }
    if ($proc.MainWindowHandle -ne 0) { break }
    foreach ($box in [VoLumE2eUi]::OwnedDialogs($proc.Id)) {
      $text = [VoLumE2eUi]::DialogText($box)
      if ($AcknowledgeNotice -and $text -match $AcknowledgeNotice) {
        if (-not $acknowledged.ContainsKey($box)) {
          $acknowledged[$box] = $true
          $result.notices += $text
        }
        [VoLumE2eUi]::PressOk($box)
      }
      elseif (-not $blocking.ContainsKey($text)) {
        $blocking[$text] = $true
        Write-Host ("  startup blocked by a message box: {0}" -f $text) -ForegroundColor Yellow
      }
    }
    Start-Sleep -Milliseconds 250
  }

  if ($proc.HasExited) {
    $result.exitCode = $proc.ExitCode
    return $result
  }
  $proc.Refresh()
  $result.started = ($proc.MainWindowHandle -ne 0)

  # Let the editor finish opening, restoring, and running its idle save.
  Start-Sleep -Seconds $SettleSec

  if ($Drive) {
    $proc.Refresh()
    try { $result.drive = & $Drive $proc }
    catch { Write-Host ("  drive step failed: {0}" -f $_.Exception.Message) -ForegroundColor Yellow }
  }

  [void]$proc.CloseMainWindow()
  if ($proc.WaitForExit(20000)) {
    $result.exitCode = $proc.ExitCode
    # Exiting is not enough. The shutdown watchdog exists precisely because a wedged
    # driver could hang the exit, and it reports a distinct non-zero code when it
    # kills the process - see kVoLumShutdownWatchdogExitCode. Accepting any exit
    # would let the release's headline fix regress into "the watchdog covers for it",
    # which is a leaked audio device and lost settings on every quit.
    $result.graceful = ($proc.ExitCode -eq 0)
    if (-not $result.graceful) {
      Write-Host ("  exit code {0} on close" -f $proc.ExitCode) -ForegroundColor Yellow
    }
  }
  else {
    $boxes = @([VoLumE2eUi]::OwnedDialogs($proc.Id) | ForEach-Object { [VoLumE2eUi]::DialogText($_) })
    $proc.Kill()
    [void]$proc.WaitForExit(10000)
    Write-Host "  window did not close within 20 s; process killed" -ForegroundColor Yellow
    foreach ($b in $boxes) { Write-Host ("  close blocked by a message box: {0}" -f $b) -ForegroundColor Yellow }
  }
  return $result
}

function Read-Json {
  param([string]$Path)
  if (-not (Test-Path $Path)) { return $null }
  return Get-Content $Path -Raw -Encoding UTF8 | ConvertFrom-Json
}

function Get-ContentIds {
  param($Registry)
  return [ordered]@{
    amps   = @($Registry.customAmps  | ForEach-Object { $_.id }) | Sort-Object
    irs    = @($Registry.irLibrary   | ForEach-Object { $_.id }) | Sort-Object
    pedals = @($Registry.customPedals | ForEach-Object { $_.id }) | Sort-Object
    scenes = @($Registry.customScenes | ForEach-Object { $_.id }) | Sort-Object
    presetBanks = @($Registry.presetBanks.PSObject.Properties.Name) | Sort-Object
  }
}

function Assert-NoContentLoss {
  param($Before, $After)
  $b = Get-ContentIds $Before
  $a = Get-ContentIds $After
  foreach ($kind in @("amps", "irs", "pedals", "scenes", "presetBanks")) {
    $missing = @($b[$kind] | Where-Object { $a[$kind] -notcontains $_ })
    Assert-True ("no {0} lost across launch" -f $kind) ($missing.Count -eq 0) ("missing: " + ($missing -join ", "))
  }
}

# --------------------------------------------------------------------------
# Scenario: first launch on a machine that has never run VoLum
# --------------------------------------------------------------------------
function Test-Fresh {
  Write-Host "`n[fresh] first launch with no existing state" -ForegroundColor Cyan
  $sandbox = New-Sandbox "fresh"
  Write-SandboxAudioConfig $sandbox
  $root = Join-Path $sandbox "VoLum"

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opened a window" $run.started
  Assert-True "app closed gracefully" $run.graceful

  $settings = Read-Json (Join-Path $root "volum-settings.json")
  Assert-True "volum-settings.json written on first run" ($null -ne $settings)
  if ($settings) {
    Assert-True "settings carry a schema version" ($null -ne $settings.version)
    Assert-True "settings carry per-amp scenes" ($null -ne $settings.amps)
  }

  $log = Join-Path $root "volum.log"
  Assert-True "diagnostic log created" (Test-Path $log)
  if (Test-Path $log) {
    $text = Get-Content $log -Raw
    Assert-True "log records startup" ($text -match "startup")
    Assert-True "log records the audio configuration" ($text -match "reset|samplerate|sample rate|block")
  }
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: a populated library survives a launch untouched
# --------------------------------------------------------------------------
function Test-Roundtrip {
  Write-Host "`n[roundtrip] existing library survives launch and quit" -ForegroundColor Cyan
  $sandbox = New-Sandbox "roundtrip"
  Copy-SeedState $sandbox
  $root = Join-Path $sandbox "VoLum"
  $contentPath = Join-Path $root "content\volum-content.json"
  $settingsPath = Join-Path $root "volum-settings.json"

  $contentBefore = Read-Json $contentPath
  $settingsBefore = Read-Json $settingsPath
  if (-not $contentBefore) { throw "Seed library has no volum-content.json; nothing to round-trip." }

  # Assigned + invalid slots must survive shutdown save. Holes are omitted; an
  # invalid row keeps its program number.
  $contentBefore | Add-Member -NotePropertyName midiSoundMap -NotePropertyValue @(
    [pscustomobject]@{ slot = 4; ampId = "factory:0"; presetId = "factory:0:v1" },
    [pscustomobject]@{ slot = 17; ampId = "deleted-amp"; presetId = "gone-preset" }
  ) -Force
  $contentBefore | ConvertTo-Json -Depth 60 | Set-Content $contentPath -Encoding UTF8
  $contentBefore = Read-Json $contentPath

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opened a window" $run.started
  Assert-True "app closed gracefully" $run.graceful

  $contentAfter = Read-Json $contentPath
  Assert-True "content registry still parses" ($null -ne $contentAfter)
  if ($contentAfter) {
    Assert-NoContentLoss $contentBefore $contentAfter

    $mapKey = {
      param($reg)
      @($reg.midiSoundMap | Sort-Object { [int]$_.slot } | ForEach-Object { "{0}:{1}:{2}" -f $_.slot, $_.ampId, $_.presetId }) -join "|"
    }
    Assert-Equal "midiSoundMap survives quit including invalid slot" (& $mapKey $contentBefore) (& $mapKey $contentAfter)

    # IR shaping is the newest field in the format and the easiest to drop on a
    # rewrite, so compare it value by value rather than just checking the id.
    foreach ($ir in @($contentBefore.irLibrary)) {
      $now = @($contentAfter.irLibrary | Where-Object { $_.id -eq $ir.id })[0]
      if (-not $now) { continue }
      foreach ($field in @("lowCutHz", "highCutHz")) {
        Assert-Equal ("IR '{0}' keeps {1}" -f $ir.name, $field) $ir.$field $now.$field
      }
    }

    # Every capture keeps its gain stage: this is the "custom amp came back on
    # channel 1 instead of 5" bug, seen from the library side.
    foreach ($amp in @($contentBefore.customAmps)) {
      $now = @($contentAfter.customAmps | Where-Object { $_.id -eq $amp.id })[0]
      if (-not $now) { continue }
      $wasCh = (@($amp.files | ForEach-Object { "$($_.slot):$($_.channel)" }) | Sort-Object) -join ","
      $nowCh = (@($now.files | ForEach-Object { "$($_.slot):$($_.channel)" }) | Sort-Object) -join ","
      Assert-Equal ("amp '{0}' keeps its slot/channel map" -f $amp.name) $wasCh $nowCh
    }
  }

  $settingsAfter = Read-Json $settingsPath
  Assert-True "settings still parse" ($null -ne $settingsAfter)
  if ($settingsBefore -and $settingsAfter) {
    # Which custom amp and preset the editor comes back on. Getting this wrong is
    # the "VST3 reopen drops the custom amp" family of bugs.
    foreach ($field in @("volumCustomMainId", "volumActivePresetId", "lastAmpIdx")) {
      Assert-Equal ("selection '{0}' survives" -f $field) $settingsBefore.$field $settingsAfter.$field
    }

    # The focused lane's scene is what the user sees on relaunch. Speaker and
    # channel are the two fields the reopen bugs corrupted.
    foreach ($prop in @($settingsBefore.amps.PSObject.Properties)) {
      $was = $prop.Value
      $now = $settingsAfter.amps.$($prop.Name)
      if (-not $now) {
        Assert-True ("amp scene '{0}' survives" -f $prop.Name) $false
        continue
      }
      foreach ($field in @("speaker", "channel", "activeIrId", "supportActiveIrId")) {
        if ($null -eq $was.$field) { continue }
        Assert-Equal ("amp '{0}' keeps {1}" -f $prop.Name, $field) $was.$field $now.$field
      }
    }
  }
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: in-place upgrade from a 1.2.0-shaped library
# --------------------------------------------------------------------------
function Test-Upgrade {
  Write-Host "`n[upgrade] 1.2.0 schema-v2 library upgraded in place" -ForegroundColor Cyan
  $sandbox = New-Sandbox "upgrade"
  Copy-SeedState $sandbox
  $root = Join-Path $sandbox "VoLum"
  $contentPath = Join-Path $root "content\volum-content.json"

  # Rewind the seed library to exactly what 1.2.0 would have written: schema v2 and
  # no IR shaping keys at all. Everything else (amps, pedals, presets, payloads on
  # disk) is untouched, so this is a real upgrade rather than a synthetic fixture.
  $reg = Read-Json $contentPath
  $reg.schemaVersion = 2
  foreach ($ir in @($reg.irLibrary)) {
    foreach ($field in @("trimDb", "lowCutHz", "highCutHz")) {
      if ($ir.PSObject.Properties.Name -contains $field) { $ir.PSObject.Properties.Remove($field) }
    }
  }
  $reg | ConvertTo-Json -Depth 40 | Set-Content $contentPath -Encoding UTF8
  $before = Read-Json $contentPath
  $irCount = @($before.irLibrary).Count

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opened a window" $run.started
  Assert-True "app closed gracefully" $run.graceful

  $after = Read-Json $contentPath
  Assert-True "upgraded registry still parses" ($null -ne $after)
  if ($after) {
    Assert-NoContentLoss $before $after
    Assert-Equal ("registry upgraded to the current schema v{0}" -f $script:ContentSchemaVersion) `
      $script:ContentSchemaVersion $after.schemaVersion
    if ($irCount -gt 0) {
      $calibrated = @($after.irLibrary | Where-Object { $null -ne $_.trimDb }).Count
      Assert-Equal "every IR gained a measured trim" $irCount $calibrated
      # The migration rewrites the library in place and 1.2.0 cannot read the
      # result back, so the pre-migration copy is the user's only way home.
      Assert-True "pre-migration backup kept" (Test-Path (Join-Path $root "content\volum-content.json.pre-1.2.1.bak"))
    }
    Assert-True "corrupt-file backup NOT triggered" (-not (Test-Path (Join-Path $root "content\volum-content.json.bak")))
  }

  # Second launch must be a no-op: a migration that re-runs every time would keep
  # rewriting the library and could drift the trims.
  $second = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "second launch opened" $second.started
  $again = Read-Json $contentPath
  if ($after -and $again) {
    $a = ($after.irLibrary | ConvertTo-Json -Depth 20 -Compress)
    $b = ($again.irLibrary | ConvertTo-Json -Depth 20 -Compress)
    Assert-Equal "migration is idempotent across relaunch" $a $b
  }
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: the custom-amp reopen bugs, reproduced from disk
# --------------------------------------------------------------------------
function Test-CustomLane {
  Write-Host "`n[custom] custom MAIN amp on its upper channel with an active IR" -ForegroundColor Cyan
  $sandbox = New-Sandbox "custom"
  Copy-SeedState $sandbox
  $root = Join-Path $sandbox "VoLum"
  $settingsPath = Join-Path $root "volum-settings.json"
  $contentPath = Join-Path $root "content\volum-content.json"

  $reg = Read-Json $contentPath
  # Needs a custom amp with more than one DIRECT capture, otherwise there is no
  # upper channel to lose. Pick the first one that qualifies.
  $amp = @($reg.customAmps | Where-Object { @($_.files | Where-Object { $_.slot -lt 0 }).Count -gt 1 })[0]
  if (-not $amp) {
    Write-Host "  SKIP  seed library has no custom amp with two DIRECT captures" -ForegroundColor Yellow
    if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
    return
  }
  $ir = @($reg.irLibrary)[0]
  if (-not $ir) {
    Write-Host "  SKIP  seed library has no IR" -ForegroundColor Yellow
    if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
    return
  }

  # Stepper position of the amp's highest assigned channel. This is the number that
  # got clobbered: restoring an active IR forced the lane onto a DIRECT capture
  # derived from a runtime default instead of from this saved position, so a lane
  # saved on gain stage 5 came back on 1 and the position was rewritten to 0.
  $channels = @($amp.files | ForEach-Object { $_.channel }) | Sort-Object -Unique
  $topPos = $channels.Count - 1
  if ($topPos -lt 1) {
    Write-Host "  SKIP  custom amp has only one assigned channel" -ForegroundColor Yellow
    if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
    return
  }

  # A custom amp keeps its own scene in the content store; the settings file only
  # records which one is focused. Seeding the wrong one silently tests nothing, so
  # both halves are written here.
  $scene = $reg.customScenes.$($amp.id)
  if (-not $scene) {
    Write-Host ("  SKIP  no stored scene for amp {0}" -f $amp.id) -ForegroundColor Yellow
    if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
    return
  }
  $scene.channel = $topPos
  $scene.activeIrId = $ir.id
  $reg | ConvertTo-Json -Depth 60 | Set-Content $contentPath -Encoding UTF8

  $settings = Read-Json $settingsPath
  $settings.volumCustomMainId = $amp.id
  $settings | ConvertTo-Json -Depth 60 | Set-Content $settingsPath -Encoding UTF8

  Write-Host ("  seeded amp '{0}' ({1}) on channel position {2} of [{3}] with IR '{4}'" -f `
      $amp.name, $amp.id, $topPos, ($channels -join ","), $ir.name)

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opened a window" $run.started
  Assert-True "app closed gracefully" $run.graceful

  $after = Read-Json $settingsPath
  Assert-True "settings still parse" ($null -ne $after)
  if ($after) {
    Assert-Equal "custom MAIN amp still selected" $amp.id $after.volumCustomMainId
  }
  $regAfter = Read-Json $contentPath
  if ($regAfter) {
    $sceneAfter = $regAfter.customScenes.$($amp.id)
    Assert-Equal "channel position survives the IR restore" $topPos $sceneAfter.channel
    Assert-Equal "active IR survives" $ir.id $sceneAfter.activeIrId
  }

  # The settings file agreeing with itself is not enough: the bug this pins left the
  # persisted position correct while loading the WRONG capture, so the UI said
  # channel 5 and the user heard channel 1. Only the load log can tell them apart.
  # "read", not "loaded": the loader thread logs the file it parsed, which is the
  # part that identifies the capture. Accept either word so this keeps working
  # across the wording change.
  $topChannel = $channels[$topPos]
  $expected = @($amp.files | Where-Object { $_.slot -lt 0 -and $_.channel -eq $topChannel })[0]
  $log = Join-Path $root "volum.log"
  if ($expected -and (Test-Path $log)) {
    $loaded = @(Get-Content $log | Select-String -Pattern "\[model\] MAIN (?:read|loaded) (.+)$" |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() })
    $last = if ($loaded.Count) { $loaded[-1] } else { "" }
    $leaf = Split-Path $expected.storedPath -Leaf
    Assert-True ("MAIN loaded the channel {0} capture" -f $topChannel) ($last -like ("*" + $leaf)) `
      ("expected *{0}, log says '{1}'" -f $leaf, $last)
  }
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: references that no longer resolve on this machine
# --------------------------------------------------------------------------
function Test-BrokenRefs {
  Write-Host "`n[brokenrefs] scenes pointing at content that is gone" -ForegroundColor Cyan
  $sandbox = New-Sandbox "brokenrefs"
  Copy-SeedState $sandbox
  $root = Join-Path $sandbox "VoLum"
  $settingsPath = Join-Path $root "volum-settings.json"
  $contentPath = Join-Path $root "content\volum-content.json"

  # Phase 1: ids that name nothing. A user gets here by deleting content, or by
  # opening a library that was built on another machine.
  $reg = Read-Json $contentPath
  $settings = Read-Json $settingsPath
  $lane = $settings.amps.PSObject.Properties.Name[$settings.lastAmpIdx]

  $settings.volumCustomMainId = "amp_does_not_exist"
  $settings.volumActivePresetId = "preset_does_not_exist"
  $settings.amps.$lane.activeIrId = "ir_does_not_exist"
  $settings | ConvertTo-Json -Depth 60 | Set-Content $settingsPath -Encoding UTF8

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opens with unresolvable references" $run.started
  Assert-True "app closed gracefully" $run.graceful

  $after = Read-Json $settingsPath
  Assert-True "settings still parse" ($null -ne $after)
  if ($after) {
    # A dangling id must be dropped, not written back out: keeping it would make
    # every future save carry a reference that can never resolve again.
    Assert-Equal "unknown custom amp id dropped" "" $after.volumCustomMainId
    Assert-Equal "preset id of the unknown amp dropped" "" $after.volumActivePresetId
    Assert-Equal "unknown IR id dropped from the lane" "" $after.amps.$lane.activeIrId
  }
  $regAfter = Read-Json $contentPath
  Assert-True "library survived the bad references" (
    $regAfter -and @($regAfter.customAmps).Count -eq @($reg.customAmps).Count)

  # Phase 2: the focused custom amp itself is damaged - a channel position past the
  # end of its channel list, and the capture it should load deleted from disk. Both
  # only reach the restore path when that amp is the focused one, so this needs its
  # own launch rather than being folded into phase 1.
  $reg = Read-Json $contentPath
  $amp = @($reg.customAmps | Where-Object { $reg.customScenes.PSObject.Properties.Name -contains $_.id })[0]
  if (-not $amp) {
    Write-Host "  SKIP  seed library has no custom amp with a stored scene" -ForegroundColor Yellow
    if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
    return
  }
  $channels = @($amp.files | ForEach-Object { $_.channel }) | Sort-Object -Unique
  $topChannel = $channels[-1]
  $victim = @($amp.files | Where-Object { $_.slot -lt 0 -and $_.channel -eq $topChannel })[0]
  $reg.customScenes.$($amp.id).channel = 99
  $reg.customScenes.$($amp.id).activeIrId = ""
  $reg | ConvertTo-Json -Depth 60 | Set-Content $contentPath -Encoding UTF8

  $settings = Read-Json $settingsPath
  $settings.volumCustomMainId = $amp.id
  $settings | ConvertTo-Json -Depth 60 | Set-Content $settingsPath -Encoding UTF8

  $victimPath = $null
  if ($victim) {
    $victimPath = Join-Path $root ("content\" + ($victim.storedPath -replace "/", "\"))
    if (Test-Path $victimPath) { Remove-Item $victimPath -Force } else { $victimPath = $null }
  }
  Remove-Item (Join-Path $root "volum.log") -Force -ErrorAction SilentlyContinue

  $run2 = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opens with a damaged focused amp" $run2.started
  Assert-True "app closed gracefully after the damaged amp" $run2.graceful

  $regAfter = Read-Json $contentPath
  if ($regAfter) {
    $pos = $regAfter.customScenes.$($amp.id).channel
    Assert-True "out-of-range channel position clamped into range" (
      $pos -ge 0 -and $pos -lt $channels.Count) ("position is $pos, amp has $($channels.Count) channels")
  }
  if ($victimPath) {
    $log = Join-Path $root "volum.log"
    $text = if (Test-Path $log) { Get-Content $log -Raw } else { "" }
    Assert-True "missing capture recorded in the log" ($text -match "(?i)fail|missing|not found|error") `
      "nothing in volum.log mentions the failed load"
  }
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: library written by a build newer than this one
# --------------------------------------------------------------------------
function Test-FutureSchema {
  Write-Host "`n[future] library from a newer build (A/B downgrade)" -ForegroundColor Cyan
  $sandbox = New-Sandbox "future"
  Copy-SeedState $sandbox
  $root = Join-Path $sandbox "VoLum"
  $contentPath = Join-Path $root "content\volum-content.json"
  $settingsPath = Join-Path $root "volum-settings.json"

  # Running an older build for one session is a normal thing to do when comparing
  # releases. It may not understand everything it reads, but it must not treat the
  # file as corrupt and it must not silently drop the user's library.
  $reg = Read-Json $contentPath
  $reg.schemaVersion = 99
  $reg | Add-Member -NotePropertyName "somethingFromTheFuture" -NotePropertyValue @{ a = 1 } -Force
  $reg | ConvertTo-Json -Depth 60 | Set-Content $contentPath -Encoding UTF8
  $settings = Read-Json $settingsPath
  $settings.version = 99
  $settings | ConvertTo-Json -Depth 60 | Set-Content $settingsPath -Encoding UTF8
  $before = Read-Json $contentPath

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opens on a newer-schema library" $run.started
  Assert-True "app closed gracefully" $run.graceful
  Assert-True "newer library not treated as corrupt" (-not (Test-Path (Join-Path $root "content\volum-content.json.bak")))

  $after = Read-Json $contentPath
  Assert-True "newer library still parses" ($null -ne $after)
  if ($after) { Assert-NoContentLoss $before $after }
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: every amp's selected preset comes back, not just the focused one
#
# Reported from 1.2.1 testing: "only the last active amp remembers the selected
# preset". Settings stored a single active preset id - whichever amp was in focus
# when the file was written - so every other amp reopened reading "No Preset".
#
# Two amps are seeded with a preset each and the app is launched focused on the
# first. The second is never visited during the run, which is the whole point: its
# selection has to survive a session that never touches it.
# --------------------------------------------------------------------------
function Test-PresetMemory {
  Write-Host "`n[presets] every amp's selected preset survives a restart" -ForegroundColor Cyan
  $sandbox = New-Sandbox "presets"
  Copy-SeedState $sandbox
  $root = Join-Path $sandbox "VoLum"
  $contentPath = Join-Path $root "content\volum-content.json"
  $settingsPath = Join-Path $root "volum-settings.json"

  $focusedOwner = "factory:0"
  $awayOwner = "factory:5"
  $focusedPreset = "preset_e2e_focused"
  $awayPreset = "preset_e2e_away"

  $reg = Read-Json $contentPath
  if (-not $reg) { throw "Seed library has no volum-content.json; nothing to hang a preset bank on." }
  if (-not $reg.presetBanks) {
    $reg | Add-Member -NotePropertyName presetBanks -NotePropertyValue ([pscustomobject]@{}) -Force
  }
  # A bank entry needs an id to be kept by the registry reader; `settings` is left
  # empty so the preset simply means "factory defaults", which is enough to be
  # found by id.
  $reg.presetBanks | Add-Member -NotePropertyName $focusedOwner -NotePropertyValue `
  @([pscustomobject]@{ id = $focusedPreset; name = "E2E Focused"; settings = [pscustomobject]@{} }) -Force
  $reg.presetBanks | Add-Member -NotePropertyName $awayOwner -NotePropertyValue `
  @([pscustomobject]@{ id = $awayPreset; name = "E2E Away"; settings = [pscustomobject]@{} }) -Force
  $reg | ConvertTo-Json -Depth 60 | Set-Content $contentPath -Encoding UTF8

  $settings = Read-Json $settingsPath
  if (-not $settings) { throw "Seed library has no volum-settings.json." }
  # Focus a factory amp, not a custom one, so the owner key is predictable.
  $settings | Add-Member -NotePropertyName volumCustomMainId -NotePropertyValue "" -Force
  $settings | Add-Member -NotePropertyName lastAmpIdx -NotePropertyValue 0 -Force
  $settings | Add-Member -NotePropertyName volumActivePresetId -NotePropertyValue $focusedPreset -Force
  $settings | Add-Member -NotePropertyName volumActivePresetIdByOwner -NotePropertyValue ([pscustomobject]@{
      $focusedOwner = $focusedPreset
      $awayOwner    = $awayPreset
    }) -Force
  $settings | ConvertTo-Json -Depth 60 | Set-Content $settingsPath -Encoding UTF8
  $writtenBefore = (Get-Item $settingsPath).LastWriteTimeUtc

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opened a window" $run.started
  Assert-True "app closed gracefully" $run.graceful

  # Without this the rest is vacuous: if the app never rewrote the file, the seeded
  # keys would still be sitting there and every assertion below would pass against
  # a build that cannot read them.
  Assert-True "settings rewritten on shutdown" ((Get-Item $settingsPath).LastWriteTimeUtc -ne $writtenBefore)

  $after = Read-Json $settingsPath
  Assert-True "settings still parse" ($null -ne $after)
  if ($after) {
    Assert-True "per-amp preset map written back" ($null -ne $after.volumActivePresetIdByOwner)
    Assert-Equal "focused amp keeps its preset" $focusedPreset $after.volumActivePresetIdByOwner.$focusedOwner
    # The regression itself: an amp that was not in focus for the whole session.
    Assert-Equal "amp never visited keeps its preset" $awayPreset $after.volumActivePresetIdByOwner.$awayOwner
    # The 1.2.0 key stays truthful for the focused amp, so an older build reading
    # this file behaves exactly as it did before.
    Assert-Equal "single-id key still names the focused amp's preset" $focusedPreset $after.volumActivePresetId
  }
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: unreadable library must not brick the app
# --------------------------------------------------------------------------
function Test-Corrupt {
  Write-Host "`n[corrupt] truncated content registry" -ForegroundColor Cyan
  $sandbox = New-Sandbox "corrupt"
  Write-SandboxAudioConfig $sandbox
  $root = Join-Path $sandbox "VoLum"
  New-Item -ItemType Directory -Path (Join-Path $root "content") -Force | Out-Null
  '{ "schemaVersion": 3, "customAmps": [ { "id": "amp_trunc"' |
    Set-Content (Join-Path $root "content\volum-content.json") -Encoding UTF8

  # Since 1.3.0 the recovery is announced in a message box while the window opens
  # (OnUIOpen), and the window appears once it is acknowledged.
  $run = Invoke-VoLumRun -SandboxRoot $sandbox -AcknowledgeNotice "^VoLum: Could not read the library"
  Assert-Equal "recovery notice shown once" 1 @($run.notices).Count
  Assert-True "recovery notice names the .bak" (@($run.notices | Where-Object { $_ -match "volum-content\.json\.bak" }).Count -eq 1) `
    ("notices: " + ($run.notices -join " | "))
  Assert-True "app still opens with an unreadable library" $run.started
  Assert-True "app closed gracefully" $run.graceful
  Assert-True "unreadable library moved aside as .bak" (Test-Path (Join-Path $root "content\volum-content.json.bak"))
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: settings.ini names a sample rate the device cannot open
#
# Reachable from an ordinary file: settings.ini records whatever rate was in use at
# the last quit, and the next session may be on a different interface - or on the
# same one after its control panel was changed. Before 1.2.1 the open failed, the
# failure was reported through a MessageBox posted to a window that did not exist
# yet, and the app came up with no audio and no explanation.
# --------------------------------------------------------------------------
function Test-SampleRate {
  Write-Host "`n[samplerate] settings.ini names a rate the device does not offer" -ForegroundColor Cyan
  $sandbox = New-Sandbox "samplerate"
  $root = Join-Path $sandbox "VoLum"
  $ini = Join-Path $root "settings.ini"

  # DirectSound with the default devices, so this runs the same way on any Windows
  # machine instead of depending on which interface happens to be plugged in. 12345 Hz
  # is not in RtAudio's SAMPLE_RATES table, so no device can report it as supported.
  @(
    "[audio]",
    "driver=0",
    "indev=Default Device",
    "outdev=Default Device",
    "in1=1",
    "out1=1",
    "out2=2",
    "buffer=512",
    "sr=12345"
  ) | Set-Content $ini -Encoding ASCII

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "app opens despite an unopenable stored rate" $run.started
  Assert-True "app closed gracefully" $run.graceful

  # The correction needs a device to be corrected against. When no driver on this
  # machine can open a stream at all, VoLum logs no "stream open" line and there is
  # nothing to assert - report that rather than passing or failing on nothing.
  $log = Join-Path $root "volum.log"
  $opened = if (Test-Path $log) { (Get-Content $log -Raw) -match 'stream open: (\d+) Hz' } else { $false }
  if (-not $opened) {
    Write-Host "  SKIP  no audio device opened a stream on this machine" -ForegroundColor Yellow
    if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
    return
  }
  $openedRate = [int]$Matches[1]

  $after = Get-Content $ini -Raw
  $rate = if ($after -match '(?m)^sr=(\d+)') { [int]$Matches[1] } else { 0 }
  Assert-True "stream opened at a real rate, not the stored one" ($openedRate -ne 12345) "opened at $openedRate"
  Assert-Equal "settings.ini records the rate the driver opened" $openedRate $rate

  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: PLAY "Add this sound" on an unsaved sound, cancelled and then saved
#
# Reported on 1.3.0: with PLAY empty and the Default sound, Add opened the name
# dialog and Cancel still created a preset (sometimes a PLAY slot too); a few
# rounds left "New Preset", "New Preset 2", ... behind. Cancel / Esc / a click
# outside must write nothing; Save must create one preset and one PLAY slot.
#
# Input goes to VoLum's plugin window as window messages, not through the cursor
# and the foreground window: it runs the real iPlug WndProc -> IGraphics -> control
# path, and it works on a locked desktop, where synthetic clicks land on the lock
# screen. Canvas coordinates are the 900x600 layout (dialog 420x188 centred; see
# VoLumNameDialog.h BoxRect).
# --------------------------------------------------------------------------
if (-not ([System.Management.Automation.PSTypeName]'VoLumE2eUi').Type) {
  Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class VoLumE2eUi {
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint type);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr h, uint cmd);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] static extern bool AttachThreadInput(uint from, uint to, bool attach);
  [DllImport("user32.dll")] static extern bool GetKeyboardState(byte[] s);
  [DllImport("user32.dll")] static extern bool SetKeyboardState(byte[] s);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [DllImport("user32.dll", EntryPoint = "SendMessageW", CharSet = CharSet.Unicode)]
  static extern IntPtr SendText(IntPtr h, uint msg, IntPtr w, StringBuilder l);
  [StructLayout(LayoutKind.Sequential)] struct RECT { public int Left, Top, Right, Bottom; }

  // Visible owned dialog boxes of a process: its MessageBoxes. VoLum's main window
  // is an unowned #32770, so it never matches.
  public static IntPtr[] OwnedDialogs(int pid) {
    var found = new System.Collections.Generic.List<IntPtr>();
    EnumWindows((h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p != (uint)pid || !IsWindowVisible(h) || GetWindow(h, 4) == IntPtr.Zero) return true;
      var c = new StringBuilder(64);
      GetClassName(h, c, 64);
      if (c.ToString() == "#32770") found.Add(h);
      return true;
    }, IntPtr.Zero);
    return found.ToArray();
  }
  static string TextOf(IntPtr h) {
    var s = new StringBuilder(1024);
    SendText(h, 0x000D, (IntPtr)s.Capacity, s); // WM_GETTEXT
    return s.ToString();
  }
  // "Caption: body" of a MessageBox.
  public static string DialogText(IntPtr box) {
    var body = new StringBuilder();
    EnumChildWindows(box, (h, l) => {
      var c = new StringBuilder(64);
      GetClassName(h, c, 64);
      if (c.ToString() == "Static") body.Append(TextOf(h));
      return true;
    }, IntPtr.Zero);
    return TextOf(box) + ": " + body.ToString().Trim();
  }
  // WM_COMMAND with the button's own id: a lone OK button in a MessageBox is
  // IDCANCEL, not IDOK. Works without input, so also on a locked desktop.
  public static void PressOk(IntPtr box) {
    IntPtr button = IntPtr.Zero;
    EnumChildWindows(box, (h, l) => {
      var c = new StringBuilder(64);
      GetClassName(h, c, 64);
      if (c.ToString() != "Button") return true;
      button = h;
      return false;
    }, IntPtr.Zero);
    if (button != IntPtr.Zero) PostMessage(box, 0x0111, (IntPtr)GetDlgCtrlID(button), button);
  }

  public static IntPtr PlugWindow(IntPtr main) {
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(main, (h, l) => {
      var c = new StringBuilder(64);
      GetClassName(h, c, 64);
      if (c.ToString() != "IPlugWndClass") return true;
      found = h;
      return false;
    }, IntPtr.Zero);
    return found;
  }
  // Canvas pixels, scaled to the actual client size.
  public static void Click(IntPtr plug, int cx, int cy) {
    RECT r; GetClientRect(plug, out r);
    double s = (r.Right - r.Left) / 900.0;
    int x = (int)Math.Round(cx * s), y = (int)Math.Round(cy * s);
    IntPtr at = (IntPtr)((y << 16) | (x & 0xFFFF));
    SendMessage(plug, 0x0200, IntPtr.Zero, at);     // WM_MOUSEMOVE
    SendMessage(plug, 0x0201, (IntPtr)1, at);       // WM_LBUTTONDOWN, MK_LBUTTON
    SendMessage(plug, 0x0202, IntPtr.Zero, at);     // WM_LBUTTONUP
  }
  // iPlug derives the character from WM_KEYDOWN via ToAscii on the app thread's own
  // keyboard state, which a message cannot carry: Shift is never down, so text is
  // lowercase.
  public static void Key(IntPtr plug, int vk) {
    long scan = MapVirtualKey((uint)vk, 0);
    SendMessage(plug, 0x0100, (IntPtr)vk, (IntPtr)(1 | (scan << 16)));                   // WM_KEYDOWN
    SendMessage(plug, 0x0101, (IntPtr)vk, (IntPtr)(1 | (scan << 16) | 0xC0000000L));     // WM_KEYUP
  }
  // iPlug reads Ctrl / Shift with GetKeyState on its own thread, so the key state is
  // shared with that thread for the stroke. Returns false when it could not be.
  public static bool KeyMod(IntPtr plug, int vk, bool shift, bool ctrl) {
    uint pid;
    uint target = GetWindowThreadProcessId(plug, out pid);
    uint self = GetCurrentThreadId();
    if (!AttachThreadInput(self, target, true)) return false;
    byte[] saved = new byte[256];
    GetKeyboardState(saved);
    byte[] state = (byte[])saved.Clone();
    if (shift) { state[0x10] = 0x80; state[0xA0] = 0x80; }
    if (ctrl) { state[0x11] = 0x80; state[0xA2] = 0x80; }
    SetKeyboardState(state);
    Key(plug, vk);
    SetKeyboardState(saved);
    AttachThreadInput(self, target, false);
    return true;
  }
  // Lowercase letters, digits and spaces.
  public static void Type(IntPtr plug, string text) {
    foreach (char c in text) Key(plug, c == ' ' ? 0x20 : (int)char.ToUpperInvariant(c));
  }
}
'@
}

function Get-PresetRows {
  param($Registry)
  $rows = @()
  if ($Registry -and $Registry.presetBanks) {
    foreach ($bank in $Registry.presetBanks.PSObject.Properties) {
      foreach ($pr in @($bank.Value)) { $rows += [pscustomobject]@{ owner = $bank.Name; id = $pr.id; name = $pr.name } }
    }
  }
  return , $rows
}

function Get-MidiMapRows {
  param($Registry)
  if ($Registry -and $Registry.midiSoundMap) { return , @($Registry.midiSoundMap) }
  return , @()
}

function Test-SaveDialog {
  Write-Host "`n[savedialog] PLAY Add this sound: Cancel writes nothing, Save adds one Sound" -ForegroundColor Cyan
  $sandbox = New-Sandbox "savedialog"
  $root = Join-Path $sandbox "VoLum"
  Write-SandboxAudioConfig $sandbox
  $contentPath = Join-Path $root "content\volum-content.json"
  $settingsPath = Join-Path $root "volum-settings.json"
  $logPath = Join-Path $root "volum.log"

  # First launch writes a real settings file; the second opens straight into PLAY
  # on factory amp 0 with no preset selected (the Default sound).
  $first = Invoke-VoLumRun -SandboxRoot $sandbox
  Assert-True "first launch opened a window" $first.started
  $settings = Read-Json $settingsPath
  if (-not $settings) { Assert-True "first launch wrote volum-settings.json" $false; return }
  $settings | Add-Member -NotePropertyName volumUiMode -NotePropertyValue "play" -Force
  $settings | Add-Member -NotePropertyName volumCustomMainId -NotePropertyValue "" -Force
  $settings | Add-Member -NotePropertyName volumActivePresetId -NotePropertyValue "" -Force
  $settings | Add-Member -NotePropertyName volumActivePresetIdByOwner -NotePropertyValue ([pscustomobject]@{}) -Force
  $settings | ConvertTo-Json -Depth 60 | Set-Content $settingsPath -Encoding UTF8
  $presetsBefore = (Get-PresetRows (Read-Json $contentPath)).Count
  $mapBefore = (Get-MidiMapRows (Read-Json $contentPath)).Count
  Remove-Item $logPath -Force -ErrorAction SilentlyContinue

  $addSound = @(450, 324)   # PLAY empty board: "+ Add this sound"
  $cancel = @(351, 351)     # dialog Cancel button
  $outside = @(450, 150)    # scrim above the dialog box

  $run = Invoke-VoLumRun -SandboxRoot $sandbox -SettleSec 7 -Drive {
    param($proc)
    $h = [VoLumE2eUi]::PlugWindow($proc.MainWindowHandle)
    if ($h -eq [IntPtr]::Zero) { throw "no IPlugWndClass child under the main window" }
    $open = { [VoLumE2eUi]::Click($h, $addSound[0], $addSound[1]); Start-Sleep -Milliseconds 400 }
    for ($i = 0; $i -lt 5; $i++) {
      & $open
      [VoLumE2eUi]::Click($h, $cancel[0], $cancel[1]); Start-Sleep -Milliseconds 400
    }
    & $open
    [VoLumE2eUi]::Key($h, 0x1B); Start-Sleep -Milliseconds 400
    & $open
    [VoLumE2eUi]::Click($h, $outside[0], $outside[1]); Start-Sleep -Milliseconds 400
    $afterCancels = Read-Json $contentPath

    # Save through the keyboard route: the seed is selected, typing replaces it,
    # Enter commits.
    & $open
    [VoLumE2eUi]::Type($h, "e2e lead"); Start-Sleep -Milliseconds 200
    [VoLumE2eUi]::Key($h, 0x0D); Start-Sleep -Milliseconds 1000
    return @{ afterCancels = $afterCancels }
  }
  Assert-True "app opened a window" $run.started
  Assert-True "app closed gracefully" $run.graceful

  $log = if (Test-Path $logPath) { Get-Content $logPath -Raw } else { "" }
  $opens = ([regex]::Matches($log, "save dialog open")).Count
  $cancels = ([regex]::Matches($log, "save dialog cancelled")).Count
  $commits = ([regex]::Matches($log, "save dialog commit")).Count
  # Positive control: without it a missed click would pass every "nothing written"
  # check below for free.
  Assert-Equal "Add opened the name dialog 8 times" 8 $opens
  Assert-Equal "Cancel x5, Esc and outside click each cancelled" 7 $cancels
  Assert-Equal "exactly one commit (Enter)" 1 $commits

  $mid = if ($run.drive) { $run.drive.afterCancels } else { $null }
  Assert-Equal "no preset written by 7 cancelled dialogs" $presetsBefore (Get-PresetRows $mid).Count
  Assert-Equal "no PLAY slot written by 7 cancelled dialogs" $mapBefore (Get-MidiMapRows $mid).Count

  $after = Read-Json $contentPath
  $rows = Get-PresetRows $after
  Assert-Equal "Save created exactly one preset" ($presetsBefore + 1) $rows.Count
  $saved = @($rows | Where-Object { $_.name -ceq "e2e lead" })[0]
  Assert-True "preset carries the typed name" ($null -ne $saved) ("names: " + (($rows | ForEach-Object { $_.name }) -join ", "))
  $map = Get-MidiMapRows $after
  Assert-Equal "Save added exactly one PLAY slot" ($mapBefore + 1) $map.Count
  if ($saved -and $map.Count -ge 1) {
    Assert-True "the PLAY slot is the saved preset" (@($map | Where-Object { $_.presetId -eq $saved.id }).Count -eq 1)
  }

  # BUILD: Ctrl+S opens the dialog from PRE and POST, also once a knob is selected
  # there (a clicked PRE knob, a POST knob picked with Enter). A selected knob used to
  # take every key and drop the ones it did not use, Ctrl+S among them.
  Write-Host "  [savedialog] BUILD Ctrl+S from PRE / POST, with and without a selected knob" -ForegroundColor Cyan
  $settings = Read-Json $settingsPath
  $settings | Add-Member -NotePropertyName volumUiMode -NotePropertyValue "build" -Force
  $settings | ConvertTo-Json -Depth 60 | Set-Content $settingsPath -Encoding UTF8
  Remove-Item $logPath -Force -ErrorAction SilentlyContinue
  $preKnob = @(439, 437)   # COMP INPUT, the first knob of the PRE focus that key 1 lands on
  $build = Invoke-VoLumRun -SandboxRoot $sandbox -SettleSec 7 -Drive {
    param($proc)
    $h = [VoLumE2eUi]::PlugWindow($proc.MainWindowHandle)
    if ($h -eq [IntPtr]::Zero) { throw "no IPlugWndClass child under the main window" }
    $save = {
      if (-not [VoLumE2eUi]::KeyMod($h, 0x53, $false, $true)) { $script:ctrlNotShared = $true }
      Start-Sleep -Milliseconds 400
      [VoLumE2eUi]::Key($h, 0x1B); Start-Sleep -Milliseconds 300
    }
    $script:ctrlNotShared = $false
    [VoLumE2eUi]::Key($h, 0x31); Start-Sleep -Milliseconds 300   # 1 = PRE
    & $save
    [VoLumE2eUi]::Click($h, $preKnob[0], $preKnob[1]); Start-Sleep -Milliseconds 300
    & $save
    [VoLumE2eUi]::Key($h, 0x33); Start-Sleep -Milliseconds 300   # 3 = POST
    & $save
    [VoLumE2eUi]::Key($h, 0x0D); Start-Sleep -Milliseconds 300   # Enter selects the POST knob
    & $save
    return @{ ctrlShared = -not $script:ctrlNotShared }
  }
  Assert-True "BUILD run opened a window" $build.started
  Assert-True "BUILD run closed gracefully" $build.graceful
  Assert-True "Ctrl reached VoLum (AttachThreadInput)" ($build.drive -and $build.drive.ctrlShared)
  $log = if (Test-Path $logPath) { Get-Content $logPath -Raw } else { "" }
  Assert-Equal "Ctrl+S opened the dialog from PRE, PRE knob, POST, POST knob" 4 ([regex]::Matches($log, "save dialog open")).Count
  Assert-Equal "each BUILD dialog cancelled with Esc" 4 ([regex]::Matches($log, "save dialog cancelled")).Count
  Assert-Equal "cancelled BUILD dialogs wrote no preset" $rows.Count (Get-PresetRows (Read-Json $contentPath)).Count
  if (-not $KeepSandbox) { Remove-Item $sandbox -Recurse -Force -ErrorAction SilentlyContinue }
}

# --------------------------------------------------------------------------
# Scenario: Pack export -> import through the real Settings -> SYSTEM overlay
#
# Library A (the screenshot seed plus a Sound on the custom amp and a PLAY map)
# exports Everything, a Sounds subset and A whole amp through the overlay. Each
# Pack is imported through the overlay into a fresh, empty library B, and B's
# registry and payload files are compared with A's for exactly what that scope
# promises. Then the three verbs run against a library that already has the
# items, and a truncated Pack is refused without writing anything.
#
# The native Save / Open dialogs cannot be driven on a locked desktop, so
# VOLUM_PACK_SAVE_PATH / VOLUM_PACK_OPEN_PATH stand in for them
# (VoLumPackActions.inc.cpp); everything after the chosen path is the code the
# button runs. Canvas coordinates: the modal is 560x408 centred in 900x600
# (VoLumPackOverlay.h, VoLumPackLayout.h).
# --------------------------------------------------------------------------
$script:PackUi = @{
  gear = @(869, 22); system = @(600, 113); export = @(525, 346); import = @(718, 346)
  go = @(529, 473); cancel = @(371, 473)
  scopeSounds = @(250, 174); scopeAmp = @(250, 198)
  exportRow0 = 227 # centre of the first export tick row; rows are 20 px apart
  verbOverwrite = @(272, 397); verbAdd = @(447, 397); verbReset = @(622, 397)
  alsoSettings = @(300, 441)
}

function Invoke-PackClick {
  param([IntPtr]$Plug, $At, [int]$SleepMs = 450)
  [VoLumE2eUi]::Click($Plug, $At[0], $At[1])
  Start-Sleep -Milliseconds $SleepMs
}

# VoLum paints itself into <CaptureDir>\<stem>.bmp (VoLumSelfCapture.h), which
# works on a locked workstation. Only with -ShotsDir.
function Save-PackShot {
  param([string]$CaptureDir, [string]$Name)
  if (-not $ShotsDir) { return }
  $stem = "e2e-" + [DateTime]::UtcNow.Ticks
  $done = Join-Path $CaptureDir "done.txt"
  Remove-Item $done -Force -ErrorAction SilentlyContinue
  [IO.File]::WriteAllText((Join-Path $CaptureDir "request.txt"), $stem)
  $until = (Get-Date).AddSeconds(8)
  $landed = $false
  while ((Get-Date) -lt $until) {
    if ((Test-Path $done) -and ((Get-Content $done -Raw) -like "$stem.bmp *")) { $landed = $true; break }
    Start-Sleep -Milliseconds 100
  }
  if (-not $landed) { Write-Host ("  shot {0} did not land" -f $Name) -ForegroundColor Yellow; return }
  Add-Type -AssemblyName System.Drawing
  New-Item -ItemType Directory -Path $ShotsDir -Force | Out-Null
  $bmp = Join-Path $CaptureDir "$stem.bmp"
  $img = [System.Drawing.Image]::FromFile($bmp)
  try { $img.Save((Join-Path $ShotsDir "$Name.png"), [System.Drawing.Imaging.ImageFormat]::Png) }
  finally { $img.Dispose() }
  Remove-Item $bmp -Force -ErrorAction SilentlyContinue
}

function Read-PackArchive {
  param([string]$Path)
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $zip = [IO.Compression.ZipFile]::OpenRead($Path)
  try {
    $out = @{ names = @($zip.Entries | ForEach-Object { $_.FullName }) }
    foreach ($n in @("manifest.json", "library.json", "settings.json")) {
      $e = $zip.GetEntry($n)
      if ($e) {
        $r = New-Object IO.StreamReader($e.Open(), [Text.Encoding]::UTF8)
        $out[$n] = $r.ReadToEnd()
        $r.Close()
      }
    }
    return $out
  }
  finally { $zip.Dispose() }
}

function ConvertTo-Canon { param($Value) return ($Value | ConvertTo-Json -Depth 40 -Compress) }

function Get-PresetIndex {
  param($Registry)
  $ix = @{}
  if ($Registry -and $Registry.presetBanks) {
    foreach ($bank in $Registry.presetBanks.PSObject.Properties) {
      foreach ($pr in @($bank.Value)) {
        $ix[$pr.id] = [pscustomobject]@{ owner = $bank.Name; name = $pr.name; settings = (ConvertTo-Canon $pr.settings) }
      }
    }
  }
  return $ix
}

function Get-MapKey {
  param($Registry)
  $rows = Get-MidiMapRows $Registry
  return (@($rows | Sort-Object { [int]$_.slot } |
      ForEach-Object { "{0}:{1}:{2}" -f $_.slot, $_.ampId, $_.presetId }) -join "|")
}

function Join-Ids { param($Ids) return ((@($Ids) | Where-Object { $_ } | Sort-Object) -join ",") }

function Get-StoredFile {
  param([string]$Root, [string]$Rel)
  return (Join-Path $Root ("content\" + ($Rel -replace "/", "\")))
}

function Get-FileSha {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { return "" }
  return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

# B holds exactly the promised items, each identical to A's, with the same bytes
# behind every stored path.
function Assert-PackLanded {
  param([string]$Label, $SrcReg, [string]$SrcRoot, $DstReg, [string]$DstRoot,
    [string[]]$Amps = @(), [string[]]$Irs = @(), [string[]]$Pedals = @(), [string[]]$Presets = @())
  if (-not $DstReg) { Assert-True "[$Label] receiver library written" $false; return }
  $sameBytes = {
    param($rel)
    $b = Get-StoredFile $DstRoot $rel
    (Test-Path -LiteralPath $b) -and ((Get-FileSha (Get-StoredFile $SrcRoot $rel)) -eq (Get-FileSha $b))
  }
  foreach ($id in $Amps) {
    $s = @($SrcReg.customAmps | Where-Object { $_.id -eq $id })[0]
    $d = @($DstReg.customAmps | Where-Object { $_.id -eq $id })[0]
    Assert-True "[$Label] custom amp '$($s.name)' imported" ($null -ne $d)
    if (-not $d) { continue }
    Assert-Equal "[$Label] custom amp '$($s.name)' identical" (ConvertTo-Canon $s) (ConvertTo-Canon $d)
    foreach ($f in @($s.files | Where-Object { $_.storedPath })) {
      Assert-True "[$Label] capture '$($f.storedPath)' on disk with the same bytes" (& $sameBytes $f.storedPath)
    }
  }
  foreach ($kind in @(@{ key = "irLibrary"; ids = $Irs; what = "IR" }, @{ key = "customPedals"; ids = $Pedals; what = "pedal" })) {
    foreach ($id in $kind.ids) {
      $s = @($SrcReg.($kind.key) | Where-Object { $_.id -eq $id })[0]
      $d = @($DstReg.($kind.key) | Where-Object { $_.id -eq $id })[0]
      Assert-True "[$Label] $($kind.what) '$($s.name)' imported" ($null -ne $d)
      if (-not $d) { continue }
      Assert-Equal "[$Label] $($kind.what) '$($s.name)' identical" (ConvertTo-Canon $s) (ConvertTo-Canon $d)
      Assert-True "[$Label] $($kind.what) file '$($s.path)' on disk with the same bytes" (& $sameBytes $s.path)
    }
  }
  $si = Get-PresetIndex $SrcReg
  $di = Get-PresetIndex $DstReg
  foreach ($id in $Presets) {
    $s = $si[$id]
    $d = $di[$id]
    Assert-True "[$Label] preset '$($s.name)' imported" ($null -ne $d)
    if (-not $d) { continue }
    Assert-Equal "[$Label] preset '$($s.name)' stays on its amp" $s.owner $d.owner
    Assert-Equal "[$Label] preset '$($s.name)' keeps its name" $s.name $d.name
    Assert-Equal "[$Label] preset '$($s.name)' keeps its settings" $s.settings $d.settings
  }
  Assert-Equal "[$Label] no other custom amps" (Join-Ids $Amps) (Join-Ids ($DstReg.customAmps | ForEach-Object { $_.id }))
  Assert-Equal "[$Label] no other IRs" (Join-Ids $Irs) (Join-Ids ($DstReg.irLibrary | ForEach-Object { $_.id }))
  Assert-Equal "[$Label] no other pedals" (Join-Ids $Pedals) (Join-Ids ($DstReg.customPedals | ForEach-Object { $_.id }))
  Assert-Equal "[$Label] no other presets" (Join-Ids $Presets) (Join-Ids $di.Keys)
}

# A fresh library B opens `$PackPath` through Settings -> SYSTEM -> Import Pack...
# `$Choose` clicks verbs / the settings box before Import.
function Invoke-PackImportFresh {
  param([string]$Name, [string]$PackPath, [scriptblock]$Choose)
  $sandbox = New-Sandbox ("pack-import-" + $Name)
  Write-SandboxAudioConfig $sandbox
  $cap = Join-Path $sandbox "capture"
  New-Item -ItemType Directory -Path $cap -Force | Out-Null
  $run = Invoke-VoLumRun -SandboxRoot $sandbox -SettleSec 7 `
    -Environment @{ VOLUM_PACK_OPEN_PATH = $PackPath; VOLUM_SELF_CAPTURE_DIR = $cap } -Drive {
    param($proc)
    $h = [VoLumE2eUi]::PlugWindow($proc.MainWindowHandle)
    if ($h -eq [IntPtr]::Zero) { throw "no IPlugWndClass child under the main window" }
    $ui = $script:PackUi
    Invoke-PackClick $h $ui.gear
    Invoke-PackClick $h $ui.system
    Invoke-PackClick $h $ui.import 900
    Save-PackShot $cap "06-import-$Name"
    if ($Choose) { & $Choose $h $cap }
    Invoke-PackClick $h $ui.go 1500
    Save-PackShot $cap "06-import-$Name-done"
  }
  Assert-True "[$Name] receiver opened" $run.started
  Assert-True "[$Name] receiver closed gracefully" $run.graceful
  $root = Join-Path $sandbox "VoLum"
  $log = Join-Path $root "volum.log"
  return @{
    sandbox = $sandbox; root = $root
    reg = Read-Json (Join-Path $root "content\volum-content.json")
    settings = Read-Json (Join-Path $root "volum-settings.json")
    log = $(if (Test-Path $log) { Get-Content $log -Raw } else { "" })
  }
}

function Test-Pack {
  Write-Host "`n[pack] export Everything / Sounds / A whole amp, import each into a fresh library" -ForegroundColor Cyan
  $ui = $script:PackUi
  $seed = Join-Path (Split-Path -Parent $slnDir) "docs\screenshot-seed"
  $sandA = New-Sandbox "pack-source"
  $rootA = Join-Path $sandA "VoLum"
  Copy-Item (Join-Path $seed "volum-settings.json") $rootA -Force
  Copy-Item (Join-Path $seed "volum-dual-amp-settings.json") $rootA -Force
  Copy-Item (Join-Path $seed "content") $rootA -Recurse -Force
  Write-SandboxAudioConfig $sandA
  $contentA = Join-Path $rootA "content\volum-content.json"

  # A Sound on the custom amp (its owner and PRE pedal must travel with it), the
  # custom pedal on a factory Sound, and a PLAY map over all three kinds of owner.
  $j = Read-Json $contentA
  $ampId = $j.customAmps[0].id
  $irId = $j.irLibrary[0].id
  $pedalId = $j.customPedals[0].id
  $pedalIdx = $j.customPedals[0].legacyIndex
  $skelId = "preset_e2e_skel"
  $leadBoost = @($j.presetBanks.'factory:13' | Where-Object { $_.name -eq "Lead Boost" })[0]
  if (-not $leadBoost -or $leadBoost.settings.activeIrId -ne $irId) { throw "seed changed: Lead Boost no longer carries the custom IR" }
  $j.presetBanks | Add-Member -NotePropertyName $ampId -NotePropertyValue @([pscustomobject]@{
      id = $skelId; name = "Skeleton Lead"; settings = [pscustomobject]@{ preNam1Capture = $pedalIdx }
    }) -Force
  foreach ($pr in $j.presetBanks.'factory:13') { if ($pr.name -eq "Crunch Rhythm") { $pr.settings.preNam1Capture = $pedalIdx } }
  $j | Add-Member -NotePropertyName midiSoundMap -NotePropertyValue @(
    [pscustomobject]@{ slot = 0; ampId = "factory:13"; presetId = "preset_402e30dc" },
    [pscustomobject]@{ slot = 1; ampId = $ampId; presetId = $skelId },
    [pscustomobject]@{ slot = 5; ampId = "factory:14"; presetId = "preset_sunset1c3" }
  ) -Force
  $j | ConvertTo-Json -Depth 100 | Set-Content $contentA -Encoding UTF8
  $crunchId = "preset_402e30dc"
  $sunsetId = "preset_sunset1c3"

  $packDir = Join-Path $sandA "packs"
  New-Item -ItemType Directory -Path $packDir -Force | Out-Null
  $drop = Join-Path $packDir "drop.volumpack"
  $capA = Join-Path $sandA "capture"
  New-Item -ItemType Directory -Path $capA -Force | Out-Null
  $state = @{ emptySoundsWrote = $null; took = @() }

  # PLAY Sounds first by program number (not bank key): 00 Crunch Rhythm, 01 Skeleton
  # Lead, 05 Sunset Crunch, then non-PLAY Lead Boost / Clean Verb.
  $runA = Invoke-VoLumRun -SandboxRoot $sandA -SettleSec 7 `
    -Environment @{ VOLUM_PACK_SAVE_PATH = $drop; VOLUM_SELF_CAPTURE_DIR = $capA } -Drive {
    param($proc)
    $h = [VoLumE2eUi]::PlugWindow($proc.MainWindowHandle)
    if ($h -eq [IntPtr]::Zero) { throw "no IPlugWndClass child under the main window" }
    $take = {
      param($name)
      $until = (Get-Date).AddSeconds(6)
      while (-not (Test-Path $drop) -and (Get-Date) -lt $until) { Start-Sleep -Milliseconds 100 }
      if (Test-Path $drop) {
        Move-Item $drop (Join-Path $packDir "$name.volumpack") -Force
        $state.took += $name
      }
    }
    Invoke-PackClick $h $ui.gear
    Invoke-PackClick $h $ui.system
    Save-PackShot $capA "06-settings-system"
    Invoke-PackClick $h $ui.export
    Save-PackShot $capA "06-export-everything"
    Invoke-PackClick $h $ui.go 900
    & $take "everything"

    # Sounds with nothing ticked: Export... is dead (dimmed).
    Invoke-PackClick $h $ui.export
    Invoke-PackClick $h $ui.scopeSounds
    Save-PackShot $capA "06-export-sounds-empty"
    Save-PackShot $capA "23-export-disabled"
    Invoke-PackClick $h $ui.go 900
    $state.emptySoundsWrote = Test-Path $drop

    # Tick all three PLAY rows (program order) to prove list order via the manifest.
    Invoke-PackClick $h @(300, $ui.exportRow0)
    Invoke-PackClick $h @(300, ($ui.exportRow0 + 1 * 20))
    Invoke-PackClick $h @(300, ($ui.exportRow0 + 2 * 20))
    Save-PackShot $capA "23-export-sounds-order"
    Invoke-PackClick $h $ui.go 900
    & $take "sounds-play-order"

    # Re-open Sounds: Skeleton Lead (row 1) + Lead Boost (row 3) for the Share pack.
    Invoke-PackClick $h $ui.export
    Invoke-PackClick $h $ui.scopeSounds
    Invoke-PackClick $h @(300, ($ui.exportRow0 + 1 * 20))
    Invoke-PackClick $h @(300, ($ui.exportRow0 + 3 * 20))
    Save-PackShot $capA "06-export-sounds"
    Invoke-PackClick $h $ui.go 900
    & $take "sounds"

    Invoke-PackClick $h $ui.export
    Invoke-PackClick $h $ui.scopeAmp
    Invoke-PackClick $h @(300, $ui.exportRow0)
    Save-PackShot $capA "06-export-amp"
    Invoke-PackClick $h $ui.go 900
    & $take "amp"
  }
  Assert-True "source opened" $runA.started
  Assert-True "source closed gracefully" $runA.graceful
  Assert-equal "zero-tick Sounds export wrote nothing" $false $state.emptySoundsWrote
  Assert-equal "four Packs written through Export..." "everything,sounds-play-order,sounds,amp" ($state.took -join ",")
  $regA = Read-Json $contentA
  $logA = if (Test-Path (Join-Path $rootA "volum.log")) { Get-Content (Join-Path $rootA "volum.log") -Raw } else { "" }
  Assert-equal "log records four exports" 4 ([regex]::Matches($logA, "\[pack\] export wrote")).Count
  if (-not $regA -or $state.took.Count -ne 4) {
    if (-not $KeepSandbox) { Remove-Item $sandA -Recurse -Force -ErrorAction SilentlyContinue }
    return
  }
  $allAmps = @($regA.customAmps | ForEach-Object { $_.id })
  $allIrs = @($regA.irLibrary | ForEach-Object { $_.id })
  $allPedals = @($regA.customPedals | ForEach-Object { $_.id })
  $allPresets = @((Get-PresetIndex $regA).Keys)
  $everything = Join-Path $packDir "everything.volumpack"
  $soundsPlayOrder = Join-Path $packDir "sounds-play-order.volumpack"
  $sounds = Join-Path $packDir "sounds.volumpack"
  $amp = Join-Path $packDir "amp.volumpack"

  # PLAY-only Sounds: selection order follows the program-sorted tick list.
  $po = Read-PackArchive $soundsPlayOrder
  $mpo = $po["manifest.json"] | ConvertFrom-Json
  Assert-equal "PLAY Sounds export order is program number" ($crunchId + "," + $skelId + "," + $sunsetId) ((@($mpo.presets) -join ","))

    # What each file carries, straight from the archive.
  $pe = Read-PackArchive $everything
  $me = $pe["manifest.json"] | ConvertFrom-Json
  Assert-Equal "Everything manifest job" "everything" $me.job
  Assert-True "Everything carries settings and the MIDI map" ($me.includesSettings -and $me.includesMidiSoundMap -and $pe["settings.json"])
  Assert-Equal "Everything carries every preset" (Join-Ids $allPresets) (Join-Ids $me.presets)
  Assert-Equal "Everything carries the PLAY map" (Get-MapKey $regA) (Get-MapKey ($pe["library.json"] | ConvertFrom-Json))
  $ps = Read-PackArchive $sounds
  $ms = $ps["manifest.json"] | ConvertFrom-Json
  Assert-Equal "Sounds manifest job" "share" $ms.job
  Assert-Equal "Sounds carries the two ticked presets" (Join-Ids @($skelId, $leadBoost.id)) (Join-Ids $ms.presets)
  Assert-Equal "Sounds pulls the custom-amp owner" $ampId (Join-Ids $ms.customAmps)
  Assert-Equal "Sounds pulls Lead Boost's IR" $irId (Join-Ids $ms.irLibrary)
  Assert-Equal "Sounds pulls Skeleton Lead's pedal" $pedalId (Join-Ids $ms.pedals)
  Assert-True "Sounds carries no settings" (-not $ps["settings.json"] -and -not $ms.includesSettings)
  Assert-Equal "Sounds carries no MIDI map" "" (Get-MapKey ($ps["library.json"] | ConvertFrom-Json))
  $pa = Read-PackArchive $amp
  $ma = $pa["manifest.json"] | ConvertFrom-Json
  Assert-Equal "Amp manifest job" "share" $ma.job
  Assert-Equal "Amp carries the amp" $ampId (Join-Ids $ma.customAmps)
  Assert-Equal "Amp carries its bank" $skelId (Join-Ids $ma.presets)
  Assert-Equal "Amp pulls its preset's pedal" $pedalId (Join-Ids $ma.pedals)
  Assert-Equal "Amp carries no IR" "" (Join-Ids $ma.irLibrary)

  # Everything + "Also restore machine settings".
  $e = Invoke-PackImportFresh "everything" $everything {
    param($h, $cap)
    Invoke-PackClick $h $ui.alsoSettings
    Save-PackShot $cap "06-import-everything-settings"
  }
  Assert-PackLanded "everything" $regA $rootA $e.reg $e.root $allAmps $allIrs $allPedals $allPresets
  Assert-Equal "[everything] PLAY map restored" (Get-MapKey $regA) (Get-MapKey $e.reg)
  $packSettings = $pe["settings.json"] | ConvertFrom-Json
  if ($e.settings -and $packSettings) {
    foreach ($field in @("lastAmpIdx", "volumCustomMainId", "volumActivePresetId", "liteMode")) {
      Assert-Equal "[everything] machine setting '$field' restored" $packSettings.$field $e.settings.$field
    }
    # Compared after the receiver quit: the restored rig has to survive its own
    # next settings save, not just land in the file.
    Assert-Equal "[everything] per-amp scenes restored" (ConvertTo-Canon $packSettings.amps) (ConvertTo-Canon $e.settings.amps)
    Assert-Equal "[everything] POST effects restored" (ConvertTo-Canon $packSettings.effects) (ConvertTo-Canon $e.settings.effects)
  }
  else { Assert-True "[everything] machine settings readable" $false }
  Assert-True "[everything] log records the settings import" ($e.log -match "\[pack\] import overwrite \+settings: applied")

  # Everything without the box: library only, machine and MIDI slots untouched.
  $n = Invoke-PackImportFresh "everything-nosettings" $everything $null
  Assert-PackLanded "everything-nosettings" $regA $rootA $n.reg $n.root $allAmps $allIrs $allPedals $allPresets
  Assert-Equal "[everything-nosettings] PLAY map not applied without the box" "" (Get-MapKey $n.reg)
  Assert-True "[everything-nosettings] machine settings not restored" (
    $n.settings -and $n.settings.lastAmpIdx -ne $packSettings.lastAmpIdx) ("lastAmpIdx " + $n.settings.lastAmpIdx)
  if (-not $KeepSandbox) { Remove-Item $n.sandbox -Recurse -Force -ErrorAction SilentlyContinue }

  $s = Invoke-PackImportFresh "sounds" $sounds $null
  Assert-PackLanded "sounds" $regA $rootA $s.reg $s.root @($ampId) @($irId) @($pedalId) @($skelId, $leadBoost.id)
  Assert-Equal "[sounds] a Share Pack brings no MIDI slots" "" (Get-MapKey $s.reg)
  if (-not $KeepSandbox) { Remove-Item $s.sandbox -Recurse -Force -ErrorAction SilentlyContinue }

  $a = Invoke-PackImportFresh "amp" $amp $null
  Assert-PackLanded "amp" $regA $rootA $a.reg $a.root @($ampId) @() @($pedalId) @($skelId)
  if (-not $KeepSandbox) { Remove-Item $a.sandbox -Recurse -Force -ErrorAction SilentlyContinue }

  # The verbs, against the library that already holds everything: a renamed
  # preset and replaced capture bytes stand for "mine", plus one local-only preset.
  $vRoot = $e.root
  $vContent = Join-Path $vRoot "content\volum-content.json"
  $vReg = Read-Json $vContent
  if ($vReg) {
    foreach ($pr in @($vReg.presetBanks.$ampId)) { if ($pr.id -eq $skelId) { $pr.name = "Mine Renamed" } }
    $vReg.presetBanks | Add-Member -NotePropertyName "factory:0" -NotePropertyValue @([pscustomobject]@{
        id = "preset_e2e_local"; name = "Local Only"; settings = [pscustomobject]@{}
      }) -Force
    $vReg | ConvertTo-Json -Depth 100 | Set-Content $vContent -Encoding UTF8
    $capRel = @($regA.customAmps[0].files | Where-Object { $_.storedPath })[0].storedPath
    $capFile = Get-StoredFile $vRoot $capRel
    [IO.File]::WriteAllText($capFile, "LOCAL-CAPTURE-BYTES")
    # Point the local catalog at a different leaf so Add writes the Pack path as an
    # orphan and Overwrite leaves the local leaf unreferenced.
    $localRel = ($capRel -replace '\.nam$', '_local.nam')
    $localFile = Get-StoredFile $vRoot $localRel
    New-Item -ItemType Directory -Path (Split-Path $localFile) -Force | Out-Null
    [IO.File]::WriteAllText($localFile, "LOCAL-CAPTURE-BYTES")
    foreach ($a in @($vReg.customAmps)) {
      foreach ($f in @($a.files)) { if ($f.storedPath -eq $capRel) { $f.storedPath = $localRel } }
    }
    $vReg | ConvertTo-Json -Depth 100 | Set-Content $vContent -Encoding UTF8
    Remove-Item $capFile -Force -ErrorAction SilentlyContinue
    $localSha = Get-FileSha $localFile
    $packSha = Get-FileSha (Get-StoredFile $rootA $capRel)
    $verbState = @{}
    $vCap = Join-Path $e.sandbox "capture"
    $readState = {
      $r = Read-Json $vContent
      $ix = Get-PresetIndex $r
      $ampPath = @($r.customAmps[0].files | Where-Object { $_.storedPath })[0].storedPath
      @{ skelName = $(if ($ix[$skelId]) { $ix[$skelId].name } else { "" }); local = $ix.ContainsKey("preset_e2e_local")
        sha = (Get-FileSha (Get-StoredFile $vRoot $ampPath))
        packPathExists = [bool](Test-Path (Get-StoredFile $vRoot $capRel))
        localPathExists = [bool](Test-Path (Get-StoredFile $vRoot $localRel))
        ampPath = $ampPath }
    }
    $runV = Invoke-VoLumRun -SandboxRoot $e.sandbox -SettleSec 7 `
      -Environment @{ VOLUM_PACK_OPEN_PATH = $everything; VOLUM_SELF_CAPTURE_DIR = $vCap } -Drive {
      param($proc)
      $h = [VoLumE2eUi]::PlugWindow($proc.MainWindowHandle)
      if ($h -eq [IntPtr]::Zero) { throw "no IPlugWndClass child under the main window" }
      Invoke-PackClick $h $ui.gear
      Invoke-PackClick $h $ui.system
      Invoke-PackClick $h $ui.import 900
      Invoke-PackClick $h $ui.verbAdd
      Save-PackShot $vCap "06-import-verb-add"
      Save-PackShot $vCap "23-import-keep-mine"
      Invoke-PackClick $h $ui.go 1500
      $verbState.add = & $readState
      Invoke-PackClick $h $ui.import 900
      Invoke-PackClick $h $ui.go 1500
      $verbState.overwrite = & $readState
      Invoke-PackClick $h $ui.import 900
      Invoke-PackClick $h $ui.verbReset
      Save-PackShot $vCap "06-import-verb-reset"
      Invoke-PackClick $h $ui.go 1500
      $verbState.reset = & $readState
    }
    Assert-True "[verbs] app opened" $runV.started
    Assert-True "[verbs] app closed gracefully" $runV.graceful
    if ($verbState.add) {
      Assert-Equal "[verbs] Add keeps my preset name" "Mine Renamed" $verbState.add.skelName
      Assert-Equal "[verbs] Add keeps my capture bytes" $localSha $verbState.add.sha
      Assert-True "[verbs] Add keeps my local-only preset" $verbState.add.local
      Assert-equal "[verbs] Add keeps my catalog path" $localRel $verbState.add.ampPath
      Assert-True "[verbs] Add deletes the unreferenced Pack payload" (-not $verbState.add.packPathExists)
      Assert-True "[verbs] Add keeps my payload file" $verbState.add.localPathExists
    }
    else { Assert-True "[verbs] Add ran" $false }
    if ($verbState.overwrite) {
      Assert-Equal "[verbs] Overwrite takes the Pack's name" "Skeleton Lead" $verbState.overwrite.skelName
      Assert-Equal "[verbs] Overwrite takes the Pack's capture bytes" $packSha $verbState.overwrite.sha
      Assert-True "[verbs] Overwrite keeps my local-only preset" $verbState.overwrite.local
      Assert-equal "[verbs] Overwrite takes the Pack catalog path" $capRel $verbState.overwrite.ampPath
      Assert-True "[verbs] Overwrite keeps the Pack payload" $verbState.overwrite.packPathExists
      Assert-True "[verbs] Overwrite deletes my replaced payload" (-not $verbState.overwrite.localPathExists)
    }
    else { Assert-True "[verbs] Overwrite ran" $false }
    if ($verbState.reset) {
      Assert-True "[verbs] Reset deletes my local-only preset" (-not $verbState.reset.local)
      Assert-Equal "[verbs] Reset keeps the Pack's preset" "Skeleton Lead" $verbState.reset.skelName
    }
    else { Assert-True "[verbs] Reset ran" $false }
    $vAfter = Read-Json $vContent
    Assert-PackLanded "verbs after Reset" $regA $rootA $vAfter $vRoot $allAmps $allIrs $allPedals $allPresets
  }
  else { Assert-True "[verbs] library readable" $false }
  if (-not $KeepSandbox) { Remove-Item $e.sandbox -Recurse -Force -ErrorAction SilentlyContinue }

  # A truncated download: refused with a reason, nothing written.
  $bytes = [IO.File]::ReadAllBytes($everything)
  $corrupt = Join-Path $packDir "truncated.volumpack"
  $cut = New-Object byte[] ([int]($bytes.Length * 0.6))
  [Array]::Copy($bytes, $cut, $cut.Length)
  [IO.File]::WriteAllBytes($corrupt, $cut)
  $c = Invoke-PackImportFresh "corrupt" $corrupt $null
  if ($ShotsDir -and (Test-Path (Join-Path $ShotsDir "06-import-corrupt.png"))) {
    Copy-Item (Join-Path $ShotsDir "06-import-corrupt.png") (Join-Path $ShotsDir "23-import-damaged.png") -Force
  }
  Assert-True "[corrupt] refusal logged with Pack copy" ($c.log -match "\[pack\] open refused: This Pack is damaged\.") `
  (($c.log -split "`n" | Where-Object { $_ -match "\[pack\]" }) -join " / ")
  Assert-True "[corrupt] refusal log keeps the technical reason" ($c.log -match "\[pack\] open refused: This Pack is damaged\. \(.+\)")
  Assert-True "[corrupt] no import ran" ($c.log -notmatch "\[pack\] import ")
  $cr = $c.reg
  Assert-True "[corrupt] library holds nothing" (-not $cr -or (
      @($cr.customAmps).Count + @($cr.irLibrary).Count + @($cr.customPedals).Count + (Get-PresetIndex $cr).Count -eq 0))
  $stray = @(Get-ChildItem (Join-Path $c.root "content") -Recurse -File -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -notin @("volum-content.json", "volum-content.lock") })
  Assert-True "[corrupt] no payload file written" ($stray.Count -eq 0) (($stray | ForEach-Object { $_.FullName }) -join ", ")
  if (-not $KeepSandbox) {
    Remove-Item $c.sandbox -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $sandA -Recurse -Force -ErrorAction SilentlyContinue
  }
}

# --------------------------------------------------------------------------

Get-Process -Name VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

Write-Host ("VoLum standalone end-to-end scenarios") -ForegroundColor White
Write-Host ("  exe:  {0}" -f $Exe)
Write-Host ("  seed: {0}" -f $SeedFrom)

if ($Scenario -in @("all", "fresh")) { Test-Fresh }
if ($Scenario -in @("all", "roundtrip")) { Test-Roundtrip }
if ($Scenario -in @("all", "custom")) { Test-CustomLane }
if ($Scenario -in @("all", "brokenrefs")) { Test-BrokenRefs }
if ($Scenario -in @("all", "future")) { Test-FutureSchema }
if ($Scenario -in @("all", "upgrade")) { Test-Upgrade }
if ($Scenario -in @("all", "presets")) { Test-PresetMemory }
if ($Scenario -in @("all", "corrupt")) { Test-Corrupt }
if ($Scenario -in @("all", "samplerate")) { Test-SampleRate }
if ($Scenario -in @("all", "savedialog")) { Test-SaveDialog }
if ($Scenario -in @("all", "pack")) { Test-Pack }

Write-Host ""
if ($script:Failures.Count -eq 0) {
  Write-Host ("ALL PASS  {0} checks" -f $script:Checks) -ForegroundColor Green
  exit 0
}
Write-Host ("FAILED  {0} of {1} checks" -f $script:Failures.Count, $script:Checks) -ForegroundColor Red
$script:Failures | ForEach-Object { Write-Host ("  - {0}" -f $_) -ForegroundColor Red }
exit 1
