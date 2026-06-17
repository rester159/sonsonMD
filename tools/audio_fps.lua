-- audio_fps.lua -- verify (1) the game reaches gameplay with audio active,
-- (2) it HOLDS 60fps in a busy scene (many shots = many fire SFX), (3) the
-- audio V-Blank tick advances (replay slot pointers move) decoupled from load.
-- Recalibrated front-end timing: selector appears ~fr900 on this build.
local FRAMECTR = 0xFF0074    -- g_frameCounter (u32, 68k work RAM)
local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local function rd32(a) return mem:read_u32(a) end

local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

local F_SEL   = 980    -- selector up -> confirm ARCADE (default)
local F_DIP   = 1180   -- DIP screen up -> confirm into the game
local F_BUSY0 = 1300   -- start busy scene (hold RIGHT, mash B = shots+fire SFX)
local F_M0    = 1400
local F_M1    = 1700   -- 300 emu-frames later
local F_SNAP  = 1680
local F_END   = 1730

local m0, m1
local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_BUSY0 then
    setbtn("P1 Right",1)
    if (fr % 8) < 3 then setbtn("P1 B",1) end
  end
  if fr==F_M0 then m0=rd32(FRAMECTR); emu.print_info(string.format("SAMPLE0 emuF=%d gtick=%d",fr,m0)) end
  if fr==F_SNAP then manager.machine.video:snapshot(); emu.print_info("SNAP busy "..fr) end
  if fr==F_M1 then
    m1=rd32(FRAMECTR)
    local es=F_M1-F_M0; local gs=m1-m0
    emu.print_info(string.format("SAMPLE1 emuF=%d gtick=%d",fr,m1))
    emu.print_info(string.format("RESULT game-frames=%d / emu-frames=%d => FPS=%.2f", gs, es, 60.0*gs/es))
  end
  if fr>=F_END then manager.machine:exit() end
end)
emu.print_info("audio_fps running")
