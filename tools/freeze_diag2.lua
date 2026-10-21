-- freeze_diag2.lua : disambiguate the ~17s "freeze".
-- The hung PC (0xD1FE while(vtimer==t)) is the FRONT-END vblank-wait pattern. So
-- either (A) a REAL V-int freeze (vtimer frozen, CPU truly stuck), or (B) the idle
-- player died -> GAME OVER -> back to the selector (g_frameCounter naturally stops
-- because we left game_update, but vtimer keeps ticking + it's just the menu).
-- Decisive test: when g_frameCounter stalls, is vtimer (0xFF0064) still advancing?
local GFC    = 0xFF0076   -- g_frameCounter (u32, +1/game_update)
local VTIMER = 0xFF0064   -- SGDK vtimer (u32, +1/V-int)
local LEVELW = 0xFF078A   -- g_levelW (u16)
local GAME   = 0xFF07E0   -- g_game
local cpu, mem
local em = 0
local last_gfc, stall = -1, 0
local phase = 0
local t0
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"]; cpu=manager.machine.devices[":maincpu"] end
  em = em + 1
  pad("P1 Start",0)
  local lw = mem:read_u16(LEVELW)
  if lw == 0 then if (em%80)<12 then pad("P1 Start",1) end; return end

  local gfc = mem:read_u32(GFC)
  if phase == 0 then
    if gfc == last_gfc then stall = stall + 1 else stall = 0; last_gfc = gfc end
    if stall >= 30 then
      phase = 1; t0 = em
      print(string.format("*** g_frameCounter STALLED at %d (em=%d). Now watching vtimer + state...", gfc, em))
      -- dump g_game state bytes for context (flow/round/lives somewhere in here)
      local s = {}
      for off=24,44 do s[#s+1]=string.format("+%d=%d", off, mem:read_u8(GAME+off)) end
      print("  g_game[+24..+44]: "..table.concat(s," "))
    end
  elseif phase == 1 then
    -- log vtimer + gfc over a window
    if ((em - t0) % 12) == 0 then
      print(string.format("  em=%d  vtimer=%d  g_frameCounter=%d  g_levelW=%d", em, mem:read_u32(VTIMER), gfc, lw))
    end
    if em - t0 >= 96 then
      local v1 = mem:read_u32(VTIMER)
      -- one more sample to confirm
      local ok,pc = pcall(function() return cpu.state["PC"].value end)
      print(string.format("VERDICT: over %d emu-frames after the g_frameCounter stall, vtimer went (see above); final vtimer=%d, gfc=%d, PC=%06X",
            em - t0, v1, gfc, ok and pc or 0))
      manager.machine:exit()
    end
  end
  if em > 3500 and phase == 0 then print("NO STALL by em=3500"); manager.machine:exit() end
end)
print("freeze_diag2.lua running")
