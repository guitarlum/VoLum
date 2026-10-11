#!/usr/bin/env bash
# Prove this run's macOS installer upgrades the published 1.2.x releases cleanly.
#
# CI only. It installs into /Applications and /Library/Audio/Plug-Ins with sudo,
# deletes whatever VoLum is installed there, forgets the VoLum package receipts and
# writes a VoLum library under ~/Library/Application Support. On a developer Mac that
# replaces the installed VoLum and puts the real library in reach, so it refuses unless
# GITHUB_ACTIONS=true or --throwaway-vm is passed, before anything is downloaded,
# mounted, installed or written.
#
# One job, both legs in sequence (v1.2.3, then v1.2.2) rather than a matrix: the job is
# looked up by its exact name, which a matrix suffix would change. A runner cannot be
# reset mid-job, so each leg ends by deleting the installed app, VST3, AU and bundled
# rigs and forgetting the VoLum receipts, and the next leg starts only if none of
# them, and no VoLum library, is left.
#
# Negative control first: the version checker every leg uses is run against the app,
# VST3 and AU inside this run's installer package (expanded, not installed) with the
# right version, which must pass, and with 9.9.9, which must fail on all three. Only
# then does it print NEGATIVE CONTROL OK.
#
# Per leg:
#   1. Install the old release's "VoLum Installer.pkg" (every VoLum choice selected):
#      app, VST3 and AU report the old version (CFBundleShortVersionString), and the
#      app, VST3, AU and rigs receipts carry it.
#   2. Seed a 1.2.x library: run the installed old standalone's binary with HOME set
#      to a temp folder (VoLum resolves every library path from $HOME) and quit it
#      with an AppleScript quit, so its shutdown save writes the library. The result
#      is mirrored into the runner's own ~/Library/Application Support/VoLum, where an
#      installer that touched user data would hit it, and every file of both copies
#      is hashed.
#   3. Install this run's package over it: app, VST3 and AU report the new version,
#      the four receipts carry it, every rig file the package ships is present with
#      the repo's bytes, both library hashes are unchanged, and auval -v finds and
#      validates the new AU.
#   4. Gatekeeper (first leg only): quarantined copies of this run's DMG, package and
#      installed app go through spctl --assess; the verbatim output is written to the
#      job summary. Recorded, not asserted: the build is ad-hoc signed, not notarized.
#   5. Run the installed new standalone's binary once against the seeded temp HOME,
#      quit it the same way: it exits by itself, no VoLum crash report appears under
#      any DiagnosticReports folder, its library still parses, and the runner's own
#      library mirror is unchanged.
#
#   bash NeuralAmpModeler/scripts/installer-upgrade-mac.sh --artifact-dir <VoLum-mac artifact>

set -o pipefail

ARTIFACT_DIR=""
FROM_TAGS="v1.2.3 v1.2.2"
LAUNCH_SETTLE_SEC=15
QUIT_TIMEOUT_SEC=60
THROWAWAY_VM=0
ARGS_ERROR=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --artifact-dir) ARTIFACT_DIR="${2:-}"; shift 2 ;;
    --from) FROM_TAGS="${2:-}"; shift 2 ;;
    --throwaway-vm) THROWAWAY_VM=1; shift ;;
    *) ARGS_ERROR="unknown argument: $1"; shift ;;
  esac
done

if [[ "${GITHUB_ACTIONS:-}" != "true" && "$THROWAWAY_VM" != "1" ]]; then
  echo "REFUSED: installer-upgrade-mac.sh installs VoLum into /Applications and /Library/Audio/Plug-Ins with sudo,"
  echo "deletes the installed VoLum, forgets its package receipts and writes a VoLum library under"
  echo "~/Library/Application Support. It runs on GitHub Actions (GITHUB_ACTIONS=true) or with --throwaway-vm"
  echo "on a machine you are prepared to lose. Nothing was changed."
  exit 2
fi

if [[ -n "$ARGS_ERROR" ]]; then
  echo "ERROR: $ARGS_ERROR" >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK="${RUNNER_TEMP:-/tmp}/volum-installer-upgrade-mac"
DIAG="$WORK/diagnostics"

