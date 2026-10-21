-- wave_sched_verify.lua -- verify the NEW arcade $FCD9 wave-schedule driver
-- (mame genesis -cart out/rom.bin, USA NTSC 60Hz). Confirms:
--   (1) enemy TYPES spawned follow the $FCD9 mask schedule, keyed by wave-id =
--       cameraX*87/ribbonW (the native re-expression of arcade $90&0x7F);
--   (2) the scheduled on-screen count stays under SPAWN_ONSCREEN_CAP (=10);
--   (3) deterministic FPS (g_frameCounter ratio) stays ~1.000 under busy load.
--
-- Addresses from out/symbol.txt (THIS build, md5 re-derive each build):
--   g_frameCounter @ 0xFF007C (u32)
--   g_levelW       @ 0xFF0072 (u16; 0 = front-end, else ribbon pixel width)
--   g_game         @ 0xFF0808 (Player=0x1A: score u32@+0x1A, lives u8@+0x1E,
--                              round u8@+0x24, stage u8@+0x25, cameraX s16@+0x26)
--   g_entities     @ 0xFF0834 (stride 0x2A; active@+0, type@+1, x s16@+2,
--                              fromSched u8@+0x1C)
local FRAMECTR = 0xFF007C
local GLEVELW  = 0xFF0072
local GGAME    = 0xFF0808
local ENTITIES = 0xFF0834
local EMAX     = 48
local STRIDE   = 0x2A
local OFF_TYPE = 1
local OFF_FROMSCHED = 0x1C
local SCORE_OFF = 0x1A
local LIVES_OFF = 0x1E
local ROUND_OFF = 0x24
local CAM_OFF   = 0x26
local WAVE_COUNT = 87

local TYPENAME = { [1]="ENEMY_A",[2]="SHOT",[3]="ITEM",[4]="FLYER",[5]="THROWER",
  [6]="ENEMY_SHOT",[7]="CREATURE_G",[8]="CREATURE_C",[9]="COMPOSITE_E",
  [10]="HOPPER",[11]="LARGE",[12]="EMITTER",[13]="ORB",[14]="POPUP" }

local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd8(a)  return mem:read_u8(a)  end
local function wr8(a,v) mem:write_u8(a,v) end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end

local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/wave_sched_verify.txt","w")

local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

-- per-wave-id accumulators: which scheduled TYPES were seen live while in that wave
local perWave = {}          -- waveId -> { type -> count-of-frames-seen }
local globalHist = {}       -- type -> frames-seen (any scheduled body)
local visited = {}          -- waveId -> true
local maxSched = 0
local maxTotal = 0
local camMin, camMax = 1e9, -1e9

local function census(wid)
  local sched, total = 0, 0
  perWave[wid] = perWave[wid] or {}
  for i=0,EMAX-1 do
    local b=ENTITIES+i*STRIDE
    if rd8(b)~=0 then
      total=total+1
      if rd8(b+OFF_FROMSCHED)~=0 then
        sched=sched+1
        local t=rd8(b+OFF_TYPE)
        perWave[wid][t]=(perWave[wid][t] or 0)+1
        globalHist[t]=(globalHist[t] or 0)+1
      end
    end
  end
  if sched>maxSched then maxSched=sched end
  if total>maxTotal then maxTotal=total end
  return sched,total
end

local fr=0
-- FPS measurement window (busy steady-state)
local fpsM0_fr, fpsM0_g, fpsRatio = nil, nil, nil
local FPS_START = 2000     -- begin window after we're well into play
local FPS_LEN   = 600      -- 10s window

local END_FR = 16000       -- enough emu-frames to sweep the full ribbon (>=1 loop)

emu.register_frame_done(function()
  fr=fr+1; release_all()
  local lw = rd16(GLEVELW)

  if lw==0 then
    -- front-end: pulse Start to advance splash->selector->dip/options->game
    if (fr % 16) < 8 then setbtn("P1 Start",1) end
    return
  end

  -- in-game: keep lives topped so a death never ends the run (camera keeps
  -- scrolling through death/respawn anyway); drive a busy forward sweep.
  wr8(GGAME+LIVES_OFF, 5)
  setbtn("P1 Right",1)
  if (fr%8)==0 then setbtn("P1 B",1) end       -- realistic fire stream
  local ph=(fr//40)%4
  if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end

  local cam = rds16(GGAME+CAM_OFF)
  if cam<camMin then camMin=cam end
  if cam>camMax then camMax=cam end
  local wid = 0
  if lw>0 then wid = math.floor(cam*WAVE_COUNT/lw); if wid>=WAVE_COUNT then wid=WAVE_COUNT-1 end; if wid<0 then wid=0 end end
  visited[wid]=true

  local sched,total = census(wid)

  -- FPS window
  if fr==FPS_START then fpsM0_fr=fr; fpsM0_g=rd32(FRAMECTR) end
  if fpsM0_g and fpsRatio==nil and fr>=FPS_START+FPS_LEN then
    local gspan = rd32(FRAMECTR)-fpsM0_g
    fpsRatio = gspan/FPS_LEN
  end

  if (fr%120)==0 then
    f:write(string.format("fr=%d cam=%d wid=%d round=%d sched=%d total=%d\n",
      fr, cam, wid, rd8(GGAME+ROUND_OFF), sched, total))
    f:flush()
  end

  if fr==2500 then manager.machine.video:snapshot(); f:write("SNAP busy F2500\n") end
  if fr==8000 then manager.machine.video:snapshot(); f:write("SNAP busy F8000\n") end

  if fr>=END_FR then
    f:write("\n=== WAVE-ID -> SCHEDULED TYPES SEEN (frames-live per type) ===\n")
    local nVisited=0
    for w=0,WAVE_COUNT-1 do
      if perWave[w] then
        nVisited=nVisited+1
        local parts={}
        for t,n in pairs(perWave[w]) do parts[#parts+1]=string.format("%s:%d",(TYPENAME[t] or ("t"..t)),n) end
        table.sort(parts)
        f:write(string.format("wave %2d : %s\n", w, table.concat(parts,"  ")))
      end
    end
    f:write(string.format("\nwaves visited: %d / %d\n", nVisited, WAVE_COUNT))
    f:write("\n=== GLOBAL SCHEDULED-TYPE HISTOGRAM (frames-live) ===\n")
    local tk={}; for t,_ in pairs(globalHist) do tk[#tk+1]=t end; table.sort(tk)
    for _,t in ipairs(tk) do
      f:write(string.format("  %-12s = %d\n", (TYPENAME[t] or ("t"..t)), globalHist[t]))
    end
    f:write(string.format("\ncam range: %d .. %d  (ribbonW=%d)\n", camMin, camMax, rd16(GLEVELW)))
    f:write(string.format("maxSchedOnScreen=%d (cap=10)   maxTotalEntities=%d\n", maxSched, maxTotal))
    f:write(string.format("FPS ratio (busy %d-frame window) = %s\n",
      FPS_LEN, fpsRatio and string.format("%.4f",fpsRatio) or "n/a"))
    f:write(string.format("SCORE final=%d\n", rd32(GGAME+SCORE_OFF)))
    f:close(); manager.machine:exit()
  end
end)
print("wave_sched_verify running")
