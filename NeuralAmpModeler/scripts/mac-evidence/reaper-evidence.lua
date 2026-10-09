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
    f:write(",\"pc_presave_rms\":" .. string.format("%.8f", r.pc_presave_rms or 0))
    f:write(",\"pc_reloaded_rms\":" .. string.format("%.8f", r.pc_reloaded_rms or 0))
    f:write(",\"pc_roundtrip\":" .. bool(r.pc_roundtrip))
    f:write(",\"pc_state_restored\":" .. bool(r.pc_state_restored))
    f:write(",\"load_recall_delta\":" .. tostring(r.load_recall_delta or -1))
    f:write(",\"no_recall_on_load\":" .. bool(r.no_recall_on_load))
    f:write(",\"fresh_nondefault_reload\":" .. bool(r.fresh_nondefault_reload))
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

local function wav_stats(path)
  local file = assert(io.open(path, "rb"), "cannot open rendered WAV: " .. tostring(path))
  local bytes = file:read("*a")
  file:close()
  if bytes:sub(1, 4) ~= "RIFF" or bytes:sub(9, 12) ~= "WAVE" then
    error("rendered file is not RIFF/WAVE: " .. tostring(path))
  end

  local audio_format, channels, bits, data_start, data_size
  local pos = 13
  while pos + 7 <= #bytes do
    local chunk_id = bytes:sub(pos, pos + 3)
    local chunk_size = string.unpack("<I4", bytes, pos + 4)
    local payload = pos + 8
    if chunk_id == "fmt " then
      audio_format = string.unpack("<I2", bytes, payload)
      channels = string.unpack("<I2", bytes, payload + 2)
      bits = string.unpack("<I2", bytes, payload + 14)
    elseif chunk_id == "data" then
      data_start, data_size = payload, chunk_size
    end
    pos = payload + chunk_size + (chunk_size % 2)
  end
  if not audio_format or not channels or not bits or not data_start then
    error("rendered WAV lacks fmt/data chunks: " .. tostring(path))
  end

  local width = bits // 8
  local count = data_size // width
  local peak, sumsq, bad = 0.0, 0.0, 0
  local sample_pos = data_start
  for _ = 1, count do
    local value
    if audio_format == 3 and bits == 32 then
      value = string.unpack("<f", bytes, sample_pos)
    elseif audio_format == 1 and bits == 16 then
      value = string.unpack("<i2", bytes, sample_pos) / 32768.0
    else
      error(("unsupported rendered WAV format=%d bits=%d"):format(audio_format, bits))
    end
    if value ~= value or value == math.huge or value == -math.huge then
      bad = bad + 1
      value = 0.0
    end
    peak = math.max(peak, math.abs(value))
    sumsq = sumsq + value * value
    sample_pos = sample_pos + width
  end
  return {peak=peak, rms=math.sqrt(sumsq / count), bad=bad, samples=count, channels=channels}
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
  local measured = wav_stats(source_file)
  L(("stats[%s] samples=%d channels=%d peak=%.8f rms=%.8f bad=%d src=%s")
    :format(label, measured.samples, measured.channels, measured.peak, measured.rms,
      measured.bad, tostring(source_file)))
  reaper.DeleteTrack(rendered_track)
  reaper.SetMediaTrackInfo_Value(track, "B_MUTE", 0)
  reaper.SetOnlyTrackSelected(track)
  return measured
end

local function state_chunk(track)
  local ok, chunk = reaper.GetTrackStateChunk(track, "", false)
  if not ok then error("GetTrackStateChunk failed") end
  return chunk
end

local function midi_recall_count()
  local home = os.getenv("HOME") or ""
  local file = io.open(home .. "/Library/Application Support/VoLum/volum.log", "r")
  if not file then return 0 end
  local count = 0
  for line in file:lines() do
    if line:find("[midi] recall", 1, true) then count = count + 1 end
  end
  file:close()
  return count
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

