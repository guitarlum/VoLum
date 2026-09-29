# Headless screenshot helper for the opus2 PLAY prototype.
# Edge refuses to write the PNG into .scratch (Access denied), so it always
# renders into $env:TEMP with an absolute path and copies the result back.
param(
  [Parameter(Mandatory = $true)][string] $Name,     # e.g. opus2-A-v1
  [string] $Variant = "A",
  [string] $Freeze = "0.75",
  [string] $ExtraQuery = ""
)
$ErrorActionPreference = "Stop"
$browser = @(
  "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
  "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
  "$env:ProgramFiles\Google\Chrome\Application\chrome.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $browser) { Write-Error "Edge/Chrome not found" }

$root = "c:\dev\VoLum\.scratch\release-1.3.0\play-proto"
$html = Join-Path $root "opus2\index.html"
$shots = Join-Path $root "shots"
if (-not (Test-Path $shots)) { New-Item -ItemType Directory -Force -Path $shots | Out-Null }

$q = @("variant=$Variant")
if ($Freeze -ne "") { $q += "freeze=$Freeze" }
if ($ExtraQuery) { $q += $ExtraQuery.TrimStart("&") }
$uri = ([Uri]$html).AbsoluteUri + "?" + ($q -join "&")

$userData = Join-Path $env:TEMP ("volum-opus2-" + [guid]::NewGuid().ToString("n"))
New-Item -ItemType Directory -Force -Path $userData | Out-Null
$tmpPng = Join-Path $env:TEMP ("$Name-" + [guid]::NewGuid().ToString("n") + ".png")

try {
  # A leftover headless Edge from a previous shot makes the next launch hang forever.
  Get-Process msedge -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  $args = @(
    "--headless=new", "--disable-gpu", "--allow-file-access-from-files", "--hide-scrollbars",
    "--no-first-run", "--no-default-browser-check", "--disable-extensions",
    "--virtual-time-budget=1500", "--user-data-dir=$userData", "--window-size=1100,820",
    "--screenshot=$tmpPng", $uri
  )
  $p = Start-Process -FilePath $browser -ArgumentList $args -PassThru -WindowStyle Hidden
  if (-not $p.WaitForExit(25000)) { $p.Kill() }
  $deadline = (Get-Date).AddSeconds(10)
  while (-not (Test-Path $tmpPng) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 200 }
  if (-not (Test-Path $tmpPng)) { Write-Error "Screenshot was not written: $tmpPng" }
  $dest = Join-Path $shots "$Name.png"
  Copy-Item -Force $tmpPng $dest
  Write-Output $dest
} finally {
  Get-Process msedge -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Remove-Item -Force $tmpPng -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 300
  Remove-Item -Recurse -Force $userData -ErrorAction SilentlyContinue
}