APP="/Applications/VoLum.app"
APP_BIN="$APP/Contents/MacOS/VoLum"
VST3="/Library/Audio/Plug-Ins/VST3/VoLum.vst3"
AU="/Library/Audio/Plug-Ins/Components/VoLum.component"
SYSTEM_SUPPORT="/Library/Application Support/VoLum"
RIGS="$SYSTEM_SUPPORT/VoLumRigs"
RECEIPTS="com.Lum.app.pkg.VoLum com.Lum.vst3.pkg.VoLum com.Lum.au.pkg.VoLum com.Lum.rigs.pkg.VoLum"
REAL_HOME="$HOME"
REAL_LIBRARY="$REAL_HOME/Library/Application Support/VoLum"
CRASH_DIRS="$REAL_HOME/Library/Logs/DiagnosticReports /Library/Logs/DiagnosticReports"

CHECKS=0
FAILED=0
FAILURES=""
LEG="setup"
MOUNTS=""
LAUNCHED_PID=""
CREATED_REAL_LIBRARY=0
GATEKEEPER_DONE=0

check() {
  local what="$1" ok="$2" detail="${3:-}"
  CHECKS=$((CHECKS + 1))
  local label="[$LEG] $what"
  if [[ "$ok" == "1" ]]; then
    echo "  PASS  $label"
  else
    FAILED=$((FAILED + 1))
    FAILURES="${FAILURES}  - ${label}"$'\n'
    echo "  FAIL  $label${detail:+ - $detail}"
  fi
}

check_eq() {
  if [[ "$2" == "$3" ]]; then check "$1" 1; else check "$1" 0 "expected '$2', got '$3'"; fi
}

truth() {
  if "$@" >/dev/null 2>&1; then echo 1; else echo 0; fi
}

one_line() {
  tr '\n' ';' | sed 's/;$//; s/;/; /g'
}

mount_dmg() {
  local dmg="$1" mountpoint="$2"
  mkdir -p "$mountpoint"
  if ! hdiutil attach "$dmg" -nobrowse -readonly -mountpoint "$mountpoint" >/dev/null; then
    return 1
  fi
  MOUNTS="$MOUNTS"$'\n'"$mountpoint"
}

unmount_dmg() {
  hdiutil detach "$1" >/dev/null 2>&1 || hdiutil detach -force "$1" >/dev/null 2>&1 || true
}

cleanup() {
  if [[ -n "$LAUNCHED_PID" ]] && kill -0 "$LAUNCHED_PID" 2>/dev/null; then
    kill -9 "$LAUNCHED_PID" 2>/dev/null || true
  fi
  local m
  while IFS= read -r m; do
    [[ -n "$m" ]] && unmount_dmg "$m"
  done <<< "$MOUNTS"
}
trap cleanup EXIT

