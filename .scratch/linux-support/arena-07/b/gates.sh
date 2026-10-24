#!/usr/bin/env bash
# Throwaway ticket 07 gates. The box script's cwd is the worktree.
set -u
ROOT="$(pwd)"
VST3="$ROOT/build-linux/VoLum.vst3"
SO="$VST3/Contents/x86_64-linux/VoLum.so"
VAL="$ROOT/build-linux/validator"
PLUGINVAL="${HOME}/volum-linux/tools/pluginval/pluginval"
REAPER="${HOME}/volum-linux/tools/bin/reaper"
if [[ ! -x "$REAPER" ]]; then
  REAPER="${HOME}/volum-linux/tools/reaper/reaper_linux_x86_64/REAPER/reaper"
fi
OUT="${HOME}/volum-linux/out/07b"
mkdir -p "$OUT"
fail=0

echo "=== rebuild ==="
cmake --build "$ROOT/build-linux" -j 10 --target VoLum-vst3
if [[ $? -ne 0 ]]; then
  echo "rebuild failed"
  exit 1
fi

echo "=== validator ==="
"$VAL" "$VST3" | tee "$OUT/validator.txt"
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
  echo "validator failed"
  fail=1
fi

echo "=== pluginval ==="
xvfb-run -a -s "-screen 0 1920x1080x24" "$PLUGINVAL" --strictness-level 10 --validate-in-process "$VST3" >"$OUT/pluginval.txt" 2>&1
pv=$?
tail -30 "$OUT/pluginval.txt"
if grep -q "IPlugLinux X error" "$OUT/pluginval.txt"; then
  echo "pluginval logged an X error"
  fail=1
fi
if [[ $pv -ne 0 ]]; then
  echo "pluginval exit $pv"
  fail=1
fi

echo "=== screenshots ==="
if ! command -v import >/dev/null 2>&1; then
  sudo apt-get install -y imagemagick
fi
SDK="$ROOT/iPlug2/Dependencies/IPlug/VST3_SDK"
g++ -std=c++20 -O2 -I"$SDK" \
  /tmp/volum-embed.cpp \
  "$SDK/pluginterfaces/base/coreiids.cpp" \
  "$SDK/pluginterfaces/base/funknown.cpp" \
  "$SDK/pluginterfaces/base/ustring.cpp" \
  "$SDK/public.sdk/source/common/commoniids.cpp" \
  "$SDK/public.sdk/source/vst/vstinitiids.cpp" \
  -o "$OUT/embed" -lX11 -ldl -pthread
if [[ $? -ne 0 ]]; then
  echo "embed compile failed"
  fail=1
else
  shot() {
    local scale="$1" png="$2"
    rm -f /tmp/volum-embed-ready /tmp/volum-embed-done
    rm -f /tmp/volum-embed-display
    xvfb-run -a -s "-screen 0 1920x1080x24" bash -c 'echo "$DISPLAY" > /tmp/volum-embed-display; exec "$1" "$2" "$3"' _ "$OUT/embed" "$SO" "$scale" >"$OUT/embed-$scale.log" 2>&1 &
    local pid=$!
    local i
    for i in $(seq 1 100); do
      [[ -f /tmp/volum-embed-ready ]] && break
      sleep 0.1
    done
    if [[ ! -f /tmp/volum-embed-ready ]]; then
      echo "embed scale $scale did not become ready"
      cat "$OUT/embed-$scale.log"
      kill "$pid" 2>/dev/null || true
      fail=1
      return
    fi
    local disp
    disp=$(cat /tmp/volum-embed-display)
    DISPLAY="$disp" import -window root "$png" || true
    : > /tmp/volum-embed-done
    wait "$pid" || true
    if grep -q "IPlugLinux X error" "$OUT/embed-$scale.log"; then
      echo "embed scale $scale logged an X error"
      fail=1
    fi
    echo "wrote $png"
  }
  shot 1 "$OUT/scale1.png"
  shot 2 "$OUT/scale2.png"
fi

