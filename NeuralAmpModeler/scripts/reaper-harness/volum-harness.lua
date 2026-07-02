-- VoLum REAPER render harness (headless, version-agnostic).
--
-- Runs from REAPER's Scripts/__startup.lua ONLY when the sentinel file exists,
-- so it never fires on a normal REAPER launch. It loads VoLum as a track FX on
-- a track holding a test tone, reads the track's POST-FX output directly via an
-- audio accessor (no render dialog, no temp WAVs), and writes peak/RMS/NaN stats
-- per scenario plus a save/reload round-trip comparison to a results file the
-- companion PowerShell runner asserts on.
--
-- Params are resolved BY NAME (not EParam index) so the harness keeps working
-- across parameter-order changes between VoLum versions.
--
-- Driven by env VOLUM_HARNESS_DIR (input.wav in, results.json + harness.log out).

local function getenv(name)
  local ok, v = pcall(function() return reaper.GetExtState("VOLUM_HARNESS", name) end)
  if ok and v ~= nil and v ~= "" then return v end
  return os.getenv(name)
end

local dir = getenv("VOLUM_HARNESS_DIR")
-- Sentinel gate: do nothing unless the runner asked for a run.
if not dir or dir == "" then return end
local sentinel = dir .. "\\go.txt"
local f = io.open(sentinel, "r")
if not f then return end
f:close()

local logPath = dir .. "\\harness.log"
local resPath = dir .. "\\results.json"
local log = io.open(logPath, "w")
local function L(s) if log then log:write(tostring(s) .. "\n"); log:flush() end end

local results = {}
local function jstr(s) return '"' .. tostring(s):gsub('\\', '\\\\'):gsub('"', '\\"') .. '"' end

local function fail(msg)
  L("FATAL: " .. tostring(msg))
  local r = io.open(resPath, "w")
  if r then r:write('{"ok":false,"error":' .. jstr(msg) .. '}'); r:close() end
  os.remove(sentinel)
  if log then log:close() end
end

local ok, err = pcall(function()
  local SR = 48000
  local inputWav = dir .. "\\input.wav"
  L("harness start; dir=" .. dir)

  -- Fresh project state.
  reaper.Main_OnCommand(40860, 0) -- Close all projects? (no-op-safe); then new:
  reaper.Main_OnCommand(40023, 0) -- File: New project (in new tab is 40859); 40023 = New project
  reaper.InsertTrackAtIndex(0, true)
  local track = reaper.GetTrack(0, 0)
  if not track then error("no track created") end
  reaper.SetOnlyTrackSelected(track)
  reaper.SetEditCurPos(0.0, false, false)
  local n = reaper.InsertMedia(inputWav, 0)
  L("InsertMedia returned " .. tostring(n))
  if (n or 0) < 1 then error("InsertMedia failed for " .. inputWav) end

  local fx = reaper.TrackFX_AddByName(track, "VoLum", false, -1)
  L("TrackFX_AddByName VoLum -> " .. tostring(fx))
  if fx < 0 then error("VoLum VST3 not found by REAPER (scan it first)") end

  local fxname = select(2, reaper.TrackFX_GetFXName(track, fx, ""))
  L("FX name: " .. tostring(fxname))

  -- Map param name -> index (case/space tolerant contains match).
  local nparams = reaper.TrackFX_GetNumParams(track, fx)
  L("num params: " .. tostring(nparams))
  local byName = {}
  for p = 0, nparams - 1 do
    local _, pn = reaper.TrackFX_GetParamName(track, fx, p, "")
    byName[#byName + 1] = { idx = p, name = pn }
  end
  local function setByName(needle, norm)
    needle = needle:lower()
    for _, e in ipairs(byName) do
      if e.name and e.name:lower():find(needle, 1, true) then
        reaper.TrackFX_SetParamNormalized(track, fx, e.idx, norm)
        L(("set [%s]=%.3f (matched '%s')"):format(e.name, norm, needle))
        return true
      end
    end
    L("PARAM NOT FOUND for '" .. needle .. "' (non-fatal; version drift)")
    return false
  end

  -- Read track post-FX output stats over the tone region.
  local function stats(label)
    local aa = reaper.CreateTrackAudioAccessor(track)
    reaper.AudioAccessorUpdate(aa)
    local t0 = reaper.GetAudioAccessorStartTime(aa)
    local t1 = reaper.GetAudioAccessorEndTime(aa)
    local dur = math.min(2.0, math.max(0.1, t1 - t0))
    local ns = math.floor(dur * SR)
    local nch = 2
    local buf = reaper.new_array(ns * nch)
    buf.clear()
    local got = reaper.GetAudioAccessorSamples(aa, SR, nch, t0, ns, buf)
    local peak, sumsq, bad = 0.0, 0.0, 0
    local tbl = buf.table()
    for i = 1, ns * nch do
      local v = tbl[i] or 0.0
      if v ~= v or v == math.huge or v == -math.huge then bad = bad + 1; v = 0.0 end
      local a = v < 0 and -v or v
      if a > peak then peak = a end
      sumsq = sumsq + v * v
    end
    local rms = math.sqrt(sumsq / (ns * nch))
    reaper.DestroyAudioAccessor(aa)
    L(("stats[%s] got=%d peak=%.6f rms=%.6f bad=%d"):format(label, got or -1, peak, rms, bad))
    return { peak = peak, rms = rms, bad = bad, samples = ns * nch }
  end

  -- Scenario 1: default loaded rig (whatever VoLum ships as default), full chain.
  results["default"] = stats("default")

  -- Scenario 2: enable POST Tremolo (by name) if present.
  setByName("trem", 1.0) -- TREMOLO active-ish (first tremolo-named toggle)
  results["tremolo_on"] = stats("tremolo_on")

  -- Scenario 3: enable PRE Pitch (by name) if present.
  setByName("pitch", 1.0)
  results["pitch_on"] = stats("pitch_on")

  -- Emit results JSON (best-effort: write BEFORE the round-trip so a blocking
  -- reopen can never starve the runner of a result).
  local function emit(keys)
    local r = io.open(resPath, "w")
    r:write("{\n  \"ok\": true,\n  \"fxname\": " .. jstr(fxname) .. ",\n  \"scenarios\": {\n")
    local written = {}
    for _, k in ipairs(keys) do if results[k] then written[#written + 1] = k end end
    for ki, k in ipairs(written) do
      local s = results[k]
      r:write(("    %s: {\"peak\": %.8f, \"rms\": %.8f, \"bad\": %d, \"samples\": %d}%s\n")
        :format(jstr(k), s.peak, s.rms, s.bad, s.samples, ki < #written and "," or ""))
    end
    r:write("  }\n}\n")
    r:close()
    L("results written (" .. #written .. " scenarios): " .. resPath)
  end
  emit({ "default", "tremolo_on", "pitch_on" })

  -- Round-trip: save, then reopen with the noprompt: prefix so REAPER never pops
  -- a "save changes?" modal that would hang a headless run.
  local rpp = dir .. "\\roundtrip.rpp"
  reaper.Main_SaveProjectEx(0, rpp, 0)
  L("saved project: " .. rpp)
  reaper.Main_openProject("noprompt:" .. rpp)
  local tr2 = reaper.GetTrack(0, 0)
  if tr2 then track = tr2 end
  results["reloaded"] = stats("reloaded")
  emit({ "default", "tremolo_on", "pitch_on", "reloaded" })
end)

if not ok then
  fail(err)
else
  os.remove(sentinel)
  L("harness done OK")
  if log then log:close() end
end
