# Checkable predicate for .scratch/1.3.0-hunt/loop.md
# -Quick: structural checks only (guides iteration; never closes the loop).
param([switch]$Quick)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$scripts = Join-Path $root "NeuralAmpModeler\scripts"
$fail = [System.Collections.Generic.List[string]]::new()

function Get-Text([string]$Rel) {
  $p = Join-Path $root $Rel
  if (-not (Test-Path -LiteralPath $p)) { $script:fail.Add("missing $Rel"); return "" }
  return Get-Content -LiteralPath $p -Raw -Encoding UTF8
}

# Lanes all done
$loop = Get-Text ".scratch/1.3.0-hunt/loop.md"
foreach ($m in [regex]::Matches($loop, '(?m)^- \[ \] (L\d+ .*)$')) { $fail.Add("lane open: $($m.Groups[1].Value)") }

# Every finding has a disposition
$findings = Get-Text ".scratch/1.3.0-hunt/findings.md"
$openFindings = [regex]::Matches($findings, '(?ms)^### (F-\d+[^\r\n]*)(?:(?!^### ).)*?^- Disposition:\s*open\b')
foreach ($m in $openFindings) { $fail.Add("finding open: $($m.Groups[1].Value)") }
$opps = Get-Text ".scratch/1.3.0-hunt/opportunities.md"
foreach ($m in [regex]::Matches($opps, '(?ms)^### (O-\d+[^\r\n]*)(?:(?!^### ).)*?status:?\s*`?open\b')) { $fail.Add("opportunity open: $($m.Groups[1].Value)") }

# GIF on top of both READMEs and in the release-notes draft
$gif = Join-Path $root "docs\volum-dual-amp.gif"
if (-not (Test-Path $gif)) { $fail.Add("missing docs/volum-dual-amp.gif") }
elseif ((Get-Item $gif).Length -gt 8MB) { $fail.Add("docs/volum-dual-amp.gif is over 8 MB") }
foreach ($r in @("README.md", "README.de.md")) {
  $head = ((Get-Text $r) -split "`n" | Select-Object -First 15) -join "`n"
  if ($head -notmatch 'volum-dual-amp\.gif') { $fail.Add("$r: GIF not in the first 15 lines") }
}
if ((Get-Text ".scratch/release-1.3.0/release-notes-draft.md") -notmatch 'volum-dual-amp\.gif') {
  $fail.Add("release-notes draft does not show the GIF")
}

# Every screenshot reshot tonight (committed after the loop started)
$since = [DateTimeOffset]::new(2026, 10, 9, 0, 0, 0, [TimeSpan]::FromHours(2)).ToUnixTimeSeconds()
Get-ChildItem (Join-Path $root "docs") -Filter "user-guide-*.png" | ForEach-Object {
  $ct = git -C $root log -1 --format=%ct -- "docs/$($_.Name)"
  if (-not $ct -or [int64]$ct -lt $since) { $fail.Add("screenshot not reshot: docs/$($_.Name)") }
}

# Guides in sync: same images referenced in EN and DE
$en = Get-Text "docs/user-guide.en.md"; $de = Get-Text "docs/user-guide.de.md"
$imgEn = @([regex]::Matches($en, 'user-guide-[\w-]+\.png') | ForEach-Object Value | Sort-Object -Unique)
$imgDe = @([regex]::Matches($de, 'user-guide-[\w-]+\.png') | ForEach-Object Value | Sort-Object -Unique)
if (Compare-Object $imgEn $imgDe) { $fail.Add("EN and DE guides reference different screenshots") }
foreach ($img in $imgEn) { if (-not (Test-Path (Join-Path $root "docs\$img"))) { $fail.Add("guide references missing docs/$img") } }

# Morning checklist trimmed: no Windows HX steps left
$cl = Get-Text ".scratch/1.3.0-uat/checklist.md"
if ($cl -match 'Line 6 driver installed') { $fail.Add("UAT checklist still has the Windows HX setup") }
if ($cl -notmatch '(?m)^## Mac pass') { $fail.Add("UAT checklist has no Mac pass") }

if ($Quick) {
  if ($fail.Count -gt 0) { Write-Host "VERIFY (quick) FAIL"; $fail | ForEach-Object { Write-Host "- $_" }; exit 1 }
  Write-Host "VERIFY (quick) OK"; exit 0
}

function Invoke-Step([string]$Name, [scriptblock]$Body) {
  Write-Host "=== $Name"
  & $Body
  if ($LASTEXITCODE -ne 0) { $script:fail.Add("$Name exit $LASTEXITCODE") }
}
Invoke-Step "run-tests-win" { pwsh (Join-Path $scripts "run-tests-win.ps1") }
Invoke-Step "run-tests-win -Asan" { pwsh (Join-Path $scripts "run-tests-win.ps1") -Asan }
Invoke-Step "build standalone" {
  pwsh (Join-Path $scripts "run-app-win.ps1"); Start-Sleep 6
  Get-Process VoLum, VoLum_x64 -ErrorAction SilentlyContinue | Stop-Process -Force
}
Invoke-Step "e2e all (MIDI required)" { pwsh (Join-Path $scripts "e2e-standalone-win.ps1") -RequireMidi }
Invoke-Step "reaper harness" { pwsh (Join-Path $scripts "reaper-harness\run-reaper-harness.ps1") }

$asioName = "ASIO4ALL v2"
$profile = Join-Path ([IO.Path]::GetTempPath()) "volum-hunt-stress-profile"
New-Item -ItemType Directory -Force (Join-Path $profile "VoLum") | Out-Null
@"
[audio]
driver=1
indev=$asioName
outdev=$asioName
in1=1
in2=1
out1=1
out2=2
buffer=128
sr=48000
[midi]
indev=off
outdev=off
inchan=0
outchan=0
"@ | Set-Content -Encoding ASCII (Join-Path $profile "VoLum\settings.ini")
$saved = $env:LOCALAPPDATA
try {
  $env:LOCALAPPDATA = $profile
  Invoke-Step "stress rate switch ($asioName on UA-2X2)" { pwsh (Join-Path $scripts "stress-standalone-rate-switch-win.ps1") }
} finally { $env:LOCALAPPDATA = $saved }

$devSha = (git -C $root rev-parse refs/remotes/origin/dev).Trim()
$run = gh run list --repo guitarlum/VoLum --branch dev --workflow ci.yml --limit 1 --json headSha,conclusion,status | ConvertFrom-Json
if (-not $run -or $run[0].headSha -ne $devSha -or $run[0].conclusion -ne "success") {
  $fail.Add("latest dev CI is not green on origin/dev $devSha")
}

if ($fail.Count -gt 0) { Write-Host "VERIFY FAIL"; $fail | ForEach-Object { Write-Host "- $_" }; exit 1 }
Write-Host "VERIFY OK"
exit 0
