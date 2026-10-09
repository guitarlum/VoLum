# glitchscan-win.ps1 - build and run the real-audio glitch scan.
#
# LOCAL ONLY. Needs an interface with output 1 patched into input 1. Plays a steady
# 200 Hz tone on output 1 (left) and records the render endpoint through WASAPI
# loopback while VoLum processes input 1 and plays on output 2 (right). The analyser
# then reports dropouts, clicks and hard steps on VoLum's output, with the tone as a
# control so a system-wide hiccup is not blamed on VoLum.
#
# VoLum must run on DirectSound (shared mode) with BOTH outputs routed to channel 2:
# settings.ini driver=0, indev=Line (...), outdev=Speakers (...), out1=2, out2=2.
# Anything on output 1 feeds straight back into input 1 through the cable - an amp
# model in that loop screams. ASIO cannot be scanned this way: the driver takes the
# device exclusively and nothing else can play or listen.
#
# Usage (start VoLum first, then drive whatever you want to test while it records):
#   powershell -File NeuralAmpModeler/scripts/glitchscan-win.ps1 -Seconds 120 -Wav C:\tmp\scan.wav
#   powershell -File NeuralAmpModeler/scripts/glitchscan-win.ps1 -Analyze C:\tmp\scan.wav -Events C:\tmp\events.json
#
# events.json, if given, is a list of {"t": <unix ms>, "what": "<action>"}; each
# finding is tagged with the action before it.

param(
  [string]$Render = "UA-2X2",
  [double]$Seconds = 60,
  [double]$Amp = 0.25,
  [string]$Wav = (Join-Path $env:TEMP "volum-glitchscan.wav"),
  [string]$StopFile = "",
  [string]$Analyze = "",
  [string]$Events = "",
  [switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$toolDir = Join-Path $repo "NeuralAmpModeler\tools\glitchscan"
$src = Join-Path $toolDir "volum_glitchscan.cpp"
$analyzer = Join-Path $toolDir "analyze_glitchscan.py"
$outDir = Join-Path $repo "NeuralAmpModeler\build-win\glitchscan"
$exe = Join-Path $outDir "volum_glitchscan.exe"

function Invoke-Analyzer([string]$path) {
  $a = @($analyzer, $path)
  if ($Events) { $a += @("--events", $Events) }
  $a += @("--json", "$path.report.json")
  python @a
  if ($LASTEXITCODE -ne 0) { throw "analyser failed" }
}

if ($Analyze) { Invoke-Analyzer $Analyze; exit 0 }

function Find-VsDevCmd {
  foreach ($r in @("${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022", "${env:ProgramFiles}\Microsoft Visual Studio\2022")) {
    if (-not (Test-Path $r)) { continue }
    foreach ($ed in @("BuildTools", "Community", "Professional", "Enterprise")) {
      $p = Join-Path $r "$ed\Common7\Tools\VsDevCmd.bat"
      if (Test-Path $p) { return $p }
    }
  }
  throw "VsDevCmd.bat not found; install VS 2022 Build Tools."
}

$needsBuild = $Rebuild -or -not (Test-Path $exe) -or ((Get-Item $src).LastWriteTimeUtc -gt (Get-Item $exe).LastWriteTimeUtc)
if ($needsBuild) {
  New-Item -ItemType Directory -Force -Path $outDir | Out-Null
  $vsDevCmd = Find-VsDevCmd
  $log = Join-Path $outDir "build.log"
  $cl = "cl /nologo /EHsc /O2 /std:c++17 /MD /DUNICODE /D_UNICODE `"$src`" /Fe`"$exe`" /Fo`"$outDir\\`" /link ole32.lib user32.lib"
  Write-Host "Building glitch scan tool..."
  cmd /c "call `"$vsDevCmd`" -arch=amd64 -host_arch=amd64 >nul && $cl" > $log 2>&1
  if ($LASTEXITCODE -ne 0) {
    Get-Content $log | Select-Object -Last 40 | ForEach-Object { Write-Host $_ }
    throw "Glitch scan build failed (see $log)"
  }
}

$toolArgs = @("--render", $Render, "--seconds", "$Seconds", "--amp", "$Amp", "--wav", $Wav)
if ($StopFile) { $toolArgs += @("--stop-file", $StopFile) }
& $exe @toolArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Invoke-Analyzer $Wav