local function deliver_midi(track, audio_item, status, data1, data2, label)
  local before = state_chunk(track)
  local midi_item = add_midi_message(track, status, data1, data2)
  render_stats(track, audio_item, label .. "-offline")
  spin(1.0)
  reaper.DeleteTrackMediaItem(track, midi_item)
  local after = state_chunk(track)
  local moved = before ~= after
  L(("%s state changed=%s"):format(label, tostring(moved)))
  return moved, moved and "serialized plugin state changed after offline MIDI render"
    or "offline MIDI render produced no observable serialized-state change"
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
  for index = reaper.CountTracks(0) - 1, 0, -1 do
    reaper.DeleteTrack(reaper.GetTrack(0, index))
  end
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

  local pc_changed, pc_why =
    deliver_midi(track, item, 0xC0, 1, 0, spec.format .. " Program Change 1")
  local pc_stats = render_stats(track, item, spec.format .. "-after-pc1")
  if not pc_changed and math.abs(pc_stats.rms - initial.rms) > initial.rms * 0.01 then
    pc_changed = true
    pc_why = "render RMS changed by more than 1% after Program Change 1"
  end

  -- Save immediately after PC 1, then reopen while stopped. The VST3 wrapper's
  -- restore guard must keep host-restored program-list values from generating a
  -- second MIDI recall or jumping the instance to slot 0.
  local pc_rpp = dir .. "/" .. spec.format:lower() .. "-pc1-roundtrip.rpp"
  reaper.Main_SaveProjectEx(0, pc_rpp, 0)
  local saved_pc_state = state_chunk(track)
  local recalls_before_load = midi_recall_count()
  reaper.Main_openProject("noprompt:" .. pc_rpp)
  spin(3.0)
  track = assert(reaper.GetTrack(0, 0), "PC-reloaded track missing")
  item = assert(reaper.GetTrackMediaItem(track, 0), "PC-reloaded audio item missing")
  local restored_pc_state = state_chunk(track)
  local recalls_after_load = midi_recall_count()
  local pc_reloaded = render_stats(track, item, spec.format .. "-pc1-reloaded")
  local pc_tolerance = math.max(0.0001, pc_stats.rms * 0.02)
  local pc_roundtrip = pc_reloaded.bad == 0 and math.abs(pc_reloaded.rms - pc_stats.rms) <= pc_tolerance
  local pc_state_restored = restored_pc_state == saved_pc_state
  local load_recall_delta = recalls_after_load - recalls_before_load
  local no_recall_on_load = load_recall_delta == 0
  local moved_from_slot0 = math.abs(pc_stats.rms - initial.rms) > math.max(0.0001, initial.rms * 0.01)
  local fresh_nondefault_reload = pc_roundtrip and moved_from_slot0
  L(("%s PC1 reload rms before=%.8f after=%.8f state_equal=%s recall_delta=%d nondefault=%s")
    :format(spec.format, pc_stats.rms, pc_reloaded.rms, tostring(pc_state_restored),
      load_recall_delta, tostring(fresh_nondefault_reload)))

  local cc_changed, cc_why =
    deliver_midi(track, item, 0xB0, 102, 2, spec.format .. " CC102=2")
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
    pc_status=pc_changed and "PASS" or "FAIL", pc_evidence=pc_why,
    pc_presave_rms=pc_stats.rms, pc_reloaded_rms=pc_reloaded.rms,
    pc_roundtrip=pc_roundtrip, pc_state_restored=pc_state_restored,
    load_recall_delta=load_recall_delta, no_recall_on_load=no_recall_on_load,
    fresh_nondefault_reload=fresh_nondefault_reload,
    cc_status=cc_changed and "PASS" or "FAIL", cc_evidence=cc_why,
    roundtrip=roundtrip, reloaded_rms=reloaded.rms
  }
  reaper.TrackFX_Show(track, 0, 0)
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
