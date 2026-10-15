# Predicate for the perf-safe-wins conductor loop (spec: spec.md next to this file).
# Exit 0 only when, in the worktree:
#   - the Windows suite is green,
#   - no golden reference, golden tolerance or realtime-budget threshold changed since the loop-start SHA,
#   - no AVX / fast-math build flag was introduced,
#   - every changed submodule pointer is on the pushed fork branch volum/perf-safe-wins,
#   - clang-format reports no replacements on changed C++ files (superproject and submodules),
#   - every ticket in issues/ is Status: resolved.
# Does not build the app, take screenshots or listen: UI tickets carry their own PNG-diff acceptance.
param([string] $Worktree = "C:\dev\VoLum")

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$failures = @()
function Fail([string] $msg) { $script:failures += $msg; Write-Output "FAIL: $msg" }

$loop = Get-Content -LiteralPath (Join-Path $here "loop.md") -Raw
$m = [regex]::Match($loop, 'Loop-start SHA:\s*`?([0-9a-f]{7,40})`?')
if (-not $m.Success) { Write-Output "FAIL: loop.md has no 'Loop-start SHA:' line yet"; exit 1 }
$base = $m.Groups[1].Value
if (-not (Test-Path -LiteralPath (Join-Path $Worktree ".git"))) { Write-Output "FAIL: worktree $Worktree missing"; exit 1 }

Push-Location $Worktree
try {
  # 1. Suite
  $global:LASTEXITCODE = 0
  & pwsh -NoProfile -File (Join-Path $Worktree "NeuralAmpModeler\scripts\run-tests-win.ps1")
  if ($LASTEXITCODE) { Fail "run-tests-win.ps1 exit $LASTEXITCODE" }

  # 2. Guardrail files untouched
  $frozen = @(
    "NeuralAmpModeler/tests/golden",
    "NeuralAmpModeler/tests/test_golden_dsp.cpp",
    "NeuralAmpModeler/tests/test_golden_rigs.cpp"
  )
  foreach ($f in $frozen) {
    & git diff --quiet $base -- $f
    if ($LASTEXITCODE -ne 0) { Fail "golden guard changed: $f" }
  }
  $renderDiff = (& git diff $base -- NeuralAmpModeler/tests/test_volum_golden_render.cpp) -join "`n"
  if ($renderDiff -match '(?m)^[-+][^-+].*(kTight|kDecisive|Tolerance|tolerance)') { Fail "golden render tolerances touched" }
  $budgetDiff = (& git diff $base -- NeuralAmpModeler/tests/test_volum_realtime_budget.cpp) -join "`n"
  # Owner decision 2026-09-26 (ticket 05): the `ratio <= 0.6` assertion was replaced by a direct
  # NamResetBlockSize check. Every other budget threshold stays frozen.
  if ($budgetDiff -match '(?m)^-.*(maxMedianShare|, 0\.50,)') { Fail "realtime-budget thresholds touched" }
  if ((Get-Content (Join-Path $Worktree "NeuralAmpModeler\tests\test_volum_realtime_budget.cpp") -Raw) -notmatch 'NamResetBlockSize\(block\)') { Fail "realtime-budget direct reset-size check missing" }

  # 3. No AVX / fast-math flags
  $flagDiff = (& git diff $base -- "*.vcxproj" "*.props" "*.xcconfig" "*.pbxproj" "*CMakeLists.txt") -join "`n"
  if ($flagDiff -match '(?m)^\+.*(AdvancedVectorExtensions|/arch:AVX|fp:fast|FloatingPointModel>Fast|ffast-math|-mavx|GCC_FAST_MATH\s*=\s*YES)') {
    Fail "AVX or fast-math flag introduced"
  }

  # 4. Submodule pointers pushed
  $forks = @{ "NeuralAmpModelerCore" = "https://github.com/guitarlum/NeuralAmpModelerCore.git"; "AudioDSPTools" = "https://github.com/guitarlum/AudioDSPTools.git" }
  $changedSubs = @()
  foreach ($sub in $forks.Keys) {
    $old = ((& git ls-tree $base $sub) -split "\s+")[2]
    $new = ((& git ls-tree HEAD $sub) -split "\s+")[2]
    if ($old -and $new -and $old -ne $new) {
      $changedSubs += @{ name = $sub; old = $old; new = $new }
      $remote = ((& git ls-remote --heads $forks[$sub] "refs/heads/volum/perf-safe-wins") -split "\s+")[0]
      if (-not $remote) { Fail "$sub pointer moved but volum/perf-safe-wins is not on the fork"; continue }
      & git -C $sub merge-base --is-ancestor $new $remote 2>$null
      if ($LASTEXITCODE -ne 0) { Fail "$sub pointer $new is not contained in pushed volum/perf-safe-wins ($remote)" }
    }
  }

  # 5. clang-format on changed C++ (superproject + moved submodules)
  $clang = Get-Command clang-format -ErrorAction SilentlyContinue
  $clangPath = if ($clang) { $clang.Source } else { $null }
  if (-not $clangPath) {
    $fallbacks = @(
      (Join-Path $env:LOCALAPPDATA "Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\Scripts\clang-format.exe")
    ) + @(Get-ChildItem "C:\Program Files*\Microsoft Visual Studio\2022\*\VC\Tools\Llvm\x64\bin\clang-format.exe" -ErrorAction SilentlyContinue | ForEach-Object FullName)
    $clangPath = $fallbacks | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
  }
  $cpp = @("*.h", "*.hpp", "*.cpp", "*.cc", "*.c")
  $files = @(& git diff --name-only $base -- $cpp | Where-Object { $_ } | ForEach-Object { Join-Path $Worktree $_ })
  foreach ($s in $changedSubs) {
    $files += @(& git -C $s.name diff --name-only $s.old $s.new -- $cpp | Where-Object { $_ } | ForEach-Object { Join-Path (Join-Path $Worktree $s.name) $_ })
  }
  $files = @($files | Where-Object { Test-Path -LiteralPath $_ } | Sort-Object -Unique)
  if ($files.Count -gt 0) {
    if (-not $clangPath) { Fail "clang-format 18 not found" }
    else {
      $dirty = @($files | Where-Object { (& $clangPath --output-replacements-xml $_ 2>&1 | Out-String) -match "<replacement " })
      if ($dirty.Count -gt 0) { Fail ("clang-format replacements: " + ($dirty -join ", ")) }
      else { Write-Output "clang-format: clean ($($files.Count) files)" }
    }
  }
}
finally { Pop-Location }

# 6. Tickets
$open = @(Get-ChildItem -LiteralPath (Join-Path $here "issues") -Filter "*.md" | Where-Object {
    $raw = Get-Content -LiteralPath $_.FullName -Raw
    -not ($raw -match '(?im)^Status:\s*(resolved|parked)\b')
  } | ForEach-Object { $_.Name })
if ($open.Count -gt 0) { Fail ("tickets not resolved: " + ($open -join ", ")) }

if ($failures.Count -gt 0) { Write-Output "predicate: NOT MET ($($failures.Count) failure(s))"; exit 1 }
Write-Output "predicate: MET"
exit 0
