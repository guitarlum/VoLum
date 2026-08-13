# Stress the standalone's sample-rate and buffer-size switching, then quit and
# relaunch. Windows only, and only meaningful on a machine with a real ASIO driver.
#
# Why this exists
# ---------------
# 2026-08-03: a tester spent ten minutes changing the rate and buffer "aggressively"
# in Preferences, closed VoLum, and could not start it again. The process was still
# in the table, could not be ended from Task Manager, and held the single-instance
# mutex. Its one remaining thread was parked in SleepEx inside asio4all64.dll: the
# exit had reached the driver's DLL_PROCESS_DETACH and never came back. Both of
# VoLum's own teardowns had already logged "complete".
#
# Nothing in the unit tests can reach that - it needs a real driver, real switching
# and a real process exit - so this drives the real dialog and then asserts the three
# things the report was about: the process actually goes away, the settings it wrote
# on the way out are there afterwards, and the next launch comes up.
#
# What this does NOT do, so nobody reads more into a green run than is there: it does
# not reproduce the driver wedge. Reverting VoLumExitProcessNow and running five
# cycles of this passed, because ASIO4ALL granted every request that day and never
# got into the state it was in on the reporter's machine. The wedge itself is pinned
# by the source guards in test_iplug_app_shutdown.cpp, and the mechanism was proved
# separately with a DLL that sleeps in DLL_PROCESS_DETACH: returning from main left a
# process that was still listed after 8 s with one thread and HasExited true - the
# reporter's signature exactly - while TerminateProcess was gone in 246 ms.
#
# The audio settings live in a sandboxed LOCALAPPDATA so a run cannot disturb the
# machine's own configuration, seeded from it so the same driver is exercised.

param(
  [int] $Cycles = 4,
  [int] $SwitchesPerCycle = 12,
  [int] $ExitSeconds = 15,
  # Defaults to the local Release build. Point it at an unpacked portable zip to
  # stress the binary that will actually ship, which is where the report came from.
  [string] $Exe
)

$ErrorActionPreference = "Stop"

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = (Resolve-Path (Join-Path $here "..")).Path
$exe = if ($Exe) { (Resolve-Path $Exe).Path } else { Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe" }
$procName = [System.IO.Path]::GetFileNameWithoutExtension($exe)

$realSettings = Join-Path $env:LOCALAPPDATA "VoLum\settings.ini"
$sandbox = Join-Path ([System.IO.Path]::GetTempPath()) ("volum-stress-" + [System.Guid]::NewGuid().ToString("N").Substring(0, 8))
$sandboxData = Join-Path $sandbox "VoLum"
$settingsPath = Join-Path $sandboxData "settings.ini"
$logPath = Join-Path $sandboxData "volum.log"

$IDAPPLY = 40024
$IDC_COMBO_AUDIO_BUF_SIZE = 40012
$IDC_COMBO_AUDIO_SR = 40013
$ID_PREFERENCES = 40006

$failures = New-Object System.Collections.Generic.List[string]

Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class VoLumStressWin32
{
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr FindWindow(string className, string windowName);

  [DllImport("user32.dll")]
  public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);

  [DllImport("user32.dll", CharSet=CharSet.Auto)]
  public static extern bool PostMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto, EntryPoint="SendMessage")]
  public static extern IntPtr SendMessageInt(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll", CharSet=CharSet.Auto, EntryPoint="SendMessage")]
  public static extern IntPtr SendMessageText(IntPtr hWnd, UInt32 Msg, IntPtr wParam, StringBuilder lParam);

  [DllImport("user32.dll")]
  public static extern bool SetForegroundWindow(IntPtr hWnd);

  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern IntPtr OpenMutex(uint access, bool inherit, string name);

  [DllImport("kernel32.dll")]
  public static extern bool CloseHandle(IntPtr h);
}
'@

function Fail {
  param([string] $Message)
  $failures.Add($Message) | Out-Null
  Write-Host "FAIL: $Message" -ForegroundColor Red
}

function Get-Title {
  param([IntPtr] $Window)
  $sb = New-Object System.Text.StringBuilder 256
  [VoLumStressWin32]::GetWindowText($Window, $sb, $sb.Capacity) | Out-Null
  return $sb.ToString()
}

