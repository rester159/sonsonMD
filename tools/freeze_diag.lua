-- freeze_diag.lua : locate the ~15s in-gameplay FREEZE (reproduces idle, no input).
-- Detect when the game loop stalls (g_frameCounter stops advancing while emu frames
-- keep coming), then sample the 68k PC to find the hung routine. Map the top PCs to
-- out/symbol.txt afterward.
local GFC    = 0xFF0076   -- g_frameCounter (volatile u32, +1 per game_update)
local LEVELW = 0xFF078A   -- g_levelW (u16): nonzero == in gameplay
local cpu, mem
local em = 0
local last_gfc, stall = -1, 0
local frozen_at = nil
local samples, hist = 0, {}
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"]; cpu=manager.machine.devices[":maincpu"] end
  em = em + 1
  pad("P1 Start",0)
  local lw = mem:read_u16(LEVELW)
  if lw == 0 then if (em%80)<12 then pad("P1 Start",1) end; return end

  -- IN GAME, IDLE (no movement -- the freeze needs no input).
  local gfc = mem:read_u32(GFC)
  if not frozen_at then
    if gfc == last_gfc then stall = stall + 1 else stall = 0; last_gfc = gfc end
    if (em % 120) == 0 then print(string.format("em=%d gfc=%d (advancing)", em, gfc)) end
    if stall >= 30 then        -- 30 emu-frames, 0 game-frames => the loop hung
      frozen_at = em
      print(string.format("*** FROZEN: g_frameCounter STUCK at %d (em=%d)", gfc, em))
    end
  else
    local ok, pc = pcall(function() return cpu.state["PC"].value end)
    if not ok then ok, pc = pcall(function() return cpu.state["CURPC"].value end) end
    if ok and pc then hist[pc] = (hist[pc] or 0) + 1 end
    samples = samples + 1
    if samples >= 200 then
      local arr = {}
      for k,v in pairs(hist) do arr[#arr+1]={k,v} end
      table.sort(arr, function(a,b) return a[2]>b[2] end)
      print("=== 68k PC histogram during freeze (top 14) ===")
      for i=1,math.min(14,#arr) do print(string.format("  PC=%06X  x%d", arr[i][1], arr[i][2])) end
      manager.machine:exit()
    end
  end
  if em > 3500 and not frozen_at then print("NO FREEZE by em=3500 (gfc="..gfc..")"); manager.machine:exit() end
end)
print("freeze_diag.lua running")
