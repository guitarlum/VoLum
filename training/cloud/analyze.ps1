# Full A1-vs-A2 diff + curve-based escalation-candidate analysis (all local).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)  # repo root
$outDir = Join-Path $root "training\a2-out"

# --- A1 baseline ESR per basename, from rigs/ ---
$a1 = @{}
Get-ChildItem -Recurse (Join-Path $root "rigs") -Filter *.nam | ForEach-Object {
  $raw = Get-Content $_.FullName -Raw
  if ($raw -match '"validation_esr":\s*([0-9eE.\-]+)') { $a1[$_.BaseName] = [double]$Matches[1] }
}

# --- A2 results (all 249) from local shard CSVs ---
$okRows = New-Object System.Collections.Generic.List[object]
$seen = @{}
Get-ChildItem (Join-Path $outDir "results_*.csv") | ForEach-Object {
  Import-Csv $_.FullName | Where-Object { $_.status -eq 'ok' } | ForEach-Object {
    if (-not $seen.ContainsKey($_.name)) {
      $seen[$_.name] = $true
      $okRows.Add([pscustomobject]@{ name=$_.name; esr=[double]$_.full_esr; best_epoch=[int]$_.best_epoch; escalated=$_.escalated })
    }
  }
}

# --- Diff table ---
$diff = New-Object System.Collections.Generic.List[object]
foreach ($r in $okRows) {
  $A2 = $r.esr
  $A1v = $null; $delta = $null; $pct = $null; $verdict = "no_A1"
  if ($a1.ContainsKey($r.name)) {
    $A1v = $a1[$r.name]
    $delta = $A1v - $A2
    if ($A1v -ne 0) { $pct = [math]::Round((($A1v - $A2) / $A1v) * 100, 1) }
    if ($A2 -lt $A1v) { $verdict = "A2_better" } else { $verdict = "A1_better" }
  }
  $diff.Add([pscustomobject]@{
    name = $r.name; A1_esr = $A1v; A2_esr = $A2; delta = $delta; pct_better = $pct
    verdict = $verdict; best_epoch = $r.best_epoch; escalated = $r.escalated
  })
}
$diff | Sort-Object @{e={if($_.delta -ne $null){[double]$_.delta}else{-999}}} -Descending |
  Export-Csv (Join-Path $outDir "A1_vs_A2_diff_full.csv") -NoTypeInformation

$withA1 = $diff | Where-Object { $_.A1_esr -ne $null }
$mA1 = ($withA1 | Measure-Object A1_esr -Average).Average
$mA2 = ($withA1 | Measure-Object A2_esr -Average).Average
$sortedA2 = ($withA1.A2_esr | Sort-Object)
$med = $sortedA2[[int]($withA1.Count/2)]
Write-Host ("=== A1 vs A2  (total A2={0}, matched to A1={1}) ===" -f $okRows.Count, $withA1.Count)
Write-Host ("A2_better={0}  A1_better={1}  no_A1_match={2}" -f ($diff|Where-Object verdict -eq A2_better).Count,($diff|Where-Object verdict -eq A1_better).Count,($diff|Where-Object verdict -eq no_A1).Count)
Write-Host ("mean ESR  A1={0:N5}  A2={1:N5}  ({2:N1}% lower)" -f $mA1,$mA2,((($mA1-$mA2)/$mA1)*100))
Write-Host ("A2 median ESR={0:N5}   excellent(<0.01)={1}  good(0.01-0.02)={2}  review(>=0.02)={3}" -f $med,($withA1|Where-Object {$_.A2_esr -lt 0.01}).Count,($withA1|Where-Object {$_.A2_esr -ge 0.01 -and $_.A2_esr -lt 0.02}).Count,($withA1|Where-Object {$_.A2_esr -ge 0.02}).Count)
Write-Host ""
Write-Host "--- worst 6 A2 (highest ESR, review) ---"
$withA1 | Sort-Object A2_esr -Descending | Select-Object -First 6 | ForEach-Object { "  {0,-22} A1={1:N5} A2={2:N5} bestEp={3}" -f $_.name,$_.A1_esr,$_.A2_esr,$_.best_epoch }
Write-Host "--- A2 regressions vs A1 (worse) ---"
$reg = $withA1 | Where-Object verdict -eq A1_better | Sort-Object delta
Write-Host ("  count={0}; worst:" -f $reg.Count)
$reg | Select-Object -First 8 | ForEach-Object { "  {0,-22} A1={1:N5} A2={2:N5} delta={3:N5}" -f $_.name,$_.A1_esr,$_.A2_esr,$_.delta }

# --- Curve-based escalation candidates (recomputed from raw ESR_packed_1) ---
$cand = New-Object System.Collections.Generic.List[object]
foreach ($f in Get-ChildItem (Join-Path $outDir "curves\*.json")) {
  $c = Get-Content $f.FullName -Raw | ConvertFrom-Json
  $s = $c.tags.ESR_packed_1
  if (-not $s) { continue }
  $lastStep = [double]$s[-1][0]; if ($lastStep -le 0) { $lastStep = 1 }
  $bestPt = $s | Sort-Object { [double]$_[1] } | Select-Object -First 1
  $best = [double]$bestPt[1]; $bestStep = [double]$bestPt[0]
  $bestEp = [int][math]::Round($bestStep/$lastStep*$c.epochs)
  $lastEsr = [double]$s[-1][1]
  $cut = 0.9 * $lastStep
  $p90pt = ($s | Where-Object { [double]$_[0] -ge $cut } | Select-Object -First 1)
  $esr90 = $lastEsr; if ($p90pt) { $esr90 = [double]$p90pt[1] }
  $relLate = 0.0; if ($esr90 -gt 0) { $relLate = ($esr90 - $best) / $esr90 }
  $isCand = ($bestEp -ge 665 -and $relLate -ge 0.03)
  $cand.Add([pscustomobject]@{
    name=$c.name; best_esr=[math]::Round($best,6); best_epoch=$bestEp
    esr_at_90pct=[math]::Round($esr90,6); final_esr=[math]::Round($lastEsr,6)
    rel_late_drop_pct=[math]::Round($relLate*100,1); candidate=$isCand
  })
}
$cand | Sort-Object rel_late_drop_pct -Descending | Export-Csv (Join-Path $outDir "escalation_candidates.csv") -NoTypeInformation
$yes = $cand | Where-Object { $_.candidate }
$strong = $cand | Where-Object { $_.best_epoch -ge 665 -and $_.rel_late_drop_pct -ge 8 }
Write-Host ""
Write-Host ("=== Escalation analysis (curves={0}; first 67 models have no curve) ===" -f $cand.Count)
Write-Host ("candidates (bestEp>=665 & >=3% late drop): {0}    strong (>=8% late drop): {1}" -f $yes.Count, $strong.Count)
Write-Host "--- top 12 by late-stage ESR drop ---"
$cand | Sort-Object rel_late_drop_pct -Descending | Select-Object -First 12 | ForEach-Object {
  "  {0,-22} bestEp={1,3} esr@90%={2:N5} best={3:N5} lateDrop={4}%" -f $_.name,$_.best_epoch,$_.esr_at_90pct,$_.best_esr,$_.rel_late_drop_pct
}
Write-Host ""
Write-Host "wrote: A1_vs_A2_diff_full.csv, escalation_candidates.csv"
