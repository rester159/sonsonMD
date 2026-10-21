-- verify_fall.lua : prove the player FALLS through a gap and lands on the tier below.
-- Boot to gameplay, climb to TIER 5 (top, which has gaps in level1: floor row7 = 10
-- holes), then hold RIGHT and watch for a FALL (vstate->3) + a landing (RUN on a
-- lower tier). Also confirms 60fps.
local GAME   = 0xFF0844
local PX     = GAME + 0
local PY     = GAME + 2
local PVST   = GAME + 12
local PTIER  = GAME + 13
local LEVELW = 0xFF07EE
local GFC    = 0xFF0076
local VNAME  = {[0]="RUN",[1]="CLMBUP",[2]="CLMBDN",[3]="FALL"}
local mem
local function rd8(a) return mem:read_u8(a) end
local function rd16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function rd32(a) return mem:read_u32(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local fr=0; local ig0; local gfc0; local last=""; local fell=false; local landed_after_fall=false; local was_fall=false; local snaps=0
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; release_all()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (fr%80)<12 then setbtn("P1 Start",1) end return end
  if not ig0 then ig0=fr; gfc0=rd32(GFC) end
  local rel=fr-ig0
  -- climb to top tier first (hold UP 0-120), then run RIGHT (120+) to cross gaps
  if rel<120 then setbtn("P1 Up",1) else setbtn("P1 Right",1) end
  local y=rd16(PY); local vst=rd8(PVST); local ti=rd8(PTIER)
  local vn=VNAME[vst] or ("?"..vst)
  if vst==3 then fell=true end
  if was_fall and vst==0 then landed_after_fall=true end
  was_fall=(vst==3)
  local s=string.format("Y=%d v=%s tier=%d",y,vn,ti)
  if s~=last then print(string.format("rel%-4d %s X=%d",rel,s,rd16(PX))); last=s
    if vst==3 and snaps<3 then manager.machine.video:snapshot(); snaps=snaps+1 end
  end
  if rel>=900 then
    local gf=rd32(GFC)-gfc0; local ef=fr-ig0
    print(string.format("DONE: %.3f game-frames/emu-frame  FELL=%s LANDED_AFTER_FALL=%s",gf/ef,tostring(fell),tostring(landed_after_fall)))
    manager.machine:exit()
  end
  if fr>=24000 then print("HARD TIMEOUT"); manager.machine:exit() end
end)
print("verify_fall running")
