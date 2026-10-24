#!/usr/bin/env bash
# Red proof for test_iplug_linux_runloop.cpp: revert one fix at a time in
# IPlugLinuxRunLoop.cpp, rebuild the doctest binary, show the failing CHECK,
# restore. Run from the worktree root after `run-gates.sh tests` built the
# Release test tree.

set -euo pipefail

SRC=iPlug2/IPlug/Linux/IPlugLinuxRunLoop.cpp
BUILD=NeuralAmpModeler/build-tests-linux
BIN=$BUILD/NeuralAmpModeler-Tests
JOBS="${JOBS:-10}"
trap 'git -C iPlug2 checkout -- IPlug/Linux/IPlugLinuxRunLoop.cpp' EXIT

revert() {
  local name="$1" test="$2" perl_expr="$3"
  git -C iPlug2 checkout -- IPlug/Linux/IPlugLinuxRunLoop.cpp
  perl -0pi -e "$perl_expr" "$SRC"
  if ! grep -q "REVERTED" "$SRC"; then
    echo "### $name: the revert did not apply" >&2
    return 1
  fi
  cmake --build "$BUILD" -j "$JOBS" > /dev/null
  echo "### $name (reverted: $(grep -o 'REVERTED[^*]*' "$SRC" | head -1))"
  set +e
  "$BIN" --test-case="$test" --no-version > /tmp/07a-redproof.log 2>&1
  local rc=$?
  set -e
  grep -E "ERROR|values:|CHECK|REQUIRE|test cases:" /tmp/07a-redproof.log | head -8
  echo "exit $rc"
  [[ "$rc" != 0 ]]
}

revert "single process-wide loop (ticket 05)" "*instance B's timers keep firing*" \
  's/    r\.loops\.erase\(r\.loops\.begin\(\) \+ i\);/    r.loops.clear(); for (auto& b : r.bindings) Detach(b); return; \/* REVERTED: ticket 05 single loop *\//'

revert "no reference count" "*reference counted*" \
  's/    if \(--r\.loops\[i\]\.refs > 0\)\n      return;/    --r.loops[i].refs; \/* REVERTED: no refcount *\//'

revert "pending timers never attach" "*a timer made before any loop*" \
  's/      Attach\(r\.bindings\[i\], runLoop\);/      (void)i; \/* REVERTED: no late attach *\//'

revert "no UI-thread check" "*UI-thread only*" \
  's/  return !r\.haveUIThread \|\| r\.uiThread == std::this_thread::get_id\(\);/  return true; \/* REVERTED: no thread check *\//'

revert "poll loop keeps unregistered timers" "IPlugLinuxPollRunLoop*" \
  's/      close\(mSources\[i\]\.fd\);\n      mSources\.erase\(mSources\.begin\(\) \+ i\);/      return; \/* REVERTED: unregister is a no-op *\//'

git -C iPlug2 checkout -- IPlug/Linux/IPlugLinuxRunLoop.cpp
cmake --build "$BUILD" -j "$JOBS" > /dev/null
echo "### restored"
"$BIN" --test-case="*IPlugLinux*" --no-version 2>&1 | grep -E "test cases:|assertions:|Status"
