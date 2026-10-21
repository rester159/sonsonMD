-- re_soak.lua : soak the SESSION RE-ENTRY path. Let the player die (no lives-top)
-- -> game-over -> selector; pulse START to start a fresh game; repeat many cycles.
-- Verifies each re-entry starts clean (gfc resumes, score resets to 0) and that the
-- selector always recovers (no stuck-menu / crash / corruption) over many sessions.
local GFC=0xFF007A; local LEVELW=0xFF0072; local SCORE=0xFF0832; local IDX=0xFF0075
local mem; local em=0; local lastgfc=-1; local stall=0; local reentries=0; local maxstall=0
local in_run=false; local run_start_gfc=0; local bad={}
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function relall() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end
local function rd32(a) return mem:read_u32(a) end
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1; relall()
  local gfc=rd32(GFC); local lw=mem:read_u16(LEVELW)
  -- track gfc stall (boot OR selector/game-over = not running game_update)
  if gfc==lastgfc then stall=stall+1 else
    if stall>=30 then     -- resumed after a stall => a game (re)started
      reentries=reentries+1
      print(string.format("GAME START #%d  em=%d  gfc=%d score=%d idx=%d",
            reentries, em, gfc, rd32(SCORE), mem:read_u8(IDX)))
      if reentries>=2 and rd32(SCORE)>50 then bad[#bad+1]="re-entry #"..reentries.." score not reset (="..rd32(SCORE)..")" end
    end
    stall=0
  end
  if stall>maxstall then maxstall=stall end
  lastgfc=gfc
  -- pulse START while at a menu (boot, or stalled at selector/game-over); idle in-game (let him die)
  if lw==0 or stall>=20 then if (em%40)<12 then pad("P1 Start",1) end end
  -- a permanent stall (stuck menu / crash) = re-entry failure
  if stall>=900 then bad[#bad+1]="PERMANENT STALL ~"..stall.."f at em="..em.." (stuck menu / crash?)"; print(bad[#bad]); manager.machine:exit() end
  if em>=22000 then
    print(string.format("RE-ENTRY SOAK DONE: %d game starts over %d frames; max menu-stall=%df; bad=%d", reentries, em, maxstall, #bad))
    for _,b in ipairs(bad) do print("  BAD: "..b) end
    manager.machine:exit()
  end
end)
print("re_soak.lua running")
