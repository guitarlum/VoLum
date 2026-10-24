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