echo "=== reaper ==="
CFG=$(mktemp -d)
export HOME="$CFG/home"
mkdir -p "$HOME/.config/REAPER/Scripts" "$HOME/.local/share"
cat > "$HOME/.config/REAPER/reaper.ini" <<EOF
[reaper]
loadlastproj=0
splashfeedback=0
vstpath=$ROOT/build-linux
EOF
cat > "$HOME/.config/REAPER/Scripts/__startup.lua" <<'LUA'
local resultPath = os.getenv("VOLUM_SMOKE_RESULT") or "/tmp/volum-reaper-result.txt"
local function report(line)
  local f = io.open(resultPath, "a")
  if f then f:write(line, "\n"); f:close() end
end
local function fail(reason)
  report("FAIL " .. reason)
  reaper.Main_OnCommand(40004, 0)
end
local steps = {}
local stepIndex = 1
local waitUntil = 0
local tracks = {}
local fxs = {}
local function wait(seconds) waitUntil = reaper.time_precise() + seconds end
steps[#steps + 1] = function()
  reaper.InsertTrackAtIndex(0, true)
  reaper.InsertTrackAtIndex(1, true)
  tracks[1] = reaper.GetTrack(0, 0)
  tracks[2] = reaper.GetTrack(0, 1)
  local names = {}
  local i = 0
  while true do
    local ok, name = reaper.EnumInstalledFX(i)
    if not ok then break end
    if name:find("VoLum", 1, true) then names[#names + 1] = name end
    i = i + 1
  end
  local fxName = names[1]
  if not fxName then return fail("VoLum is not installed; scanned " .. tostring(i)) end
  report("STEP fx " .. fxName)
  for n = 1, 2 do
    fxs[n] = reaper.TrackFX_AddByName(tracks[n], fxName, false, -1)
    if fxs[n] < 0 then return fail("insert failed for " .. fxName) end
  end
  report("STEP inserted 2")
  wait(1)
end
for n = 1, 2 do
  for cycle = 1, 20 do
    local nn, cc = n, cycle
    steps[#steps + 1] = function()
      reaper.TrackFX_Show(tracks[nn], fxs[nn], 3)
      report("STEP open " .. nn .. " " .. cc)
      wait(0.2)
    end
    steps[#steps + 1] = function()
      reaper.TrackFX_Show(tracks[nn], fxs[nn], 2)
      report("STEP close " .. nn .. " " .. cc)
      wait(0.05)
    end
  end
end
steps[#steps + 1] = function()
  report("PASS")
  reaper.Main_OnCommand(40004, 0)
end
local function tick()
  if reaper.time_precise() >= waitUntil then
    local step = steps[stepIndex]
    if not step then return end
    stepIndex = stepIndex + 1
    step()
  end
  reaper.defer(tick)
end
reaper.defer(tick)
LUA
export VOLUM_SMOKE_RESULT="$OUT/reaper-result.txt"
rm -f "$VOLUM_SMOKE_RESULT"
if [[ ! -x "$REAPER" ]]; then
  echo "reaper missing at $REAPER"
  fail=1
else
  xvfb-run -a -s "-screen 0 1920x1080x24" "$REAPER" -nosplash >"$OUT/reaper.txt" 2>&1 &
  rpid=$!
  for i in $(seq 1 180); do
    if ! kill -0 "$rpid" 2>/dev/null; then break; fi
    if [[ -f "$VOLUM_SMOKE_RESULT" ]] && grep -q PASS "$VOLUM_SMOKE_RESULT"; then
      sleep 2
      kill "$rpid" 2>/dev/null || true
      break
    fi
    sleep 1
  done
  kill "$rpid" 2>/dev/null || true
  wait "$rpid" 2>/dev/null || true
  echo "reaper result:"
  cat "$VOLUM_SMOKE_RESULT" 2>/dev/null || echo "(no result file)"
  if ! grep -q '^PASS$' "$VOLUM_SMOKE_RESULT" 2>/dev/null; then
    echo "reaper smoke failed"
    tail -40 "$OUT/reaper.txt"
    fail=1
  fi
  if grep -q "IPlugLinux X error" "$OUT/reaper.txt"; then
    echo "reaper logged an X error"
    fail=1
  fi
fi

echo "=== gate exit $fail ==="
exit $fail
