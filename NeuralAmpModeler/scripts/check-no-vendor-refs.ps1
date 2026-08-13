# Fail if tracked files name a competitor product or describe how it was studied.
#
# VoLum's DSP research is done locally against gear and plug-ins we own. That is
# fine to do and fine to keep notes on; it is not fine to publish. This repo is
# public, and `installer/changelog.txt` is additionally installed into the app
# folder with a Start Menu shortcut, so a changelog line naming a competitor
# ships to every user.
#
# Rather than trusting everyone to remember that while writing a thorough
# changelog entry, this check runs in the normal test pass and fails loudly.
#
# What belongs in the denylist: names of competing SOFTWARE and the vocabulary
# of binary analysis. What does NOT: names of physical amplifiers VoLum models
# (see $allowed) - naming captured hardware is ordinary for a modeller.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path

# Case-insensitive regexes. Keep each one tight: a false positive here trains
# people to ignore the check, which is worse than not having it.
$denied = @(
  @{ Pattern = 'reverse[-\s]?engineer'; Why = "reverse-engineering vocabulary" },
  @{ Pattern = 'clean[-\s]?room'; Why = "clean-room claim (legally self-defeating next to any disassembly)" },
  @{ Pattern = '\bghidra\b'; Why = "disassembler" },
  @{ Pattern = '\bfrida\b'; Why = "dynamic instrumentation tool" },
  @{ Pattern = '\bx64dbg\b|\bIDA Pro\b'; Why = "debugger / disassembler" },
  @{ Pattern = 'decompil'; Why = "decompilation" },
  @{ Pattern = 'PACE-encrypted|PACE-protected'; Why = "describes copy protection on an analysed binary" },
  @{ Pattern = 'iLok-licensed'; Why = "describes licensing of an analysed binary" },
  @{ Pattern = '\bNDSP\b|Neural DSP'; Why = "competitor name" },
  @{ Pattern = '\bRabea\b|Archetype [A-Z]'; Why = "competitor product name" },
  @{ Pattern = '\bPOG\b|\bOC-2\b|\bOC-5\b'; Why = "competitor product name" }
)

# Substrings that legitimately contain a denied pattern.
$allowed = @(
  'LoadedInDSP',      # mNewModelLoadedInDSP / mNewNAMLoadedInDSP
  'PACEAntiPiracy',   # AAX signing toolchain, a real build dependency
  'ILOK_ID',          # AAX signing credentials placeholder
  'ILOK_PWD',
  'pace-central',     # AAX signing docs URL
  'paceap.com'
)

# Vendored/third-party trees we do not author.
$skipDirs = @('iPlug2/', 'eigen/', 'NeuralAmpModelerCore/', 'AudioDSPTools/',
  'NeuralAmpModeler/tests/third_party/')

Push-Location $repoRoot
try
{
  $files = git ls-files | Where-Object {
    $f = $_
    -not ($skipDirs | Where-Object { $f.StartsWith($_) })
  }

  $findings = @()
  foreach ($file in $files)
  {
    # Text only: binaries would produce meaningless byte matches.
    if ($file -match '\.(png|jpg|jpeg|gif|ico|icns|wav|nam|zip|pdf|ttf|otf|dll|exe|lib|pdb)$') { continue }
    if (-not (Test-Path $file)) { continue }

    $lineNo = 0
    foreach ($line in (Get-Content -LiteralPath $file -ErrorAction SilentlyContinue))
    {
      $lineNo++
      foreach ($rule in $denied)
      {
        if ($line -notmatch $rule.Pattern) { continue }
        if ($allowed | Where-Object { $line -match [regex]::Escape($_) }) { continue }
        $findings += [pscustomobject]@{
          File = $file; Line = $lineNo; Why = $rule.Why
          Text = $line.Trim().Substring(0, [Math]::Min(110, $line.Trim().Length))
        }
      }
    }
  }

  if ($findings.Count -gt 0)
  {
    Write-Host ""
    Write-Host "Vendor / reverse-engineering references found in tracked files:" -ForegroundColor Red
    foreach ($f in $findings)
    {
      Write-Host ("  {0}:{1}  [{2}]" -f $f.File, $f.Line, $f.Why) -ForegroundColor Yellow
      Write-Host ("      {0}" -f $f.Text)
    }
    Write-Host ""
    Write-Host "This repo is public, and changelog.txt ships inside the installer." -ForegroundColor Red
    Write-Host "Describe the design, not the product it was compared against, and keep" -ForegroundColor Red
    Write-Host "research material in local scratch only." -ForegroundColor Red
    Write-Host ""
    exit 1
  }

  Write-Host "No vendor / reverse-engineering references in tracked files." -ForegroundColor Green
  exit 0
}
finally
{
  Pop-Location
}
