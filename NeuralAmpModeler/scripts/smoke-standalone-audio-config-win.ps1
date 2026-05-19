# Smoke-test VoLum standalone audio configuration on Windows.
# Exercises the real APP executable by restarting it with persisted edge-case
# audio settings. This catches startup/reconfiguration crashes that doctests
# cannot cover because RtAudio owns the driver callback.

param(
  [switch] $SkipBuild,
  [switch] $RequireAsioFallback,
  [int[]] $Buffers = @(32, 96, 8192, 192),
  [int] $StartupSeconds = 5
)

$ErrorActionPreference = "Stop"

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = (Resolve-Path (Join-Path $here "..")).Path
$exe = Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe"
$settingsDir = Join-Path $env:LOCALAPPDATA "VoLum"
$settingsPath = Join-Path $settingsDir "settings.ini"

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class VoLumSmokeWin32
{
  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr FindWindow(string className, string windowName);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern bool IsWindow(IntPtr hWnd);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern bool IsWindowVisible(IntPtr hWnd);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern bool PostMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto, EntryPoint="SendMessage")]
  public static extern IntPtr SendMessageInt(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr SendMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, System.Text.StringBuilder lParam);
}
'@

function Get-MSBuild {
  if ($env:GITHUB_ACTIONS -eq "true") {
    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return $cmd.Source }
  }

  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found. Install Visual Studio Build Tools."
  }

  $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
  if (-not $msbuild) {
    Write-Error "MSBuild.exe not found."
  }
  return $msbuild
}

function Close-VoLum {
  $pref = [VoLumSmokeWin32]::FindWindow("#32770", "Preferences")
  if ($pref -ne [IntPtr]::Zero) {
    [VoLumSmokeWin32]::PostMessage($pref, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null # WM_COMMAND / IDCANCEL
    Start-Sleep -Milliseconds 300
  }

  $main = [VoLumSmokeWin32]::FindWindow("#32770", "VoLum")
  if ($main -ne [IntPtr]::Zero) {
    [VoLumSmokeWin32]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null # WM_CLOSE
    Start-Sleep -Seconds 2
  }

  Get-Process VoLum -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

function Close-AudioErrorDialogs {
  foreach ($title in @("Audio Error", "Error")) {
    $dialog = [VoLumSmokeWin32]::FindWindow("#32770", $title)
    if ($dialog -ne [IntPtr]::Zero) {
      [VoLumSmokeWin32]::PostMessage($dialog, 0x0111, [IntPtr]1, [IntPtr]::Zero) | Out-Null # WM_COMMAND / IDOK
      Start-Sleep -Milliseconds 300
    }
  }
}

function Wait-VoLumAlive {
  param([string] $CaseName)

  $process = $null
  for ($i = 0; $i -lt ($StartupSeconds * 4); ++$i) {
    Close-AudioErrorDialogs

    $process = Get-Process VoLum -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $process) {
      Start-Sleep -Milliseconds 250
      continue
    }

    $main = [VoLumSmokeWin32]::FindWindow("#32770", "VoLum")
    if ($main -ne [IntPtr]::Zero) {
      return $process
    }

    Start-Sleep -Milliseconds 250
  }

  if (-not $process) {
    throw "$CaseName failed: VoLum.exe exited during startup."
  }
  throw "$CaseName failed: VoLum window was not found."
}

function Set-IniValue {
  param(
    [string] $Content,
    [string] $Key,
    [string] $Value
  )

  $pattern = "(?m)^$([regex]::Escape($Key))=.*$"
  $replacement = "$Key=$Value"
  if ($Content -match $pattern) {
    return $Content -replace $pattern, $replacement
  }

  return $Content.TrimEnd() + "`r`n$replacement`r`n"
}

function Get-IniValue {
  param(
    [string] $Path,
    [string] $Key
  )

  $match = Select-String -Path $Path -Pattern "^$([regex]::Escape($Key))=" -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $match) { return $null }
  return ($match.Line -split "=", 2)[1]
}

