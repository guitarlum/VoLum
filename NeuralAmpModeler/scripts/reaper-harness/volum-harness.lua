-- VoLum REAPER render harness (headless, version-agnostic).
--
-- Runs from REAPER's Scripts/__startup.lua ONLY when the sentinel file exists,
-- so it never fires on a normal REAPER launch. It loads VoLum as a track FX on
-- a track holding a test tone, renders the track's audio THROUGH the plugin with
-- "apply track FX to items as a new take", reads that take back, and writes
-- peak/RMS/NaN stats per scenario plus PASS/FAIL/SKIP/WARN checks and FACT lines
-- to a results file the companion PowerShell runner asserts on.
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
-- Why the whole run is a coroutine driven by reaper.defer: VoLum recalls a MIDI
-- Sound in OnIdle, which iPlug drives from a Win32 timer on REAPER's main thread.
-- A busy-wait in this script starves that timer, so a Program Change would sit in
-- VoLum's queue forever. Every wait here yields back to REAPER instead.
--
-- Params are resolved BY NAME (not EParam index) so the harness keeps working
-- across parameter-order changes between VoLum versions.
--
-- Driven by env VOLUM_HARNESS_DIR (input.wav in, results.json + harness.log out),
-- VOLUM_HARNESS_SCENARIOS (comma list, default "core") and VOLUM_HARNESS_SANDBOX
-- ("1" when LOCALAPPDATA is a fresh library, so the five pre-filled Sounds on
-- programs 0-4 are known).

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
os.remove(sentinel) -- one run per arm, even if a later REAPER launch re-runs startup

local logPath = dir .. "\\harness.log"
local resPath = dir .. "\\results.json"
local log = io.open(logPath, "w")
local function L(s) if log then log:write(tostring(s) .. "\n"); log:flush() end end

local function jstr(s)
  local t = tostring(s):gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\r', '\\r'):gsub('\n', '\\n'):gsub('\t', '\\t')
  return '"' .. t .. '"'
end

local selected = {}
do
  local list = getenv("VOLUM_HARNESS_SCENARIOS")
  if not list or list == "" then list = "core" end
  for name in list:gmatch("[^,%s]+") do selected[name] = true end
end
local function wants(name) return selected["all"] or selected[name] end
local sandbox = getenv("VOLUM_HARNESS_SANDBOX") == "1"

local SR = 48000
local APPLY_FX_STEREO = 40361 -- Item: Apply track/take FX to items (stereo output)
local DELETE_ACTIVE_TAKE = 40129 -- Take: Delete active take from items
local TRANSPORT_RECORD = 1013
local TRANSPORT_STOP_SAVE = 40667 -- Transport: Stop (save all recorded media), never prompts
local inputWav = dir .. "\\input.wav"
local emptyRpp = dir .. "\\empty.rpp"
local volumLog = (os.getenv("LOCALAPPDATA") or "") .. "\\VoLum\\volum.log"

-- The five Sounds a fresh library pre-fills (VoLumPlayModel.h kPlayPrefillSounds).
local PREFILL = {
  [0] = { amp = "factory:12", preset = "factory:12:v1", name = "The bestest Clean" },
  [1] = { amp = "factory:13", preset = "factory:13:v2", name = "SLO Crunch" },
  [2] = { amp = "factory:6", preset = "factory:6:v1", name = "Modern Rhythm" },
  [3] = { amp = "factory:8", preset = "factory:8:v1", name = "Crack the Skye" },
  [4] = { amp = "factory:0", preset = "factory:0:v2", name = "Ampete Lead" },
}

-- ---------------------------------------------------------------- results ---

local results, order = {}, {}
local checks, facts, factOrder = {}, {}, {}
local fxname = "?"

