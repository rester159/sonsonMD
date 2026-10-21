-- verify_lobcap.lua : confirm enemy lobs are capped at 4 (arcade $0DA8 pool) +
-- enemies still fire + 60fps. Play (run right + fire to survive + climb); count
-- max simultaneous ET_ENEMY_SHOT (type=6); report fps.
local GFC=0xFF0076; local LEVELW=0xFF078A; local ENTS=0xFF080C; local STRIDE=0x2A; local EMAX=48
local mem; local em=0; local ig; local gfc0; local maxlob=0; local sawlob=0
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1; rel()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (em%80)<12 then pad("P1 Start",1) end; return end
  if not ig then ig=em; gfc0=mem:read_u32(GFC) end
  local t=em-ig
  pad("P1 Right",1)
  if (t%90)<8 then pad("P1 B",1) end          -- occasional fire to survive
  if (t%200)<40 then pad("P1 Up",1) end        -- climb around
  local n=0
  for i=0,EMAX-1 do local b=ENTS+i*STRIDE; if mem:read_u8(b)~=0 and mem:read_u8(b+1)==6 then n=n+1 end end
  if n>maxlob then maxlob=n end
  if n>0 then sawlob=sawlob+1 end
  if (t%120)==0 and t>0 then print(string.format("t=%d lobs=%d gfc=%d", t, n, mem:read_u32(GFC))) end
  if t>=520 then
    local gf=mem:read_u32(GFC)-gfc0
    print(string.format("RESULT: MAX simultaneous enemy lobs=%d (cap=4); frames-with-a-lob=%d (enemies fire); fps=%.3f (1.000=60)", maxlob, sawlob, gf/t))
    manager.machine:exit()
  end
end)
print("verify_lobcap.lua running")
