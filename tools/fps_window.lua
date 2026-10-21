-- fps_window.lua : measure game-frame rate in successive windows to localize any
-- slowdown. Boot to game, hold RIGHT, print gfc delta per 300-emu-frame window.
local LEVELW=0xFF07EE; local GFC=0xFF0076; local GAME=0xFF0844
local mem
local function rd16(a) return mem:read_u16(a) end
local function rd32(a) return mem:read_u32(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start"}) do setbtn(n,0) end end
local fr=0; local ig0; local wbase; local wfr
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; release_all()
  local lw=rd16(LEVELW)
  if lw==0 then if (fr%80)<12 then setbtn("P1 Start",1) end return end
  if not ig0 then ig0=fr; wbase=rd32(GFC); wfr=fr end
  setbtn("P1 Right",1)
  if (fr-wfr)>=300 then
    local g=rd32(GFC); local d=g-wbase
    print(string.format("window rel%d..%d: gfc+%d/300 = %.3f",wfr-ig0,fr-ig0,d,d/300.0))
    wbase=g; wfr=fr
  end
  if fr>=ig0+3000 then manager.machine:exit() end
  if fr>=20000 then manager.machine:exit() end
end)
print("fps_window running")