function Find-MainWindow {
  $main = [VoLumStressWin32]::FindWindow("#32770", "VoLum")
  return $main
}

# The rate notice, the audio error and the graphics error are all modal and would
# stall the sweep. Dismissing them is also the only assertion available that the
# notice appears at all, so the count is reported at the end of a cycle.
function Close-Notices {
  $closed = 0
  foreach ($title in @("Sample Rate", "Audio Error", "Graphics Error", "Error")) {
    for ($i = 0; $i -lt 4; ++$i) {
      $dlg = [VoLumStressWin32]::FindWindow("#32770", $title)
      if ($dlg -eq [IntPtr]::Zero) { break }
      [VoLumStressWin32]::PostMessage($dlg, 0x0111, [IntPtr]1, [IntPtr]::Zero) | Out-Null # IDOK
      Start-Sleep -Milliseconds 200
      ++$closed
    }
  }
  return $closed
}

function Open-Preferences {
  $main = Find-MainWindow
  if ($main -eq [IntPtr]::Zero) { return [IntPtr]::Zero }

  [VoLumStressWin32]::SetForegroundWindow($main) | Out-Null
  [VoLumStressWin32]::PostMessage($main, 0x0111, [IntPtr]$ID_PREFERENCES, [IntPtr]::Zero) | Out-Null

  for ($i = 0; $i -lt 40; ++$i) {
    Start-Sleep -Milliseconds 250
    $pref = [VoLumStressWin32]::FindWindow("#32770", "Preferences")
    if ($pref -ne [IntPtr]::Zero) { return $pref }
  }
  return [IntPtr]::Zero
}

function Get-ComboItems {
  param([IntPtr] $Dialog, [int] $ControlId)

  $combo = [VoLumStressWin32]::GetDlgItem($Dialog, $ControlId)
  if ($combo -eq [IntPtr]::Zero) { return @() }

  $count = [VoLumStressWin32]::SendMessageInt($combo, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() # CB_GETCOUNT
  $items = @()
  for ($i = 0; $i -lt $count; ++$i) {
    $sb = New-Object System.Text.StringBuilder 64
    [VoLumStressWin32]::SendMessageText($combo, 0x0148, [IntPtr]$i, $sb) | Out-Null # CB_GETLBTEXT
    $items += $sb.ToString()
  }
  return $items
}

# CB_SETCURSEL moves the control but does not notify its parent, and the dialog only
# copies the value into mState when it sees CBN_SELCHANGE. Both are needed, in that
# order, or Apply re-applies the rate that was already there.
function Select-ComboItem {
  param([IntPtr] $Dialog, [int] $ControlId, [int] $Index)

  $combo = [VoLumStressWin32]::GetDlgItem($Dialog, $ControlId)
  if ($combo -eq [IntPtr]::Zero) { return $false }

  [VoLumStressWin32]::SendMessageInt($combo, 0x014E, [IntPtr]$Index, [IntPtr]::Zero) | Out-Null # CB_SETCURSEL
  $wParam = [IntPtr](($ControlId -band 0xFFFF) -bor (1 -shl 16))                                # CBN_SELCHANGE
  [VoLumStressWin32]::SendMessageInt($Dialog, 0x0111, $wParam, $combo) | Out-Null
  return $true
}

function Invoke-Apply {
  param([IntPtr] $Dialog)
  [VoLumStressWin32]::PostMessage($Dialog, 0x0111, [IntPtr]$IDAPPLY, [IntPtr]::Zero) | Out-Null
}

function Get-IniValue {
  param([string] $Path, [string] $Key)

  $match = Select-String -Path $Path -Pattern "^$([regex]::Escape($Key))=" -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $match) { return $null }
  return ($match.Line -split "=", 2)[1]
}

function Test-MutexHeld {
  $h = [VoLumStressWin32]::OpenMutex(0x00100000, $false, "VoLum") # SYNCHRONIZE
  if ($h -eq [IntPtr]::Zero) { return $false }
  [VoLumStressWin32]::CloseHandle($h) | Out-Null
  return $true
}

# ---------------------------------------------------------------------------

if (-not (Test-Path $exe)) {
  Write-Error "VoLum executable not found at $exe. Build the app first, or pass -Exe."
}

