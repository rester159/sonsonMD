-- freeze_diag3.lua : is the "~17s freeze" actually GAME-OVER -> selector?
-- vtimer keeps ticking (V-int alive) + g_frameCounter froze => we left game_update.
-- Test: snapshot at the stall; then press START -- if a fresh game starts
-- (g_frameCounter resumes), it was the responsive selector after the idle player
-- died (NOT a freeze). Snapshot what's on screen so we can SEE it.
local GFC=0xFF0076; local VTIMER=0xFF0064; local LEVELW=0xFF078A
local mem; local em=0; local last_gfc,stall=-1,0; local phase=0; local t0; local gfc_stall
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() pad("P1 Start",0); pad("P1 A",0) end

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1; rel()
  local lw=mem:read_u16(LEVELW); local gfc=mem:read_u32(GFC)

  if phase==0 then
    -- boot into the first game
    if lw==0 then if (em%80)<12 then pad("P1 Start",1) end; return end
    if gfc==last_gfc then stall=stall+1 else stall=0; last_gfc=gfc end
    if stall>=30 then
      phase=1; t0=em; gfc_stall=gfc
      print(string.format("STALL at gfc=%d em=%d lw=%d -- snapshotting the screen", gfc, em, lw))
      manager.machine.video:snapshot()       -- what's on screen at the "freeze"?
    end
  elseif phase==1 then
    -- after the stall: try to navigate (press START) to see if it RECOVERS
    if (em-t0) > 20 and (em-t0) % 40 < 12 then pad("P1 Start",1) end
    if (em-t0) == 200 then
      local resumed = (gfc > gfc_stall + 5)
      print(string.format("AFTER-INPUT: gfc=%d (was %d) vtimer=%d lw=%d  -> %s",
            gfc, gfc_stall, mem:read_u32(VTIMER), lw,
            resumed and "RECOVERED (responsive selector -> NOT a freeze; idle player died->game over)"
                      or "NO RECOVERY (real hang)"))
      manager.machine.video:snapshot()        -- screen after pressing START
      manager.machine:exit()
    end
  end
  if em>3500 and phase==0 then print("no stall by 3500"); manager.machine:exit() end
end)
print("freeze_diag3.lua running")