# Every version assertion goes through here, the negative control included. Takes
# the expected version, then label/bundle pairs; prints one line per bundle that is
# missing or reports another version, nothing when all of them report it.
version_problems() {
  local expected="$1"
  shift
  while [[ $# -ge 2 ]]; do
    local label="$1" bundle="$2" reported
    shift 2
    if [[ ! -f "$bundle/Contents/Info.plist" ]]; then
      echo "$label missing at $bundle"
      continue
    fi
    reported="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$bundle/Contents/Info.plist" 2>/dev/null)"
    if [[ "$reported" != "$expected" ]]; then
      echo "$label reports '$reported', expected '$expected'"
    fi
  done
}

assert_versions() {
  local expected="$1" when="$2" problems
  problems="$(version_problems "$expected" "VoLum.app" "$APP" "VoLum.vst3" "$VST3" "VoLum.component" "$AU")"
  check "app, VST3 and AU report $expected $when" "$([[ -z "$problems" ]] && echo 1 || echo 0)" "$(echo "$problems" | one_line)"
}

receipt_version() {
  pkgutil --pkg-info "$1" 2>/dev/null | awk '/^version: /{print $2}'
}

assert_receipts() {
  local expected="$1" when="$2" listed id
  listed="$(pkgutil --pkgs 2>/dev/null)"
  for id in $RECEIPTS; do
    check "pkgutil --pkgs lists $id $when" "$(echo "$listed" | grep -qxF "$id" && echo 1 || echo 0)"
    check_eq "receipt $id is version $expected $when" "$expected" "$(receipt_version "$id")"
  done
}

# Relative path and SHA-256 of every file under $1, sorted; nothing for an absent root.
tree_hash() {
  [[ -d "$1" ]] || return 0
  (cd "$1" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256)
}

assert_tree_unchanged() {
  local what="$1" root="$2" before="$3" when="$4" now diff
  now="$(tree_hash "$root")"
  diff="$(diff <(echo "$before") <(echo "$now") | grep '^[<>]' | head -n 10 | one_line)"
  check "$what at $root unchanged $when ($(echo "$now" | grep -c .) files)" "$([[ "$before" == "$now" ]] && echo 1 || echo 0)" "$diff"
}

# Every rig file the package's rigs component ships, as makeinstaller-mac.sh stages it.
bundled_rig_list() {
  local ampdir f
  for ampdir in "$REPO_ROOT/rigs"/*/; do
    for f in "$ampdir"*.nam; do
      [[ -f "$f" ]] && echo "$(basename "$ampdir")/$(basename "$f")"
    done
  done
  echo "factory-presets.json"
}

assert_bundled_rigs() {
  local when="$1" rel missing="" differ="" total=0
  while IFS= read -r rel; do
    [[ -z "$rel" ]] && continue
    total=$((total + 1))
    if [[ ! -f "$RIGS/$rel" ]]; then
      missing="$missing$rel"$'\n'
    elif [[ "$(shasum -a 256 "$RIGS/$rel" | awk '{print $1}')" != "$(shasum -a 256 "$REPO_ROOT/rigs/$rel" | awk '{print $1}')" ]]; then
      differ="$differ$rel"$'\n'
    fi
  done <<< "$BUNDLED_RIGS"
  check "all $total bundled rig files present in $RIGS $when" "$([[ -z "$missing" ]] && echo 1 || echo 0)" \
    "missing: $(echo "$missing" | head -n 10 | one_line)"
  check "bundled rig files carry the repo's bytes $when" "$([[ -z "$differ" ]] && echo 1 || echo 0)" \
    "differ: $(echo "$differ" | head -n 10 | one_line)"
}

# Select every VoLum choice the package offers, whatever that release shipped.
write_choices() {
  local pkg="$1" out="$2"
  installer -showChoicesXML -pkg "$pkg" > "$out.offered" || return 1
  python3 - "$out.offered" "$out" <<'PY'
import plistlib
import sys

with open(sys.argv[1], "rb") as f:
    offered = plistlib.load(f)
ids = []

def walk(items):
    for item in items:
        cid = item.get("choiceIdentifier", "")
        if cid.startswith("com.Lum.") and cid not in ids:
            ids.append(cid)
        walk(item.get("childItems", []))

walk(offered)
with open(sys.argv[2], "wb") as f:
    plistlib.dump([{"choiceIdentifier": i, "choiceAttribute": "selected", "attributeSetting": 1} for i in ids], f)
print(" ".join(ids))
PY
}

install_pkg() {
  local pkg="$1" label="$2" choices="$WORK/$label-choices.plist" ids code
  ids="$(write_choices "$pkg" "$choices")" || { echo "  could not read the choices of $pkg"; return 1; }
  echo "  $label: sudo installer -pkg \"$pkg\" -target / (choices: $ids)"
  sudo installer -pkg "$pkg" -target / -applyChoiceChangesXML "$choices" 2>&1 | tee "$DIAG/$label-installer.log" | sed 's/^/    /'
  code=${PIPESTATUS[0]}
  return "$code"
}

remove_volum() {
  sudo rm -rf "$APP" "$REAL_HOME/Applications/VoLum.app" "$VST3" "$AU" "$RIGS"
  sudo rmdir "$SYSTEM_SUPPORT" 2>/dev/null || true
  local id
  for id in $RECEIPTS; do
    sudo pkgutil --forget "$id" >/dev/null 2>&1 || true
  done
}

machine_clean() {
  local when="$1" before=$FAILED listed
  check "no VoLum.app in /Applications or ~/Applications $when" "$([[ ! -e "$APP" && ! -e "$REAL_HOME/Applications/VoLum.app" ]] && echo 1 || echo 0)"
  check "no VoLum VST3 at $VST3 $when" "$([[ ! -e "$VST3" ]] && echo 1 || echo 0)"
  check "no VoLum AU at $AU $when" "$([[ ! -e "$AU" ]] && echo 1 || echo 0)"
  check "no bundled rigs at $RIGS $when" "$([[ ! -e "$RIGS" ]] && echo 1 || echo 0)"
  listed="$(pkgutil --pkgs 2>/dev/null | grep -F 'com.Lum.' | one_line)"
  check "no VoLum package receipts $when" "$([[ -z "$listed" ]] && echo 1 || echo 0)" "found: $listed"
  check "no VoLum library at $REAL_LIBRARY $when" "$([[ ! -e "$REAL_LIBRARY" ]] && echo 1 || echo 0)"
  [[ $FAILED -eq $before ]]
}

# A child that exited but is not reaped yet is a zombie, which kill -0 still finds.
running() {
  local state
  state="$(ps -p "$1" -o stat= 2>/dev/null | tr -d ' ')"
  [[ -n "$state" && "$state" != Z* ]]
}

crash_reports_since() {
  local marker="$1" dir
  for dir in $CRASH_DIRS "$2/Library/Logs/DiagnosticReports"; do
    [[ -d "$dir" ]] || continue
    find "$dir" -maxdepth 2 -type f -iname 'VoLum*' -newer "$marker" 2>/dev/null
  done
}

# Run the installed standalone's binary with HOME=$2 and quit it through an AppleScript
# quit, which NSApplication turns into terminate: and the app's own shutdown path.
launch_and_quit() {
  local version="$1" home="$2" label="$3" marker="$WORK/$label.marker" out="$DIAG/$label-stdout.log"
  local lib="$home/Library/Application Support/VoLum" alive=1 waited=0 exited=0 code="" quit_out reports
  touch "$marker"
  sleep 1
  echo "  $label: HOME=$home $APP_BIN"
  HOME="$home" "$APP_BIN" > "$out" 2>&1 &
  LAUNCHED_PID=$!
  while [[ $waited -lt $LAUNCH_SETTLE_SEC ]]; do
    if ! running "$LAUNCHED_PID"; then alive=0; break; fi
    sleep 1
    waited=$((waited + 1))
  done
  check "$version standalone is still running ${LAUNCH_SETTLE_SEC} s after launch on the temp HOME" "$alive"
  screencapture -x "$DIAG/$label.png" >/dev/null 2>&1 || true

  if [[ $alive -eq 1 ]]; then
    quit_out="$(osascript -e 'with timeout of 30 seconds' -e 'tell application "VoLum" to quit' -e 'end timeout' 2>&1)"
    echo "    AppleScript quit: ${quit_out:-sent}"
    waited=0
    while [[ $waited -lt $QUIT_TIMEOUT_SEC ]]; do
      if ! running "$LAUNCHED_PID"; then exited=1; break; fi
      sleep 1
      waited=$((waited + 1))
    done
  else
    exited=1
  fi
  if [[ $exited -eq 1 ]]; then
    wait "$LAUNCHED_PID"
    code=$?
    echo "    exit status $code after ${waited} s"
  else
    echo "    still running ${QUIT_TIMEOUT_SEC} s after the AppleScript quit; killed"
    kill -9 "$LAUNCHED_PID" 2>/dev/null || true
    wait "$LAUNCHED_PID" 2>/dev/null || true
  fi
  LAUNCHED_PID=""
  if [[ $alive -eq 1 ]]; then
    check "$version standalone exits on its own within ${QUIT_TIMEOUT_SEC} s of an AppleScript quit" "$exited"
  fi
  check "$version standalone was not killed by a signal (exit status '$code')" "$([[ -n "$code" && $code -lt 128 ]] && echo 1 || echo 0)"
  sleep 10
  reports="$(crash_reports_since "$marker" "$home")"
  check "no VoLum crash report under DiagnosticReports after the $version run" "$([[ -z "$reports" ]] && echo 1 || echo 0)" "$(echo "$reports" | one_line)"
  if [[ -d "$lib" ]]; then
    mkdir -p "$DIAG/$label-library"
    cp "$lib"/*.log "$lib"/*.json "$lib"/*.ini "$DIAG/$label-library/" 2>/dev/null || true
  fi
}

json_ok() {
  python3 -c 'import json, sys; json.load(open(sys.argv[1]))' "$1" 2>/dev/null
}

content_has_schema() {
  python3 -c 'import json, sys; sys.exit(0 if json.load(open(sys.argv[1])).get("schemaVersion") is not None else 1)' "$1" 2>/dev/null
}

assert_auval() {
  local plist="$AU/Contents/Info.plist" type subtype mfr log="$DIAG/auval.log" code
  type="$(/usr/libexec/PlistBuddy -c 'Print :AudioComponents:0:type' "$plist" 2>/dev/null)"
  subtype="$(/usr/libexec/PlistBuddy -c 'Print :AudioComponents:0:subtype' "$plist" 2>/dev/null)"
  mfr="$(/usr/libexec/PlistBuddy -c 'Print :AudioComponents:0:manufacturer' "$plist" 2>/dev/null)"
  killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
  echo "  auval -v $type $subtype $mfr"
  auval -v "$type" "$subtype" "$mfr" > "$log" 2>&1
  code=$?
  grep -E 'Component Version|AU VALIDATION|FAILED|not find' "$log" | sed 's/^/    /'
  check "auval -v $type $subtype $mfr exits 0 and reports AU VALIDATION SUCCEEDED" \
    "$([[ $code -eq 0 ]] && grep -q 'AU VALIDATION SUCCEEDED' "$log" && ! grep -q 'FAILED' "$log" && echo 1 || echo 0)" "exit $code"
  check "auval finds the $NEW_VERSION component" "$(grep -qF "Component Version: $NEW_VERSION " "$log" && echo 1 || echo 0)" \
    "$(grep -F 'Component Version' "$log" | one_line)"
}

record_gatekeeper() {
  local dir="$WORK/gatekeeper" out="$DIAG/gatekeeper.txt" quarantine
  rm -rf "$dir"
  mkdir -p "$dir"
  cp "$NEW_DMG" "$dir/VoLum-installer.dmg"
  cp "$NEW_PKG" "$dir/VoLum Installer.pkg"
  ditto "$APP" "$dir/VoLum.app"
  quarantine="0083;$(printf '%x' "$(date +%s)");Safari;$(uuidgen)"
  xattr -w com.apple.quarantine "$quarantine" "$dir/VoLum-installer.dmg"
  xattr -w com.apple.quarantine "$quarantine" "$dir/VoLum Installer.pkg"
  xattr -w com.apple.quarantine "$quarantine" "$dir/VoLum.app"
  {
    echo "\$ spctl --status"
    spctl --status 2>&1
    echo "\$ sw_vers"
    sw_vers 2>&1
    echo "\$ xattr -p com.apple.quarantine VoLum.app"
    xattr -p com.apple.quarantine "$dir/VoLum.app" 2>&1
    echo
    echo "\$ spctl --assess --type open --context context:primary-signature -vvv VoLum-installer.dmg"
    spctl --assess --type open --context context:primary-signature -vvv "$dir/VoLum-installer.dmg" 2>&1
    echo "exit $?"
    echo
    echo "\$ spctl --assess --type install -vvv 'VoLum Installer.pkg'"
    spctl --assess --type install -vvv "$dir/VoLum Installer.pkg" 2>&1
    echo "exit $?"
    echo
    echo "\$ spctl --assess --type exec -vvv VoLum.app"
    spctl --assess --type exec -vvv "$dir/VoLum.app" 2>&1
    echo "exit $?"
  } > "$out"
  echo "  Gatekeeper on quarantined copies (recorded, not asserted):"
  sed 's/^/    /' "$out"
  if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    {
      echo "### macOS Gatekeeper on this run's $NEW_VERSION build (recorded, not asserted)"
      echo
      echo "Quarantined copies of the installer DMG, the installer package and the installed app."
      echo "The build is ad-hoc signed and not notarized, so a rejection is expected."
      echo
      echo '```'
      cat "$out"
      echo '```'
    } >> "$GITHUB_STEP_SUMMARY"
  fi
  check "Gatekeeper verdicts for the DMG, package and app captured" \
    "$([[ $(grep -c '^exit ' "$out") -eq 3 ]] && echo 1 || echo 0)"
  rm -rf "$dir"
}

negative_control() {
  LEG="negative control"
  echo
  echo "[negative control] the version checker must reject a wrong version"
  local expanded="$WORK/negative-control" app vst3 au right wrong rejected
  rm -rf "$expanded"
  if ! pkgutil --expand-full "$NEW_PKG" "$expanded" >/dev/null; then
    echo "NEGATIVE CONTROL FAILED: could not expand $NEW_PKG"
    exit 1
  fi
  app="$(dirname "$(dirname "$(find "$expanded" -path '*/VoLum.app/Contents/Info.plist' -print -quit)")")"
  vst3="$(dirname "$(dirname "$(find "$expanded" -path '*/VoLum.vst3/Contents/Info.plist' -print -quit)")")"
  au="$(dirname "$(dirname "$(find "$expanded" -path '*/VoLum.component/Contents/Info.plist' -print -quit)")")"
  right="$(version_problems "$NEW_VERSION" "package VoLum.app" "$app" "package VoLum.vst3" "$vst3" "package VoLum.component" "$au")"
  wrong="$(version_problems "9.9.9" "package VoLum.app" "$app" "package VoLum.vst3" "$vst3" "package VoLum.component" "$au")"
  echo "  expected $NEW_VERSION: ${right:-accepted}"
  echo "  expected 9.9.9: ${wrong:-accepted}"
  rejected="$(echo "$wrong" | grep -cE "reports '[^']+', expected '9\.9\.9'" || true)"
  if [[ -n "$right" || "$rejected" -ne 3 ]]; then
    echo "NEGATIVE CONTROL FAILED: the checker must accept $NEW_VERSION and reject 9.9.9 on the app, VST3 and AU."
    exit 1
  fi
  rm -rf "$expanded"
  echo "NEGATIVE CONTROL OK"
}