Get-Process $procName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

New-Item -ItemType Directory -Force -Path $sandboxData | Out-Null
if (Test-Path $realSettings) {
  Copy-Item $realSettings $settingsPath
  Write-Host "Seeded sandbox from the machine's own audio settings."
}
else {
  Write-Host "No machine settings to seed from; VoLum will pick its defaults."
}

$savedLocalAppData = $env:LOCALAPPDATA
$env:LOCALAPPDATA = $sandbox
Write-Host "Sandbox: $sandbox"
Write-Host ""

$rateNotices = 0

try {
  for ($cycle = 1; $cycle -le $Cycles; ++$cycle) {
    Write-Host "=== cycle $cycle of $Cycles ==="

    $proc = Start-Process $exe -PassThru
    try { $proc.WaitForInputIdle(60000) | Out-Null } catch { }
    Start-Sleep -Seconds 2
    Close-Notices | Out-Null

    $main = Find-MainWindow
    if ($main -eq [IntPtr]::Zero) {
      Fail "cycle ${cycle}: the VoLum window never appeared"
      Get-Process $procName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
      continue
    }

    $pref = Open-Preferences
    if ($pref -eq [IntPtr]::Zero) {
      Fail "cycle ${cycle}: Preferences did not open"
    }
    else {
      $rates = Get-ComboItems -Dialog $pref -ControlId $IDC_COMBO_AUDIO_SR
      $buffers = Get-ComboItems -Dialog $pref -ControlId $IDC_COMBO_AUDIO_BUF_SIZE
      Write-Host "  rates offered:   $($rates -join ', ')"
      Write-Host "  buffers offered: $($buffers -join ', ')"

      if ($rates.Count -lt 2) {
        Write-Host "  WARN: fewer than two sample rates offered; the sweep will not prove much."
      }

      $rng = New-Object System.Random (1234 + $cycle)
      for ($s = 0; $s -lt $SwitchesPerCycle; ++$s) {
        if ($rates.Count -gt 0) {
          $r = $rng.Next(0, $rates.Count)
          Select-ComboItem -Dialog $pref -ControlId $IDC_COMBO_AUDIO_SR -Index $r | Out-Null
        }
        if ($buffers.Count -gt 0 -and ($s % 2) -eq 1) {
          $b = $rng.Next(0, $buffers.Count)
          Select-ComboItem -Dialog $pref -ControlId $IDC_COMBO_AUDIO_BUF_SIZE -Index $b | Out-Null
        }

        Invoke-Apply -Dialog $pref
        Start-Sleep -Milliseconds 700
        $rateNotices += Close-Notices

        if (-not (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
          Fail "cycle ${cycle}: VoLum died during the sweep, after $s switches"
          break
        }
      }

      # Leave through OK, the way a user does.
      [VoLumStressWin32]::PostMessage($pref, 0x0111, [IntPtr]1, [IntPtr]::Zero) | Out-Null
      Start-Sleep -Milliseconds 800
      $rateNotices += Close-Notices
    }

    # The part the bug was about: close, and see whether the process actually goes.
    $main = Find-MainWindow
    if ($main -ne [IntPtr]::Zero) {
      [VoLumStressWin32]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null # WM_CLOSE
    }

    $exited = $proc.WaitForExit($ExitSeconds * 1000)
    if (-not $exited) {
      Fail "cycle ${cycle}: VoLum did not exit within ${ExitSeconds}s of closing its window"
      Get-Process -Id $proc.Id -ErrorAction SilentlyContinue |
        ForEach-Object { "  threads=$($_.Threads.Count) responding=$($_.Responding)" } | Write-Host
      Get-Process -Id $proc.Id -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
      continue
    }

    Write-Host "  exited in $([math]::Round(($proc.ExitTime - $proc.StartTime).TotalSeconds, 1))s, code $($proc.ExitCode)"
    if ($proc.ExitCode -ne 0) {
      Fail "cycle ${cycle}: exit code $($proc.ExitCode), expected 0 (3 is the shutdown watchdog firing)"
    }

    Start-Sleep -Milliseconds 500

    # A process that lingers here is the reported bug, whether or not it is running:
    # its handle table keeps the mutex alive and the next launch reads that as "busy".
    $lingering = @(Get-Process $procName -ErrorAction SilentlyContinue)
    if ($lingering.Count -gt 0) {
      Fail "cycle ${cycle}: $($lingering.Count) VoLum process(es) still listed after exit"
    }
    if (Test-MutexHeld) {
      Fail "cycle ${cycle}: the single-instance mutex outlived the process"
    }

    # TerminateProcess runs nothing on the way out, and WritePrivateProfileString
    # writes behind a cache, so the settings the session ended on have to have been
    # flushed while it was still alive. Compare what is on disk with the last rate
    # the driver actually opened.
    $lastOpen = Select-String -Path $logPath -Pattern "stream open: (\d+) Hz" | Select-Object -Last 1
    $storedSR = Get-IniValue -Path $settingsPath -Key "sr"
    if ($lastOpen -and $storedSR) {
      $openedSR = $lastOpen.Matches[0].Groups[1].Value
      if ($storedSR.Trim() -ne $openedSR) {
        Fail "cycle ${cycle}: settings.ini says sr=$storedSR but the last stream opened at $openedSR Hz"
      }
      else {
        Write-Host "  settings.ini kept sr=$storedSR across the exit"
      }
    }
    else {
      Fail "cycle ${cycle}: could not compare the stored rate with the one last opened"
    }
  }

  # And the symptom the user actually reported: start it again.
  Write-Host ""
  Write-Host "=== relaunch after the sweep ==="
  $proc = Start-Process $exe -PassThru
  try { $proc.WaitForInputIdle(60000) | Out-Null } catch { }
  Start-Sleep -Seconds 2
  Close-Notices | Out-Null

  # Parenthesised: "Find-MainWindow -eq ..." would pass -eq to the function as an
  # argument and test the truthiness of whatever came back.
  if ((Find-MainWindow) -eq [IntPtr]::Zero) {
    Fail "the relaunch did not produce a window"
  }
  else {
    Write-Host "OK: VoLum started again"
  }

  $main = Find-MainWindow
  if ($main -ne [IntPtr]::Zero) {
    [VoLumStressWin32]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
  }
  $proc.WaitForExit(($ExitSeconds * 1000)) | Out-Null
  Get-Process $procName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
finally {
  $env:LOCALAPPDATA = $savedLocalAppData
}

Write-Host ""
Write-Host "Sample-rate notices dismissed: $rateNotices"

# A sweep that never changed the rate would sail through every assertion above
# without testing anything, so make the harness prove it did some work.
if (Test-Path $logPath) {
  # Match objects, not .Groups: the pipeline unrolls a GroupCollection and the
  # per-line structure is lost.
  $opens = @(Select-String -Path $logPath -Pattern "stream open: (\d+) Hz, buffer (\d+), requested (\d+) Hz" |
      ForEach-Object { $_.Matches[0] })
  $distinctRates = @($opens | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
  $distinctBuffers = @($opens | ForEach-Object { $_.Groups[2].Value } | Sort-Object -Unique)
  $substituted = @($opens | Where-Object { $_.Groups[1].Value -ne $_.Groups[3].Value })

  Write-Host "Streams opened: $($opens.Count), at rates $($distinctRates -join '/'), buffers $($distinctBuffers -join '/')"
  Write-Host "Requests the driver refused: $($substituted.Count)"
  foreach ($s in $substituted | Select-Object -First 8) {
    Write-Host "  asked $($s.Groups[3].Value) Hz, opened $($s.Groups[1].Value) Hz"
  }

  if ($distinctRates.Count -lt 3) {
    Fail "the sweep only ever opened $($distinctRates.Count) distinct rate(s); it is not exercising the driver"
  }

  Write-Host "--- teardown lines from the sandboxed log ---"
  Select-String -Path $logPath -Pattern "shutdown|did not fade|stream open failed" |
    Select-Object -Last 12 | ForEach-Object { $_.Line } | Write-Host
}

Write-Host ""
if ($failures.Count -gt 0) {
  Write-Host "$($failures.Count) failure(s):" -ForegroundColor Red
  $failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
  exit 1
}

Write-Host "All $Cycles cycles quit cleanly and relaunched." -ForegroundColor Green
