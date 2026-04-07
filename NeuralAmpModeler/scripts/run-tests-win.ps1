# Run after building NeuralAmpModeler-Tests (Release|x64 recommended).
# From repo: VoLum\NeuralAmpModeler\scripts

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnDir = Resolve-Path (Join-Path $here "..")
Set-Location $slnDir

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
  Write-Error "vswhere.exe not found. Install Visual Studio Build Tools."
}
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msbuild "NeuralAmpModeler.sln" /t:NeuralAmpModeler-Tests /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $slnDir "build-win\tests\x64\Release\NeuralAmpModeler-Tests.exe"
& $exe
exit $LASTEXITCODE
