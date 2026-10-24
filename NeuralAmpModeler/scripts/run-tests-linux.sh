#!/usr/bin/env bash
# Build and run the VoLum doctest suite on Linux.
#
#   bash NeuralAmpModeler/scripts/run-tests-linux.sh
#   bash NeuralAmpModeler/scripts/run-tests-linux.sh --clang --sanitize
#
# Default is GCC Release. --clang selects Clang. --sanitize (implies --clang)
# is Debug with ASan and UBSan, and it excludes the same cases as
# run-tests-mac.sh. At most 14 compile jobs: the box shares two build slots.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/.." && pwd)"
JOBS=14

# Mirrors run-tests-mac.sh: the tests compile plugin headers that depend on the
# local iPlug2 patches, so apply them before configuring. Idempotent.
bash "$PROJECT_DIR/iplug2-patches/apply-iplug2-patches.sh"

CLANG=0
SANITIZE=0
for arg in "$@"; do
  case "$arg" in
    --clang) CLANG=1 ;;
    --sanitize) SANITIZE=1; CLANG=1 ;;
    *)
      echo "Usage: $0 [--clang] [--sanitize]" >&2
      exit 1
      ;;
  esac
done

# Guard scripts from run-tests-win.ps1. The .ps1 files need PowerShell, which
# this host does not install. The checks that are not Windows-only are ported
# below; the rest are skipped on purpose:
#   check-agent-artifact-links.ps1 — PowerShell heuristic over the guidance
#     tree. run-tests-mac.sh does not run it. Nothing in the doctest binary
#     depends on it.
#   check-local-guards.ps1 — the owner's Windows clone (denylist + git hooks).
#     CI exits 0 when CI or GITHUB_ACTIONS is set. The box worktree has neither
#     the untracked denylist nor those hooks.
#   check-no-vendor-refs.ps1 — fail-closed without the untracked
#     vendor-denylist.txt. CI skips the scan when that file is absent. The box
#     does not have it.
echo "skipped guard: check-agent-artifact-links.ps1 (PowerShell-only; not run by run-tests-mac.sh)"
echo "skipped guard: check-local-guards.ps1 (Windows developer-machine hooks; CI no-ops it)"
echo "skipped guard: check-no-vendor-refs.ps1 (needs the untracked denylist; CI skips without it)"

python3 - "$PROJECT_DIR" "$REPO_ROOT" <<'PY'
import pathlib, re, subprocess, sys
project = pathlib.Path(sys.argv[1])
repo = pathlib.Path(sys.argv[2])
tests = project / "tests"
cmake = (tests / "CMakeLists.txt").read_text(encoding="utf-8")
vcx = (project / "projects" / "NeuralAmpModeler-Tests.vcxproj").read_text(encoding="utf-8")
names = sorted(p.name for p in tests.glob("*.cpp") if p.is_file())
missing = []
for name in names:
    in_cmake = re.search(r"(?m)^\s*" + re.escape(name) + r"\s*$", cmake) is not None
    in_vcx = ('ClCompile Include="..\\tests\\' + name + '"') in vcx
    if in_cmake and not in_vcx:
        missing.append(f"{name}: present in CMakeLists.txt, missing from NeuralAmpModeler-Tests.vcxproj")
    elif in_vcx and not in_cmake:
        missing.append(f"{name}: present in NeuralAmpModeler-Tests.vcxproj, missing from CMakeLists.txt")
    elif not in_cmake and not in_vcx:
        missing.append(f"{name}: present on disk, missing from BOTH build descriptors")
if missing:
    print("Test source parity check FAILED:", file=sys.stderr)
    for line in missing:
        print("  " + line, file=sys.stderr)
    sys.exit(1)
print(f"Test source parity OK ({len(names)} tests / both descriptors agree).")

listed = subprocess.check_output(
    ["git", "ls-files", "-s", "--", "NeuralAmpModeler/*.sh", "*.sh",
     ":(exclude)iPlug2/**", ":(exclude)NeuralAmpModelerCore/**",
     ":(exclude)AudioDSPTools/**", ":(exclude)eigen/**"],
    cwd=repo, text=True)
offenders = []
count = 0
for entry in listed.splitlines():
    if not entry.endswith(".sh"):
        continue
    count += 1
    mode = entry.split()[0]
    path = entry.split("\t", 1)[1]
    if mode != "100755":
        offenders.append(path)