run_leg() {
  local tag="$1" old_dmg="$2" old="${1#v}" new="$NEW_VERSION"
  local leg_dir="$WORK/$tag" seed_home seed_lib seed_hash real_hash code
  LEG="$tag"
  mkdir -p "$leg_dir"
  echo
  echo "[$tag] $old -> $new"

  machine_clean "before the $old install" || { echo "  machine is not clean; leg skipped"; return 1; }

  mount_dmg "$old_dmg" "$leg_dir/old-dmg" || { echo "  could not mount $old_dmg"; return 1; }
  check "$tag installer DMG holds 'VoLum Installer.pkg'" "$([[ -f "$leg_dir/old-dmg/VoLum Installer.pkg" ]] && echo 1 || echo 0)"
  install_pkg "$leg_dir/old-dmg/VoLum Installer.pkg" "$tag-install"
  code=$?
  unmount_dmg "$leg_dir/old-dmg"
  check_eq "$old install exits 0" 0 "$code"
  [[ $code -eq 0 ]] || return 1
  assert_versions "$old" "after the $old install"
  assert_receipts "$old" "after the $old install"

  seed_home="$leg_dir/home"
  seed_lib="$seed_home/Library/Application Support/VoLum"
  mkdir -p "$seed_lib"
  echo "  seeding a $old library: the installed $old standalone on a temp HOME"
  launch_and_quit "$old" "$seed_home" "$tag-seed"
  check "$old wrote volum-settings.json on quit" "$(truth json_ok "$seed_lib/volum-settings.json")"
  check "$old wrote content/volum-content.json with a schema version on quit" \
    "$(truth content_has_schema "$seed_lib/content/volum-content.json")"

  mkdir -p "$(dirname "$REAL_LIBRARY")"
  cp -R "$seed_lib" "$REAL_LIBRARY"
  CREATED_REAL_LIBRARY=1
  seed_hash="$(tree_hash "$seed_lib")"
  real_hash="$(tree_hash "$REAL_LIBRARY")"
  echo "  library hashed: $(echo "$seed_hash" | grep -c .) files"
  echo "$seed_hash" | sed 's/^/    /'
  check "the runner's library mirror matches the seeded library" "$([[ -n "$seed_hash" && "$seed_hash" == "$real_hash" ]] && echo 1 || echo 0)"

  install_pkg "$NEW_PKG" "$tag-upgrade"
  code=$?
  check_eq "$new install over $old exits 0" 0 "$code"
  [[ $code -eq 0 ]] || return 1
  assert_versions "$new" "after the upgrade"
  assert_receipts "$new" "after the upgrade"
  assert_bundled_rigs "after the upgrade"
  assert_tree_unchanged "seeded library" "$seed_lib" "$seed_hash" "by the upgrade"
  assert_tree_unchanged "runner library" "$REAL_LIBRARY" "$real_hash" "by the upgrade"
  assert_auval

  if [[ $GATEKEEPER_DONE -eq 0 ]]; then
    record_gatekeeper
    GATEKEEPER_DONE=1
  fi

  echo "  the installed $new standalone on the seeded $old library"
  launch_and_quit "$new" "$seed_home" "$tag-upgraded"
  check "volum-settings.json still parses after the $new run" "$(truth json_ok "$seed_lib/volum-settings.json")"
  check "content/volum-content.json still has a schema version after the $new run" \
    "$(truth content_has_schema "$seed_lib/content/volum-content.json")"
  assert_tree_unchanged "runner library" "$REAL_LIBRARY" "$real_hash" "by the temp-HOME $new run"
  return 0
}

