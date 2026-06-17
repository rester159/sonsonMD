-- verify_regress.lua : confirm NO regression of horizontal scroll / auto-advance /
-- entities / 60fps after the vertical-collision rewrite.
-- Holds RIGHT through gameplay; checks g_levelW cycles (scenery advance+loop),
-- active entity count stays >0 (pool alive), and g_frameCounter/emu-frame ~1.000.
local LEVELW = 0xFF07EE
local GFC    = 0xFF0076
local GAME   = 0xFF0844
local PX     = GAME + 0
local ENT    = 0xFF0870
local ESTRIDE= 0x28
local mem
local function rd8(a) return mem:read_u8(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd32(a) return mem:read_u32(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function nactive()
  local n=0
  for i=0,47 do if rd8(ENT+i*ESTRIDE)~=0 then n=n+1 end end
  return n
end
local fr=0; local ig0; local gfc0; local prev_lw=-1; local advances=0; local maxEnt=0; local minEnt=999
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; release_all()
  local lw=rd16(LEVELW)
  if lw==0 then if (fr%80)<12 then setbtn("P1 Start",1) end return end
  if not ig0 then ig0=fr; gfc0=rd32(GFC); prev_lw=lw end
  -- survive: keep lives topped (lives offset uncertain post-shift; instead just run)
  setbtn("P1 Right",1)
  local na = (fr%30==0) and nactive() or minEnt   -- scan sparingly (lua cost)
  if fr%30==0 then
    if na>maxEnt then maxEnt=na end
    if na<minEnt then minEnt=na end
  end
  if lw~=prev_lw then advances=advances+1; print(string.format("F%d ADVANCE #%d g_levelW %d->%d ent=%d X=%d",fr,advances,prev_lw,lw,na,rd16(PX))); prev_lw=lw end
  if fr>=ig0+3000 then
    local gf=rd32(GFC)-gfc0; local ef=fr-ig0
    print(string.format("DONE: %d emu-frames, gfc+%d => %.3f game-frames/emu-frame",ef,gf,gf/ef))
    print(string.format("advances=%d  entities active range=%d..%d",advances,minEnt,maxEnt))
    manager.machine:exit()
  end
  if fr>=20000 then print("HARD TIMEOUT"); manager.machine:exit() end
end)
print("verify_regress running")