local function record(key, s)
  if not results[key] then order[#order + 1] = key end
  results[key] = s
end

local function check(scn, name, status, detail)
  checks[#checks + 1] = { scenario = scn, name = name, status = status, detail = detail or "" }
  L(("%-4s  %s/%s -- %s"):format(status, scn, name, detail or ""))
end

local function fact(key, value)
  if facts[key] == nil then factOrder[#factOrder + 1] = key end
  facts[key] = tostring(value)
  L("FACT " .. key .. "=" .. tostring(value))
end

local function emit(ok, err, done)
  local r = io.open(resPath, "w")
  if not r then return end
  r:write("{\n  \"ok\": " .. tostring(ok) .. ",\n")
  if err then r:write("  \"error\": " .. jstr(err) .. ",\n") end
  r:write("  \"done\": " .. tostring(done) .. ",\n")
  r:write("  \"sandbox\": " .. tostring(sandbox) .. ",\n")
  r:write("  \"fxname\": " .. jstr(fxname) .. ",\n  \"scenarios\": {\n")
  for ki, k in ipairs(order) do
    local s = results[k]
    r:write(("    %s: {\"peak\": %.8f, \"rms\": %.8f, \"bad\": %d, \"samples\": %d, \"energy\": %.8f, \"bright\": %.8f}%s\n")
      :format(jstr(k), s.peak, s.rms, s.bad, s.samples, s.energy or 0, s.bright or 0, ki < #order and "," or ""))
  end
  r:write("  },\n  \"checks\": [\n")
  for i, c in ipairs(checks) do
    r:write(("    {\"scenario\": %s, \"name\": %s, \"status\": %s, \"detail\": %s}%s\n")
      :format(jstr(c.scenario), jstr(c.name), jstr(c.status), jstr(c.detail), i < #checks and "," or ""))
  end
  r:write("  ],\n  \"facts\": {\n")
  for i, k in ipairs(factOrder) do
    r:write(("    %s: %s%s\n"):format(jstr(k), jstr(facts[k]), i < #factOrder and "," or ""))
  end
  r:write("  }\n}\n")
  r:close()
end

-- ------------------------------------------------------------------ waits ---

local function now() return reaper.time_precise() end
local function sleep(sec)
  local t = now() + sec
  while now() < t do coroutine.yield() end
end

-- ------------------------------------------------------------- rendering ---

-- Peak/RMS/NaN, total energy (sum of squares / SR, both channels; independent of
-- a time shift, which is what lets a realtime capture be compared with an offline
-- render) and a brightness figure (first-difference energy over energy, left
-- channel) that separates two amps whose levels happen to match.
local function measureTake(take, maxDur)
  local aa = reaper.CreateTakeAudioAccessor(take)
  local t0 = reaper.GetAudioAccessorStartTime(aa)
  local t1 = reaper.GetAudioAccessorEndTime(aa)
  local dur = math.min(maxDur, math.max(0.1, t1 - t0))
  local ns = math.floor(dur * SR)
  local nch = 2
  local buf = reaper.new_array(ns * nch)
  buf.clear()
  local got = reaper.GetAudioAccessorSamples(aa, SR, nch, t0, ns, buf)
  local peak, sumsq, bad = 0.0, 0.0, 0
  local dsq, lsq, prevL = 0.0, 0.0, 0.0
  local tbl = buf.table()
  for i = 1, ns * nch do
    local v = tbl[i] or 0.0
    if v ~= v or v == math.huge or v == -math.huge then bad = bad + 1; v = 0.0 end
    local a = v < 0 and -v or v
    if a > peak then peak = a end
    sumsq = sumsq + v * v
    if i % 2 == 1 then
      local d = v - prevL
      dsq = dsq + d * d
      lsq = lsq + v * v
      prevL = v
    end
  end
  reaper.DestroyAudioAccessor(aa)
  return {
    peak = peak, rms = math.sqrt(sumsq / (ns * nch)), bad = bad, samples = ns * nch,
    energy = sumsq / SR, bright = lsq > 0 and dsq / lsq or 0, got = got or -1, dur = dur,
  }
end

-- Render one item through its track's FX chain into a new take and measure that
-- take. The take is deleted again so the next render starts from the source tone
-- rather than from the previous render's output.
local function renderItem(track, item, label)
  if not item then error("no media item to render for " .. label) end
  reaper.SelectAllMediaItems(0, false)
  reaper.SetMediaItemSelected(item, true)
  reaper.UpdateArrange()
  reaper.Main_OnCommand(APPLY_FX_STEREO, 0)

  local take = reaper.GetActiveTake(item)
  if not take then error("apply-FX produced no take for " .. label) end
  local src = reaper.GetMediaItemTake_Source(take)
  local srcFile = reaper.GetMediaSourceFileName(src, "")
  local s = measureTake(take, 2.0)

  -- Drop the rendered take again; keeping it would stack renders scenario on
  -- scenario and every later number would describe VoLum applied twice.
  reaper.Main_OnCommand(DELETE_ACTIVE_TAKE, 0)

  L(("stats[%s] got=%d peak=%.6f rms=%.6f bright=%.6f bad=%d src=%s")
    :format(label, s.got, s.peak, s.rms, s.bright, s.bad, tostring(srcFile)))
  return s
end

local function close(a, b, tol)
  return math.abs(a - b) <= math.max(1e-6, tol * math.max(math.abs(a), math.abs(b)))
end
local function sameAudio(a, b, tol)
  tol = tol or 0.02
  return close(a.rms, b.rms, tol) and close(a.peak, b.peak, tol) and close(a.bright, b.bright, tol)
end
local function differentAudio(a, b)
  return not (close(a.rms, b.rms, 0.01) and close(a.peak, b.peak, 0.01) and close(a.bright, b.bright, 0.01))
end
local function fmtAudio(s)
  return ("rms %.5f peak %.5f bright %.5f"):format(s.rms, s.peak, s.bright)
end

-- A capture swapped in by a recall is loaded on a worker thread and staged by the
-- next process call, so the first render after a change can catch a half-staged
-- rig. Render until two consecutive takes agree.
local function stableRender(rig, label)
  local prev = renderItem(rig.track, rig.item, label .. " #1")
  for i = 2, 5 do
    local cur = renderItem(rig.track, rig.item, label .. " #" .. i)
    if close(cur.rms, prev.rms, 0.002) and close(cur.peak, prev.peak, 0.005) then
      cur.stable = true
      record(label, cur)
      return cur
    end
    prev = cur
    sleep(0.3)
  end
  L("WARNING: render never settled for " .. label)
  prev.stable = false
  record(label, prev)
  return prev
end

-- --------------------------------------------------------- plugin state ---

-- FNV-1a over the FX's base64 state lines in the track chunk. GetTrackStateChunk
-- (not TrackFX_GetNamedConfigParm "vst_chunk") because the Lua binding returns the
-- whole chunk however large VoLum's id-tail JSON makes it.
local function fnv(s)
  local h = 2166136261
  for i = 1, #s do h = ((h ~ s:byte(i)) * 16777619) & 0xffffffff end
  return ("%08x"):format(h)
end

local function stateHash(rig)
  local ok, chunk = reaper.GetTrackStateChunk(rig.track, "", false)
  if not ok or not chunk then return "none", 0 end
  local n, inVst, depth, body = -1, false, 0, {}
  for line in chunk:gmatch("[^\r\n]+") do
    local t = line:match("^%s*(.-)%s*$")
    if not inVst then
      if t:sub(1, 4) == "<VST" then
        n = n + 1
        if n == rig.fx then inVst, depth = true, 1 end
      end
    else
      if t:sub(1, 1) == "<" then depth = depth + 1
      elseif t == ">" then
        depth = depth - 1
        if depth == 0 then break end
      end
      body[#body + 1] = t
    end
  end
  local s = table.concat(body, "\n")
  return fnv(s), #s
end

local function findParam(rig, name)
  local n = reaper.TrackFX_GetNumParams(rig.track, rig.fx)
  for p = 0, n - 1 do
    local _, pn = reaper.TrackFX_GetParamName(rig.track, rig.fx, p, "")
    if pn == name then return p end
  end
  return nil
end

-- The host's view of VoLum's own parameters (what automation lanes and REAPER's
-- generic UI show). Stops at iPlug's MIDI-CC parameter block or REAPER's Bypass.
local function paramSnapshot(rig)
  local t = {}
  local n = reaper.TrackFX_GetNumParams(rig.track, rig.fx)
  for p = 0, n - 1 do
    local _, pn = reaper.TrackFX_GetParamName(rig.track, rig.fx, p, "")
    if pn == "BankSel.MSB" or pn == "Bypass" then break end
    t[#t + 1] = { name = pn, v = reaper.TrackFX_GetParamNormalized(rig.track, rig.fx, p) }
  end
  return t
end

local function paramDiff(a, b)
  local names = {}
  for i = 1, math.min(#a, #b) do
    if math.abs(a[i].v - b[i].v) > 1e-6 then names[#names + 1] = a[i].name end
  end
  return #names, table.concat(names, ",", 1, math.min(#names, 6))
end

-- ------------------------------------------------------------- volum.log ---

local function logSize()
  local fh = io.open(volumLog, "rb")
  if not fh then return 0 end
  local s = fh:seek("end")
  fh:close()
  return s or 0
end

-- New complete lines since `off`. A shrunk file means the log rolled, so read it
-- from the start (lines written just before the roll are lost; harmless here).
local function logReadFrom(off)
  local fh = io.open(volumLog, "rb")
  if not fh then return "", off end
  local size = fh:seek("end")
  if size < off then off = 0 end
  fh:seek("set", off)
  local s = fh:read("a") or ""
  fh:close()
  local cut = s:match(".*()\n")
  if not cut then return "", off end
  return s:sub(1, cut), off + cut
end

local function midiLines(text)
  local out = {}
  for line in text:gmatch("[^\r\n]+") do
    local slot, amp, preset = line:match("%[midi%] recall slot=(%d+) amp=(%S*) preset=(%S*)")
    if slot then
      out[#out + 1] = { slot = tonumber(slot), amp = amp, preset = preset, recalled = true, raw = line }
    else
      local s2 = line:match("%[midi%] slot=(%d+) has no playable Sound")
      if s2 then out[#out + 1] = { slot = tonumber(s2), recalled = false, raw = line } end
    end
  end
  return out
end

-- Wait for VoLum's OnIdle to drain the MIDI queue. After the first [midi] line,
-- keep reading a little longer so a burst that drained in two idles is complete.
local function waitMidi(off, timeout)
  local lines, deadline, settle = {}, now() + timeout, nil
  while now() < deadline do
    local text
    text, off = logReadFrom(off)
    for _, l in ipairs(midiLines(text)) do lines[#lines + 1] = l end
    if #lines > 0 and not settle then settle = now() + 0.6 end
    if settle and now() >= settle then break end
    coroutine.yield()
  end
  return lines, off
end

local function logContains(off, needle)
  local text = logReadFrom(off)
  return text:find(needle, 1, true) ~= nil
end

-- ------------------------------------------------------------ MIDI paths ---

local MIDI_ITEM_POS = 5.0 -- clear of the 2 s tone item, so the two never overlap

-- Offline: a MIDI item on VoLum's track, rendered through the FX chain with the
-- same apply-FX action the audio renders use. Needs no audio device.
local function deliverViaItem(track, msgs)
  local item = reaper.CreateNewMIDIItemInProj(track, MIDI_ITEM_POS, MIDI_ITEM_POS + 1.0, false)
  if not item then return false, "CreateNewMIDIItemInProj failed" end
  local take = reaper.GetActiveTake(item)
  for _, m in ipairs(msgs) do
    local ppq = reaper.MIDI_GetPPQPosFromProjTime(take, MIDI_ITEM_POS + m.t)
    reaper.MIDI_InsertCC(take, false, false, ppq, m.status & 0xF0, m.status & 0x0F, m.d1, m.d2)
  end
  reaper.MIDI_Sort(take)
  reaper.SelectAllMediaItems(0, false)
  reaper.SetMediaItemSelected(item, true)
  reaper.Main_OnCommand(APPLY_FX_STEREO, 0)
  reaper.DeleteTrackMediaItem(track, item)
  reaper.UpdateArrange()
  return true
end

-- Live: the virtual MIDI keyboard queue into the armed, monitored track. Only
-- possible with REAPER's audio engine running; used when the offline path showed
-- nothing, to tell "REAPER never passes this to a VST3" from "apply-FX does not".
local function deliverLive(track, msgs)
  if not reaper.Audio_IsRunning() then return false, "audio engine not running" end
  local keys = { "I_RECINPUT", "I_RECMODE", "I_RECMON", "I_RECARM" }
  local prev = {}
  for _, k in ipairs(keys) do prev[k] = reaper.GetMediaTrackInfo_Value(track, k) end
  reaper.SetMediaTrackInfo_Value(track, "I_RECINPUT", 4096 + (62 << 5)) -- VKB, all channels
  reaper.SetMediaTrackInfo_Value(track, "I_RECMODE", 2) -- record nothing, monitor only
  reaper.SetMediaTrackInfo_Value(track, "I_RECMON", 1)
  reaper.SetMediaTrackInfo_Value(track, "I_RECARM", 1)
  sleep(0.4)
  local t0 = now()
  for _, m in ipairs(msgs) do
    while now() - t0 < m.t do coroutine.yield() end
    reaper.StuffMIDIMessage(0, m.status, m.d1, m.d2)
  end
  return true, nil, function()
    for _, k in ipairs(keys) do reaper.SetMediaTrackInfo_Value(track, k, prev[k]) end
  end
end

local function midiMsgs(kind, values)
  local msgs = {}
  for i, v in ipairs(values) do
    local t = 0.05 + (i - 1) * 0.45
    if kind == "pc" then
      msgs[#msgs + 1] = { t = t, status = 0xC0, d1 = v, d2 = 0 }
    elseif kind == "cc" then
      msgs[#msgs + 1] = { t = t, status = 0xB0, d1 = 102, d2 = v }
    else -- kind = { cc = n }: a CC that is not the recall CC
      msgs[#msgs + 1] = { t = t, status = 0xB0, d1 = kind.cc, d2 = v }
    end
  end
  return msgs
end

-- First path that produced a [midi] line, per message kind. Only successes are
-- cached: one silent message (a deduplicated repeat, say) must not write a path off.
local viaFor = {}

-- Send `values` as one burst and return VoLum's [midi] lines plus the path used.
-- `paths` overrides the default order (cached path, else item then live).
local function sendMidi(track, kind, values, label, paths)
  local key = type(kind) == "table" and ("cc" .. kind.cc) or kind
  paths = paths or (viaFor[key] and { viaFor[key] } or { "item", "live" })
  local tried = {}
  for _, path in ipairs(paths) do
    local off = logSize()
    local ok, why, restore
    if path == "item" then ok, why = deliverViaItem(track, midiMsgs(kind, values))
    else ok, why, restore = deliverLive(track, midiMsgs(kind, values)) end
    if ok then
      local lines = waitMidi(off, 4.0)
      if restore then restore() end
      if #lines > 0 then
        if not viaFor[key] then viaFor[key] = path end
        local raw = {}
        for _, l in ipairs(lines) do raw[#raw + 1] = l.raw:match("%[midi%].*") end
        L(("%s: via %s -> %s"):format(label, path, table.concat(raw, " | ")))
        return lines, path
      end
      L(("%s: no [midi] line via %s"):format(label, path))
      tried[#tried + 1] = path
    else
      L(("%s: path %s skipped (%s)"):format(label, path, tostring(why)))
      tried[#tried + 1] = path .. " skipped: " .. tostring(why)
    end
  end
  return {}, "none", table.concat(tried, "; ")
end

-- ------------------------------------------------------------- projects ---

do
  local e = io.open(emptyRpp, "w")
  if e then e:write('<REAPER_PROJECT 0.1 "7.0/win64" 0\n>\n'); e:close() end
end

-- Replace the current project (noprompt: never a "save changes?" modal) with one
-- holding `n` tracks, each with the test tone and its own VoLum. The master is
-- muted so a live MIDI or realtime-record step can never reach the speakers.
local function freshProject(n)
  reaper.Main_openProject("noprompt:" .. emptyRpp)
  reaper.GetSetProjectInfo_String(0, "RECORD_PATH", dir, true)
  reaper.SetMediaTrackInfo_Value(reaper.GetMasterTrack(0), "B_MUTE", 1)
  local rigs = {}
  for i = 0, n - 1 do
    reaper.InsertTrackAtIndex(i, true)
    local tr = reaper.GetTrack(0, i)
    -- Built by hand: InsertMedia adds to the last-touched track, which is not
    -- necessarily the one just inserted.
    local src = reaper.PCM_Source_CreateFromFile(inputWav)
    if not src then error("PCM_Source_CreateFromFile failed for " .. inputWav) end
    local item = reaper.AddMediaItemToTrack(tr)
    local take = reaper.AddTakeToMediaItem(item)
    reaper.SetMediaItemTake_Source(take, src)
    reaper.SetMediaItemInfo_Value(item, "D_POSITION", 0.0)
    reaper.SetMediaItemInfo_Value(item, "D_LENGTH", (reaper.GetMediaSourceLength(src)))
    reaper.UpdateItemInProject(item)
    local fx = reaper.TrackFX_AddByName(tr, "VoLum", false, -1)
    if fx < 0 then error("VoLum VST3 not found by REAPER (scan it first)") end
    if fxname == "?" then fxname = select(2, reaper.TrackFX_GetFXName(tr, fx, "")) end
    rigs[#rigs + 1] = { track = tr, item = item, fx = fx }
  end
  reaper.UpdateArrange()
  return rigs
end

local function warm(rigs, label)
  sleep(3.0)
  for i, rig in ipairs(rigs) do renderItem(rig.track, rig.item, label .. " warmup " .. i .. " (discarded)") end
end

-- ------------------------------------------------------- recall + checks ---

-- One recall step: send, then judge the [midi] line against the requested slot.
-- Returns an observation with the post-recall render, state hash and host params.
local function recallStep(scn, rig, kind, slot, label, opts)
  opts = opts or {}
  local values = opts.values or { slot }
  local lines, via, tried = sendMidi(rig.track, kind, values, scn .. "/" .. label, opts.paths)
  local obs = { lines = lines, via = via, ok = false }
  local last = lines[#lines]
  if not last then
    check(scn, label .. " reaches VoLum", opts.soft and "WARN" or "FAIL",
      "no [midi] line in volum.log within 4 s per path (" .. tostring(tried) .. ")")
  elseif not last.recalled then
    check(scn, label .. " reaches VoLum", "FAIL",
      ("VoLum got slot %d but it has no playable Sound%s"):format(last.slot, sandbox and " (pre-fill missing? factory presets file not found next to the rigs)" or ""))
    obs.slot = last.slot
  elseif last.slot ~= slot then
    check(scn, label .. " reaches VoLum", "FAIL",
      ("asked for slot %d, VoLum recalled slot %d%s"):format(slot, last.slot,
        last.slot == slot - 1 and " (off by one: the normalized MIDI value truncates in iPlug's VST3 IMidiMapping path)" or ""))
    obs.slot = last.slot
  else
    check(scn, label .. " reaches VoLum", "PASS",
      ("via %s: recall slot=%d amp=%s preset=%s (%d [midi] line(s))"):format(via, last.slot, last.amp, last.preset, #lines))
    obs.ok, obs.slot, obs.amp, obs.preset = true, last.slot, last.amp, last.preset
    local want = PREFILL[slot]
    if sandbox and want then
      local good = last.amp == want.amp and last.preset == want.preset
      check(scn, label .. " recalls " .. want.name, good and "PASS" or "FAIL",
        ("expected amp=%s preset=%s, got amp=%s preset=%s"):format(want.amp, want.preset, last.amp, last.preset))
    end
  end
  sleep(1.5) -- the recalled capture loads on VoLum's worker thread
  obs.render = stableRender(rig, scn .. "/" .. label)
  obs.hash = stateHash(rig)
  obs.params = paramSnapshot(rig)
  return obs
end

local function snapshot(rig, label)
  local o = { render = stableRender(rig, label) }
  o.hash = stateHash(rig)
  o.params = paramSnapshot(rig)
  return o
end

-- The recall must show up in what VoLum renders and in the state the host saves.
local function judgeChange(scn, label, before, after, beforeName)
  if not (after.ok and before and before.render and after.render) then return end
  check(scn, label .. " changes the audio", differentAudio(before.render, after.render) and "PASS" or "FAIL",
    ("%s: %s; after: %s"):format(beforeName, fmtAudio(before.render), fmtAudio(after.render)))
  check(scn, label .. " changes the saved state", before.hash ~= after.hash and "PASS" or "FAIL",
    ("state hash %s -> %s"):format(before.hash, after.hash))
end

-- Advisory: VoLum applies a recalled preset to its own parameters but not to the
-- VST3 controller's copy, so the host may keep showing the old knob values.
local function judgeHostView(scn, before, afters)
  local stateMoved, viewMoved = false, false
  for _, a in ipairs(afters) do
    if a.ok and a.hash ~= before.hash then stateMoved = true end
    if a.ok and a.params and paramDiff(before.params, a.params) > 0 then viewMoved = true end
  end
  if not stateMoved then return end
  fact("host_param_view_follows_midi_recall", viewMoved and "yes" or "no")
  check(scn, "host parameter view follows the recall", viewMoved and "PASS" or "WARN",
    viewMoved and "TrackFX_GetParam moved with the recalled Sound"
      or "the saved state changed but TrackFX_GetParam still reads the pre-recall knobs (REAPER's generic UI and automation lanes would show stale values)")
end

-- "yes" when every step that produced a [midi] line recalled exactly the slot it
-- asked for.
local function slotsExact(steps)
  for _, s in ipairs(steps) do
    if s.obs.slot ~= nil and s.obs.slot ~= s.want then return "no" end
  end
  return "yes"
end

-- Program Change first unless only the CC is known to reach VoLum in this run;
-- the CC is the fallback either way.
local function recallAny(scn, rig, slot, label)
  local kinds = (viaFor.cc and not viaFor.pc) and { "cc" } or { "pc", "cc" }
  local obs
  for i, k in ipairs(kinds) do
    obs = recallStep(scn, rig, k, slot, label .. " (" .. k .. ")", { soft = i < #kinds })
    if obs.ok then return obs end
  end
  return obs
end

-- -------------------------------------------------------------- scenarios ---

local function scenarioCore()
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

  fxname = select(2, reaper.TrackFX_GetFXName(track, fx, ""))
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

  local function stats(label)
    local s = renderItem(track, reaper.GetTrackMediaItem(track, 0), label)
    return s
  end

  -- The amp capture loads on a worker thread and is staged into the DSP by the
  -- audio callback, so a render started immediately after instantiation can catch
  -- a half-staged rig. Wait, then throw one render away.
  sleep(3.0)
  local warmup = stats("warmup (discarded)")
  L(("warmup peak=%.6f rms=%.6f"):format(warmup.peak, warmup.rms))

  -- Scenario 1: default loaded rig (whatever the library says was last in use).
  record("default", stats("default"))

  -- Scenario 2: the tripwire. With the plugin bypassed the render must come back
  -- as the untouched input tone, and it must NOT match the default scenario. If
  -- these two ever agree, the harness is measuring something other than VoLum's
  -- output and every other number here is worthless.
  reaper.TrackFX_SetEnabled(track, fx, false)
  record("bypassed", stats("bypassed"))
  reaper.TrackFX_SetEnabled(track, fx, true)

  -- Scenario 3: POST Tremolo at its stock depth/rate - audible amplitude
  -- modulation, so the render has to differ from the default one.
  setParam("TremoloActive", 1.0)
  record("tremolo_on", stats("tremolo_on"))
  setParam("TremoloActive", 0.0)

  -- Scenario 4: PRE Pitch, transposed a full octave down (normalized 0.0 on a
  -- -12..+7 semitone range) so it cannot be a no-op either.
  setParam("PrePitchActive", 1.0)
  setParam("PrePitchSemitones", 0.0)
  record("pitch_on", stats("pitch_on"))
  setParam("PrePitchActive", 0.0)
  setParam("PrePitchSemitones", 12.0 / 19.0) -- back to 0 st

  -- Best-effort: write BEFORE the round-trip so a blocking reopen can never
  -- starve the runner of a result.
  emit(true, nil, false)

  -- Round-trip: save, then reopen with the noprompt: prefix so REAPER never pops
  -- a "save changes?" modal that would hang a headless run.
  local rpp = dir .. "\\roundtrip.rpp"
  reaper.Main_SaveProjectEx(0, rpp, 0)
  L("saved project: " .. rpp)
  reaper.Main_openProject("noprompt:" .. rpp)
  local tr2 = reaper.GetTrack(0, 0)
  if tr2 then track = tr2 end
  sleep(3.0)
  record("reloaded", stats("reloaded"))
end

-- (a) Program Change. Order matters: PC 0 first, because VST3 carries Program
-- Change as a parameter that starts at 0, and a host that only sends changed
-- values would swallow it on a fresh instance; program 0 is the first footswitch.
local function scenarioMidiPc()
  local scn = "midi-pc"
  local rig = freshProject(1)[1]
  warm({ rig }, scn)
  local base = snapshot(rig, scn .. "/baseline")
  local logOff = logSize()

  local p0 = recallStep(scn, rig, "pc", 0, "PC 0 on a fresh instance", { soft = true })
  local p1 = recallStep(scn, rig, "pc", 1, "PC 1")
  judgeChange(scn, "PC 1", p0.ok and p0 or base, p1, p0.ok and "after PC 0" or "baseline")
  local p3 = recallStep(scn, rig, "pc", 3, "PC 3")
  judgeChange(scn, "PC 3", p1, p3, "after PC 1")

  local reached = p0.ok or p1.ok or p3.ok
  fact("pc_reaches_volum_vst3", reached and "yes" or "no")
  fact("pc_path", viaFor.pc or "none")
  fact("pc0_on_fresh_instance", p0.ok and "yes" or "no")
  if not reached then
    check(scn, "remaining PC checks", "SKIP", "no Program Change reached VoLum")
    return
  end
  fact("pc_slot_exact", slotsExact({ { obs = p0, want = 0 }, { obs = p1, want = 1 }, { obs = p3, want = 3 } }))

  -- Latest wins: PC 4 then PC 1 in one item must end on slot 1 and sound like it.
  local burst = recallStep(scn, rig, "pc", 1, "burst PC 4 then PC 1", { values = { 4, 1 } })
  if burst.ok and p1.render then
    check(scn, "burst ends on the last program", sameAudio(burst.render, p1.render) and "PASS" or "FAIL",
      ("after burst %s; PC 1 alone %s"):format(fmtAudio(burst.render), fmtAudio(p1.render)))
  end

  -- Pressing the same footswitch again must re-recall: knock Bass off the preset
  -- first so the re-recall is audible, then send PC 1 again.
  local bass = findParam(rig, "Bass")
  if bass and p1.render then
    reaper.TrackFX_SetParamNormalized(rig.track, rig.fx, bass, 0.95)
    sleep(0.3)
    local tweaked = renderItem(rig.track, rig.item, scn .. "/bass tweaked")
    local rep = recallStep(scn, rig, "pc", 1, "PC 1 again (same program)", { soft = true })
    fact("pc_repeat_same_program_recalls", rep.ok and "yes" or "no")
    if rep.ok then
      check(scn, "repeat PC restores the Sound", sameAudio(rep.render, p1.render) and "PASS" or "FAIL",
        ("tweaked %s; repeat %s; PC 1 %s"):format(fmtAudio(tweaked), fmtAudio(rep.render), fmtAudio(p1.render)))
    end
  end

  -- An empty program is logged and ignored; the Sound keeps playing.
  if sandbox then
    local before = renderItem(rig.track, rig.item, scn .. "/before PC 100")
    local lines = sendMidi(rig.track, "pc", { 100 }, scn .. "/PC 100 (empty)", viaFor.pc and { viaFor.pc } or { "item" })
    local l = lines[#lines]
    check(scn, "empty program is ignored", (l and not l.recalled and l.slot == 100) and "PASS" or "FAIL",
      l and l.raw:match("%[midi%].*") or "no [midi] line")
    sleep(0.5)
    local after = renderItem(rig.track, rig.item, scn .. "/after PC 100")
    check(scn, "empty program keeps the Sound", sameAudio(before, after) and "PASS" or "FAIL",
      ("before %s; after %s"):format(fmtAudio(before), fmtAudio(after)))
  end

  judgeHostView(scn, base, { p0, p1, p3 })
  if logContains(logOff, "UnserializeState failed") then
    check(scn, "no state errors", "FAIL", "volum.log has 'UnserializeState failed' during the scenario")
  end
end

-- (b) The recall CC (102). Same shape as midi-pc, plus a CC that is not the
-- recall CC, which must do nothing.
local function scenarioMidiCc()
  local scn = "midi-cc102"
  local rig = freshProject(1)[1]
  warm({ rig }, scn)
  local base = snapshot(rig, scn .. "/baseline")

  local c0 = recallStep(scn, rig, "cc", 0, "CC 102=0 on a fresh instance", { soft = true })
  local c2 = recallStep(scn, rig, "cc", 2, "CC 102=2")
  judgeChange(scn, "CC 102=2", c0.ok and c0 or base, c2, c0.ok and "after CC 102=0" or "baseline")
  local c4 = recallStep(scn, rig, "cc", 4, "CC 102=4")
  judgeChange(scn, "CC 102=4", c2, c4, "after CC 102=2")

  local reached = c0.ok or c2.ok or c4.ok
  fact("cc102_reaches_volum_vst3", reached and "yes" or "no")
  fact("cc102_path", viaFor.cc or "none")
  fact("cc102_value0_on_fresh_instance", c0.ok and "yes" or "no")
  if not reached then
    check(scn, "remaining CC checks", "SKIP", "no CC 102 reached VoLum")
    return
  end
  fact("cc102_slot_exact", slotsExact({ { obs = c0, want = 0 }, { obs = c2, want = 2 }, { obs = c4, want = 4 } }))

  local burst = recallStep(scn, rig, "cc", 2, "burst CC 102=3 then 2", { values = { 3, 2 } })
  if burst.ok and c2.render then
    check(scn, "burst ends on the last value", sameAudio(burst.render, c2.render) and "PASS" or "FAIL",
      ("after burst %s; CC 102=2 alone %s"):format(fmtAudio(burst.render), fmtAudio(c2.render)))
  end

  local bass = findParam(rig, "Bass")
  if bass and c2.render then
    reaper.TrackFX_SetParamNormalized(rig.track, rig.fx, bass, 0.95)
    sleep(0.3)
    local tweaked = renderItem(rig.track, rig.item, scn .. "/bass tweaked")
    local rep = recallStep(scn, rig, "cc", 2, "CC 102=2 again (same value)", { soft = true })
    fact("cc102_repeat_same_value_recalls", rep.ok and "yes" or "no")
    if rep.ok then
      check(scn, "repeat CC restores the Sound", sameAudio(rep.render, c2.render) and "PASS" or "FAIL",
        ("tweaked %s; repeat %s; CC 102=2 %s"):format(fmtAudio(tweaked), fmtAudio(rep.render), fmtAudio(c2.render)))
    end
  end

  -- CC 103 is not the recall CC: no [midi] line, no change.
  local before = renderItem(rig.track, rig.item, scn .. "/before CC 103")
  local lines = sendMidi(rig.track, { cc = 103 }, { 1 }, scn .. "/CC 103=1", { viaFor.cc or "item" })
  check(scn, "other CC is ignored", #lines == 0 and "PASS" or "FAIL",
    #lines == 0 and "no [midi] line" or lines[#lines].raw:match("%[midi%].*"))
  local after = renderItem(rig.track, rig.item, scn .. "/after CC 103")
  check(scn, "other CC keeps the Sound", sameAudio(before, after) and "PASS" or "FAIL",
    ("before %s; after %s"):format(fmtAudio(before), fmtAudio(after)))

  judgeHostView(scn, base, { c0, c2, c4 })
end

-- (c) Recall a Sound, turn a knob, save, then move the machine's last-used Sound
-- elsewhere before reopening, so a reopen that ignored the project's own state
-- and fell back to volum-settings.json could not pass by accident.
local function scenarioRoundtripAfterRecall()
  local scn = "project-roundtrip-after-recall"
  local rig = freshProject(1)[1]
  warm({ rig }, scn)
  local r1 = recallAny(scn, rig, 1, "recall slot 1")
  if not r1.ok then
    check(scn, "reopen after recall", "SKIP", "neither PC nor CC 102 recalled a Sound")
    return
  end
  local bass = findParam(rig, "Bass")
  if bass then reaper.TrackFX_SetParamNormalized(rig.track, rig.fx, bass, 0.83) end
  sleep(0.3)
  local saved = snapshot(rig, scn .. "/saved")
  local bassSaved = bass and reaper.TrackFX_GetParamNormalized(rig.track, rig.fx, bass)
  local rpp = dir .. "\\roundtrip-recall.rpp"
  reaper.Main_SaveProjectEx(0, rpp, 0)
  L("saved project: " .. rpp)

  local moved = recallAny(scn, rig, 3, "recall slot 3 after saving")
  if moved.ok then
    check(scn, "slot 1 and slot 3 are distinguishable", differentAudio(saved.render, moved.render) and "PASS" or "WARN",
      ("saved %s; slot 3 %s"):format(fmtAudio(saved.render), fmtAudio(moved.render)))
  end
  sleep(1.0) -- let OnIdle flush the machine settings

  local logOff = logSize()
  reaper.Main_openProject("noprompt:" .. rpp)
  local tr = reaper.GetTrack(0, 0)
  if not tr then error("reopened project has no track") end
  local re = { track = tr, item = reaper.GetTrackMediaItem(tr, 0), fx = 0 }
  warm({ re }, scn .. "/reopened")
  local back = snapshot(re, scn .. "/reopened")

  local same = sameAudio(back.render, saved.render)
  local detail = ("reopened %s; saved %s"):format(fmtAudio(back.render), fmtAudio(saved.render))
  if not same and moved.ok and sameAudio(back.render, moved.render) then
    detail = detail .. " - it reopened with the machine's last-used Sound (slot 3), not the project's"
  end
  check(scn, "reopened project renders the saved Sound", same and "PASS" or "FAIL", detail)
  if bass then
    local b = reaper.TrackFX_GetParamNormalized(re.track, re.fx, bass)
    check(scn, "Bass knob survives the reopen", math.abs(b - bassSaved) < 1e-3 and "PASS" or "FAIL",
      ("saved %.4f, reopened %.4f"):format(bassSaved, b))
  end
  local n, names = paramDiff(saved.params, back.params)
  check(scn, "host parameters match after reopen", n == 0 and "PASS" or "WARN",
    n == 0 and "all equal" or (n .. " differ: " .. names))
  check(scn, "saved state identical after reopen", saved.hash == back.hash and "PASS" or "WARN",
    ("state hash %s -> %s"):format(saved.hash, back.hash))
  check(scn, "reopen logs no state error", logContains(logOff, "UnserializeState failed") and "FAIL" or "PASS",
    "volum.log since the reopen")
  fact("project_reopen_keeps_recalled_sound", same and "yes" or "no")
end

-- (d) Two instances on two tracks with different Sounds, then one is removed.
-- Track B recalls last, so B is also the instance that last claimed VoLum's
-- process-global preset hooks; deleting it is the interesting case.
local function scenarioTwoInstances()
  local scn = "two-instances"
  local logOff = logSize()
  local rigs = freshProject(2)
  local a, b = rigs[1], rigs[2]
  warm(rigs, scn)
  local text = logReadFrom(logOff)
  local created = 0
  for _ in text:gmatch("instance created") do created = created + 1 end
  check(scn, "both instances start", created >= 2 and "PASS" or "WARN",
    created .. " 'instance created' line(s) in volum.log")

  local ra = recallAny(scn, a, 1, "track A recall slot 1")
  local rb = recallAny(scn, b, 3, "track B recall slot 3")
  if not (ra.ok and rb.ok) then
    check(scn, "coexistence", "SKIP", "a recall did not reach VoLum; see the reaches-VoLum checks")
    return
  end
  check(scn, "the two instances sound different", differentAudio(ra.render, rb.render) and "PASS" or "FAIL",
    ("A %s; B %s"):format(fmtAudio(ra.render), fmtAudio(rb.render)))
  local a1 = stableRender(a, scn .. "/A after B recalled")
  check(scn, "B's recall leaves A alone", sameAudio(a1, ra.render) and "PASS" or "FAIL",
    ("A before %s; A after %s"):format(fmtAudio(ra.render), fmtAudio(a1)))
  check(scn, "B renders finite and bounded", (rb.render.bad == 0 and rb.render.peak < 8.0) and "PASS" or "FAIL",
    ("bad %d peak %.4f"):format(rb.render.bad, rb.render.peak))

  reaper.TrackFX_Delete(b.track, b.fx)
  sleep(1.0)
  local a2 = stableRender(a, scn .. "/A after B removed")
  check(scn, "A keeps playing after B is removed",
    (a2.bad == 0 and a2.peak < 8.0 and a2.rms > 1e-5 and sameAudio(a2, ra.render)) and "PASS" or "FAIL",
    ("A before %s; A after %s; bad %d"):format(fmtAudio(ra.render), fmtAudio(a2), a2.bad))

  local r2 = recallAny(scn, a, 2, "track A recall slot 2 after B removed")
  if r2.ok then
    check(scn, "A still recalls after B is removed", differentAudio(a2, r2.render) and "PASS" or "FAIL",
      ("before %s; after slot 2 %s"):format(fmtAudio(a2), fmtAudio(r2.render)))
  end
  fact("two_instances_independent", (sameAudio(a1, ra.render) and sameAudio(a2, ra.render)) and "yes" or "no")
end

-- (e) The same rig rendered offline (apply-FX) and in realtime (record the
-- track's output through a send onto a capture track while the transport runs).
-- Compared on total energy, which a few milliseconds of record latency do not
-- change. Needs REAPER's audio engine; without a device this is a SKIP.
local function scenarioOfflineVsRealtime()
  local scn = "offline-vs-realtime"
  if not reaper.Audio_IsRunning() then
    reaper.Audio_Init()
    sleep(1.5)
  end
  if not reaper.Audio_IsRunning() then
    check(scn, "realtime render", "SKIP", "REAPER's audio engine is not running (no usable audio device)")
    return
  end
  local _, srate = reaper.GetAudioDeviceInfo("SRATE", "")
  local _, bsize = reaper.GetAudioDeviceInfo("BSIZE", "")
  L(("audio device: srate=%s bsize=%s"):format(tostring(srate), tostring(bsize)))

  local rig = freshProject(1)[1]
  warm({ rig }, scn)
  local off = stableRender(rig, scn .. "/offline")

  reaper.InsertTrackAtIndex(1, true)
  local cap = reaper.GetTrack(0, 1)
  reaper.CreateTrackSend(rig.track, cap)
  reaper.SetMediaTrackInfo_Value(rig.track, "B_MAINSEND", 0)
  reaper.SetMediaTrackInfo_Value(cap, "B_MAINSEND", 0)
  reaper.SetMediaTrackInfo_Value(rig.track, "I_RECARM", 0)
  reaper.SetMediaTrackInfo_Value(cap, "I_RECMODE", 1) -- record: output (stereo)
  reaper.SetMediaTrackInfo_Value(cap, "I_RECMON", 0)
  reaper.SetMediaTrackInfo_Value(cap, "I_RECARM", 1)
  reaper.SetEditCurPos(0.0, false, false)

  reaper.Main_OnCommand(TRANSPORT_RECORD, 0)
  local deadline = now() + 15
  while now() < deadline do
    if (reaper.GetPlayState() & 4) ~= 0 and reaper.GetPlayPosition() >= 2.4 then break end
    coroutine.yield()
  end
  reaper.Main_OnCommand(TRANSPORT_STOP_SAVE, 0)
  sleep(0.8)

  local capItem = reaper.GetTrackMediaItem(cap, 0)
  if not capItem or not reaper.GetActiveTake(capItem) then
    check(scn, "realtime render", "FAIL", "recording the capture track produced no item")
    return
  end
  local rt = measureTake(reaper.GetActiveTake(capItem), 3.0)
  record(scn .. "/realtime", rt)
  L(("realtime capture: dur %.3f s, %s, energy %.6f; offline energy %.6f")
    :format(rt.dur, fmtAudio(rt), rt.energy, off.energy))
  local ratioDb = (rt.energy > 0 and off.energy > 0) and 10 * math.log(rt.energy / off.energy, 10) or -999
  check(scn, "realtime render is finite", rt.bad == 0 and "PASS" or "FAIL", ("bad %d"):format(rt.bad))
  check(scn, "realtime energy matches offline", close(rt.energy, off.energy, 0.03) and "PASS" or "FAIL",
    ("realtime %.6f vs offline %.6f (%.2f dB); device %s Hz / %s"):format(rt.energy, off.energy, ratioDb, tostring(srate), tostring(bsize)))
  check(scn, "realtime peak matches offline", close(rt.peak, off.peak, 0.05) and "PASS" or "WARN",
    ("realtime %.5f vs offline %.5f"):format(rt.peak, off.peak))
  fact("offline_matches_realtime", close(rt.energy, off.energy, 0.03) and "yes" or "no")
end

-- ------------------------------------------------------------------ driver ---

local scenarioList = {
  { "core", scenarioCore },
  { "midi-pc", scenarioMidiPc },
  { "midi-cc102", scenarioMidiCc },
  { "project-roundtrip-after-recall", scenarioRoundtripAfterRecall },
  { "two-instances", scenarioTwoInstances },
  { "offline-vs-realtime", scenarioOfflineVsRealtime },
}

local function main()
  L("harness start; dir=" .. dir)
  L("scenarios: " .. (getenv("VOLUM_HARNESS_SCENARIOS") or "core") .. "; sandbox=" .. tostring(sandbox))
  L("volum.log: " .. volumLog .. " (" .. logSize() .. " bytes at start)")
  L("audio engine running: " .. tostring(reaper.Audio_IsRunning()))
  for _, s in ipairs(scenarioList) do
    if wants(s[1]) then
      L("=== scenario " .. s[1])
      if s[1] == "core" then
        s[2]() -- a core failure is fatal, as it always was
      else
        local ok, err = pcall(s[2])
        if not ok then check(s[1], "scenario ran", "FAIL", "error: " .. tostring(err)) end
      end
      emit(true, nil, false)
    end
  end
end

local co = coroutine.create(main)
local function step()
  local ok, err = coroutine.resume(co)
  if not ok then
    L("FATAL: " .. tostring(err))
    L(debug.traceback(co))
    emit(false, tostring(err), true)
    if log then log:close(); log = nil end
    return
  end
  if coroutine.status(co) == "dead" then
    emit(true, nil, true)
    L("harness done OK")
    if log then log:close(); log = nil end
    return
  end
  reaper.defer(step)
end
step()
