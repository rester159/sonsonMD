-- arch_verify.lua : verify the EXTENDED faithful scheduler in the NATIVE port
-- (mame genesis -cart out/rom.bin, USA NTSC 60Hz):
--   (1) the per-level program spawns the FULL enemy variety (census the native
--       EntityType mix during busy play -- expect ENEMY_A + HOPPER + CREATURE_G +
--       FLYER + THROWER + ORB + ENEMY_SHOT, not just walkers), and the SCHEDULED
--       on-screen count stays under SPAWN_ONSCREEN_CAP (=12);
--   (2) deterministic worst-case FPS (g_frameCounter over a fixed emu-frame window)
--       during the BUSIEST sustained moment (max scheduled enemies + realistic fire),
--       per round -- riding the scroll to advance rounds so we sample L1 / L2 / deep.
--
-- THIS build's symbols (out/symbol.txt):
--   g_frameCounter @ 0xFF0076 (u32)
--   g_game         @ 0xFF0840   (Player[..], round u8 @+0x1E, cameraX s16 @+0x20,
--                                score u32 @+0x16)
--   g_entities     @ 0xFF0866   stride 0x2A; active@+0, type@+1, fromSched u8 @+0x1C
-- EntityType: 1 ENEMY_A 2 SHOT 3 ITEM 4 FLYER 5 THROWER 6 ENEMY_SHOT 7 CREATURE_G
--             8 CREATURE_C 9 COMPOSITE_E 10 HOPPER 11 LARGE 12 EMITTER 13 ORB 14 POPUP
local FRAMECTR = 0xFF0076
local GGAME    = 0xFF0840
local ENTITIES = 0xFF0866
local EMAX, STRIDE = 48, 0x2A
local OFF_TYPE, OFF_FROMSCHED = 1, 0x1C
local SCORE_OFF, ROUND_OFF, CAM_OFF = 0x16, 0x1E, 0x20

local TYPENAME = {[1]="ENEMY_A",[2]="SHOT",[3]="ITEM",[4]="FLYER",[5]="THROWER",
  [6]="ESHOT",[7]="CREATURE_G",[8]="CREATURE_C",[9]="COMPOSITE_E",[10]="HOPPER",
  [11]="LARGE",[12]="EMITTER",[13]="ORB",[14]="POPUP"}

local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a)  return mem:read_u8(a)  end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arch_verify.txt","w")
local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

-- census the live pool: scheduled body count, total active, and per-type counts
local function census()
  local sched, total = 0, 0
  local byType = {}
  for i=0,EMAX-1 do
    local b=ENTITIES+i*STRIDE
    if rd8(b)~=0 then
      total=total+1
      local t=rd8(b+OFF_TYPE)
      byType[t]=(byType[t] or 0)+1
      if rd8(b+OFF_FROMSCHED)~=0 then sched=sched+1 end
    end
  end
  return sched, total, byType
end

-- aggregate which types EVER appeared, per round
local typesEverByRound = {}     -- round -> {typeId=true}
local roundMaxSched = {}        -- round -> max scheduled bodies on screen
local roundMaxTotal = {}        -- round -> max total sprites on screen
local roundFpsM0    = {}        -- round -> {emuFrameStart, gameFrameStart}
local roundFps      = {}        -- round -> measured fps over its window
local roundMinFps   = {}        -- round -> min per-60-frame fps (worst sub-window)

local fr=0
local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local prevRound=nil
local winLen=180

local function note_types(rnd, byType)
  typesEverByRound[rnd] = typesEverByRound[rnd] or {}
  for t,_ in pairs(byType) do typesEverByRound[rnd][t]=true end
end

local lastGameFr, lastEmuFr = nil, nil

emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end

  if fr>=F_PLAY then
    -- BUSIEST sustained load: hold RIGHT (constant scroll -> constant spawning) +
    -- a REALISTIC fire stream (~7.5/s) + weave lanes so all rows/archetypes engage.
    setbtn("P1 Right",1)
    if (fr%8)==0 then setbtn("P1 B",1) end
    local ph=(fr//40)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end

  if fr>=F_PLAY then
    local rnd = rd8(GGAME+ROUND_OFF)
    local sched,total,byType = census()
    note_types(rnd, byType)
    if (roundMaxSched[rnd] or -1) < sched then roundMaxSched[rnd]=sched end
    if (roundMaxTotal[rnd] or -1) < total then roundMaxTotal[rnd]=total end

    if rnd ~= prevRound then
      roundFpsM0[rnd] = {fr+150, nil}
      prevRound = rnd
      lastGameFr=nil
    end
    if roundFpsM0[rnd] and roundFpsM0[rnd][2]==nil and fr>=roundFpsM0[rnd][1] then
      roundFpsM0[rnd] = {fr, rd32(FRAMECTR)}
      lastGameFr = rd32(FRAMECTR); lastEmuFr = fr
    elseif roundFpsM0[rnd] and roundFpsM0[rnd][2] and roundFps[rnd]==nil
           and fr >= roundFpsM0[rnd][1]+winLen then
      local gspan = rd32(FRAMECTR) - roundFpsM0[rnd][2]
      roundFps[rnd] = 60.0*gspan/winLen
    end

    -- track the WORST 60-frame sub-window FPS per round (the true worst case)
    if lastGameFr and (fr % 60)==0 then
      local g = rd32(FRAMECTR)
      local sub = 60.0*(g-lastGameFr)/(fr-lastEmuFr)
      if roundMinFps[rnd]==nil or sub < roundMinFps[rnd] then roundMinFps[rnd]=sub end
      lastGameFr=g; lastEmuFr=fr
    end

    if (fr%60)==0 then
      local ts={}
      for t,n in pairs(byType) do ts[#ts+1]=string.format("%s=%d",TYPENAME[t] or ("?"..t),n) end
      table.sort(ts)
      f:write(string.format("fr=%d round=%d cam=%d sched=%d total=%d [%s]\n",
        fr, rnd, rds16(GGAME+CAM_OFF), sched, total, table.concat(ts," ")))
      f:flush()
    end
  end

  if fr==1600 then manager.machine.video:snapshot() ; f:write("SNAP busy F1600\n") end
  if fr==2600 then manager.machine.video:snapshot() ; f:write("SNAP busy F2600\n") end
  if fr==4200 then manager.machine.video:snapshot() ; f:write("SNAP busy F4200\n") end

  if fr>=6400 then
    f:write("\n=== PER-ROUND (LEVEL) ENEMY VARIETY + WORST-CASE FPS ===\n")
    local rk={}; for k,_ in pairs(roundMaxSched) do rk[#rk+1]=k end; table.sort(rk)
    for _,r in ipairs(rk) do
      local tl={}
      for t,_ in pairs(typesEverByRound[r] or {}) do tl[#tl+1]=TYPENAME[t] or ("?"..t) end
      table.sort(tl)
      f:write(string.format("round %d : maxSched=%d maxTotal=%d  windowFPS=%s  worst60FPS=%s\n   types: %s\n",
        r, roundMaxSched[r] or -1, roundMaxTotal[r] or -1,
        roundFps[r] and string.format("%.2f",roundFps[r]) or "n/a",
        roundMinFps[r] and string.format("%.2f",roundMinFps[r]) or "n/a",
        table.concat(tl,", ")))
    end
    f:write(string.format("\nSCORE final=%d\n", rd32(GGAME+SCORE_OFF)))
    f:close(); manager.machine:exit()
  end
end)
print("arch_verify running")