function Open-Preferences {
  $main = [VoLumSmokeWin32]::FindWindow("#32770", "VoLum")
  if ($main -eq [IntPtr]::Zero) {
    throw "Cannot open Preferences: VoLum window was not found."
  }

  [VoLumSmokeWin32]::PostMessage($main, 0x0111, [IntPtr]40006, [IntPtr]::Zero) | Out-Null # WM_COMMAND / ID_PREFERENCES
  for ($i = 0; $i -lt 20; ++$i) {
    Start-Sleep -Milliseconds 250
    $pref = [VoLumSmokeWin32]::FindWindow("#32770", "Preferences")
    if ($pref -ne [IntPtr]::Zero) {
      return $pref
    }
  }

  throw "Preferences dialog did not open."
}

function Get-ComboText {
  param(
    [IntPtr] $Dialog,
    [int] $ControlId
  )

  $combo = [VoLumSmokeWin32]::GetDlgItem($Dialog, $ControlId)
  if ($combo -eq [IntPtr]::Zero) {
    throw "Combo control $ControlId not found."
  }

  $sel = [VoLumSmokeWin32]::SendMessageInt($combo, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() # CB_GETCURSEL
  if ($sel -lt 0) {
    return ""
  }

  $len = [VoLumSmokeWin32]::SendMessageInt($combo, 0x0149, [IntPtr]$sel, [IntPtr]::Zero).ToInt64() # CB_GETLBTEXTLEN
  $sb = New-Object Text.StringBuilder ($len + 1)
  [VoLumSmokeWin32]::SendMessage($combo, 0x0148, [IntPtr]$sel, $sb) | Out-Null # CB_GETLBTEXT
  return $sb.ToString()
}

function Test-ControlVisible {
  param(
    [IntPtr] $Dialog,
    [int] $ControlId
  )

  $control = [VoLumSmokeWin32]::GetDlgItem($Dialog, $ControlId)
  return ($control -ne [IntPtr]::Zero) -and
    [VoLumSmokeWin32]::IsWindow($control) -and
    [VoLumSmokeWin32]::IsWindowVisible($control)
}

function Get-ComboCount {
  param(
    [IntPtr] $Dialog,
    [int] $ControlId
  )

  $combo = [VoLumSmokeWin32]::GetDlgItem($Dialog, $ControlId)
  if ($combo -eq [IntPtr]::Zero) {
    throw "Combo control $ControlId not found."
  }

  return [VoLumSmokeWin32]::SendMessageInt($combo, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() # CB_GETCOUNT
}

function Get-ComboItems {
  param(
    [IntPtr] $Dialog,
    [int] $ControlId
  )

  $combo = [VoLumSmokeWin32]::GetDlgItem($Dialog, $ControlId)
  if ($combo -eq [IntPtr]::Zero) {
    throw "Combo control $ControlId not found."
  }

  $items = @()
  $count = [VoLumSmokeWin32]::SendMessageInt($combo, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() # CB_GETCOUNT
  for ($i = 0; $i -lt $count; ++$i) {
    $len = [VoLumSmokeWin32]::SendMessageInt($combo, 0x0149, [IntPtr]$i, [IntPtr]::Zero).ToInt64() # CB_GETLBTEXTLEN
    $sb = New-Object Text.StringBuilder ($len + 1)
    [VoLumSmokeWin32]::SendMessage($combo, 0x0148, [IntPtr]$i, $sb) | Out-Null # CB_GETLBTEXT
    $items += $sb.ToString()
  }

  return $items
}

function Assert-StandaloneAudioLayout {
  param([IntPtr] $Dialog)

  $audioDeviceId = 40010 # IDC_COMBO_AUDIO_DEV
  $bufferSizeId = 40012 # IDC_COMBO_AUDIO_BUF_SIZE
  $oldOutputDeviceId = 40011
  $audioInputId = 40014 # IDC_COMBO_AUDIO_IN
  $oldInputRId = 40015
  $outputRId = 40016
  $outputLId = 40017

  foreach ($controlId in @($audioDeviceId, $audioInputId, $outputLId, $outputRId)) {
    if (-not (Test-ControlVisible -Dialog $Dialog -ControlId $controlId)) {
      throw "Expected Preferences control $controlId to be visible."
    }
  }

  foreach ($controlId in @($oldOutputDeviceId, $oldInputRId)) {
    if (Test-ControlVisible -Dialog $Dialog -ControlId $controlId) {
      throw "Old Preferences control $controlId is still visible."
    }
  }

  if ((Get-ComboCount -Dialog $Dialog -ControlId $audioDeviceId) -lt 1) {
    throw "Audio device combo has no selectable devices."
  }
  if ((Get-ComboCount -Dialog $Dialog -ControlId $audioInputId) -lt 1) {
    throw "Input channel combo has no selectable channels."
  }

  $expectedBuffers = @("48", "64", "96", "128", "256", "512", "1024", "2048", "4096", "8192")
  $actualBuffers = @(Get-ComboItems -Dialog $Dialog -ControlId $bufferSizeId)
  if (($actualBuffers -join ",") -ne ($expectedBuffers -join ",")) {
    throw "Buffer size combo order/content is '$($actualBuffers -join ",")', expected '$($expectedBuffers -join ",")'."
  }
}

function Assert-IniAudioDevicePair {
  param(
    [string] $ExpectedDevice,
    [string] $CaseName
  )

  $inDev = Get-IniValue -Path $settingsPath -Key "indev"
  $outDev = Get-IniValue -Path $settingsPath -Key "outdev"
  $in2 = Get-IniValue -Path $settingsPath -Key "in2"

  if ($inDev -ne $outDev) {
    throw "$CaseName failed: indev='$inDev' outdev='$outDev', expected one shared audio device."
  }
  if ($ExpectedDevice -and $outDev -ne $ExpectedDevice) {
    throw "$CaseName failed: shared audio device is '$outDev', expected '$ExpectedDevice'."
  }
  if ($in2 -ne (Get-IniValue -Path $settingsPath -Key "in1")) {
    throw "$CaseName failed: in1/in2 did not migrate to the same mono input channel."
  }
}

function Get-ExpectedVisibleBufferSize {
  param([int] $Buffer)

  foreach ($option in @(48, 64, 96, 128, 256, 512, 1024, 2048, 4096, 8192)) {
    if ($Buffer -le $option) {
      return $option
    }
  }
  return 8192
}

if (-not $SkipBuild) {
  Set-Location $slnDir
  & (Join-Path $slnDir "iplug2-patches\apply-iplug2-patches.ps1")
  $msbuild = Get-MSBuild
  & $msbuild "NeuralAmpModeler.sln" /t:NeuralAmpModeler-app /p:Configuration=Release /p:Platform=x64 /m /v:minimal
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $exe)) {
  Write-Error "Standalone executable not found: $exe"
}

New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
$hadOriginalSettings = Test-Path $settingsPath
if ($hadOriginalSettings) {
  $originalSettings = Get-Content $settingsPath -Raw
}
else {
  $originalSettings = @"
[audio]
driver=0
indev=Default Device
outdev=Default Device
in1=1
in2=2
out1=1
out2=2
buffer=256
sr=44100
[midi]
indev=off
outdev=off
inchan=0
outchan=0
"@
}

try {
  foreach ($buffer in $Buffers) {
    Close-VoLum
    $settings = Set-IniValue -Content $originalSettings -Key "driver" -Value "0"
    $settings = Set-IniValue -Content $settings -Key "buffer" -Value "$buffer"
    Set-Content -Path $settingsPath -Value $settings -NoNewline

    Start-Process $exe
    $process = Wait-VoLumAlive -CaseName "buffer=$buffer"
    $expectedBuffer = Get-ExpectedVisibleBufferSize -Buffer $buffer
    $actualBuffer = Get-IniValue -Path $settingsPath -Key "buffer"
    if ($actualBuffer -ne "$expectedBuffer") {
      throw "buffer=$buffer failed: settings.ini has buffer=$actualBuffer, expected normalized buffer=$expectedBuffer."
    }
    Write-Host "OK: startup buffer=$buffer normalized=$expectedBuffer pid=$($process.Id)"
  }

  $pref = Open-Preferences
  Assert-StandaloneAudioLayout -Dialog $pref
  $selectedAudioDevice = Get-ComboText -Dialog $pref -ControlId 40010
  if (-not $selectedAudioDevice) {
    throw "Preferences audio device combo has an empty selection."
  }
  [VoLumSmokeWin32]::PostMessage($pref, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null # IDCANCEL
  Write-Host "OK: Preferences exposes one audio device combo and one mono input combo ('$selectedAudioDevice')"

  Close-VoLum
  $settings = Set-IniValue -Content $originalSettings -Key "driver" -Value "0"
  $settings = Set-IniValue -Content $settings -Key "indev" -Value "__VoLumMissingInputDevice__"
  $settings = Set-IniValue -Content $settings -Key "outdev" -Value $selectedAudioDevice
  $settings = Set-IniValue -Content $settings -Key "in1" -Value "1"
  $settings = Set-IniValue -Content $settings -Key "in2" -Value "2"
  Set-Content -Path $settingsPath -Value $settings -NoNewline
  Start-Process $exe
  $process = Wait-VoLumAlive -CaseName "migration prefers output device"
  Assert-IniAudioDevicePair -ExpectedDevice $selectedAudioDevice -CaseName "migration prefers output device"
  Write-Host "OK: mismatched .ini migrated to previous output device pid=$($process.Id)"

  Close-VoLum
  $settings = Set-IniValue -Content $originalSettings -Key "driver" -Value "0"
  $settings = Set-IniValue -Content $settings -Key "indev" -Value $selectedAudioDevice
  $settings = Set-IniValue -Content $settings -Key "outdev" -Value "__VoLumMissingOutputDevice__"
  $settings = Set-IniValue -Content $settings -Key "in1" -Value "1"
  $settings = Set-IniValue -Content $settings -Key "in2" -Value "2"
  Set-Content -Path $settingsPath -Value $settings -NoNewline
  Start-Process $exe
  $process = Wait-VoLumAlive -CaseName "migration falls back to input device"
  Assert-IniAudioDevicePair -ExpectedDevice $selectedAudioDevice -CaseName "migration falls back to input device"
  Write-Host "OK: missing output device migrated to previous input device pid=$($process.Id)"

  Close-VoLum
  $settings = Set-IniValue -Content $originalSettings -Key "driver" -Value "0"
  $settings = Set-IniValue -Content $settings -Key "indev" -Value "__VoLumMissingInputDevice__"
  $settings = Set-IniValue -Content $settings -Key "outdev" -Value "__VoLumMissingOutputDevice__"
  $settings = Set-IniValue -Content $settings -Key "in1" -Value "1"
  $settings = Set-IniValue -Content $settings -Key "in2" -Value "2"
  Set-Content -Path $settingsPath -Value $settings -NoNewline
  Start-Process $exe
  $process = Wait-VoLumAlive -CaseName "migration falls back to available shared device"
  Assert-IniAudioDevicePair -ExpectedDevice "" -CaseName "migration falls back to available shared device"
  Write-Host "OK: missing mismatched devices fell back to a shared audio device pid=$($process.Id)"

  Close-VoLum
  $asioSettings = Set-IniValue -Content $originalSettings -Key "driver" -Value "1"
  Set-Content -Path $settingsPath -Value $asioSettings -NoNewline
  Start-Process $exe
  $process = Wait-VoLumAlive -CaseName "ASIO fallback"
  $driver = Get-IniValue -Path $settingsPath -Key "driver"

  if ($driver -eq "0") {
    $pref = Open-Preferences
    $driverText = Get-ComboText -Dialog $pref -ControlId 40009
    if ($driverText -ne "DirectSound") {
      throw "ASIO fallback failed: Preferences driver combo shows '$driverText', expected 'DirectSound'."
    }
    [VoLumSmokeWin32]::PostMessage($pref, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null # IDCANCEL
    Write-Host "OK: ASIO unavailable fallback reverted to DirectSound pid=$($process.Id)"
  }
  elseif ($RequireAsioFallback) {
    throw "ASIO fallback was required, but settings.ini still has driver=$driver."
  }
  else {
    Write-Host "SKIP: ASIO fallback was not exercised; settings.ini has driver=$driver."
  }
}
finally {
  Close-VoLum
  if ($hadOriginalSettings) {
    Set-Content -Path $settingsPath -Value $originalSettings -NoNewline
  }
  else {
    Remove-Item -Path $settingsPath -Force -ErrorAction SilentlyContinue
  }
  Start-Process $exe
  Start-Sleep -Seconds 2
}
