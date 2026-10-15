# Prove this run's Windows installer upgrades the published 1.2.x releases cleanly.
#
# CI only. It installs into Program Files and Common Files\VST3, runs the real
# uninstaller, allows desktop apps microphone access and writes a VoLum library under
# %LOCALAPPDATA%. On a developer machine that replaces the installed VoLum and puts
# the real library in reach, so it refuses unless GITHUB_ACTIONS=true or
# -IKnowThisIsAThrowawayVm is passed, before anything is downloaded, installed or written.
#
# One job, both legs in sequence (v1.2.3, then v1.2.2) rather than a matrix: the job
# is looked up by its exact name, which a matrix suffix would change. A runner cannot
# be reset mid-job, so each leg ends with the silent uninstall it asserts on, and the
# next leg starts only if the machine has no VoLum program folder, VST3, uninstall
# entry or library left.
#
# Negative control first: the version checker every leg uses is run against this
# run's own exe and VST3 (from the portable zip in the same artifact) with the right
# version, which must pass, and with 9.9.9, which must fail on both files. Only then
# does it print NEGATIVE CONTROL OK.
#
# Per leg:
#   1. Silent install of the old setup; exe and VST3 report the old version; one entry.
#   2. Seed a 1.2.x library: launch the installed old standalone on a temp profile
#      (VoLum resolves every state path from LOCALAPPDATA) and close it gracefully.
#      The result is mirrored into the runner's own %LOCALAPPDATA%\VoLum, where an
#      installer that touched user data would hit it, and every file is hashed.
#   3. Silent install of this run's setup: exit 0, exe and VST3 report the new
#      version, exactly one VoLum uninstall entry (VoLum.iss has no AppId, so it is
#      VoLum_is1 for every release), every rig file VoLum.iss ships is present with
#      the repo's bytes, library hash unchanged.
#   4. e2e-standalone-win.ps1 scenarios fresh and upgrade against the installed exe
#      (sandboxed LOCALAPPDATA, seeded from a separate copy of the 1.2.x library).
#   5. The setup again (repair): exit 0, everything in 3 still holds, and the
#      installed exe, VST3 and rigs are byte-identical to before.
#   6. Silent uninstall: program folder, VST3, uninstall entry and rigs registry key
#      gone, library hash unchanged.
#
# The standalone needs OpenGL 2.0 and an audio device it may capture from to open its
# window and close cleanly: the CI job provisions Mesa and a virtual audio cable before
# calling this, and this script allows desktop apps microphone access.
#
#   pwsh NeuralAmpModeler/scripts/installer-upgrade-win.ps1 -ArtifactDir <VoLum-win artifact>

[CmdletBinding()]
param(
  # The VoLum-win artifact of this run: installer\VoLum-Setup.exe and out\*.zip.
  [string]$ArtifactDir,
  [string[]]$FromTags = @("v1.2.3", "v1.2.2"),
  [int]$LaunchTimeoutSec = 180,
  [switch]$IKnowThisIsAThrowawayVm
)

if ($env:GITHUB_ACTIONS -ne "true" -and -not $IKnowThisIsAThrowawayVm) {
  Write-Host "REFUSED: installer-upgrade-win.ps1 installs VoLum into Program Files, uninstalls it, allows desktop"
  Write-Host "apps microphone access and writes a VoLum library under %LOCALAPPDATA%. It runs on GitHub Actions"
  Write-Host "(GITHUB_ACTIONS=true) or with -IKnowThisIsAThrowawayVm on a machine you are prepared to lose."
  Write-Host "Nothing was changed."
  exit 2
}

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = (Resolve-Path (Join-Path $here "..")).Path
$e2eScript = Join-Path $here "e2e-standalone-win.ps1"
$issPath = Join-Path $slnDir "installer\VoLum.iss"

