-- entity_fps.lua : headline Phase-4 verification -- prove the NATIVE entity
-- system holds 60fps in a BUSY scene (several enemies + a stream of player
-- shots), where the sibling transliteration dropped to ~48 (GF48).
--
-- Method: drive the front-end to the game (ARCADE branch: confirm selector,
-- confirm DIP), then in-game HOLD RIGHT (run + scroll, enemies marching in) and
-- MASH B (spawn shots) to maximise active entities. We read the ROM's
-- free-running game-loop tick g_frameCounter (@ 0xFF011E, u32) at two points a
-- known number of EMULATED 60Hz frames apart: if the game holds 60fps the tick
-- advances 1:1 with emulated frames; any slowdown shows as fewer game-ticks per
-- emulated second. We also report MAME's speed_factor and snapshot a populated
-- scene.
--
-- genesis driver = USA NTSC (60Hz) -- the right timebase for this measure.
-- Pad: :ctrl1:mdpad:PAD, fields "P1 Right" / "P1 B" / "P1 Start".

local FRAMECTR = 0xFF011E    -- g_frameCounter (u32, big-endian 68k work RAM)

local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do
    setbtn(n, 0)
  end
end

-- read a big-endian u32 from 68k program space
local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local function rd32(addr) return mem:read_u32(addr) end

-- Timeline (emulated frames @60). Front-end splashes block several seconds.
local F_SEL    = 720     -- selector up; ARCADE default -> confirm
local F_DIP    = 930     -- DIP up -> confirm into the game
local F_BUSY0  = 1000    -- start the busy scene (hold RIGHT, mash B)
local F_M0     = 1080    -- first frameCounter sample (scene settled)
local F_M1     = 1380    -- second sample, 300 emulated frames later
local F_SNAP   = 1360    -- snapshot the populated scene just before sample 2
local F_END    = 1410

local m0, m1 = nil, nil
local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  release_all()

  -- front-end nav
  if fr >= F_SEL and fr < F_SEL + 8 then setbtn("P1 Start", 1) end   -- confirm selector
  if fr >= F_DIP and fr < F_DIP + 8 then setbtn("P1 Start", 1) end   -- confirm DIP -> game

  -- in-game busy scene: hold RIGHT continuously; pulse B every 8 frames so the
  -- press-edge debounce fires a fresh shot each pulse (a steady shot stream).
  if fr >= F_BUSY0 then
    setbtn("P1 Right", 1)
    if (fr % 8) < 3 then setbtn("P1 B", 1) end
  end

  if fr == F_M0 then m0 = rd32(FRAMECTR); print(string.format("SAMPLE0 emuF=%d gtick=%d", fr, m0)) end
  if fr == F_SNAP then manager.machine.video:snapshot(); print("SNAP busy_scene F"..fr) end
  if fr == F_M1 then
    m1 = rd32(FRAMECTR)
    local emuSpan  = F_M1 - F_M0
    local gameSpan = m1 - m0
    local fps = 60.0 * gameSpan / emuSpan
    print(string.format("SAMPLE1 emuF=%d gtick=%d", fr, m1))
    print(string.format("RESULT  game-frames=%d over emu-frames=%d  => measured FPS=%.2f",
                         gameSpan, emuSpan, fps))
    local spd = 0; pcall(function() spd = manager.machine.video.speed_factor end)
    print("speed_factor="..tostring(spd))
  end
  if fr >= F_END then manager.machine:exit() end
end)
print("entity_fps.lua running (ARCADE -> busy scene FPS probe)")
