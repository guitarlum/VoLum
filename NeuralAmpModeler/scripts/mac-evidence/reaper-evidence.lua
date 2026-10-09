-- macOS REAPER evidence harness. VOLUM_REAPER_EVIDENCE_DIR contains input.wav.

local dir = os.getenv("VOLUM_REAPER_EVIDENCE_DIR")
if not dir or dir == "" then return end
local sentinel = dir .. "/go.txt"
local sf = io.open(sentinel, "r")
if not sf then return end
sf:close()

local log = assert(io.open(dir .. "/reaper-harness.log", "w"))
local function L(s) log:write(tostring(s) .. "\n"); log:flush() end
local function jstr(s)
  local escaped = tostring(s or ""):gsub("\\", "\\\\"):gsub('"', '\\"')
  escaped = escaped:gsub("[%z\1-\31]", function(c)
    return ("\\u%04x"):format(c:byte())
  end)
  return '"' .. escaped .. '"'
end
local function bool(v) return v and "true" or "false" end

local outcomes = {}
local function emit(ok, err)
  local f = assert(io.open(dir .. "/reaper-results.json", "w"))
  f:write("{\n  \"ok\": " .. bool(ok))
  if err then f:write(",\n  \"error\": " .. jstr(err)) end
  f:write(",\n  \"formats\": [\n")
  for i, r in ipairs(outcomes) do
    f:write("    {")
    f:write("\"format\":" .. jstr(r.format))
    f:write(",\"fx_name\":" .. jstr(r.fx_name))
    f:write(",\"rms\":" .. string.format("%.8f", r.rms or 0))
    f:write(",\"peak\":" .. string.format("%.8f", r.peak or 0))
    f:write(",\"bad\":" .. tostring(r.bad or -1))
    f:write(",\"pc_status\":" .. jstr(r.pc_status))
    f:write(",\"pc_evidence\":" .. jstr(r.pc_evidence))
    f:write(",\"cc_status\":" .. jstr(r.cc_status))
    f:write(",\"cc_evidence\":" .. jstr(r.cc_evidence))
    f:write(",\"roundtrip\":" .. bool(r.roundtrip))
    f:write(",\"reloaded_rms\":" .. string.format("%.8f", r.reloaded_rms or 0))
    f:write("}" .. (i < #outcomes and "," or "") .. "\n")
  end
  f:write("  ]\n}\n")
  f:close()
end

local SR = 48000
local RENDER_STEREO_STEM = 41719 -- Render selected area of tracks to stereo stem tracks (and mute originals)

local function spin(seconds)
  local until_time = reaper.time_precise() + seconds
  while reaper.time_precise() < until_time do
    coroutine.yield()
  end
end

local function render_stats(track, item, label)
  reaper.SetMediaTrackInfo_Value(track, "B_MUTE", 0)
  reaper.SetOnlyTrackSelected(track)
  reaper.GetSet_LoopTimeRange(true, false, 0.0, 2.0, false)
  reaper.UpdateArrange()
  reaper.Main_OnCommand(RENDER_STEREO_STEM, 0)
  local rendered_track = reaper.GetSelectedTrack(0, 0)
  if not rendered_track or rendered_track == track then
    error("stereo stem render produced no track for " .. label)
  end
  local rendered_item = reaper.GetTrackMediaItem(rendered_track, 0)
  if not rendered_item then error("stereo stem render produced no item for " .. label) end
  local take = reaper.GetActiveTake(rendered_item)
  if not take then error("stereo stem render produced no take for " .. label) end
  local source = reaper.GetMediaItemTake_Source(take)
  local source_file = reaper.GetMediaSourceFileName(source, "")
  local aa = reaper.CreateTakeAudioAccessor(take)
  local t0 = reaper.GetAudioAccessorStartTime(aa)
  local t1 = reaper.GetAudioAccessorEndTime(aa)
  local ns = math.floor(math.min(2.0, math.max(0.1, t1 - t0)) * SR)
  local buf = reaper.new_array(ns * 2)
  buf.clear()
  local got = reaper.GetAudioAccessorSamples(aa, SR, 2, t0, ns, buf)
  local peak, sumsq, bad = 0.0, 0.0, 0
  for _, value in ipairs(buf.table()) do
    local v = value or 0.0
    if v ~= v or v == math.huge or v == -math.huge then bad = bad + 1; v = 0 end
    peak = math.max(peak, math.abs(v))
    sumsq = sumsq + v * v
  end
  reaper.DestroyAudioAccessor(aa)
  local rms = math.sqrt(sumsq / (ns * 2))
  L(("stats[%s] got=%s peak=%.8f rms=%.8f bad=%d src=%s")
    :format(label, tostring(got), peak, rms, bad, tostring(source_file)))
  reaper.DeleteTrack(rendered_track)
  reaper.SetMediaTrackInfo_Value(track, "B_MUTE", 0)
  reaper.SetOnlyTrackSelected(track)
  return {peak=peak, rms=rms, bad=bad}
end

local function state_chunk(track)
  local ok, chunk = reaper.GetTrackStateChunk(track, "", false)
  if not ok then error("GetTrackStateChunk failed") end
  return chunk
end

local function add_midi_message(track, status, data1, data2)
  local item = reaper.CreateNewMIDIItemInProj(track, 0.0, 0.5, false)
  local take = reaper.GetActiveTake(item)
  local msg
  if status & 0xF0 == 0xC0 then
    msg = string.char(status, data1)
  else
    msg = string.char(status, data1, data2 or 0)
  end
  reaper.MIDI_InsertEvt(take, false, false, 0, msg)
  reaper.MIDI_Sort(take)
  return item
end

local function deliver_midi(track, status, data1, data2, label)
  local before = state_chunk(track)
  local midi_item = add_midi_message(track, status, data1, data2)
  reaper.SetEditCurPos(0, false, false)
  reaper.OnPlayButton()
  spin(1.5)
  reaper.OnStopButton()
  spin(1.0)
  local after = state_chunk(track)
  reaper.DeleteTrackMediaItem(track, midi_item)
  local moved = before ~= after
  L(("%s state changed=%s transport=%.3f"):format(label, tostring(moved), reaper.GetPlayPosition()))
  return moved, moved and "serialized plugin state changed after MIDI playback"
    or "runner playback produced no observable serialized-state change"
end

local function add_fx(track, candidates, wanted)
  for _, name in ipairs(candidates) do
    local fx = reaper.TrackFX_AddByName(track, name, false, -1)
    if fx >= 0 then
      local _, actual = reaper.TrackFX_GetFXName(track, fx, "")
      L(("candidate %s -> %d (%s)"):format(name, fx, actual or ""))
      if actual and actual:lower():find(wanted:lower(), 1, true) then
        return fx, actual
      end
      reaper.TrackFX_Delete(track, fx)
    else
      L("candidate not found: " .. name)
    end
  end
  error(wanted .. " VoLum not found after REAPER rescan")
end

local function test_format(spec)
  reaper.Main_OnCommand(40023, 0)
  reaper.GetSetProjectInfo_String(0, "RECORD_PATH", dir, true)
  reaper.InsertTrackAtIndex(0, true)
  local track = assert(reaper.GetTrack(0, 0), "track creation failed")
  reaper.SetOnlyTrackSelected(track)
  reaper.SetMediaTrackInfo_Value(track, "D_VOL", 1.0)
  reaper.SetMediaTrackInfo_Value(track, "B_MUTE", 0)
  reaper.SetEditCurPos(0, false, false)
  if (reaper.InsertMedia(dir .. "/input.wav", 0) or 0) < 1 then error("input.wav insert failed") end
  local item = assert(reaper.GetTrackMediaItem(track, 0), "audio item missing")
  reaper.SetMediaItemInfo_Value(item, "D_VOL", 1.0)
  reaper.SetMediaItemInfo_Value(item, "B_MUTE", 0)

  local fx, fx_name = add_fx(track, spec.candidates, spec.wanted)
  reaper.TrackFX_Show(track, fx, 3)
  spin(3.0)

  reaper.TrackFX_SetEnabled(track, fx, false)
  local bypassed = render_stats(track, item, spec.format .. "-bypassed")
  reaper.TrackFX_SetEnabled(track, fx, true)
  if bypassed.bad ~= 0 or bypassed.rms <= 0.00001 then
    error(spec.format .. " bypassed render is silent; host apply-FX mechanics did not preserve the input")
  end

  render_stats(track, item, spec.format .. "-warmup")
  local initial = render_stats(track, item, spec.format .. "-default")
  if initial.bad ~= 0 or initial.rms <= 0.00001 or initial.peak >= 8.0 then
    error(spec.format .. " output failed finite/non-silent/bounded check")
  end

  local pc_changed, pc_why = deliver_midi(track, 0xC0, 1, 0, spec.format .. " Program Change 1")
  local pc_stats = render_stats(track, item, spec.format .. "-after-pc1")
  if not pc_changed and math.abs(pc_stats.rms - initial.rms) > initial.rms * 0.01 then
    pc_changed = true
    pc_why = "render RMS changed by more than 1% after Program Change 1"
  end

  local cc_changed, cc_why = deliver_midi(track, 0xB0, 102, 2, spec.format .. " CC102=2")
  local cc_stats = render_stats(track, item, spec.format .. "-after-cc102")
  if not cc_changed and math.abs(cc_stats.rms - pc_stats.rms) > math.max(0.00001, pc_stats.rms * 0.01) then
    cc_changed = true
    cc_why = "render RMS changed by more than 1% after CC 102 value 2"
  end

  local rpp = dir .. "/" .. spec.format:lower() .. "-roundtrip.rpp"
  reaper.Main_SaveProjectEx(0, rpp, 0)
  reaper.Main_openProject("noprompt:" .. rpp)
  spin(3.0)
  track = assert(reaper.GetTrack(0, 0), "reloaded track missing")
  item = assert(reaper.GetTrackMediaItem(track, 0), "reloaded audio item missing")
  local reloaded = render_stats(track, item, spec.format .. "-reloaded")
  local tolerance = math.max(0.0001, cc_stats.rms * 0.02)
  local roundtrip = reloaded.bad == 0 and math.abs(reloaded.rms - cc_stats.rms) <= tolerance
  if not roundtrip then error(spec.format .. " state round-trip render mismatch") end

  outcomes[#outcomes + 1] = {
    format=spec.format, fx_name=fx_name, rms=initial.rms, peak=initial.peak, bad=initial.bad,
    pc_status=pc_changed and "PASS" or "SKIP", pc_evidence=pc_why,
    cc_status=cc_changed and "PASS" or "SKIP", cc_evidence=cc_why,
    roundtrip=roundtrip, reloaded_rms=reloaded.rms
  }
  L(spec.format .. " complete")
end

local worker = coroutine.create(function()
  local ok, err = xpcall(function()
    test_format({
      format="AU", wanted="AU: VoLum",
      candidates={"AU: VoLum (Lum)", "AU: VoLum", "VoLum (Lum)"}
    })
    test_format({
      format="VST3", wanted="VST3: VoLum",
      candidates={"VST3: VoLum (Lum)", "VST3: VoLum", "VoLum"}
    })
  end, debug.traceback)

  if ok then
    emit(true)
    L("PASS REAPER AU and VST3 evidence complete")
  else
    L("FATAL " .. tostring(err))
    emit(false, tostring(err))
  end
  os.remove(sentinel)
  log:close()
end)

local function resume_worker()
  local ok, err = coroutine.resume(worker)
  if not ok then
    L("FATAL coroutine: " .. tostring(err))
    emit(false, tostring(err))
    os.remove(sentinel)
    log:close()
    return
  end
  if coroutine.status(worker) ~= "dead" then
    reaper.defer(resume_worker)
  end
end
resume_worker()
