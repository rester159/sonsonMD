-- verify_8b.lua : exercise the Phase-8b config screens in MAME (genesis/NTSC).
--
-- Boot flow: SEGA splash -> CAPCOM logo -> selector (ARCADE/HOME) -> config
-- screen (dip_screen_run / options_run) -> game (FPS 60). The splashes block a
-- few seconds each, so the timeline is generous. Pass the branch via an env
-- var BRANCH = "arcade" | "home" (default arcade). Snapshots land in
-- snap/genesis/NNNN.png; we rename-print so the caller can collect them.
--
-- Buttons on the Genesis 3-button pad port (:ctrl1:mdpad:PAD): the selector
-- confirms on ANY button; the config screens move on Up/Down, change on
-- Left/Right, confirm on Start. We give long settle windows because of the
-- 20-frame input settle + 3-read debounce in the front-end.

local branch = os.getenv("BRANCH") or "arcade"

local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end

local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start",
                      "P1 A","P1 B","P1 C"}) do setbtn(n, 0) end
end

-- a press = hold the button for `dur` frames starting at frame `at`
local presses = {}
local function press(at, name, dur) presses[#presses+1] = {at=at, name=name, off=at+(dur or 6)} end

-- Timeline (frames @60fps). Splashes: SEGA ~ a few s, CAPCOM ~6 s. Be generous.
local F_SEL   = 720    -- by here the selector is up (>12 s in)
local F_CFG   = 780    -- after confirming the selector, config screen is up
local F_SNAP1 = 840    -- snapshot the config screen
local F_CHG   = 870    -- change a couple of values (visible in a 2nd snap)
local F_SNAP2 = 900
local F_GO    = 930    -- confirm config -> enter game
local F_SNAP3 = 1140   -- in-game, steady; snapshot + read FPS
local F_END   = 1170

if branch == "home" then
  -- selector starts on ARCADE; move down to HOME, then confirm.
  press(F_SEL,      "P1 Down", 8)
  press(F_SEL + 30, "P1 Start", 8)   -- confirm selector -> options_run
else
  -- ARCADE is the default highlight; just confirm.
  press(F_SEL,      "P1 Start", 8)   -- confirm selector -> dip_screen_run
end

-- on the config screen: nudge a value (Down to a row, Right to change it)
press(F_CHG,      "P1 Down",  6)
press(F_CHG + 12, "P1 Right", 6)
press(F_GO,       "P1 Start", 8)     -- confirm config -> game

local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  release_all()
  for _, p in ipairs(presses) do
    if fr >= p.at and fr < p.off then setbtn(p.name, 1) end
  end
  if fr == F_SNAP1 then manager.machine.video:snapshot(); print("SNAP cfg     F"..fr) end
  if fr == F_SNAP2 then manager.machine.video:snapshot(); print("SNAP cfg2    F"..fr) end
  if fr == F_SNAP3 then
    manager.machine.video:snapshot()
    local fps = 0
    pcall(function() fps = manager.machine.video.frameskip end)
    print("SNAP game    F"..fr.."  speed="..tostring(manager.machine.video.speed_factor))
  end
  if fr >= F_END then manager.machine:exit() end
end)

print("verify_8b.lua running, branch="..branch)
