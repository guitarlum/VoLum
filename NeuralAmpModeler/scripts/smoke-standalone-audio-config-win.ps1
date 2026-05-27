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

if ($env:GITHUB_ACTIONS -eq "true") {
  if ($StartupSeconds -eq 5) { $StartupSeconds = 60 }
  if ($Buffers.Count -eq 4 -and $Buffers[0] -eq 32) { $Buffers = @(256, 32, 8192) }
}

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
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr FindWindow(string className, string windowName);

  [DllImport("user32.dll")]
  public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern int GetClassName(IntPtr hWnd, System.Text.StringBuilder lpClassName, int nMaxCount);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern bool IsWindowVisible(IntPtr hWnd);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern bool IsWindow(IntPtr hWnd);

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

function Find-VoLumMainWindow {
  $main = [VoLumSmokeWin32]::FindWindow("#32770", "VoLum")
  if ($main -ne [IntPtr]::Zero) {
    return $main
  }

  $process = Get-Process VoLum -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($process -and $process.MainWindowHandle -ne [IntPtr]::Zero) {
    return $process.MainWindowHandle
  }

  $found = [IntPtr]::Zero
  $callback = [VoLumSmokeWin32+EnumWindowsProc]{
    param([IntPtr]$hWnd, [IntPtr]$lParam)
    if (-not [VoLumSmokeWin32]::IsWindowVisible($hWnd)) {
      return $true
    }

    $title = New-Object System.Text.StringBuilder 256
    [VoLumSmokeWin32]::GetWindowText($hWnd, $title, $title.Capacity) | Out-Null
    if ($title.ToString() -eq "VoLum") {
      $script:found = $hWnd
      return $false
    }
    return $true
  }
  [VoLumSmokeWin32]::EnumWindows($callback, [IntPtr]::Zero) | Out-Null
  return $found
}

function Close-VoLum {
  $pref = [VoLumSmokeWin32]::FindWindow("#32770", "Preferences")
  if ($pref -ne [IntPtr]::Zero) {
    [VoLumSmokeWin32]::PostMessage($pref, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null # WM_COMMAND / IDCANCEL
    Start-Sleep -Milliseconds 300
  }

  $main = Find-VoLumMainWindow
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

function Start-VoLumApp {
  $proc = Start-Process $exe -PassThru
  try {
    if (-not $proc.WaitForInputIdle(60000)) {
      Write-Host "WARN: VoLum did not report input idle within 60s; continuing smoke wait."
    }
  }
  catch {
    Write-Host "WARN: WaitForInputIdle unavailable: $($_.Exception.Message)"
  }
  return $proc
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

    $main = Find-VoLumMainWindow
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
  $main = Find-VoLumMainWindow
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

  $inputDeviceId = 40010   # IDC_COMBO_AUDIO_IN_DEV
  $outputDeviceId = 40011  # IDC_COMBO_AUDIO_OUT_DEV
  $bufferSizeId = 40012    # IDC_COMBO_AUDIO_BUF_SIZE
  $inputLId = 40014        # IDC_COMBO_AUDIO_IN_L
  $inputRId = 40015        # IDC_COMBO_AUDIO_IN_R
  $outputRId = 40016       # IDC_COMBO_AUDIO_OUT_R
  $outputLId = 40017       # IDC_COMBO_AUDIO_OUT_L

  foreach ($controlId in @($inputLId, $inputRId, $outputLId, $outputRId)) {
    if (-not (Test-ControlVisible -Dialog $Dialog -ControlId $controlId)) {
      throw "Expected Preferences control $controlId to be visible."
    }
  }

  if ((Get-ComboCount -Dialog $Dialog -ControlId $inputDeviceId) -lt 1) {
    throw "Input device combo has no selectable devices."
  }
  if ((Get-ComboCount -Dialog $Dialog -ControlId $outputDeviceId) -lt 1) {
    throw "Output device combo has no selectable devices."
  }
  if ((Get-ComboCount -Dialog $Dialog -ControlId $inputLId) -lt 1) {
    throw "Input channel combo has no selectable channels."
  }

  $expectedBuffers = @("48", "64", "96", "128", "256", "512", "1024", "2048", "4096", "8192")
  $actualBuffers = @(Get-ComboItems -Dialog $Dialog -ControlId $bufferSizeId)
  if (($actualBuffers -join ",") -ne ($expectedBuffers -join ",")) {
    throw "Buffer size combo order/content is '$($actualBuffers -join ",")', expected '$($expectedBuffers -join ",")'."
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

    Start-VoLumApp | Out-Null
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
  $selectedInputDevice = Get-ComboText -Dialog $pref -ControlId 40010
  $selectedOutputDevice = Get-ComboText -Dialog $pref -ControlId 40011
  if (-not $selectedInputDevice) {
    throw "Preferences input device combo has an empty selection."
  }
  if (-not $selectedOutputDevice) {
    throw "Preferences output device combo has an empty selection."
  }
  [VoLumSmokeWin32]::PostMessage($pref, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null # IDCANCEL
  Write-Host "OK: Preferences exposes separate input/output device combos ('$selectedInputDevice' / '$selectedOutputDevice')"
  $asioSettings = Set-IniValue -Content $originalSettings -Key "driver" -Value "1"
  Set-Content -Path $settingsPath -Value $asioSettings -NoNewline
  Start-VoLumApp | Out-Null
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
  Start-VoLumApp | Out-Null
  Start-Sleep -Seconds 2
}
