#!/usr/bin/env bash
# Red proof for test_iplug_linux_runloop.cpp: revert one fix at a time in
# IPlugLinuxRunLoop.cpp or IGraphicsLinuxGLNesting.h, rebuild the doctest
# binary, show the failing CHECK, restore. Run from the worktree root after
# `run-gates.sh tests` built the Release test tree.

set -euo pipefail

LOOP=IPlug/Linux/IPlugLinuxRunLoop.cpp
NESTING=IGraphics/Platforms/IGraphicsLinuxGLNesting.h
BUILD=NeuralAmpModeler/build-tests-linux
BIN=$BUILD/NeuralAmpModeler-Tests
JOBS="${JOBS:-10}"
restore() { git -C iPlug2 checkout -- "$LOOP" "$NESTING"; }
trap restore EXIT

revert() {
  local name="$1" file="iPlug2/$2" test="$3" perl_expr="$4"
  restore
  perl -0pi -e "$perl_expr" "$file"
  if ! grep -q "REVERTED" "$file"; then
    echo "### $name: the revert did not apply" >&2
    return 1
  fi
  cmake --build "$BUILD" -j "$JOBS" > /dev/null
  echo "### $name (reverted: $(grep -o 'REVERTED[^*]*' "$file" | head -1))"
  set +e
  "$BIN" --test-case="$test" --no-version > /tmp/07-redproof.log 2>&1
  local rc=$?
  set -e
  grep -E "ERROR|values:|CHECK|REQUIRE|test cases:" /tmp/07-redproof.log | head -8
  echo "exit $rc"
  [[ "$rc" != 0 ]]
}

revert "single process-wide loop (ticket 05)" "$LOOP" "*instance B's timers keep firing*" \
  's/    r\.loops\.erase\(r\.loops\.begin\(\) \+ i\);/    r.loops.clear(); for (auto& b : r.bindings) Detach(b); return; \/* REVERTED: ticket 05 single loop *\//'

revert "no reference count" "$LOOP" "*reference counted*" \
  's/    if \(--r\.loops\[i\]\.refs > 0\)\n      return;/    --r.loops[i].refs; \/* REVERTED: no refcount *\//'

revert "pending timers never attach" "$LOOP" "*a timer made before any loop*" \
  's/      Attach\(r\.bindings\[i\], runLoop\);/      (void)i; \/* REVERTED: no late attach *\//'

revert "no UI-thread check" "$LOOP" "*UI-thread only*" \
  's/  return !r\.haveUIThread \|\| r\.uiThread == std::this_thread::get_id\(\);/  return true; \/* REVERTED: no thread check *\//'

revert "poll loop keeps unregistered timers" "$LOOP" "IPlugLinuxPollRunLoop*" \
  's/      close\(mSources\[i\]\.fd\);\n      mSources\.erase\(mSources\.begin\(\) \+ i\);/      return; \/* REVERTED: unregister is a no-op *\//'

# 07a's DeactivateGLContext returned before decrementing when there was no
# context to give back; the same early return, in the nesting class.
revert "GL nesting skips the decrement with nothing to restore (07a)" "$NESTING" "*no context balances*" \
  's/    if \(mDepth == 0\)\n      return false;/    if (mDepth == 0 || !mRestore) \/* REVERTED: 07a early return *\/\n      return false;/'

restore
cmake --build "$BUILD" -j "$JOBS" > /dev/null
echo "### restored"
"$BIN" --test-case="*IPlugLinux*,*IGraphicsLinuxGLNesting*" --no-version 2>&1 | grep -E "test cases:|assertions:|Status"
