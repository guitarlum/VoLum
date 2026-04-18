# Build VoLum standalone app (Release|x64) and start it for visual UI review.
# If postbuild reports "file is being used by another process", close VoLum and run again.
# From repo: VoLum\NeuralAmpModeler\scripts

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = Resolve-Path (Join-Path $here "..")
Set-Location $slnDir

# Apply our local iPlug2 patches (idempotent). See NeuralAmpModeler/iplug2-patches/README.md.
& (Join-Path $slnDir "iplug2-patches\apply-iplug2-patches.ps1")

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
  Write-Error "vswhere.exe not found. Install Visual Studio Build Tools."
}
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msbuild "NeuralAmpModeler.sln" /t:NeuralAmpModeler-app /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) {
  Write-Host "Build failed (exit $LASTEXITCODE). If postbuild could not copy the exe, close VoLum and rebuild." -ForegroundColor Yellow
  exit $LASTEXITCODE
}

$exe = Join-Path $slnDir "build-win\app\x64\Release\VoLum.exe"
Start-Process $exe
