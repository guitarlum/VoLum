-- REAPER startup script driven by smoke-reaper-mac.sh. Inserts VoLum, opens and
-- closes its editor, plays, saves, reopens the project and quits. Writes PASS
-- or FAIL <reason> to $VOLUM_SMOKE_RESULT; a crash or hang writes nothing.

local fxName = os.getenv("VOLUM_SMOKE_FX") or ""
local cycles = tonumber(os.getenv("VOLUM_SMOKE_CYCLES") or "3") or 3
local resultPath = os.getenv("VOLUM_SMOKE_RESULT") or ""
local projectPath = os.getenv("VOLUM_SMOKE_PROJECT") or ""

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
local track, fx

local function wait(seconds)
  waitUntil = reaper.time_precise() + seconds
end

local function fail(reason)
  report("FAIL " .. reason)
  reaper.Main_OnCommand(40004, 0) -- File: Quit
end

local function findVoLum()
  track = reaper.GetTrack(0, 0)
  if not track then
    return false
  end
  fx = reaper.TrackFX_AddByName(track, fxName, false, 0)
  return fx >= 0
end

steps[#steps + 1] = function()
  reaper.InsertTrackAtIndex(0, true)
  track = reaper.GetTrack(0, 0)
  fx = reaper.TrackFX_AddByName(track, fxName, false, -1)
  if fx < 0 then
    local known = {}
    local i = 0
    while true do
      local ok, name = reaper.EnumInstalledFX(i)
      if not ok then
        break
      end
      if name:find("VoLum", 1, true) then
        known[#known + 1] = name
      end
      i = i + 1
    end
    return fail("could not insert " .. fxName .. "; REAPER lists: " .. table.concat(known, " | "))
  end
  report("STEP inserted " .. fxName)
  wait(1)
end

for i = 1, cycles do
  steps[#steps + 1] = function()
    reaper.TrackFX_Show(track, fx, 3)
    report("STEP editor open " .. i)
    wait(4)
  end
  steps[#steps + 1] = function()
    reaper.TrackFX_Show(track, fx, 2)
    report("STEP editor closed " .. i)
    wait(1)
  end
end

steps[#steps + 1] = function()
  reaper.OnPlayButton()
  report("STEP playing")
  wait(3)
end

steps[#steps + 1] = function()
  reaper.OnStopButton()
  reaper.Main_SaveProjectEx(0, projectPath, 0)
  report("STEP saved")
  wait(1)
end

steps[#steps + 1] = function()
  reaper.Main_openProject("noprompt:" .. projectPath)
  report("STEP reopened")
  wait(2)
end

steps[#steps + 1] = function()
  if not findVoLum() then
    return fail("VoLum missing after reopen")
  end
  reaper.TrackFX_Show(track, fx, 3)
  report("STEP editor open after reopen")
  wait(4)
end

steps[#steps + 1] = function()
  reaper.TrackFX_Show(track, fx, 2)
  reaper.Main_SaveProject(0, false)
  report("PASS")
  reaper.Main_OnCommand(40004, 0) -- File: Quit
end

local function tick()
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

if fxName == "" or resultPath == "" or projectPath == "" then
  return
end
wait(2)
reaper.defer(tick)
