-- snap_tiers.lua : snapshot the native hero standing on the TOP, MIDDLE, BOTTOM
-- tiers (within the pre-freeze window) to visually confirm he stands ON the
-- platforms (no float). Boots to game, climbs to the top, snaps; climbs to middle,
-- snaps; climbs to bottom, snaps -- each only after vstate==RUN (seated).
local GAME=0xFF0844; local PY=GAME+2; local PVST=GAME+12; local PTIER=GAME+13
local LEVELW=0xFF07EE
local mem
local function rd8(a) return mem:read_u8(a) end
local function rd16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start"}) do setbtn(n,0) end end
local fr=0; local ig0; local stage=0; local snapped={}
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; release_all()
  local lw=rd16(LEVELW)
  if lw==0 then if (fr%80)<12 then setbtn("P1 Start",1) end return end
  if not ig0 then ig0=fr end
  local rel=fr-ig0
  local ti=rd8(PTIER); local vst=rd8(PVST); local y=rd16(PY)
  -- climb to top (UP) until tier 5, snap; then DOWN to tier 2, snap; DOWN to 0, snap.
  if stage==0 then
    setbtn("P1 Up",1)
    if ti==5 and vst==0 and not snapped[5] then
      print(string.format("rel%d TOP tier=5 Y=%d",rel,y)); manager.machine.video:snapshot(); snapped[5]=true; stage=1
    end
  elseif stage==1 then
    if ti>2 then setbtn("P1 Down",1)
    elseif ti==2 and vst==0 and not snapped[2] then
      print(string.format("rel%d MIDDLE tier=2 Y=%d",rel,y)); manager.machine.video:snapshot(); snapped[2]=true; stage=2
    end
  elseif stage==2 then
    if ti>0 then setbtn("P1 Down",1)
    elseif ti==0 and vst==0 and not snapped[0] then
      print(string.format("rel%d BOTTOM tier=0 Y=%d",rel,y)); manager.machine.video:snapshot(); snapped[0]=true; stage=3
    end
  else
    manager.machine:exit()
  end
  if rel>=850 then print("done(window)"); manager.machine:exit() end
end)
print("snap_tiers running")