$installDir = Join-Path $env:ProgramFiles "VoLum"
$installedExe = Join-Path $installDir "VoLum.exe"
$vst3Dir = Join-Path $env:ProgramFiles "Common Files\VST3\VoLum.vst3"
$vst3Binary = Join-Path $vst3Dir "Contents\x86_64-win\VoLum.vst3"
$rigsRegKey = "HKLM:\Software\VoLum\NeuralAmpModeler"
$library = Join-Path $env:LOCALAPPDATA "VoLum"
$tempRoot = $env:RUNNER_TEMP
if (-not $tempRoot) { $tempRoot = [IO.Path]::GetTempPath() }
$work = Join-Path $tempRoot "volum-installer-upgrade"

$script:Checks = 0
$script:Failures = @()
$script:Leg = "setup"
$script:LibraryHash = $null
$script:CreatedLibrary = $false

function Assert-True {
  param([string]$What, [bool]$Ok, [string]$Detail = "")
  $script:Checks++
  $label = "[{0}] {1}" -f $script:Leg, $What
  if ($Ok) {
    Write-Host ("  PASS  {0}" -f $label)
  }
  else {
    $script:Failures += $label
    Write-Host ("  FAIL  {0}{1}" -f $label, $(if ($Detail) { " - $Detail" } else { "" }))
  }
}

function Assert-Equal {
  param([string]$What, $Expected, $Actual)
  Assert-True $What ($Expected -eq $Actual) ("expected '{0}', got '{1}'" -f $Expected, $Actual)
}

# Every version assertion goes through here, the negative control included.
function Get-VersionProblems {
  param($Files, [string]$Expected)
  $problems = @()
  foreach ($label in $Files.Keys) {
    $path = $Files[$label]
    if (-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
      $problems += ("{0} missing at {1}" -f $label, $path)
      continue
    }
    $reported = ([string](Get-Item -LiteralPath $path).VersionInfo.ProductVersion).Trim()
    if ($reported -ne $Expected) {
      $problems += ("{0} reports '{1}', expected '{2}'" -f $label, $reported, $Expected)
    }
  }
  return $problems
}

function Assert-Versions {
  param([string]$Expected, [string]$When)
  $files = [ordered]@{ "VoLum.exe" = $installedExe; "VST3" = $vst3Binary }
  $problems = @(Get-VersionProblems $files $Expected)
  Assert-True ("VoLum.exe and the VST3 report {0} {1}" -f $Expected, $When) ($problems.Count -eq 0) ($problems -join "; ")
}

function Get-VoLumUninstallEntries {
  $roots = @(
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall",
    "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall",
    "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
  )
  $found = @()
  foreach ($root in $roots) {
    if (-not (Test-Path $root)) { continue }
    foreach ($key in Get-ChildItem -LiteralPath $root -ErrorAction SilentlyContinue) {
      $p = Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction SilentlyContinue
      # Inno writes "VoLum version 1.2.3" when AppVerName is unset.
      if ($p -and $p.DisplayName -and ([string]$p.DisplayName -match '^VoLum(\s|$)')) {
        $found += [pscustomobject]@{
          Key             = $key.PSChildName
          Root            = $root
          DisplayName     = [string]$p.DisplayName
          DisplayVersion  = [string]$p.DisplayVersion
          InstallLocation = [string]$p.InstallLocation
        }
      }
    }
  }
  return $found
}

