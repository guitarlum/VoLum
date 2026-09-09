# Print the live conductor loop.md from disk. Chat summaries of the goal and
# predicate are stale after compression; this script is the first tool call
# every conductor turn (including /loop wakes).
#
#   pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort midi-control
#   pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Path path\to\loop.md
#   pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort release-slug -RequirePredicate
#
# Exit 0 when a well-formed loop.md was printed.
# Exit 1 when none exists, several exist with no -Effort/-Path, Goal or
# Predicate is missing, or -RequirePredicate finds a blank loop.

[CmdletBinding()]
param(
  [string]$Effort = "",
  [string]$Path = "",
  [switch]$RequirePredicate
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path
$scratch = Join-Path $repoRoot ".scratch"
$allLoops = @()
if (Test-Path -LiteralPath $scratch) {
  $allLoops = @(Get-ChildItem -LiteralPath $scratch -Filter "loop.md" -Recurse -File -ErrorAction SilentlyContinue)
}

function Get-MdSection {
  param([string]$Text, [string]$Heading)
  $escaped = [regex]::Escape($Heading)
  $m = [regex]::Match($Text, "(?ms)^##\s+$escaped\s*\r?\n(.*?)(?=^##\s|\z)")
  if (-not $m.Success) { return "" }
  return $m.Groups[1].Value.Trim()
}

function Get-TicketStatus {
  param([string]$FilePath)
  $raw = Get-Content -LiteralPath $FilePath -Raw -ErrorAction SilentlyContinue
  if ([string]::IsNullOrEmpty($raw)) { return "" }
  $m = [regex]::Match($raw, '(?im)^(?:\*\*)?Status:\s*(?:\*\*)?\s*([A-Za-z0-9_-]+)')
  if ($m.Success) { return $m.Groups[1].Value.ToLowerInvariant() }
  return ""
}

function Get-Rel {
  param([string]$Full)
  $full = $Full
  if ($full.Length -ge $repoRoot.Length -and $full.Substring(0, $repoRoot.Length) -eq $repoRoot) {
    return $full.Substring($repoRoot.Length).TrimStart("\", "/") -replace "\\", "/"
  }
  return ($full -replace "\\", "/")
}

function Test-BlankSection([string]$Value) {
  if ([string]::IsNullOrWhiteSpace($Value)) { return $true }
  $t = $Value.Trim()
  if ($t -match '(?i)^missing' ) { return $true }
  if ($t -match '(?i)AskQuestion') { return $true }
  return $false
}

function Get-WorktreeHash {
  Push-Location $repoRoot
  try {
    $diffLines = @(& git diff HEAD 2>$null)
    $text = ($diffLines -join "`n")
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
      $bytes = [System.Text.Encoding]::UTF8.GetBytes($text)
      $hex = ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString("x2") }) -join ""
      return $hex.Substring(0, 16)
    }
    finally { $sha.Dispose() }
  }
  finally { Pop-Location }
}

function Get-VerifierState {
  param([string]$Section, [string]$CurrentHash)
  $recorded = ""
  $verdict = ""
  if (-not [string]::IsNullOrWhiteSpace($Section)) {
    $hm = [regex]::Match($Section, '(?im)^(?:[-*]\s*)?hash:\s*([a-f0-9]+)')
    if ($hm.Success) { $recorded = $hm.Groups[1].Value.ToLowerInvariant() }
    $vm = [regex]::Match($Section, '(?im)^(?:[-*]\s*)?verdict:\s*(accepted|defects|rejected)')
    if ($vm.Success) { $verdict = $vm.Groups[1].Value.ToLowerInvariant() }
  }
  if ([string]::IsNullOrWhiteSpace($recorded) -or [string]::IsNullOrWhiteSpace($verdict)) {
    return "missing (current=$CurrentHash)"
  }
  if ($recorded -ne $CurrentHash) {
    return "STALE last=$recorded current=$CurrentHash (re-run verifier)"
  }
  return "$verdict @$recorded"
}