if offenders:
    print("Shell scripts missing the executable bit in git:", file=sys.stderr)
    for path in offenders:
        print("  " + path, file=sys.stderr)
    print("Fix with: git update-index --chmod=+x <path>", file=sys.stderr)
    sys.exit(1)
print(f"Shell exec bits OK ({count} tracked .sh files).")

changelog = (project / "installer" / "changelog.txt").read_text(encoding="utf-8")
golden = project / "tests" / "golden"
problems = []
files = sorted(golden.glob("*.json"))
if not files:
    problems.append(f"no *.json references in {golden}")
for path in files:
    import json
    try:
        reason = str(json.loads(path.read_text(encoding="utf-8")).get("reason") or "")
    except Exception as exc:
        problems.append(f"{path.name}: not valid JSON ({exc})")
        continue
    if len(reason) < 12:
        problems.append(f"{path.name}: no regeneration reason (or one under 12 characters)")
        continue
    if reason not in changelog:
        problems.append(f'{path.name}: regenerated for "{reason}", but no changelog.txt line contains that text')
if problems:
    print("Golden changelog check FAILED:", file=sys.stderr)
    for line in problems:
        print("  " + line, file=sys.stderr)
    sys.exit(1)
print(f"Golden changelog OK ({len(files)} reference files, every regeneration reason is in changelog.txt).")
PY

# Headers only. run-tests-mac.sh does not download the SDK itself; macOS CI does,
# and it also builds the validator because that job packages plugins afterwards.
# The validator binary is a later gate (validate-vst3-linux.sh), not this suite.
SDK_HEADER="$REPO_ROOT/iPlug2/Dependencies/IPlug/VST3_SDK/pluginterfaces/vst/ivstaudioprocessor.h"
if [[ ! -f "$SDK_HEADER" ]]; then
  echo "Fetching VST3 SDK headers"
  ( cd "$REPO_ROOT/iPlug2/Dependencies/IPlug" && bash ./download-vst3-sdk.sh master )
fi
if [[ ! -f "$SDK_HEADER" ]]; then
  echo "VST3 SDK header missing after download: $SDK_HEADER" >&2
  exit 1
fi

# Prebuilt iPlug libs and the WAM SDK are for the app/VST3 package, which
# run-tests-mac.sh also does not build. This target does not link them.

BUILD_DIR="$PROJECT_DIR/build-tests-linux"
BUILD_TYPE=Release
CXX=g++
if [[ "$CLANG" == "1" ]]; then
  BUILD_DIR="$PROJECT_DIR/build-tests-linux-clang"
  CXX=clang++
fi
if [[ "$SANITIZE" == "1" ]]; then
  BUILD_DIR="$PROJECT_DIR/build-tests-linux-sanitized"
  BUILD_TYPE=Debug
fi

cmake -S "$PROJECT_DIR/tests" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DVOLUM_ENABLE_SANITIZERS="$SANITIZE"

cmake --build "$BUILD_DIR" -j "$JOBS"

BIN="$BUILD_DIR/NeuralAmpModeler-Tests"
cd "$REPO_ROOT"
if [[ "$SANITIZE" == "1" ]]; then
  # Same comma-separated list as run-tests-mac.sh. doctest keeps only the last
  # repeated --test-case-exclude, so this must stay one flag.
  EXCLUDE="$(python3 - "$SCRIPT_DIR/run-tests-mac.sh" <<'PY'
import pathlib, re, sys
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
found = re.findall(r'--test-case-exclude="([^"]+)"', text)
if len(found) != 1:
    sys.exit("run-tests-mac.sh sanitizer exclusion list changed shape")
print(found[0])
PY
)"
  "$BIN" --test-case-exclude="$EXCLUDE"
else
  set +e
  "$BIN" --duration=true
  rc=$?
  set -e
  if [[ "$rc" -ne 0 ]]; then
    if [[ "$rc" -gt 128 ]] && command -v gdb >/dev/null 2>&1; then
      echo "=== tests exited $rc; re-running under gdb for a backtrace ==="
      gdb -batch -ex run -ex bt -ex "quit 1" --args "$BIN" --duration=true || true
    fi
    exit "$rc"
  fi
fi
