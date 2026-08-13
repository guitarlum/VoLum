# Fail the test pass if tracked files name a third-party product.
#
# `installer/changelog.txt` is installed into the app folder with a Start Menu
# shortcut, so anything written there reaches every user. Describe what VoLum
# does; leave other people's product names out of shipped text.
#
# Patterns are read from `vendor-denylist.txt` next to this script when present,
# one regex per line, `#` for comments. Physical amplifier names VoLum models
# are fine and belong nowhere near that list.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here "..\..")).Path
$denyFile = Join-Path $here "vendor-denylist.txt"

if (-not (Test-Path $denyFile))
{
  Write-Host "No vendor-denylist.txt next to this script; nothing to check." -ForegroundColor DarkGray
  exit 0
}

# Case-insensitive. Keep each pattern tight: a false positive here trains people
# to ignore the check, which is worse than not having it.
$denied = Get-Content -LiteralPath $denyFile |
  ForEach-Object { $_.Trim() } |
  Where-Object { $_ -and -not $_.StartsWith("#") }

if ($denied.Count -eq 0)
{
  Write-Host "vendor-denylist.txt is empty; nothing to check." -ForegroundColor DarkGray
  exit 0
}

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
      foreach ($pattern in $denied)
      {
        if ($line -notmatch $pattern) { continue }
        if ($allowed | Where-Object { $line -match [regex]::Escape($_) }) { continue }
        $findings += [pscustomobject]@{
          File = $file; Line = $lineNo
          Text = $line.Trim().Substring(0, [Math]::Min(110, $line.Trim().Length))
        }
      }
    }
  }

  if ($findings.Count -gt 0)
  {
    Write-Host ""
    Write-Host "Third-party product references in tracked files:" -ForegroundColor Red
    foreach ($f in $findings)
    {
      Write-Host ("  {0}:{1}" -f $f.File, $f.Line) -ForegroundColor Yellow
      Write-Host ("      {0}" -f $f.Text)
    }
    Write-Host ""
    Write-Host "changelog.txt ships inside the installer. Describe the design," -ForegroundColor Red
    Write-Host "not what it was compared against." -ForegroundColor Red
    Write-Host ""
    exit 1
  }

  Write-Host "No third-party product references in tracked files." -ForegroundColor Green
  exit 0
}
finally
{
  Pop-Location
}