rm -rf "$WORK"
mkdir -p "$DIAG"

if [[ -z "$ARTIFACT_DIR" || ! -d "$ARTIFACT_DIR" ]]; then
  echo "ERROR: pass --artifact-dir: the downloaded VoLum-mac artifact of this run." >&2
  exit 1
fi
NEW_VERSION="$(sed -n 's/^#define PLUG_VERSION_STR "\(.*\)"/\1/p' "$REPO_ROOT/NeuralAmpModeler/config.h")"
if [[ -z "$NEW_VERSION" ]]; then
  echo "ERROR: PLUG_VERSION_STR not found in config.h" >&2
  exit 1
fi
NEW_DMG="$ARTIFACT_DIR/VoLum-v$NEW_VERSION-mac.dmg"
if [[ ! -f "$NEW_DMG" ]]; then
  echo "ERROR: this run's installer DMG is missing: $NEW_DMG" >&2
  ls -la "$ARTIFACT_DIR" >&2
  exit 1
fi

echo "VoLum macOS installer upgrade"
echo "  this run's DMG: $NEW_DMG ($NEW_VERSION)"
echo "  from:           $FROM_TAGS"
echo "  library:        $REAL_LIBRARY"
sw_vers | sed 's/^/  /'

mount_dmg "$NEW_DMG" "$WORK/new-dmg" || { echo "ERROR: could not mount $NEW_DMG" >&2; exit 1; }
NEW_PKG="$WORK/new-dmg/VoLum Installer.pkg"
if [[ ! -f "$NEW_PKG" ]]; then
  echo "ERROR: $NEW_DMG holds no 'VoLum Installer.pkg'" >&2
  exit 1
