-- entity_types_fps.lua -- verify the ENRICHED native scene: confirm the new
-- archetypes (FLYER/THROWER/ENEMY_SHOT) are alive alongside ENEMY_A + SHOT, AND
-- that the busy 3-enemy-type scene holds 60fps. Reads the native pool directly.
--
-- Addresses (from out/symbol.txt, this build):
--   g_frameCounter @ 0xFF0074 (u32)   g_entities @ 0xFF080C
-- Entity struct (m68k, 0x20 stride): +0 active(u8) +1 type(u8).
--   EntityType: 0 NONE 1 ENEMY_A 2 SHOT 3 ITEM 4 FLYER 5 THROWER 6 ENEMY_SHOT
-- genesis driver = USA NTSC (60Hz).
local FRAMECTR  = 0xFF0074
local ENTITIES  = 0xFF080C
local EMAX      = 24
local ESTRIDE   = 0x20

local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a)  return mem:read_u8(a) end

local function count_types()
  local c={0,0,0,0,0,0,0}   -- index 1..7 for type 0..6
  for i=0,EMAX-1 do
    local base=ENTITIES+i*ESTRIDE
    if rd8(base)~=0 then
      local t=rd8(base+1)
      if t>=0 and t<=6 then c[t+1]=c[t+1]+1 end
    end
  end
  return c
end

local F_SEL=720; local F_DIP=930; local F_BUSY0=1000
local F_M0=1080; local F_M1=1380; local F_SNAP=1360; local F_END=1430
local m0,m1=nil,nil
local fr=0
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_BUSY0 then
    setbtn("P1 Right",1)
    if (fr%8)<3 then setbtn("P1 B",1) end
    -- weave so the player meets flyers (high) + throwers + ground enemies
    local ph=(fr//40)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end

  if (fr%40)==0 and fr>=F_BUSY0 and fr<=F_M1 then
    local c=count_types()
    print(string.format("fr=%4d types: ENEMY_A=%d SHOT=%d ITEM=%d FLYER=%d THROWER=%d ESHOT=%d",
          fr, c[2],c[3],c[4],c[5],c[6],c[7]))
  end

  if fr==F_M0 then m0=rd32(FRAMECTR); print("SAMPLE0 emuF="..fr.." gtick="..m0) end
  if fr==F_SNAP then manager.machine.video:snapshot(); print("SNAP multi_enemy_scene F"..fr) end
  if fr==F_M1 then
    m1=rd32(FRAMECTR)
    local emuSpan=F_M1-F_M0; local gameSpan=m1-m0
    print(string.format("RESULT game-frames=%d over emu-frames=%d => FPS=%.2f", gameSpan, emuSpan, 60.0*gameSpan/emuSpan))
  end
  if fr>=F_END then manager.machine:exit() end
end)
print("entity_types_fps running")
