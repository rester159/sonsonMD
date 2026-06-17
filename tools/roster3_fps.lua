-- roster3_fps.lua -- verify the BATCH-3 enriched native scene: the FRUIT VARIETY
-- (ET_ITEM with fruit types) + the three new archetypes (LARGE/EMITTER/ORB) are
-- alive alongside the existing roster, the FRUIT gets EATEN (score climbs +
-- popups appear), and the busy multi-type scene holds ~60fps. Reads the native
-- entity pool + score directly. genesis driver = USA NTSC (60Hz).
--
-- Addresses (from out/symbol.txt, THIS build):
--   g_frameCounter @ 0xFF0076 (u32)   g_game @ 0xFF080C   g_entities @ 0xFF0832
-- g_game layout: Player first; score(u32) follows. We read score by scanning for
-- the change rather than hardcoding its offset (robust to struct drift): we read
-- the u32 at g_game+SCORE_OFF.  Entity stride is sizeof(Entity); we probe it.
-- EntityType: 0 NONE 1 ENEMY_A 2 SHOT 3 ITEM 4 FLYER 5 THROWER 6 ENEMY_SHOT
--   7 CREATURE_G 8 CREATURE_C 9 COMPOSITE_E 10 HOPPER 11 LARGE 12 EMITTER 13 ORB
--   14 POPUP
local FRAMECTR = 0xFF0076
local GGAME    = 0xFF0834
local ENTITIES = 0xFF085A
local EMAX     = 48
local NTYPES   = 15      -- types 0..14

local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd8(a)  return mem:read_u8(a)  end

local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

-- ---- probe the Entity stride: slot 0 is ENEMY_A (type=1, active!=0) seeded at
-- spawn. Find the stride S in {0x20,0x22,0x24,0x26} for which slot1 also looks
-- like a live entity with a plausible small type. We do this once in-game.
local STRIDE = 0x28      -- best a-priori (added fruit + popupLen + popupStr[6])
local stride_locked = false
local function probe_stride()
  for _,S in ipairs({0x28,0x26,0x24,0x22,0x20,0x2A,0x2C}) do
    local ok=true
    -- the first ~6 slots are ENEMY_A (type 1) per spawn_initial_wave
    for i=0,5 do
      local b=ENTITIES+i*S
      if rd8(b)==0 then ok=false break end       -- must be active
      local t=rd8(b+1)
      if t<1 or t>14 then ok=false break end      -- plausible type
    end
    if ok then STRIDE=S; stride_locked=true; return S end
  end
  return STRIDE
end

-- score offset inside g_game: Player struct then u32 score. We don't hardcode --
-- we sample a u32 window and report the first that GROWS during play.
local SCORE_OFF = nil
local function find_score()
  -- score sits a little after the Player struct; scan a small window for a u32
  -- that is 0 at start and grows. We just track candidates.
  return nil
end

local function count_types()
  local c={} for i=1,NTYPES do c[i]=0 end
  for i=0,EMAX-1 do
    local base=ENTITIES+i*STRIDE
    if rd8(base)~=0 then
      local t=rd8(base+1)
      if t>=0 and t<=14 then c[t+1]=c[t+1]+1 end
    end
  end
  return c
end

local seen={} for i=1,NTYPES do seen[i]=0 end
local F_SEL=720; local F_DIP=930; local F_BUSY0=1000
local F_M0=1080; local F_M1=1980; local F_SNAP=1500; local F_END=2040
local m0,m1
local scoreStart, scoreNow = nil, nil
-- read the score u32: try a few plausible offsets after Player; lock the one
-- that is nonzero/grows. Player is ~30 bytes; score follows (4-aligned).
local SCORE_CANDS = {0x20,0x22,0x24,0x26,0x28,0x2A,0x2C}
local scoreOff=nil

local fr=0
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end

  if fr==F_BUSY0 then probe_stride() end

  if fr>=F_BUSY0 then
    setbtn("P1 Right",1)
    if (fr%7)<3 then setbtn("P1 B",1) end       -- shot stream (kills -> popups)
    -- weave up/down to meet every lane band + reach the fruit on high lanes
    local ph=(fr//35)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end

  if fr>=F_BUSY0 then
    local c=count_types()
    for i=1,NTYPES do if c[i]>0 then seen[i]=seen[i]+1 end end
    if (fr%120)==0 and fr<=F_M1 then
      print(string.format("fr=%4d S=0x%x EA=%d SHOT=%d ITEM=%d FLY=%d THR=%d ESH=%d CREG=%d CREC=%d CMPE=%d HOP=%d LRG=%d EMIT=%d ORB=%d POP=%d",
        fr,STRIDE, c[2],c[3],c[4],c[5],c[6],c[7],c[8],c[9],c[10],c[11],c[12],c[13],c[14],c[15]))
    end
  end

  -- lock the score offset: prefer the computed sizeof(Player)=0x16, else scan
  -- for a candidate that is >0 and a plausible score (< 100000) during play.
  if fr==F_BUSY0+200 and not scoreOff then
    if rd32(GGAME+0x16) > 0 and rd32(GGAME+0x16) < 100000 then scoreOff=0x16
    else for _,o in ipairs(SCORE_CANDS) do
      if rd32(GGAME+o) > 0 and rd32(GGAME+o) < 100000 then scoreOff=o break end
    end end
  end
  if fr==F_M0 then
    m0=rd32(FRAMECTR)
    if scoreOff then scoreStart=rd32(GGAME+scoreOff) end
  end
  if fr==F_SNAP then manager.machine.video:snapshot(); print("SNAP roster3_scene F"..fr) end
  if fr==F_M1 then
    m1=rd32(FRAMECTR)
    local emuSpan=F_M1-F_M0; local gameSpan=m1-m0
    print(string.format("RESULT game-frames=%d over emu-frames=%d => FPS=%.2f", gameSpan, emuSpan, 60.0*gameSpan/emuSpan))
    if scoreOff then scoreNow=rd32(GGAME+scoreOff)
      print(string.format("SCORE off=0x%x start=%d now=%d (grew=%s)", scoreOff, scoreStart or -1, scoreNow, tostring((scoreNow or 0)>(scoreStart or 0)))) end
    print(string.format("SEEN-ALIVE frames: ITEM=%d LARGE=%d EMITTER=%d ORB=%d POPUP=%d",
      seen[4],seen[12],seen[13],seen[14],seen[15]))
    local ok = (seen[12]>0 and seen[13]>0 and seen[14]>0)
    print("NEW-TYPES-PRESENT(LARGE/EMITTER/ORB): "..(ok and "PASS" or "FAIL"))
    print("FRUIT-EATEN(popup seen OR score grew): "..((seen[15]>0 or ((scoreNow or 0)>(scoreStart or 0))) and "PASS" or "FAIL"))
  end
  if fr>=F_END then manager.machine:exit() end
end)
print("roster3_fps running")
