-- roster_fps.lua -- verify the BATCH-2 ENRICHED native scene: confirm the four
-- new archetypes (CREATURE_G/CREATURE_C/COMPOSITE_E/HOPPER) are alive alongside
-- the existing roster, AND that the busy multi-type scene holds 60fps. Reads the
-- native entity pool directly.
--
-- Addresses (from out/symbol.txt, THIS build):
--   g_frameCounter @ 0xFF0076 (u32)   g_entities @ 0xFF081A
-- Entity struct (m68k, 0x20 stride): +0 active(u8) +1 type(u8) +3 y(s16 hi@+2?)
--   EntityType: 0 NONE 1 ENEMY_A 2 SHOT 3 ITEM 4 FLYER 5 THROWER 6 ENEMY_SHOT
--               7 CREATURE_G 8 CREATURE_C 9 COMPOSITE_E 10 HOPPER
-- genesis driver = USA NTSC (60Hz).
local FRAMECTR  = 0xFF0076
local ENTITIES  = 0xFF081A
local EMAX      = 32
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

local NTYPES=11   -- types 0..10
local function count_types()
  local c={} for i=1,NTYPES do c[i]=0 end
  for i=0,EMAX-1 do
    local base=ENTITIES+i*ESTRIDE
    if rd8(base)~=0 then
      local t=rd8(base+1)
      if t>=0 and t<=10 then c[t+1]=c[t+1]+1 end
    end
  end
  return c
end

-- track that each new type was SEEN alive at least once across the run
local seen={} for i=1,NTYPES do seen[i]=0 end

local F_SEL=720; local F_DIP=930; local F_BUSY0=1000
local F_M0=1080; local F_M1=1680; local F_SNAP=1500; local F_END=1740
local m0,m1=nil,nil
local minfps=999
local fr=0
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_BUSY0 then
    setbtn("P1 Right",1)
    if (fr%8)<3 then setbtn("P1 B",1) end
    -- weave up/down so the player meets every lane band (flyers high, hoppers/
    -- creatures low, throwers/composite mid)
    local ph=(fr//40)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end

  if fr>=F_BUSY0 then
    local c=count_types()
    for i=1,NTYPES do if c[i]>0 then seen[i]=seen[i]+1 end end
    if (fr%60)==0 and fr<=F_M1 then
      print(string.format("fr=%4d  EA=%d SHOT=%d ITEM=%d FLY=%d THR=%d ESH=%d  CREG=%d CREC=%d CMPE=%d HOP=%d",
            fr, c[2],c[3],c[4],c[5],c[6],c[7], c[8],c[9],c[10],c[11]))
    end
  end

  if fr==F_M0 then m0=rd32(FRAMECTR) end
  if fr==F_SNAP then manager.machine.video:snapshot(); print("SNAP roster_scene F"..fr) end
  if fr==F_M1 then
    m1=rd32(FRAMECTR)
    local emuSpan=F_M1-F_M0; local gameSpan=m1-m0
    print(string.format("RESULT game-frames=%d over emu-frames=%d => FPS=%.2f", gameSpan, emuSpan, 60.0*gameSpan/emuSpan))
    print(string.format("SEEN-ALIVE frames: CREATURE_G=%d CREATURE_C=%d COMPOSITE_E=%d HOPPER=%d",
          seen[8],seen[9],seen[10],seen[11]))
    local ok = (seen[8]>0 and seen[9]>0 and seen[10]>0 and seen[11]>0)
    print("NEW-TYPES-PRESENT: "..(ok and "PASS" or "FAIL"))
  end
  if fr>=F_END then manager.machine:exit() end
end)
print("roster_fps running")
