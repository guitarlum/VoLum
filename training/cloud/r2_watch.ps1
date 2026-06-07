. "$PSScriptRoot\podlib.ps1"
$ErrorActionPreference = "Continue"
$ip = "213.173.110.102"; $port = 11125
$podId = "ava0dfnqr8sbpf"
$target = 30
$maxHours = 16          # hard safety: tear down no matter what after this
$dst = "$PSScriptRoot\..\..\training\a2-r2-out"
New-Item -ItemType Directory -Force -Path $dst,"$dst\curves" | Out-Null
$start = Get-Date

function Pull-All {
  scp -i $Key -P $port -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL "root@${ip}:/workspace/a2_r2/out/*.nam" "$dst\" 2>$null
  scp -i $Key -P $port -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL "root@${ip}:/workspace/a2_r2/curves/*.json" "$dst\curves\" 2>$null
  scp -i $Key -P $port -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL "root@${ip}:/workspace/a2_r2/results_*.csv" "$dst\" 2>$null
}
function Teardown($reason) {
  Write-Host "R2_TEARDOWN ($reason) terminating $podId"
  for ($t=0; $t -lt 5; $t++) {
    try { Rp-Delete "pods/$podId" | Out-Null } catch {}
    Start-Sleep -Seconds 3
    $alive = $true
    try { $p = Rp-Get "pods/$podId"; if (-not $p -or -not $p.id) { $alive = $false } } catch { $alive = $false }
    if (-not $alive) { Write-Host "R2_TEARDOWN_OK pod gone"; return $true }
    Write-Host "  retry teardown ($t)..."
  }
  Write-Host "R2_TEARDOWN_FAIL pod may still be running - CHECK MANUALLY"
  return $false
}

while ($true) {
  $out = 0
  try { $out = [int](((Pod-Run $ip $port "ls /workspace/a2_r2/out/*.nam 2>/dev/null | wc -l") | Out-String) -replace '\D','') } catch {}
  $hrs = [math]::Round(((Get-Date)-$start).TotalHours,2)
  $cost = [math]::Round($hrs*0.69,2)
  Write-Host ("[{0}] R2 out={1}/{2} hrs={3} cost=`${4}" -f (Get-Date -Format "HH:mm"),$out,$target,$hrs,$cost)

  if ($out -ge $target) {
    Write-Host "R2_ALLDONE pulling artifacts"
    Pull-All
    $n = (Get-ChildItem "$dst\*.nam" -ErrorAction SilentlyContinue).Count
    Write-Host "R2_LOCAL nam=$n"
    if ($n -lt $target) { Write-Host "R2_PULL_WARN only $n/$target local; retrying once"; Start-Sleep 5; Pull-All; $n = (Get-ChildItem "$dst\*.nam").Count; Write-Host "R2_LOCAL2 nam=$n" }
    Teardown("complete") | Out-Null
    Write-Host "R2_DONE final_local=$n"
    break
  }
  if ($hrs -ge $maxHours) {
    Write-Host "R2_TIMEOUT hrs=$hrs exceeded $maxHours; pulling what exists then tearing down"
    Pull-All
    Teardown("timeout") | Out-Null
    Write-Host "R2_DONE_TIMEOUT"
    break
  }
  Start-Sleep -Seconds 600
}
