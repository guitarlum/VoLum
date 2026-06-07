. "$PSScriptRoot\podlib.ps1"
. "$PSScriptRoot\fleet.ps1"
$ErrorActionPreference = "Continue"
# Set this to the fleet launch time when you start a run.
$runStart = if ($env:A2_RUN_START) { Get-Date $env:A2_RUN_START } else { Get-Date }
$log = "$PSScriptRoot\monitor.log"
$costGuardHit = $false
while ($true) {
  $ts = Get-Date -Format "yyyy-MM-dd HH:mm"
  $out = 0
  try { $out = [int](((Fleet-OutCount) | Out-String) -replace '\D','') } catch {}
  $hrs = ((Get-Date) - $runStart).TotalHours
  $cost = [math]::Round($hrs * $FleetRatePerHr, 2)
  $res = ""
  try { $res = (Fleet-Results | Out-String) } catch {}
  $okN  = ([regex]::Matches($res, ',ok,')).Count
  $errN = ([regex]::Matches($res, 'error:|no_model|export_missing')).Count
  $escN = ([regex]::Matches($res, ',True,')).Count
  $line = "[$ts] out=$out/249 ok=$okN err=$errN esc=$escN hrs=$([math]::Round($hrs,1)) cost=`$$cost"
  $line | Tee-Object -FilePath $log -Append
  if ($errN -gt 0) { Write-Host "MONITOR_ERR errors=$errN out=$out" }
  if ($out -ge 249) { Write-Host "MONITOR_ALLDONE out=$out cost=`$$cost"; break }
  if ($cost -ge 45 -and -not $costGuardHit) { Write-Host "MONITOR_COSTGUARD cost=`$$cost out=$out"; $costGuardHit = $true }
  Start-Sleep -Seconds 1800
}
