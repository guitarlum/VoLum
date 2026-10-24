-- REAPER startup script for smoke-reaper-linux.sh.
--   VOLUM_SMOKE_MODE=gates: open the editor, render 10 s of the DI twice
--     (the shell nulls the two renders, then nulls one against the committed
--     reference), then open two instances and close them.
--   VOLUM_SMOKE_MODE=cycles: N open/close cycles of VOLUM_SMOKE_INSTANCES
--     instances (the sanitizer host smoke uses 20 x 2).
-- Writes STEP lines, then PASS or FAIL <reason>, to VOLUM_SMOKE_RESULT.

local mode = os.getenv("VOLUM_SMOKE_MODE") or "gates"
local fxName = os.getenv("VOLUM_SMOKE_FX") or "VST3:VoLum"
local cycles = tonumber(os.getenv("VOLUM_SMOKE_CYCLES") or "20") or 20
local instances = tonumber(os.getenv("VOLUM_SMOKE_INSTANCES") or "2") or 2
local resultPath = os.getenv("VOLUM_SMOKE_RESULT") or ""
local diPath = os.getenv("VOLUM_SMOKE_DI") or ""
local renderDir = os.getenv("VOLUM_SMOKE_RENDER_DIR") or ""

local function report(line)
  local f = io.open(resultPath, "a")
  if f then
    f:write(line, "\n")
    f:close()
  end
end

local steps = {}
local stepIndex = 1
local waitUntil = 0
local tracks = {}

local function wait(seconds) waitUntil = reaper.time_precise() + seconds end

local function quit() reaper.Main_OnCommand(40004, 0) end -- File: Quit

local function fail(reason)
  report("FAIL " .. reason)
  quit()
end

local function show(i, flag) reaper.TrackFX_Show(tracks[i], 0, flag) end -- 3 float, 2 close

local function isOpen(i) return reaper.TrackFX_GetOpen(tracks[i], 0) end

local APPLY_FX = 40361 -- Item: Apply track/take FX to items (stereo output)
local DELETE_TAKE = 40129 -- Take: Delete active take from items

local function copyFile(src, dst)
  local inp = io.open(src, "rb")
  if not inp then return false end
  local data = inp:read("*a")
  inp:close()
  local out = io.open(dst, "wb")
  if not out then return false end
  out:write(data)
  out:close()
  return true
end

local function renderTake(label, keepAs)
  local item = reaper.GetTrackMediaItem(tracks[1], 0)
  if not item then
    fail("no media item")
    return false
  end
  reaper.SelectAllMediaItems(0, false)
  reaper.SetMediaItemSelected(item, true)
  reaper.Main_OnCommand(APPLY_FX, 0)
  local take = reaper.GetActiveTake(item)
  if not take then
    fail(label .. ": apply-FX produced no take")
    return false
  end
  local src = reaper.GetMediaItemTake_Source(take)
  local path = reaper.GetMediaSourceFileName(src, "")
  if not path or path == "" then
    fail(label .. ": render has no file")
    return false
  end
  if keepAs then
    if not copyFile(path, keepAs) then
      fail(label .. ": could not copy " .. path)
      return false
    end
    report("STEP " .. label .. " " .. keepAs)
  else
    report("STEP " .. label .. " discarded")
  end
  reaper.Main_OnCommand(DELETE_TAKE, 0)
  return true
end

steps[#steps + 1] = function()
  local count = mode == "gates" and 1 or instances
  for i = 1, count do
    reaper.InsertTrackAtIndex(i - 1, true)
    tracks[i] = reaper.GetTrack(0, i - 1)
    if reaper.TrackFX_AddByName(tracks[i], fxName, false, -1) < 0 then
      return fail("could not insert " .. fxName)
    end
  end
  report("STEP inserted " .. count .. " x " .. fxName)
  wait(2)
end

if mode == "gates" then
  steps[#steps + 1] = function()
    reaper.GetSetProjectInfo(0, "PROJECT_SRATE", 48000, true)
    reaper.GetSetProjectInfo(0, "PROJECT_SRATE_USE", 1, true)
    reaper.GetSetProjectInfo_String(0, "RECORD_PATH", renderDir, true)
    local track = tracks[1]
    reaper.SetOnlyTrackSelected(track)
    reaper.SetEditCurPos(0, false, false)
    local n = reaper.InsertMedia(diPath, 0)
    report("STEP media " .. tostring(n))
    if (n or 0) < 1 then return fail("InsertMedia failed for " .. diPath) end
    -- The capture loads on a worker thread. Wait before the first render.
    wait(8)
  end
  steps[#steps + 1] = function()
    show(1, 3)
    wait(1)
  end
  steps[#steps + 1] = function()
    if not isOpen(1) then return fail("editor did not open") end
    report("STEP editor open")
    if not renderTake("warmup", nil) then return end
    wait(0.2)
  end
  steps[#steps + 1] = function()
    if not renderTake("render-a", renderDir .. "/render-a.wav") then return end
    wait(0.2)
  end
  steps[#steps + 1] = function()
    if not renderTake("render-b", renderDir .. "/render-b.wav") then return end
    show(1, 2)
    wait(0.5)
  end
  steps[#steps + 1] = function()
    if isOpen(1) then return fail("editor still open after the render") end
    reaper.InsertTrackAtIndex(1, true)
    tracks[2] = reaper.GetTrack(0, 1)
    if reaper.TrackFX_AddByName(tracks[2], fxName, false, -1) < 0 then
      return fail("could not insert the second instance")
    end
    report("STEP second instance inserted")
    show(1, 3)
    show(2, 3)
    wait(1)
  end
  steps[#steps + 1] = function()
    if not isOpen(1) or not isOpen(2) then
      return fail("the two instances did not both open")
    end
    report("STEP two editors open")
    show(1, 2)
    show(2, 2)
    wait(0.6)
  end
  steps[#steps + 1] = function()
    if isOpen(1) or isOpen(2) then return fail("an editor stayed open") end
    report("STEP two editors closed")
    wait(0.2)
  end
else
  for c = 1, cycles do
    local first, second = 1, math.min(2, instances)
    if c % 2 == 0 then first, second = second, first end
    steps[#steps + 1] = function() show(first, 3); wait(0.6) end
    steps[#steps + 1] = function() show(second, 3); wait(0.6) end
    steps[#steps + 1] = function()
      if not isOpen(first) or not isOpen(second) then
        return fail("cycle " .. c .. ": an editor did not open")
      end
      show(first, 2)
      wait(0.6)
    end
    steps[#steps + 1] = function()
      if isOpen(first) or not isOpen(second) then
        return fail("cycle " .. c .. ": closing one editor changed the other")
      end
      show(second, 2)
      wait(0.3)
    end
    steps[#steps + 1] = function()
      report("STEP cycle " .. c)
      wait(0.1)
    end
  end
end

steps[#steps + 1] = function()
  for i = 1, #tracks do
    if isOpen(i) then return fail("editor " .. i .. " still open at the end") end
  end
  for i = #tracks, 1, -1 do
    reaper.DeleteTrack(tracks[i])
  end
  report("STEP instances deleted")
  wait(1)
end

steps[#steps + 1] = function()
  local projectPath = os.getenv("VOLUM_SMOKE_PROJECT") or ""
  reaper.Main_SaveProjectEx(0, projectPath, 0)
  reaper.Main_openProject("noprompt:" .. projectPath)
  wait(1)
end

steps[#steps + 1] = function()
  reaper.Main_SaveProject(0, false)
  report("PASS")
  quit()
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

if resultPath == "" then return end
wait(2)
reaper.defer(tick)
