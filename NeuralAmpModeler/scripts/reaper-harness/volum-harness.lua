-- VoLum REAPER render harness (headless, version-agnostic).
--
-- Runs from REAPER's Scripts/__startup.lua ONLY when the sentinel file exists,
-- so it never fires on a normal REAPER launch. It loads VoLum as a track FX on
-- a track holding a test tone, renders the track's audio THROUGH the plugin with
-- "apply track FX to items as a new take", reads that take back, and writes
-- peak/RMS/NaN stats per scenario plus a save/reload round-trip comparison to a
-- results file the companion PowerShell runner asserts on.
--
-- Why apply-FX rather than a track audio accessor: a track audio accessor returns
-- the track's SOURCE audio, not its post-FX output. The earlier version of this
-- harness read one and so measured the input tone - every scenario reported the
-- identical peak/RMS, byte for byte, whether VoLum was loaded, bypassed, or
-- driven with tremolo at full depth. Applying the FX chain to a new take is a
-- real offline render, so the numbers below are VoLum's output. The `bypassed`
-- scenario exists purely as a tripwire: if it ever matches `default` again, the
-- harness is measuring the wrong thing and the runner fails loudly instead of
-- printing green.
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
  local APPLY_FX_STEREO = 40361 -- Item: Apply track/take FX to items (stereo output)
  local DELETE_ACTIVE_TAKE = 40129 -- Take: Delete active take from items
  local inputWav = dir .. "\\input.wav"
  L("harness start; dir=" .. dir)

  -- Fresh project state.
  reaper.Main_OnCommand(40860, 0) -- Close all projects? (no-op-safe); then new:
  reaper.Main_OnCommand(40023, 0) -- File: New project (in new tab is 40859); 40023 = New project
  -- Apply-FX writes its rendered takes into the project's media path. An unsaved
  -- project has none, so REAPER would drop them in the user's Documents\REAPER
  -- Media folder and leave a pile of "input render NNN.wav" behind after every
  -- run. Point it at the harness work directory, which is wiped per run.
  reaper.GetSetProjectInfo_String(0, "RECORD_PATH", dir, true)
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

  -- Map param name -> index. Exact names are used where the plugin's own name is
  -- known; the contains-match fallback keeps older/newer builds working.
  local nparams = reaper.TrackFX_GetNumParams(track, fx)
  L("num params: " .. tostring(nparams))
  local byName = {}
  local exact = {}
  for p = 0, nparams - 1 do
    local _, pn = reaper.TrackFX_GetParamName(track, fx, p, "")
    byName[#byName + 1] = { idx = p, name = pn }
    if pn and pn ~= "" then exact[pn] = p end
  end
  local function setParam(name, norm)
    if exact[name] then
      reaper.TrackFX_SetParamNormalized(track, fx, exact[name], norm)
      L(("set [%s]=%.3f"):format(name, norm))
      return true
    end
    local needle = name:lower()
    for _, e in ipairs(byName) do
      if e.name and e.name:lower():find(needle, 1, true) then
        reaper.TrackFX_SetParamNormalized(track, fx, e.idx, norm)
        L(("set [%s]=%.3f (contains match for '%s')"):format(e.name, norm, name))
        return true
      end
    end
    L("PARAM NOT FOUND for '" .. name .. "' (non-fatal; version drift)")
    return false
  end

  -- Render the item through the track's FX chain into a new take and measure that
  -- take. The take is deleted again so the next scenario starts from the source
  -- tone rather than from the previous scenario's output.
  local function stats(label)
    local item = reaper.GetTrackMediaItem(track, 0)
    if not item then error("no media item on the track") end
    reaper.SelectAllMediaItems(0, false)
    reaper.SetMediaItemSelected(item, true)
    reaper.UpdateArrange()
    reaper.Main_OnCommand(APPLY_FX_STEREO, 0)

    local take = reaper.GetActiveTake(item)
    if not take then error("apply-FX produced no take for " .. label) end
    local src = reaper.GetMediaItemTake_Source(take)
    local srcFile = reaper.GetMediaSourceFileName(src, "")
    local aa = reaper.CreateTakeAudioAccessor(take)
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

    -- Drop the rendered take again; keeping it would stack renders scenario on
    -- scenario and every later number would describe VoLum applied twice.
    reaper.Main_OnCommand(DELETE_ACTIVE_TAKE, 0)

    L(("stats[%s] got=%d peak=%.6f rms=%.6f bad=%d src=%s")
      :format(label, got or -1, peak, rms, bad, tostring(srcFile)))
    return { peak = peak, rms = rms, bad = bad, samples = ns * nch }
  end

  -- The amp capture loads on a worker thread and is staged into the DSP by the
  -- audio callback, so a render started immediately after instantiation can catch
  -- a half-staged rig. Wait, then throw one render away.
  local waitUntil = reaper.time_precise() + 3.0
  while reaper.time_precise() < waitUntil do end
  local warmup = stats("warmup (discarded)")
  L(("warmup peak=%.6f rms=%.6f"):format(warmup.peak, warmup.rms))

  -- Scenario 1: default loaded rig (whatever the library says was last in use).
  results["default"] = stats("default")

  -- Scenario 2: the tripwire. With the plugin bypassed the render must come back
  -- as the untouched input tone, and it must NOT match the default scenario. If
  -- these two ever agree, the harness is measuring something other than VoLum's
  -- output and every other number here is worthless.
  reaper.TrackFX_SetEnabled(track, fx, false)
  results["bypassed"] = stats("bypassed")
  reaper.TrackFX_SetEnabled(track, fx, true)

  -- Scenario 3: POST Tremolo at its stock depth/rate - audible amplitude
  -- modulation, so the render has to differ from the default one.
  setParam("TremoloActive", 1.0)
  results["tremolo_on"] = stats("tremolo_on")
  setParam("TremoloActive", 0.0)

  -- Scenario 4: PRE Pitch, transposed a full octave down (normalized 0.0 on a
  -- -12..+7 semitone range) so it cannot be a no-op either.
  setParam("PrePitchActive", 1.0)
  setParam("PrePitchSemitones", 0.0)
  results["pitch_on"] = stats("pitch_on")
  setParam("PrePitchActive", 0.0)
  setParam("PrePitchSemitones", 12.0 / 19.0) -- back to 0 st

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
  local order = { "default", "bypassed", "tremolo_on", "pitch_on" }
  emit(order)

  -- Round-trip: save, then reopen with the noprompt: prefix so REAPER never pops
  -- a "save changes?" modal that would hang a headless run.
  local rpp = dir .. "\\roundtrip.rpp"
  reaper.Main_SaveProjectEx(0, rpp, 0)
  L("saved project: " .. rpp)
  reaper.Main_openProject("noprompt:" .. rpp)
  local tr2 = reaper.GetTrack(0, 0)
  if tr2 then track = tr2 end
  local waitReload = reaper.time_precise() + 3.0
  while reaper.time_precise() < waitReload do end
  results["reloaded"] = stats("reloaded")
  order[#order + 1] = "reloaded"
  emit(order)
end)

if not ok then
  fail(err)
else
  os.remove(sentinel)
  L("harness done OK")
  if log then log:close() end
end
