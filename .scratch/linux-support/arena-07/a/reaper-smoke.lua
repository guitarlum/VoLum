-- REAPER startup script for the ticket 07a gates (reaper-smoke.sh).
--   VOLUM_SMOKE_MODE=cycles: N instances, each editor opened and closed
--     VOLUM_SMOKE_CYCLES times, interleaved so one editor closes while the
--     other is open.
--   VOLUM_SMOKE_MODE=shot: one instance, editor open until the shell has taken
--     its screenshot and created VOLUM_SMOKE_SHOT_DONE.
-- Writes STEP lines, then PASS or FAIL <reason>, to VOLUM_SMOKE_RESULT.

local mode = os.getenv("VOLUM_SMOKE_MODE") or "cycles"
local fxName = os.getenv("VOLUM_SMOKE_FX") or "VST3:VoLum"
local cycles = tonumber(os.getenv("VOLUM_SMOKE_CYCLES") or "20") or 20
local instances = tonumber(os.getenv("VOLUM_SMOKE_INSTANCES") or "2") or 2
local resultPath = os.getenv("VOLUM_SMOKE_RESULT") or ""
local shotDone = os.getenv("VOLUM_SMOKE_SHOT_DONE") or ""

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
local waitFor = nil
local tracks = {}

local function wait(seconds) waitUntil = reaper.time_precise() + seconds end

local function quit() reaper.Main_OnCommand(40004, 0) end -- File: Quit

local function fail(reason)
  report("FAIL " .. reason)
  quit()
end

local function show(i, flag) reaper.TrackFX_Show(tracks[i], 0, flag) end -- 3 float, 2 close

local function isOpen(i) return reaper.TrackFX_GetOpen(tracks[i], 0) end

steps[#steps + 1] = function()
  local count = mode == "shot" and 1 or instances
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

if mode == "shot" then
  steps[#steps + 1] = function()
    show(1, 3)
    wait(1)
  end
  steps[#steps + 1] = function()
    if not isOpen(1) then
      return fail("editor did not open")
    end
    report("STEP shot-ready")
    waitFor = function() local f = io.open(shotDone, "r"); if f then f:close(); return true end return false end
  end
  steps[#steps + 1] = function()
    -- 1 when a key VoLum does not use (space) reached REAPER's transport.
    report("STEP playstate " .. reaper.GetPlayState())
    reaper.OnStopButton()
    show(1, 2)
    wait(1)
  end
else
  for c = 1, cycles do
    -- Odd cycles close the first editor while the second is open, even
    -- cycles the other way round.
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
    if isOpen(i) then
      return fail("editor " .. i .. " still open at the end")
    end
  end
  -- Destroy the instances while REAPER still runs its loop; with no plug-in
  -- left nothing can dirty the project between the save and File: Quit.
  for i = #tracks, 1, -1 do
    reaper.DeleteTrack(tracks[i])
  end
  report("STEP instances deleted")
  wait(1)
end

-- An untitled project always asks "Save unsaved project?" on File: Quit, so
-- it is saved, reopened from that file, and saved in place before quitting.
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
  if waitFor then
    if not waitFor() then
      reaper.defer(tick)
      return
    end
    waitFor = nil
  end
  if reaper.time_precise() >= waitUntil then
    local step = steps[stepIndex]
    if not step then
      return
    end
    stepIndex = stepIndex + 1
    step()
  end
  reaper.defer(tick)
end

if resultPath == "" then
  return
end
wait(2)
reaper.defer(tick)