function Get-ChildLoopPaths {
  param([string]$Section)
  $paths = @()
  if ([string]::IsNullOrWhiteSpace($Section)) { return $paths }
  foreach ($m in [regex]::Matches($Section, '(?:`([^`\r\n]*loop\.md)`|\(([^)\r\n]*loop\.md)\))')) {
    $ref = $m.Groups[1].Value
    if ([string]::IsNullOrWhiteSpace($ref)) { $ref = $m.Groups[2].Value }
    $ref = $ref.Trim().TrimStart("/", "\")
    if ([string]::IsNullOrWhiteSpace($ref)) { continue }
    $cand = Join-Path $repoRoot ($ref -replace "/", "\")
    if (Test-Path -LiteralPath $cand) {
      $paths += (Resolve-Path -LiteralPath $cand).Path
    }
  }
  return @($paths | Select-Object -Unique)
}

function Get-LoopPredicateState {
  param([string]$FilePath)
  $raw = Get-Content -LiteralPath $FilePath -Raw -ErrorAction SilentlyContinue
  $g = Get-MdSection $raw "Goal"
  $p = Get-MdSection $raw "Predicate"
  if ((Test-BlankSection $g) -or (Test-BlankSection $p)) { return "blank" }
  return "ok"
}

$loopPath = $null
if (-not [string]::IsNullOrWhiteSpace($Path)) {
  if (-not [System.IO.Path]::IsPathRooted($Path)) {
    $Path = Join-Path $repoRoot $Path
  }
  if (-not (Test-Path -LiteralPath $Path)) {
    Write-Output "CONDUCTOR_STATUS"
    Write-Output "error: loop.md not found at $Path"
    exit 1
  }
  $loopPath = (Resolve-Path -LiteralPath $Path).Path
}
elseif (-not [string]::IsNullOrWhiteSpace($Effort)) {
  $cand = Join-Path (Join-Path $scratch $Effort) "loop.md"
  if (Test-Path -LiteralPath $cand) {
    $loopPath = (Resolve-Path -LiteralPath $cand).Path
  }
  else {
    Write-Output "CONDUCTOR_STATUS"
    Write-Output ("error: loop.md not found for -Effort {0}" -f $Effort)
    exit 1
  }
}
else {
  if ($allLoops.Count -eq 1) {
    $loopPath = $allLoops[0].FullName
  }
  elseif ($allLoops.Count -gt 1) {
    Write-Output "CONDUCTOR_STATUS"
    Write-Output "error: several loop.md files; pass -Effort <slug> or -Path"
    foreach ($h in ($allLoops | Sort-Object FullName)) {
      Write-Output ("  {0}" -f (Get-Rel $h.FullName))
    }
    exit 1
  }
}

if (-not $loopPath) {
  Write-Output "CONDUCTOR_STATUS"
  Write-Output "error: no loop.md under .scratch"
  Write-Output "hint: /conductor writes one after the predicate is checkable. AskQuestion if it is missing."
  exit 1
}

$text = Get-Content -LiteralPath $loopPath -Raw
$goal = Get-MdSection $text "Goal"
$predicate = Get-MdSection $text "Predicate"
$wake = Get-MdSection $text "Wake prompt"
$ledger = Get-MdSection $text "Ledger"
$verifierSection = Get-MdSection $text "Verifier"
$childrenSection = Get-MdSection $text "Children"

$lastLedger = ""
if (-not [string]::IsNullOrWhiteSpace($ledger)) {
  foreach ($line in ($ledger -split "\r?\n")) {
    $t = $line.Trim()
    if ([string]::IsNullOrWhiteSpace($t)) { continue }
    if ($t.StartsWith("<!--")) { continue }
    $lastLedger = $t
  }
}

$effortDir = Split-Path -Parent $loopPath
$issuesDir = Join-Path $effortDir "issues"
$claimedRel = "none"
$ready = @()
$resolved = 0
if (Test-Path -LiteralPath $issuesDir) {
  foreach ($t in Get-ChildItem -LiteralPath $issuesDir -Filter "*.md" -File) {
    $st = Get-TicketStatus $t.FullName
    $rel = Get-Rel $t.FullName
    if ($st -eq "claimed") { $claimedRel = $rel }
    elseif ($st -eq "ready-for-agent") { $ready += $rel }
    elseif ($st -eq "resolved") { $resolved++ }
  }
}

$specPath = Join-Path $effortDir "spec.md"
$hasSpec = Test-Path -LiteralPath $specPath
$currentHash = Get-WorktreeHash
$verifierState = Get-VerifierState -Section $verifierSection -CurrentHash $currentHash
$childPaths = Get-ChildLoopPaths -Section $childrenSection

Write-Output "CONDUCTOR_STATUS"
Write-Output ("path: {0}" -f (Get-Rel $loopPath))
Write-Output ("spec: {0}" -f $(if ($hasSpec) { Get-Rel $specPath } else { "none (simple goal)" }))
Write-Output "goal:"
if (Test-BlankSection $goal) { Write-Output "  (missing — AskQuestion)" }
else { foreach ($line in ($goal -split "\r?\n")) { Write-Output ("  {0}" -f $line) } }
Write-Output "predicate:"
if (Test-BlankSection $predicate) { Write-Output "  (missing — AskQuestion; do not invent)" }
else { foreach ($line in ($predicate -split "\r?\n")) { Write-Output ("  {0}" -f $line) } }
Write-Output ("claimed: {0}" -f $claimedRel)
if ($ready.Count -gt 0) {
  Write-Output ("ready_for_agent: {0}" -f ($ready -join ", "))
}
else {
  Write-Output "ready_for_agent: none"
}
Write-Output ("tickets_resolved: {0}" -f $resolved)
Write-Output ("verifier: {0}" -f $verifierState)
Write-Output ("worktree_hash: {0}" -f $currentHash)
if ($childPaths.Count -eq 0) {
  Write-Output "children: none"
}
else {
  Write-Output "children:"
  foreach ($c in $childPaths) {
    $ct = Get-Content -LiteralPath $c -Raw -ErrorAction SilentlyContinue
    $cp = Get-MdSection $ct "Predicate"
    $cv = Get-VerifierState -Section (Get-MdSection $ct "Verifier") -CurrentHash $currentHash
    $predState = $(if (Test-BlankSection $cp) { "BLANK" } else { "ok" })
    Write-Output ("  {0} predicate={1} verifier={2}" -f (Get-Rel $c), $predState, $cv)
  }
}
Write-Output ("last_ledger: {0}" -f $(if ($lastLedger) { $lastLedger } else { "(empty)" }))
Write-Output "wake_prompt:"
if ([string]::IsNullOrWhiteSpace($wake)) { Write-Output "  (missing)" }
else { foreach ($line in ($wake -split "\r?\n")) { Write-Output ("  {0}" -f $line) } }

$fail = $false
if (Test-BlankSection $goal) {
  Write-Output "error: Goal is missing"
  $fail = $true
}
if (Test-BlankSection $predicate) {
  Write-Output "error: Predicate is missing"
  $fail = $true
}

if ($RequirePredicate) {
  $blankLoops = @()
  foreach ($h in $allLoops) {
    if ((Get-LoopPredicateState $h.FullName) -eq "blank") {
      $blankLoops += Get-Rel $h.FullName
    }
  }
  foreach ($c in $childPaths) {
    $rel = Get-Rel $c
    if ((Get-LoopPredicateState $c) -eq "blank" -and ($blankLoops -notcontains $rel)) {
      $blankLoops += $rel
    }
  }
  if ($blankLoops.Count -gt 0) {
    Write-Output "error: -RequirePredicate: blank Goal/Predicate in:"
    foreach ($b in $blankLoops) { Write-Output ("  {0}" -f $b) }
    $fail = $true
  }
}

if ($fail) { exit 1 }
exit 0
