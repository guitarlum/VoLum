# Checkable predicate for .scratch/1.3.0-rc-docs/loop.md
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$fail = [System.Collections.Generic.List[string]]::new()

function Get-Text([string]$Rel) {
  $p = Join-Path $root $Rel
  if (-not (Test-Path -LiteralPath $p)) { $script:fail.Add("missing $Rel"); return "" }
  return Get-Content -LiteralPath $p -Raw
}

function Assert-Order([string]$Rel, [string[]]$Needles) {
  $t = Get-Text $Rel
  if ([string]::IsNullOrEmpty($t)) { return }
  $pos = 0
  foreach ($n in $Needles) {
    $i = $t.IndexOf($n, $pos)
    if ($i -lt 0) { $script:fail.Add("$Rel missing in order: $n"); return }
    $pos = $i + $n.Length
  }
}

function Assert-Contains([string]$Rel, [string]$Needle) {
  $t = Get-Text $Rel
  if ([string]::IsNullOrEmpty($t)) { return }
  if ($t.IndexOf($Needle) -lt 0) { $script:fail.Add("$Rel missing: $Needle") }
}

function Assert-Absent([string]$Rel, [string]$Needle) {
  $t = Get-Text $Rel
  if ([string]::IsNullOrEmpty($t)) { return }
  if ($t.IndexOf($Needle) -ge 0) { $script:fail.Add("$Rel still has: $Needle") }
}

function Assert-FreshPng([string]$Name, [datetime]$Min) {
  $p = Join-Path $root "docs\$Name"
  if (-not (Test-Path -LiteralPath $p)) { $script:fail.Add("missing docs/$Name"); return }
  $t = (Get-Item -LiteralPath $p).LastWriteTime
  if ($t -lt $Min) { $script:fail.Add("stale docs/$Name ($t)") }
}

$fresh = [datetime]"2026-09-09"
foreach ($png in @(
    "user-guide-play-empty.png",
    "user-guide-play-picker.png",
    "user-guide-play.png",
    "user-guide-main.png",
    "user-guide-settings-signal.png",
    "user-guide-settings-midi.png",
    "user-guide-settings-system.png",
    "user-guide-pack-export.png",
    "user-guide-pack-import.png",
    "user-guide-chorus.png",
    "user-guide-post.png"
  )) {
  Assert-FreshPng $png $fresh
}

$playOrder = @(
  "user-guide-play-empty.png",
  "user-guide-play-picker.png",
  "user-guide-play.png"
)
$packOrder = @(
  "user-guide-pack-export.png",
  "user-guide-pack-import.png"
)
Assert-Order "docs/user-guide.en.md" $playOrder
Assert-Order "docs/user-guide.de.md" $playOrder
Assert-Order "docs/user-guide.en.md" $packOrder
Assert-Order "docs/user-guide.de.md" $packOrder
Assert-Order "README.md" @("user-guide-play.png", "user-guide-main.png")
Assert-Order "README.de.md" @("user-guide-play.png", "user-guide-main.png")

Assert-Absent "docs/user-guide.en.md" "tall enough for both help lines"
Assert-Absent "docs/user-guide.de.md" "hoch genug fuer beide Hilfszeilen"
Assert-Absent "docs/user-guide.de.md" "hoch genug für beide Hilfszeilen"
# Privacy URL may stay; the schema dump must not.
Assert-Absent "docs/user-guide.en.md" "sparkleVersion"
Assert-Absent "docs/user-guide.de.md" "sparkleVersion"

function Assert-FiveWhy([string]$Rel, [string]$Heading, [string[]]$Need) {
  $t = Get-Text $Rel
  if ([string]::IsNullOrEmpty($t)) { return }
  $m = [regex]::Match($t, "(?ms)^##\s+$([regex]::Escape($Heading))\s*\r?\n(.*?)(?=^##\s|\z)")
  if (-not $m.Success) { $script:fail.Add("$Rel missing heading $Heading"); return }
  $body = $m.Groups[1].Value
  $items = [regex]::Matches($body, '(?m)^-\s')
  if ($items.Count -ne 5) { $script:fail.Add("$Rel $Heading has $($items.Count) bullets, want 5") }
  foreach ($n in $Need) {
    if ($body.IndexOf($n) -lt 0) { $script:fail.Add("$Rel $Heading missing: $n") }
  }
}

Assert-FiveWhy "README.md" "Why It Stands Out" @("PLAY", "MIDI", "Chorus", "Pack", "update")
Assert-FiveWhy "README.de.md" "Was VoLum Besonders Macht" @("PLAY", "MIDI", "Chorus", "Pack", "Update")

Assert-Contains "docs/screenshot-recipes.md" "user-guide-play-empty.png"
Assert-Contains "docs/screenshot-recipes.md" "user-guide-play-picker.png"
Assert-Contains "docs/screenshot-recipes.md" "user-guide-pack-export.png"
Assert-Contains "docs/screenshot-recipes.md" "Invalid slot"
Assert-Contains "docs/screenshot-recipes.md" "743, 22"

$ticket = Get-Text ".scratch/play-vs-build/issues/04-docs-and-polish.md"
if ($ticket -notmatch '(?im)^Status:\s*resolved') {
  $fail.Add("ticket 04 is not Status: resolved")
}

$cl = Get-Text "NeuralAmpModeler/installer/changelog.txt"
if ($cl -notmatch '(?m)^09/09/2026 - VoLum 1\.3\.0 \(docs') {
  $fail.Add("changelog missing 09/09/2026 1.3.0 docs line")
}

if ($fail.Count -gt 0) {
  Write-Host "VERIFY FAIL"
  $fail | ForEach-Object { Write-Host ("- " + $_) }
  exit 1
}
Write-Host "VERIFY OK"
exit 0
