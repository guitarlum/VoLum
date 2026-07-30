# loopback-latency-win.ps1 - build and run the hardware loopback latency harness.
#
# LOCAL ONLY. This needs a real audio interface with output 1 patched into input 1;
# CI runners have no audio device and will (correctly) fail with "no-duplex-device".
#
# What it gives you that no unit test can:
#   - the round trip a player actually feels, measured rather than estimated
#   - a cross-check of the driver's self-reported latency, which is what VoLum's
#     Settings page displays
#   - whether audio survives the loop at all, and whether the driver drops buffers
#
# Usage:
#   pwsh -File NeuralAmpModeler/scripts/loopback-latency-win.ps1 -List
#   pwsh -File NeuralAmpModeler/scripts/loopback-latency-win.ps1 -Api asio -Buffer 128
#   pwsh -File NeuralAmpModeler/scripts/loopback-latency-win.ps1 -Api wasapi -OutDevice "Speakers (UA-2X2)" -InDevice "Line (UA-2X2)"

param(
  [ValidateSet("asio", "wasapi", "ds")][string]$Api = "asio",
  [string]$Device = "",
  [string]$InDevice = "",
  [string]$OutDevice = "",
  [int]$Rate = 48000,
  [int]$Buffer = 128,
  [int]$Pulses = 5,
  [string]$Json = "",
  [switch]$List,
  [switch]$Rebuild
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$src = Join-Path $repo "NeuralAmpModeler\tools\loopback\volum_loopback.cpp"
$rtaudio = Join-Path $repo "iPlug2\Dependencies\IPlug\RTAudio"
$outDir = Join-Path $repo "NeuralAmpModeler\build-win\loopback"
$exe = Join-Path $outDir "volum_loopback.exe"

if (-not (Test-Path $src)) { throw "Missing harness source: $src" }

function Find-VsDevCmd {
  $roots = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022"
  )
  foreach ($r in $roots) {
    if (-not (Test-Path $r)) { continue }
    foreach ($ed in @("BuildTools", "Community", "Professional", "Enterprise")) {
      $p = Join-Path $r "$ed\Common7\Tools\VsDevCmd.bat"
      if (Test-Path $p) { return $p }
    }
  }
  throw "VsDevCmd.bat not found; install VS 2022 Build Tools."
}

$needsBuild = $Rebuild -or -not (Test-Path $exe)
if (-not $needsBuild) {
  $srcTime = (Get-Item $src).LastWriteTimeUtc
  if ($srcTime -gt (Get-Item $exe).LastWriteTimeUtc) { $needsBuild = $true }
}

if ($needsBuild) {
  New-Item -ItemType Directory -Force -Path $outDir | Out-Null
  $vsDevCmd = Find-VsDevCmd
  # Same RtAudio + ASIO SDK the standalone links, so the measurement reflects the
  # stack VoLum actually runs on rather than a lookalike.
  $sources = @(
    "`"$src`"",
    "`"$rtaudio\RtAudio.cpp`"",
    "`"$rtaudio\include\asio.cpp`"",
    "`"$rtaudio\include\asiodrivers.cpp`"",
    "`"$rtaudio\include\asiolist.cpp`"",
    "`"$rtaudio\include\iasiothiscallresolver.cpp`""
  ) -join " "
  $defines = "/D__WINDOWS_ASIO__ /D__WINDOWS_WASAPI__ /D__WINDOWS_DS__ /DWIN32 /D_WINDOWS /DNOMINMAX"
  $includes = "/I`"$rtaudio`" /I`"$rtaudio\include`""
  $libs = "ole32.lib user32.lib advapi32.lib winmm.lib dsound.lib mfplat.lib mfuuid.lib ksuser.lib wmcodecdspuuid.lib"
  $cl = "cl /nologo /EHsc /O2 /std:c++17 /MD $defines $includes $sources /Fe`"$exe`" /Fo`"$outDir\\`" /link $libs"

  Write-Host "Building loopback harness..."
  $log = Join-Path $outDir "build.log"
  cmd /c "call `"$vsDevCmd`" -arch=amd64 -host_arch=amd64 >nul && $cl" > $log 2>&1
  if ($LASTEXITCODE -ne 0) {
    Get-Content $log | Select-Object -Last 40 | ForEach-Object { Write-Host $_ }
    throw "Loopback harness build failed (see $log)"
  }
}

$harnessArgs = @("--api", $Api, "--rate", "$Rate", "--buffer", "$Buffer", "--pulses", "$Pulses")
if ($Device) { $harnessArgs += @("--device", $Device) }
if ($InDevice) { $harnessArgs += @("--in-device", $InDevice) }
if ($OutDevice) { $harnessArgs += @("--out-device", $OutDevice) }
if ($Json) { $harnessArgs += @("--json", $Json) }
if ($List) { $harnessArgs += "--list" }

& $exe @harnessArgs
exit $LASTEXITCODE
