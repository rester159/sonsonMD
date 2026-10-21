-- probe_playerY.lua : boot to gameplay, read player Y + an enemy Y, snapshot.
-- Player struct is first in GameState (g_game). Player.x@+0 (s16), Player.y@+2 (s16).
-- g_entities: Entity stride 0x28; +2 x(s16) +4 y(s16) +0 active +1 type.
local LEVELW = 0xFF07EE
local GAME   = 0xFF0840
local ENT    = 0xFF0868
local ENT_STRIDE = 0x28
local mem
local function rd8(a) return mem:read_u8(a) end
local function rd16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local fr=0
local ingame
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1
  release_all()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (fr%80)<12 then setbtn("P1 Start",1) end return end
  if not ingame then ingame=fr end
  -- let it settle, then sample at a few points; no input (let hero idle on start lane)
  if fr==ingame+30 or fr==ingame+90 then
    local px,py=rd16(GAME+0),rd16(GAME+2)
    io.write(string.format("F%d player x=%d y=%d\n",fr,px,py))
    for i=0,7 do
      local b=ENT+i*ENT_STRIDE
      local act=rd8(b); local typ=rd8(b+1)
      if act~=0 then io.write(string.format("  ent%d type=%d x=%d y=%d\n",i,typ,rd16(b+2),rd16(b+4))) end
    end
    manager.machine.video:snapshot()
  end
  if fr>=ingame+120 then manager.machine:exit() end
  if fr>=4000 then manager.machine:exit() end
end)
print("probe_playerY running")
