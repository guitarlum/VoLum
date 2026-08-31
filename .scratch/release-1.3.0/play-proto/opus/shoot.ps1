# Same flags as ../shot.ps1, but passes an absolute --screenshot path.
# Needed because this shell's inherited process CWD is not writable, so Edge's
# relative screenshot write fails with "Access is denied".
param([string]$Variant = "A", [string]$Out, [string]$Freeze = "0.75", [string]$Extra = "")
$ErrorActionPreference = "Continue"
$browser = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"
if (-not (Test-Path $browser)) { $browser = "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe" }

$html = "c:\dev\VoLum\.scratch\release-1.3.0\play-proto\opus\index.html"
$q = "variant=$Variant&freeze=$Freeze"
if ($Extra) { $q += "&" + $Extra.TrimStart("&") }
$uri = ([Uri]$html).AbsoluteUri + "?" + $q
$outFull = [System.IO.Path]::GetFullPath($Out)
$ud = Join-Path $env:TEMP ("volum-play-shoot-" + [guid]::NewGuid().ToString("n"))
New-Item -ItemType Directory -Force -Path $ud | Out-Null

$cargs = @(
  "--headless=new", "--disable-gpu", "--allow-file-access-from-files", "--hide-scrollbars",
  "--user-data-dir=$ud", "--window-size=1100,820", "--screenshot=$outFull", $uri
)
$p = Start-Process -FilePath $browser -ArgumentList $cargs -NoNewWindow -PassThru
if (-not $p.WaitForExit(30000)) { $p.Kill() }
Start-Sleep -Milliseconds 300
Remove-Item -Recurse -Force $ud -ErrorAction SilentlyContinue
if (Test-Path $outFull) { Write-Output ("OK   " + $outFull) } else { Write-Output ("FAIL " + $outFull) }
