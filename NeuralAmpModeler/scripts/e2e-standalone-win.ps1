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
    "samplerate")]
  [string]$Scenario = "all",
  [string]$Exe,
  # Seed state for the round-trip and upgrade scenarios. Defaults to a copy of the
  # live library, which is the only place real .nam/.wav payloads exist on a dev box.
  [string]$SeedFrom,
  [int]$LaunchTimeoutSec = 60,
  [switch]$KeepSandbox
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = (Resolve-Path (Join-Path $here "..")).Path

if (-not $Exe) { $Exe = Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe" }
if (-not (Test-Path $Exe)) {
  Write-Error "VoLum.exe not found at $Exe. Build it first: pwsh $here\run-app-win.ps1"
}
if (-not $SeedFrom) { $SeedFrom = Join-Path $env:LOCALAPPDATA "VoLum" }

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

# Copy the seed library into a sandbox. `settings.ini` comes along so the app picks
# the same audio device instead of prompting on a device it has never seen.
function Copy-SeedState {
  param([string]$SandboxRoot)
  if (-not (Test-Path $SeedFrom)) {
    throw "Seed state not found at $SeedFrom. Pass -SeedFrom, or launch VoLum once to create it."
  }
  Copy-Item (Join-Path $SeedFrom "*") (Join-Path $SandboxRoot "VoLum") -Recurse -Force
  # A log from the seed would make the fresh-log assertions meaningless.
  Remove-Item (Join-Path $SandboxRoot "VoLum\volum.log") -Force -ErrorAction SilentlyContinue
}

function Copy-AudioConfigOnly {
  param([string]$SandboxRoot)
  $ini = Join-Path $SeedFrom "settings.ini"
  if (Test-Path $ini) { Copy-Item $ini (Join-Path $SandboxRoot "VoLum") -Force }
}

# Launch VoLum against a sandboxed LOCALAPPDATA and close it the way a user would.
# CloseMainWindow posts WM_CLOSE, which runs the normal shutdown path - the one that
# saves settings. Killing the process instead would skip the save and make every
# persistence assertion below vacuous, so a hard kill is reported as a failure - and
# so is any non-zero exit, including the watchdog's own.
function Invoke-VoLumRun {
  param([string]$SandboxRoot, [int]$SettleSec = 6)

  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $Exe
  $psi.UseShellExecute = $false
  $psi.EnvironmentVariables["LOCALAPPDATA"] = $SandboxRoot
  $proc = [System.Diagnostics.Process]::Start($psi)

  $deadline = (Get-Date).AddSeconds($LaunchTimeoutSec)
  while ((Get-Date) -lt $deadline) {
    $proc.Refresh()
    if ($proc.HasExited) { break }
    if ($proc.MainWindowHandle -ne 0) { break }
    Start-Sleep -Milliseconds 250
  }

  $result = [ordered]@{ started = $false; graceful = $false; exitCode = $null }
  if ($proc.HasExited) {
    $result.exitCode = $proc.ExitCode
    return $result
  }
  $proc.Refresh()
  $result.started = ($proc.MainWindowHandle -ne 0)

  # Let the editor finish opening, restoring, and running its idle save.
  Start-Sleep -Seconds $SettleSec

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
    $proc.Kill()
    [void]$proc.WaitForExit(10000)
    Write-Host "  window did not close within 20 s; process killed" -ForegroundColor Yellow
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
  Copy-AudioConfigOnly $sandbox
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
    Assert-Equal "registry upgraded to schema v3" 3 $after.schemaVersion
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
  Copy-AudioConfigOnly $sandbox
  $root = Join-Path $sandbox "VoLum"
  New-Item -ItemType Directory -Path (Join-Path $root "content") -Force | Out-Null
  '{ "schemaVersion": 3, "customAmps": [ { "id": "amp_trunc"' |
    Set-Content (Join-Path $root "content\volum-content.json") -Encoding UTF8

  $run = Invoke-VoLumRun -SandboxRoot $sandbox
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
  Copy-AudioConfigOnly $sandbox
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

Write-Host ""
if ($script:Failures.Count -eq 0) {
  Write-Host ("ALL PASS  {0} checks" -f $script:Checks) -ForegroundColor Green
  exit 0
}
Write-Host ("FAILED  {0} of {1} checks" -f $script:Failures.Count, $script:Checks) -ForegroundColor Red
$script:Failures | ForEach-Object { Write-Host ("  - {0}" -f $_) -ForegroundColor Red }
exit 1
