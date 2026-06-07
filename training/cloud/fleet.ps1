# Fleet registry + ops for the VoLum A2 cloud run. Dot-source after podlib.ps1:
#   . .\training\cloud\podlib.ps1 ; . .\training\cloud\fleet.ps1
# The IDs/IPs/ports below are a WORKED EXAMPLE from the actual round-1 run
# (those pods are long gone). Replace $Fleet/$VolumeId with your provisioned
# pods before launching. Shard ranges (Lo..Hi) partition 0..NSHARD-1 across pods.
$NSHARD = 28
$VolumeId = "isjd11s57r"
$Fleet = @(
  [pscustomobject]@{ Name="calib"; Id="idz7u0eo105hco"; Ip="213.173.108.12";  Port=11057; Gpu="4000Ada"; Rate=0.26; Lo=0;  Hi=3  }
  [pscustomobject]@{ Name="w1";    Id="b9wbj2iq3a5ngg"; Ip="213.173.109.141"; Port=17882; Gpu="4090";    Rate=0.69; Lo=4;  Hi=11 }
  [pscustomobject]@{ Name="w2";    Id="l5fjqk3dfww6sm"; Ip="213.173.110.78";  Port=10962; Gpu="4090";    Rate=0.69; Lo=12; Hi=19 }
  [pscustomobject]@{ Name="w3";    Id="6fvysya00c41l7"; Ip="213.173.110.102"; Port=15108; Gpu="4090";    Rate=0.69; Lo=20; Hi=27 }
)
$FleetRatePerHr = ($Fleet | Measure-Object Rate -Sum).Sum

function Fleet-Launch($epochs=700, $escAt=100000) {
  foreach ($p in $Fleet) {
    $cmd = "NSHARD=$NSHARD EPOCHS=$epochs THREADS=2 SHARD_LO=$($p.Lo) SHARD_HI=$($p.Hi) CAP_DIR=/workspace/captures_full ESCALATE_AT=$escAt bash /workspace/a2/launch_shards.sh"
    Write-Host "=== launch $($p.Name) shards $($p.Lo)-$($p.Hi) escAt=$escAt ==="
    Pod-Run $p.Ip $p.Port $cmd
  }
}

function Fleet-Kill {
  foreach ($p in $Fleet) {
    Pod-Run $p.Ip $p.Port "tmux kill-server 2>/dev/null; echo killed $($p.Name)"
  }
}

function Fleet-OutCount {
  $p = $Fleet[0]
  (Pod-Run $p.Ip $p.Port "ls /workspace/a2/out/*.nam 2>/dev/null | wc -l")
}

function Fleet-Status {
  $ErrorActionPreference="Continue"
  foreach ($p in $Fleet) {
    $g = Pod-Run $p.Ip $p.Port "nvidia-smi --query-gpu=utilization.gpu,memory.used --format=csv,noheader 2>/dev/null; tmux ls 2>/dev/null | wc -l"
    Write-Host ("[{0,-5} {1,-7}] gpu/tmux: {2}" -f $p.Name, $p.Gpu, ($g -join ' | '))
  }
  $out = Fleet-OutCount
  Write-Host ("OUT: {0}/249" -f $out)
}

function Fleet-Results {
  # Merge all results_*.csv from the shared volume (read via calib pod).
  $p = $Fleet[0]
  Pod-Run $p.Ip $p.Port "cat /workspace/a2/results_*.csv 2>/dev/null | grep -v '^name,' | sort -u"
}

function Fleet-Teardown {
  foreach ($p in $Fleet) {
    Write-Host "terminating $($p.Name) ($($p.Id))"
    try { Rp-Delete "pods/$($p.Id)" | Out-Null } catch { Write-Host "  err: $($_.Exception.Message)" }
  }
}
Write-Host ("fleet: {0} pods, NSHARD={1}, ${2}/hr" -f $Fleet.Count, $NSHARD, $FleetRatePerHr)
