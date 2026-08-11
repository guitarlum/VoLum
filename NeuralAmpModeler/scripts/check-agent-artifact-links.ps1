# Fail if agent guidance points at something that no longer exists.
#
# Rules, skills, and AGENTS.md are full of file references, and a rename makes
# them silently wrong. Wrong guidance is worse than verbose guidance: it sends
# the next agent to a path that isn't there. Two mechanical checks:
#
#   1. Every backticked file reference resolves to a real file.
#   2. Every rule's `globs:` pattern matches at least one file - a glob that
#      matches nothing means the rule never attaches and is pure cost.
#
# Heuristics stay deliberately conservative: a false alarm here trains people to
# ignore the check, so anything ambiguous (branch names, registry keys, env-var
# paths, wildcards, runtime files) is skipped rather than guessed at.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path

# Directories that begin a real in-repo path.
$knownRoots = @("NeuralAmpModeler/", "docs/", "rigs/", ".cursor/", ".github/", "training/",
  "iPlug2/", "AudioDSPTools/", "NeuralAmpModelerCore/", "eigen/", "backlog/", "scripts/", "tests/")

# Extensions we are willing to resolve by bare filename. Runtime artifacts
# (*.json, *.log, *.vst3, *.exe) are excluded on purpose: they are described in
# the docs but never tracked.
$sourceExt = @(".h", ".cpp", ".ps1", ".sh", ".bat", ".py", ".mdc")
$pathExt = $sourceExt + @(".md", ".yml", ".yaml", ".iss", ".rtf", ".txt", ".vcxproj", ".xcodeproj")

Push-Location $repoRoot
try
{
  $tracked = & git ls-files
  if ($LASTEXITCODE -ne 0) { Write-Error "git ls-files failed" }
  $trackedSet = [System.Collections.Generic.HashSet[string]]::new([string[]]$tracked)
  $byLeaf = @{}
  foreach ($p in $tracked)
  {
    $leaf = Split-Path $p -Leaf
    if (-not $byLeaf.ContainsKey($leaf)) { $byLeaf[$leaf] = @() }
    $byLeaf[$leaf] += $p
  }

  $docs = @()
  $docs += Get-ChildItem -Path ".cursor\rules" -Filter "*.mdc" -ErrorAction SilentlyContinue
  $docs += Get-ChildItem -Path ".cursor\skills", ".agents\skills" -Recurse -Filter "SKILL.md" -ErrorAction SilentlyContinue
  if (Test-Path "AGENTS.md") { $docs += Get-Item "AGENTS.md" }

  $problems = @()

  foreach ($doc in $docs)
  {
    $rel = $doc.FullName.Replace($repoRoot + "\", "").Replace("\", "/")
    $text = Get-Content $doc.FullName -Raw
    $lines = Get-Content $doc.FullName

    # --- 1. backticked references ------------------------------------------
    foreach ($m in [regex]::Matches($text, '`([^`\r\n]+)`'))
    {
      $token = $m.Groups[1].Value.Trim()

      if ($token -match '[\s\*\?%\$~]') { continue }      # commands, wildcards, env vars
      if ($token -match '://') { continue }                # URLs
      if ($token -match '^(HK[A-Z_]+)\\') { continue }     # registry keys
      if ($token -match '^\.[A-Za-z0-9.]+$') { continue }  # bare extensions like .inc.cpp
      # "formerly `X`" marks a deliberate historical reference to a renamed file.
      $lead = $text.Substring([Math]::Max(0, $m.Index - 20), [Math]::Min(20, $m.Index))
      if ($lead -match '(?i)formerly\s*$') { continue }
      $token = $token.TrimEnd(".,;:")

      $normalized = $token.Replace("\", "/")
      # Placeholders (`<ref>`), shell operators (`||`) and C++ expressions
      # (`mModel->process`) are not paths, and GetExtension below throws on the
      # characters they contain rather than returning empty.
      if ($normalized.IndexOfAny([System.IO.Path]::GetInvalidPathChars()) -ge 0) { continue }
      $hasRoot = $false
      foreach ($root in $knownRoots) { if ($normalized.StartsWith($root)) { $hasRoot = $true; break } }
      $ext = [System.IO.Path]::GetExtension($normalized).ToLowerInvariant()

      if ($hasRoot)
      {
        # A path into the repo: resolve it directly (may be a directory).
        $probe = Join-Path $repoRoot ($normalized -replace "/", "\")
        if (-not (Test-Path $probe))
        {
          $lineNo = ($lines | Select-String -SimpleMatch $token | Select-Object -First 1).LineNumber
          $problems += "{0}:{1}  missing path: {2}" -f $rel, $lineNo, $token
        }
        continue
      }

      # A bare filename: accept it if any tracked file has that leaf.
      if ($normalized -notmatch "/" -and $sourceExt -contains $ext)
      {
        if (-not $byLeaf.ContainsKey($normalized))
        {
          $lineNo = ($lines | Select-String -SimpleMatch $token | Select-Object -First 1).LineNumber
          $problems += "{0}:{1}  missing file: {2}" -f $rel, $lineNo, $token
        }
      }
    }

    # --- 2. globs that match nothing ---------------------------------------
    if ($doc.Extension -eq ".mdc")
    {
      $inGlobs = $false
      foreach ($line in $lines)
      {
        if ($line -match "^globs:") { $inGlobs = $true; continue }
        if ($inGlobs)
        {
          if ($line -match "^\s*-\s*(.+?)\s*$")
          {
            $pattern = $matches[1].Trim("'`"")
            $hit = $tracked | Where-Object { $_ -like $pattern } | Select-Object -First 1
            if (-not $hit)
            {
              # Submodule contents and ignored trees (.agents/) are absent from
              # git ls-files but are still matched by Cursor, so fall back to disk.
              $literal = ($pattern -split '[\*\?]')[0]
              $dir = Split-Path $literal -Parent
              if ([string]::IsNullOrEmpty($dir)) { $dir = "." }
              if (Test-Path $dir)
              {
                $hit = Get-ChildItem -Path $dir -Recurse -File -Force -ErrorAction SilentlyContinue |
                  Where-Object { $_.FullName.Replace($repoRoot + "\", "").Replace("\", "/") -like $pattern } |
                  Select-Object -First 1
              }
            }
            if (-not $hit) { $problems += "{0}  glob matches nothing: {1}" -f $rel, $pattern }
          }
          else { $inGlobs = $false }
        }
      }
    }
  }

  if ($problems.Count -gt 0)
  {
    Write-Host "Agent guidance points at things that do not exist:" -ForegroundColor Red
    foreach ($p in $problems) { Write-Host "  $p" -ForegroundColor Red }
    exit 1
  }

  Write-Host "Agent artifact links OK ($($docs.Count) rule/skill/index files checked)."
  exit 0
}
finally
{
  Pop-Location
}