fi

negative_control

LEG="setup"
echo
echo "[setup] runner and inputs"
BUNDLED_RIGS="$(bundled_rig_list)"
check "the repo ships bundled rigs ($(echo "$BUNDLED_RIGS" | grep -c .) files, factory-presets.json among them)" \
  "$([[ $(echo "$BUNDLED_RIGS" | grep -c .) -gt 15 ]] && echo "$BUNDLED_RIGS" | grep -qx 'factory-presets.json' && echo 1 || echo 0)"
check "no VoLum crash report under DiagnosticReports before the run" \
  "$([[ -z "$(for d in $CRASH_DIRS; do [[ -d "$d" ]] && find "$d" -maxdepth 2 -type f -iname 'VoLum*'; done)" ]] && echo 1 || echo 0)"

old_dmg_path() {
  echo "$WORK/download-$1/VoLum-$1-macos-installer.dmg"
}

for tag in $FROM_TAGS; do
  old_dmg="$(old_dmg_path "$tag")"
  mkdir -p "$(dirname "$old_dmg")"
  gh release download "$tag" --repo guitarlum/VoLum --pattern "$(basename "$old_dmg")" --dir "$(dirname "$old_dmg")" --clobber
  rc=$?
  check "downloaded $(basename "$old_dmg")" "$([[ $rc -eq 0 && -f "$old_dmg" ]] && echo 1 || echo 0)" "gh exit $rc"
done

for tag in $FROM_TAGS; do
  old_dmg="$(old_dmg_path "$tag")"
  if [[ -f "$old_dmg" ]]; then
    run_leg "$tag" "$old_dmg" || check "leg completed" 0
  else
    LEG="$tag"
    check "leg completed" 0 "no installer DMG downloaded for $tag"
  fi
  remove_volum
  if [[ $CREATED_REAL_LIBRARY -eq 1 ]]; then
    rm -rf "$REAL_LIBRARY"
    CREATED_REAL_LIBRARY=0
  fi
done

echo
if [[ $FAILED -eq 0 ]]; then
  echo "ALL PASS $CHECKS checks"
  [[ -n "${GITHUB_STEP_SUMMARY:-}" ]] && echo "macOS installer upgrade: ALL PASS $CHECKS checks" >> "$GITHUB_STEP_SUMMARY"
  exit 0
fi
echo "FAILED $FAILED of $CHECKS checks"
printf '%s' "$FAILURES"
exit 1
