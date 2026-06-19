-- verify_bullet.lua -- native: get in-game, fire, snapshot a frame with a live
-- player ET_SHOT on screen, to confirm the bullet now renders as the projectile
-- (not the enemy-walk character). genesis driver, NTSC.
local GLEVELW=0xFF0072
local GGAME=0xFF0808
local ENTITIES=0xFF0834
local EMAX=48; local STRIDE=0x2A
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r8(a) return mem:read_u8(a) end
local function r16(a) return mem:read_u16(a) end
local function wr8(a,v) mem:write_u8(a,v) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_bullet.txt","w")
local fr=0; local shots=0; local snapped=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  local lw=r16(GLEVELW)
  if lw==0 then if (fr%16)<8 then setbtn("P1 Start",1) end return end
  wr8(0xFF0826,5)                       -- keep lives topped
  setbtn("P1 Right",1)
  if (fr%20)<2 then setbtn("P1 B",1) end   -- fire
  -- find a live ET_SHOT (type==2) that is on-screen
  local shotX=nil
  for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE
    if r8(b)~=0 and r8(b+1)==2 then shotX=i end
  end
  if shotX then
    shots=shots+1
    if snapped<3 and (fr%7)==0 then
      manager.machine.video:snapshot()
      f:write(string.format("fr=%d SNAP with ET_SHOT live (slot %d)\n",fr,shotX)); f:flush()
      snapped=snapped+1
    end
  end
  if fr>=1600 or snapped>=3 then
    f:write(string.format("done fr=%d shotsSeen=%d snaps=%d\n",fr,shots,snapped))
    f:close(); manager.machine:exit()
  end
end)
print("verify_bullet running")
