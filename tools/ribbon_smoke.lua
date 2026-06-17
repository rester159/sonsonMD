-- ribbon_smoke.lua : hold RIGHT, log every g_levelIdx change (segment under camera).
-- Proves the SEAMLESS RIBBON advances 0->1->...->7 and LOOPS back to 0, and reports
-- the g_frameCounter ratio over the whole held-RIGHT window (1.000 = full 60fps).
-- Symbols THIS build (out/symbol.txt):
local LEVELW  = 0xFF0072   -- volatile u16: 0 in front-end, ribbon width in-game
local LEVELIDX= 0xFF0075   -- u8: segment (baked level 0..7) under the camera
local GFC     = 0xFF007A   -- u32: g_frameCounter (+1 per game frame)
local GAME    = 0xFF07DC   -- g_game base
local mem; local fr=0; local ig; local gfc0
local prevSeg=-1; local segChanges=0; local seenLoop=false; local seq={}
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end
local LIVES = GAME + 32   -- u8 lives (confirmed by calib2 dump)
local ROUND = GAME + 36
local STAGE = GAME + 37
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; rel()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (fr%80)<12 then pad("P1 Start",1) end; return end
  if not ig then ig=fr; gfc0=mem:read_u32(GFC); print(string.format("INGAME f%d ribbonW=%d", fr, lw)) end
  if mem:read_u8(LIVES) < 5 then mem:write_u8(LIVES,5) end   -- survive the traversal
  pad("P1 Right",1)
  local seg=mem:read_u8(LEVELIDX)
  if seg ~= prevSeg then
    segChanges = segChanges + 1
    seq[#seq+1]=seg
    print(string.format("f%-6d SEG %d -> %d (change #%d)", fr, prevSeg, seg, segChanges))
    if prevSeg==7 and seg==0 then seenLoop=true; print("  >>> RIBBON LOOPED (7->0) <<<") end
    prevSeg=seg
  end
  if (seenLoop and segChanges>=9) or fr >= ig+40000 then
    local g=mem:read_u32(GFC)-gfc0; local e=fr-ig
    print(string.format("DONE %d emu-frames in-game, gfc+%d => %.4f game-frames/emu-frame", e, g, g/e))
    print("SEG sequence: "..table.concat(seq,","))
    print(string.format("seg-changes=%d looped(7->0)=%s final round=%d stage=%d",
      segChanges, tostring(seenLoop), mem:read_u8(ROUND), mem:read_u8(STAGE)))
    manager.machine:exit()
  end
  if fr>=60000 then print("HARD TIMEOUT"); manager.machine:exit() end
end)
print("ribbon_smoke running")
