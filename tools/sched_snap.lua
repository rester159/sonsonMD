-- sched_snap.lua -- capture a faithful BUSY scene (scheduled wave enemies marching
-- in + the player engaging) for the report, on two different levels (rounds).
-- Native genesis NTSC. Holds a lane, runs right, fires realistically.
local GGAME=0xFF0838; local ROUND_OFF=0x1E
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd8(a) return mem:read_u8(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local fr=0
local snapped1, snapped2 = false, false
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=720 and fr<728 then setbtn("P1 Start",1) end
  if fr>=930 and fr<938 then setbtn("P1 Start",1) end
  if fr>=1010 then
    setbtn("P1 Right",1)
    if (fr%8)==0 then setbtn("P1 B",1) end
  end
  -- round-1 busy snapshot once a few enemies are mid-screen
  if not snapped1 and fr==1340 then manager.machine.video:snapshot(); snapped1=true; print("SNAP L1 F1340") end
  -- a second snapshot deeper in (likely round 1 still, busy churn)
  if not snapped2 and fr==2200 then manager.machine.video:snapshot(); snapped2=true; print("SNAP busy F2200 round="..rd8(GGAME+ROUND_OFF)) end
  if fr>=2400 then manager.machine:exit() end
end)
print("sched_snap running")