function Assert-OneEntry {
  param([string]$Version, [string]$When)
  $entries = @(Get-VoLumUninstallEntries)
  $desc = ($entries | ForEach-Object { "{0}\{1} '{2}' {3}" -f $_.Root, $_.Key, $_.DisplayName, $_.DisplayVersion }) -join "; "
  Assert-True ("exactly one VoLum uninstall entry {0}" -f $When) ($entries.Count -eq 1) ("found {0}: {1}" -f $entries.Count, $desc)
  if ($entries.Count -ne 1) { return }
  $e = $entries[0]
  Assert-Equal ("uninstall entry key is VoLum_is1 {0}" -f $When) "VoLum_is1" $e.Key
  Assert-Equal ("uninstall entry reports {0} {1}" -f $Version, $When) $Version $e.DisplayVersion
  Assert-Equal ("uninstall entry points at {0} {1}" -f $installDir, $When) $installDir.TrimEnd('\') $e.InstallLocation.TrimEnd('\')
}

# Relative path -> SHA256 of every file under $Root, sorted. An absent root is empty.
function Get-TreeHash {
  param([string]$Root)
  $map = [ordered]@{}
  if (-not (Test-Path -LiteralPath $Root)) { return $map }
  $base = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\') + '\'
  foreach ($f in Get-ChildItem -LiteralPath $Root -Recurse -File -Force | Sort-Object FullName) {
    $map[$f.FullName.Substring($base.Length)] = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
  }
  return $map
}

function Compare-TreeHash {
  param($Before, $After)
  $diff = @()
  foreach ($k in $Before.Keys) {
    if (-not $After.Contains($k)) { $diff += "removed $k" }
    elseif ($After[$k] -ne $Before[$k]) { $diff += "changed $k" }
  }
  foreach ($k in $After.Keys) {
    if (-not $Before.Contains($k)) { $diff += "added $k" }
  }
  return $diff
}

function Assert-LibraryUnchanged {
  param([string]$When)
  $now = Get-TreeHash $library
  $diff = @(Compare-TreeHash $script:LibraryHash $now)
  Assert-True ("library at {0} unchanged {1} ({2} files)" -f $library, $When, $now.Count) ($diff.Count -eq 0) ($diff -join "; ")
}

# What the installer ships besides the exe and the VST3: every rig line of VoLum.iss,
# expanded against the repo rigs/ it was built from.
function Get-BundledRigFiles {
  $installerDir = Split-Path -Parent $issPath
  $list = @()
  foreach ($line in Get-Content -LiteralPath $issPath) {
    if ($line -notmatch '^\s*Source:\s*"(?<src>\.\.\\\.\.\\rigs\\[^"]+)";\s*DestDir:\s*"\{app\}\\(?<dest>VoLumRigs[^"]*)"') { continue }
    $src = Join-Path $installerDir $Matches["src"]
    $dest = $Matches["dest"].TrimEnd('\')
    $parent = Split-Path -Parent $src
    # skipifsourcedoesntexist: an optional folder the build did not have ships nothing.
    if (-not (Test-Path -LiteralPath $parent)) { continue }
    foreach ($f in Get-ChildItem -LiteralPath $parent -Filter (Split-Path -Leaf $src) -File) {
      $list += [pscustomobject]@{
        Rel  = Join-Path $dest $f.Name
        Hash = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
      }
    }
  }
  return $list
}

function Assert-BundledRigs {
  param([string]$When)
  $missing = @()
  $differ = @()
  foreach ($r in $script:Rigs) {
    $p = Join-Path $installDir $r.Rel
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { $missing += $r.Rel; continue }
    if ((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash -ne $r.Hash) { $differ += $r.Rel }
  }
  Assert-True ("all {0} bundled rig files present {1}" -f $script:Rigs.Count, $When) ($missing.Count -eq 0) `
    ("missing {0}: {1}" -f $missing.Count, (($missing | Select-Object -First 10) -join ", "))
  Assert-True ("bundled rig files carry the repo's bytes {0}" -f $When) ($differ.Count -eq 0) `
    ("differ {0}: {1}" -f $differ.Count, (($differ | Select-Object -First 10) -join ", "))
  $root = $null
  if (Test-Path $rigsRegKey) { $root = (Get-ItemProperty -Path $rigsRegKey -Name "VoLumRigsRoot" -ErrorAction SilentlyContinue).VoLumRigsRoot }
  Assert-Equal ("VoLumRigsRoot points at the installed rigs {0}" -f $When) (Join-Path $installDir "VoLumRigs") $root
}

# The installed exe, VST3 and rigs. The uninstaller and its log change on every run.
function Get-PayloadHash {
  $m = [ordered]@{}
  foreach ($e in (Get-TreeHash $installDir).GetEnumerator()) {
    if ($e.Key -match '^(unins\d+\.(exe|dat)|InstallationLogFile\.log)$') { continue }
    $m["app\" + $e.Key] = $e.Value
  }
  foreach ($e in (Get-TreeHash $vst3Dir).GetEnumerator()) { $m["vst3\" + $e.Key] = $e.Value }
  return $m
}

function Invoke-Setup {
  param([string]$Exe, [string]$Label)
  $log = Join-Path $work ("{0}.log" -f $Label)
  Write-Host ("  {0}: {1} /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG={2}" -f $Label, $Exe, $log)
  $p = Start-Process -FilePath $Exe -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LOG=`"$log`"") -Wait -PassThru
  if ($p.ExitCode -ne 0 -and (Test-Path $log)) {
    Get-Content $log -Tail 40 | ForEach-Object { Write-Host ("    {0}" -f $_) }
  }
  return $p.ExitCode
}

function Invoke-Uninstall {
  param([string]$Label)
  $unins = Join-Path $installDir "unins000.exe"
  if (-not (Test-Path $unins)) { return $null }
  $log = Join-Path $work ("{0}.log" -f $Label)
  Write-Host ("  {0}: {1} /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG={2}" -f $Label, $unins, $log)
  $p = Start-Process -FilePath $unins -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/LOG=`"$log`"") -Wait -PassThru
  # The uninstaller finishes from a copy of itself in %TEMP% so it can delete its own exe.
  $deadline = (Get-Date).AddSeconds(120)
  while ((Get-Date) -lt $deadline) {
    if (-not (Test-Path $installDir) -and -not (Test-Path $vst3Dir) -and @(Get-VoLumUninstallEntries).Count -eq 0) { break }
    Start-Sleep -Seconds 1
  }
  if ($p.ExitCode -ne 0 -and (Test-Path $log)) {
    Get-Content $log -Tail 40 | ForEach-Object { Write-Host ("    {0}" -f $_) }
  }
  return $p.ExitCode
}

Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class VoLumUpgradeUi
{
  private delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc f, IntPtr lParam);
  [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetWindowText(IntPtr hWnd, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

  public static List<IntPtr> VisibleWindows(int pid)
  {
    var found = new List<IntPtr>();
    EnumWindows((h, l) => {
      uint owner;
      GetWindowThreadProcessId(h, out owner);
      if (owner == (uint)pid)
      {
        if (IsWindowVisible(h)) found.Add(h);
      }
      return true;
    }, IntPtr.Zero);
    return found;
  }

  public static string Title(IntPtr hWnd)
  {
    var s = new StringBuilder(256);
    GetWindowText(hWnd, s, s.Capacity);
    return s.ToString();
  }
}
'@

function Write-AudioEndpoints {
  foreach ($flow in @("Render", "Capture")) {
    $root = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\$flow"
    foreach ($ep in Get-ChildItem -LiteralPath $root -ErrorAction SilentlyContinue) {
      $state = (Get-ItemProperty -LiteralPath $ep.PSPath -Name DeviceState -ErrorAction SilentlyContinue).DeviceState
      $props = Get-ItemProperty -LiteralPath (Join-Path $ep.PSPath "Properties") -ErrorAction SilentlyContinue
      $name = $null
      $fmt = $null
      if ($props) {
        $name = $props."{a45c254e-df1c-4efd-8020-67d146a850e0},2"
        $fmt = $props."{f19f064d-082c-4e27-bc73-6882a1bb8e4c},0"
      }
      $format = ""
      if ($fmt -is [byte[]] -and $fmt.Length -ge 24) {
        $format = "{0} ch, {1} Hz, {2} bit" -f [BitConverter]::ToUInt16($fmt, 10), [BitConverter]::ToUInt32($fmt, 12), [BitConverter]::ToUInt16($fmt, 22)
      }
      Write-Host ("  {0} endpoint '{1}': state {2}, {3}" -f $flow, $name, $state, $format)
    }
  }
}

# The standalone opens the default capture device. Windows lets a desktop app capture
# only while the user allows microphone access, which the hosted runner's user denies;
# the refused open is a modal Audio Error that blocks every graceful close. It applies
# to processes started afterwards.
function Grant-DesktopMicrophoneAccess {
  $consent = "SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone"
  foreach ($key in @("HKLM:\$consent", "HKCU:\$consent", "HKCU:\$consent\NonPackaged")) {
    $was = "(unset)"
    if (Test-Path $key) {
      $v = (Get-ItemProperty -Path $key -Name Value -ErrorAction SilentlyContinue).Value
      if ($v) { $was = $v }
    }
    else {
      New-Item -Path $key -Force | Out-Null
    }
    Set-ItemProperty -Path $key -Name Value -Value "Allow" -Type String
    Write-Host ("  microphone access {0}: {1} -> Allow" -f $key, $was)
  }
}

# DirectSound on the default devices, as e2e-standalone-win.ps1 seeds its sandboxes.
function Write-AudioConfig {
  param([string]$Path)
  @("[audio]", "driver=0", "indev=Default Device", "outdev=Default Device", "in1=1", "out1=1", "out2=2",
    "buffer=512", "sr=48000") | Set-Content -LiteralPath $Path -Encoding ASCII
}

# Launch the installed standalone with LOCALAPPDATA pointed at $ProfileDir and close it
# the way a user would, so its shutdown save writes the library. Any message box is
# logged and answered with OK; one that keeps the window from opening fails the seed.
function Invoke-SeedLaunch {
  param([string]$Exe, [string]$ProfileDir)
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $Exe
  $psi.UseShellExecute = $false
  $psi.EnvironmentVariables["LOCALAPPDATA"] = $ProfileDir
  $proc = [System.Diagnostics.Process]::Start($psi)
  $result = [ordered]@{ window = $false; exitCode = $null; notices = @() }

  $answerBoxes = {
    $mainWindow = [IntPtr]::Zero
    foreach ($w in [VoLumUpgradeUi]::VisibleWindows($proc.Id)) {
      $title = [VoLumUpgradeUi]::Title($w)
      if ($title -eq "VoLum") { $mainWindow = $w; continue }
      if ($result.notices -notcontains $title) {
        $result.notices += $title
        Write-Host ("    message box: '{0}' (answered OK)" -f $title)
      }
      [void][VoLumUpgradeUi]::PostMessage($w, 0x0111, [IntPtr]1, [IntPtr]::Zero) # WM_COMMAND IDOK
    }
    $mainWindow
  }

  $main = [IntPtr]::Zero
  $deadline = (Get-Date).AddSeconds($LaunchTimeoutSec)
  while ((Get-Date) -lt $deadline) {
    $proc.Refresh()
    if ($proc.HasExited) { break }
    $main = & $answerBoxes
    if ($main -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 250
  }

  if ($main -ne [IntPtr]::Zero) {
    $result.window = $true
    $settleEnd = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $settleEnd) { [void](& $answerBoxes); Start-Sleep -Milliseconds 250 }
    [void][VoLumUpgradeUi]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) # WM_CLOSE
  }
  if ($proc.WaitForExit(30000)) {
    $result.exitCode = $proc.ExitCode
  }
  else {
    Write-Host "    standalone did not exit within 30 s; killed"
    $proc.Kill()
    [void]$proc.WaitForExit(10000)
  }
  Write-Host ("    window opened: {0}, exit code: {1}" -f $result.window, $result.exitCode)
  return $result
}

function Read-JsonFile {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { return $null }
  try { return (Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json) }
  catch { return $null }
}

function Invoke-E2e {
  param([string]$Scenario, [string]$SeedDir)
  $psExe = (Get-Process -Id $PID).Path
  Write-Host ("  ---- e2e-standalone-win.ps1 -Scenario {0} ----" -f $Scenario)
  & $psExe -NoProfile -ExecutionPolicy Bypass -File $e2eScript -Exe $installedExe -Scenario $Scenario `
    -SeedFrom $SeedDir -LaunchTimeoutSec $LaunchTimeoutSec
  $code = $LASTEXITCODE
  Write-Host ("  ---- e2e {0} exit code {1} ----" -f $Scenario, $code)
  Assert-True ("e2e-standalone-win.ps1 -Scenario {0} passes against {1}" -f $Scenario, $installedExe) ($code -eq 0) ("exit code {0}" -f $code)
}

function Test-MachineClean {
  param([string]$When)
  $before = $script:Failures.Count
  Assert-True ("no VoLum program folder {0}" -f $When) (-not (Test-Path $installDir))
  Assert-True ("no VoLum VST3 {0}" -f $When) (-not (Test-Path $vst3Dir))
  Assert-True ("no VoLum uninstall entry {0}" -f $When) (@(Get-VoLumUninstallEntries).Count -eq 0)
  Assert-True ("no VoLum library at {0} {1}" -f $library, $When) (-not (Test-Path $library))
  return ($script:Failures.Count -eq $before)
}

function Invoke-NegativeControl {
  $script:Leg = "negative control"
  Write-Host "`n[negative control] the version checker must reject a wrong version"
  $zips = @(Get-ChildItem -LiteralPath (Join-Path $ArtifactDir "out") -Filter "*.zip" -File -ErrorAction SilentlyContinue)
  if ($zips.Count -eq 0) { throw "No portable zip under $ArtifactDir\out to run the negative control against." }
  $dir = Join-Path $work "negative-control"
  $i = 0
  foreach ($zip in $zips) {
    Expand-Archive -LiteralPath $zip.FullName -DestinationPath (Join-Path $dir ([string]$i)) -Force
    $i++
  }
  $exe = Get-ChildItem -LiteralPath $dir -Recurse -File -Filter "VoLum*.exe" | Select-Object -First 1
  $vst = Get-ChildItem -LiteralPath $dir -Recurse -File -Filter "VoLum.vst3" | Select-Object -First 1
  $files = [ordered]@{
    "portable VoLum.exe" = $(if ($exe) { $exe.FullName } else { $null })
    "portable VST3"      = $(if ($vst) { $vst.FullName } else { $null })
  }
  $right = @(Get-VersionProblems $files $script:NewVersion)
  $wrong = @(Get-VersionProblems $files "9.9.9")
  Write-Host ("  expected {0}: {1}" -f $script:NewVersion, $(if ($right.Count) { $right -join "; " } else { "accepted" }))
  Write-Host ("  expected 9.9.9: {0}" -f $(if ($wrong.Count) { $wrong -join "; " } else { "accepted" }))
  $rejectedForVersion = @($wrong | Where-Object { $_ -match "reports '.+', expected '9\.9\.9'" }).Count
  if ($right.Count -ne 0 -or $rejectedForVersion -ne $files.Count) {
    Write-Host "NEGATIVE CONTROL FAILED: the checker must accept $($script:NewVersion) and reject 9.9.9 on both files."
    exit 1
  }
  Write-Host "NEGATIVE CONTROL OK"
}

function Invoke-Leg {
  param([string]$Tag, [string]$OldSetup)
  $old = $Tag.TrimStart("v")
  $new = $script:NewVersion
  $script:Leg = $Tag
  $legDir = Join-Path $work $Tag
  New-Item -ItemType Directory -Force -Path $legDir | Out-Null
  Write-Host ("`n[{0}] {1} -> {2}" -f $Tag, $old, $new)

  if (-not (Test-MachineClean "before the install")) { throw "machine is not clean; leg skipped" }

  $code = Invoke-Setup $OldSetup "$Tag-install"
  Assert-Equal ("silent {0} install exits 0" -f $old) 0 $code
  if ($code -ne 0) { throw "$old install failed" }
  Assert-Versions $old "after the $old install"
  Assert-OneEntry $old "after the $old install"

  Write-Host ("  seeding a {0} library: {1} on a temp profile" -f $old, $installedExe)
  $seedProfile = Join-Path $legDir "seed-profile"
  $seedLib = Join-Path $seedProfile "VoLum"
  New-Item -ItemType Directory -Force -Path $seedLib | Out-Null
  Write-AudioConfig (Join-Path $seedLib "settings.ini")
  $run = Invoke-SeedLaunch $installedExe $seedProfile
  Assert-True ("{0} standalone opened its window on the temp profile" -f $old) $run.window ($run.notices -join " | ")
  $seedSettings = Read-JsonFile (Join-Path $seedLib "volum-settings.json")
  $seedContent = Read-JsonFile (Join-Path $seedLib "content\volum-content.json")
  Assert-True ("{0} wrote volum-settings.json on quit" -f $old) ($null -ne $seedSettings)
  Assert-True ("{0} wrote content\volum-content.json with a schema version on quit" -f $old) (
    ($null -ne $seedContent) -and ($null -ne $seedContent.schemaVersion))

  Copy-Item -LiteralPath $seedLib -Destination $library -Recurse
  $script:CreatedLibrary = $true
  $e2eSeed = Join-Path $legDir "e2e-seed"
  Copy-Item -LiteralPath $seedLib -Destination $e2eSeed -Recurse
  $script:LibraryHash = Get-TreeHash $library
  Write-Host ("  library hashed: {0} files" -f $script:LibraryHash.Count)
  foreach ($k in $script:LibraryHash.Keys) { Write-Host ("    {0}  {1}" -f $script:LibraryHash[$k], $k) }

  $code = Invoke-Setup $script:NewSetup "$Tag-upgrade"
  Assert-Equal ("silent {0} install over {1} exits 0" -f $new, $old) 0 $code
  if ($code -ne 0) { throw "$new install over $old failed" }
  Assert-Versions $new "after the upgrade"
  Assert-OneEntry $new "after the upgrade"
  Assert-BundledRigs "after the upgrade"
  Assert-LibraryUnchanged "by the upgrade"

  Invoke-E2e "fresh" $e2eSeed
  Invoke-E2e "upgrade" $e2eSeed
  Assert-LibraryUnchanged "by the e2e scenarios"

  $payload = Get-PayloadHash
  $code = Invoke-Setup $script:NewSetup "$Tag-repair"
  Assert-Equal ("second {0} install (repair) exits 0" -f $new) 0 $code
  Assert-Versions $new "after the repair"
  Assert-OneEntry $new "after the repair"
  Assert-BundledRigs "after the repair"
  $diff = @(Compare-TreeHash $payload (Get-PayloadHash))
  Assert-True ("repair leaves the installed exe, VST3 and rigs byte-identical ({0} files)" -f $payload.Count) ($diff.Count -eq 0) ($diff -join "; ")
  Assert-LibraryUnchanged "by the repair"

  Get-Process -Name VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
  $code = Invoke-Uninstall "$Tag-uninstall"
  Assert-Equal "silent uninstall exits 0" 0 $code
  $leftover = @()
  if (Test-Path $installDir) { $leftover = @(Get-ChildItem -LiteralPath $installDir -Recurse -Force | ForEach-Object { $_.FullName }) }
  Assert-True ("uninstall removed {0}" -f $installDir) (-not (Test-Path $installDir)) ("left: " + (($leftover | Select-Object -First 10) -join ", "))
  Assert-True ("uninstall removed {0}" -f $vst3Dir) (-not (Test-Path $vst3Dir))
  Assert-True "uninstall removed the VoLum uninstall entry" (@(Get-VoLumUninstallEntries).Count -eq 0)
  Assert-True ("uninstall removed {0}" -f $rigsRegKey) (-not (Test-Path $rigsRegKey))
  Assert-LibraryUnchanged "by the uninstall"
}

try {
  if (Test-Path $work) { Remove-Item -LiteralPath $work -Recurse -Force }
  New-Item -ItemType Directory -Force -Path $work | Out-Null

  if (-not $ArtifactDir) { throw "Pass -ArtifactDir: the downloaded VoLum-win artifact of this run." }
  $script:NewSetup = Join-Path $ArtifactDir "installer\VoLum-Setup.exe"
  if (-not (Test-Path $script:NewSetup)) { throw "This run's installer is missing: $($script:NewSetup)" }

  $configText = Get-Content -LiteralPath (Join-Path $slnDir "config.h") -Raw
  if ($configText -notmatch '#define\s+PLUG_VERSION_STR\s+"([^"]+)"') { throw "PLUG_VERSION_STR not found in config.h" }
  $script:NewVersion = $Matches[1]

  Write-Host "VoLum Windows installer upgrade"
  Write-Host ("  this run's setup: {0} ({1})" -f $script:NewSetup, $script:NewVersion)
  Write-Host ("  from:             {0}" -f ($FromTags -join ", "))
  Write-Host ("  library:          {0}" -f $library)

  Invoke-NegativeControl

  $script:Leg = "setup"
  Write-Host "`n[setup] runner and inputs"
  $issText = Get-Content -LiteralPath $issPath -Raw
  Assert-True ("VoLum.iss AppVersion matches config.h {0}" -f $script:NewVersion) ($issText -match ("(?m)^AppVersion=" + [regex]::Escape($script:NewVersion) + "\s*$"))
  Assert-True "VoLum.iss sets no AppId, so every release shares VoLum_is1" ($issText -notmatch '(?m)^\s*AppId\s*=')
  $script:Rigs = @(Get-BundledRigFiles)
  Assert-True ("VoLum.iss ships bundled rigs ({0} files, factory-presets.json among them)" -f $script:Rigs.Count) (
    ($script:Rigs.Count -gt 15) -and (@($script:Rigs | Where-Object { $_.Rel -eq "VoLumRigs\factory-presets.json" }).Count -eq 1))

  $audio = Get-Service -Name Audiosrv -ErrorAction SilentlyContinue
  if ($audio -and $audio.Status -ne "Running") {
    Set-Service -Name Audiosrv -StartupType Automatic
    Start-Service -Name Audiosrv
    $audio = Get-Service -Name Audiosrv
  }
  $sound = @(Get-CimInstance Win32_SoundDevice -ErrorAction SilentlyContinue)
  foreach ($s in $sound) { Write-Host ("  sound device: {0} ({1})" -f $s.Name, $s.Status) }
  Write-AudioEndpoints
  Assert-True "Windows Audio service is running" ($null -ne $audio -and $audio.Status -eq "Running")
  Assert-True "an audio device exists for the standalone to open" ($sound.Count -gt 0)
  Grant-DesktopMicrophoneAccess

  $oldSetups = [ordered]@{}
  foreach ($tag in $FromTags) {
    $dir = Join-Path $work ("download-" + $tag)
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $asset = "VoLum-$tag-windows-setup.exe"
    & gh release download $tag --repo guitarlum/VoLum --pattern $asset --dir $dir --clobber
    $path = Join-Path $dir $asset
    Assert-True ("downloaded {0}" -f $asset) (($LASTEXITCODE -eq 0) -and (Test-Path $path))
    $oldSetups[$tag] = $path
  }

  foreach ($tag in $FromTags) {
    try {
      if (-not (Test-Path $oldSetups[$tag])) { throw "no setup downloaded for $tag" }
      Invoke-Leg $tag $oldSetups[$tag]
    }
    catch {
      Assert-True "leg completed" $false $_.Exception.Message
    }
    finally {
      Get-Process -Name VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
      if (Test-Path (Join-Path $installDir "unins000.exe")) {
        Write-Host "  cleanup: VoLum still installed after the leg, uninstalling"
        [void](Invoke-Uninstall "$tag-cleanup-uninstall")
      }
      if ($script:CreatedLibrary -and (Test-Path $library)) { Remove-Item -LiteralPath $library -Recurse -Force }
      $script:CreatedLibrary = $false
    }
  }
}
catch {
  Write-Host ("ERROR: {0}" -f $_.Exception.Message)
  Write-Host $_.ScriptStackTrace
  exit 1
}

Write-Host ""
if ($script:Failures.Count -eq 0) {
  Write-Host ("ALL PASS {0} checks" -f $script:Checks)
  exit 0
}
Write-Host ("FAILED {0} of {1} checks" -f $script:Failures.Count, $script:Checks)
$script:Failures | ForEach-Object { Write-Host ("  - {0}" -f $_) }
exit 1
