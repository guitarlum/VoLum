# Build VoLum standalone app (Release|x64) and start it for visual UI review.
# Stops an already-running VoLum first so postbuild can replace the exe.
# Must also stop VoLum_x64: postbuild copies VoLum.exe -> build-win\VoLum_x64.exe,
# and a running VoLum_x64 locks that file, silently leaving a stale standalone.
# From repo: VoLum\NeuralAmpModeler\scripts

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = Resolve-Path (Join-Path $here "..")
Set-Location $slnDir

Get-Process -Name VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 200

# Apply our local iPlug2 patches (idempotent). See NeuralAmpModeler/iplug2-patches/README.md.
& (Join-Path $slnDir "iplug2-patches\apply-iplug2-patches.ps1")

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
  Write-Error "vswhere.exe not found. Install Visual Studio Build Tools."
}
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msbuild "NeuralAmpModeler.sln" /t:NeuralAmpModeler-app /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) {
  Write-Host "Build failed (exit $LASTEXITCODE)." -ForegroundColor Yellow
  exit $LASTEXITCODE
}

$exe = Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe"
Start-Process $exe
