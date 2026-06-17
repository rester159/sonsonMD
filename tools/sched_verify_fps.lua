-- sched_verify_fps.lua -- verify the FAITHFUL per-level wave scheduler in the
-- NATIVE port (mame genesis -cart out/rom.bin, USA NTSC 60Hz):
--   (1) the scheduler spawns wave enemies as the camera scrolls, and the
--       SCHEDULED on-screen count stays under SPAWN_ONSCREEN_CAP (=12) -- no
--       unbounded right-edge pile-up (the old recycler bug);
--   (2) measure deterministic FPS (g_frameCounter over a fixed emu-frame window)
--       during the BUSIEST sustained moment (max scheduled enemies + player fire),
--       and report per-"level" (round) by riding the scroll to advance rounds.
--
-- Addresses from out/symbol.txt (THIS build):
--   g_frameCounter @ 0xFF0076 (u32)
--   g_game         @ 0xFF0838     (Player[0x16], score u32 @+0x16, round u8 @+0x1E,
--                                  cameraX s16 @+0x20)
--   g_entities     @ 0xFF085E     stride 0x2A; active@+0, type@+1, x s16@+2,
--                                  fromSched u8 @+0x1C
-- EntityType: 1 ENEMY_A 8 CREATURE_C 9 COMPOSITE_E (the scheduler's three).
local FRAMECTR = 0xFF0076
local GGAME    = 0xFF0838
local ENTITIES = 0xFF085E
local EMAX     = 48
local STRIDE   = 0x2A
local OFF_FROMSCHED = 0x1C
local OFF_TYPE = 1
local SCORE_OFF = 0x16
local ROUND_OFF = 0x1E
local CAM_OFF   = 0x20

local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd8(a)  return mem:read_u8(a)  end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end

local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/sched_verify.txt","w")

local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

-- count scheduled wave enemies + total active entities on-screen
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

-- per-round (level) worst-case tracking
local roundMaxSched = {}     -- round -> max scheduled on screen
local roundFpsM0    = {}     -- round -> (emuFrame, gameFrame) at window start
local roundFps      = {}     -- round -> measured fps over its window

local fr=0
local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local prevRound=nil
local winLen=180             -- per-round FPS window (3s of emu frames)

emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end

  if fr>=F_PLAY then
    -- BUSIEST sustained load: hold RIGHT (constant scroll -> constant spawning) +
    -- a heavy fire stream (player shots) + weave lanes so enemies of all rows are
    -- engaged. This is the faithful worst case: max scheduled enemies + firing.
    setbtn("P1 Right",1)
    if (fr%8)==0 then setbtn("P1 B",1) end          -- REALISTIC fire (~7.5/s), not spam
    local ph=(fr//40)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end

  if fr>=F_PLAY then
    local rnd = rd8(GGAME+ROUND_OFF)
    local sched,total,byType = census()
    if (roundMaxSched[rnd] or -1) < sched then roundMaxSched[rnd]=sched end

    -- start a fresh FPS window at each new round (skip the first ~30 frames of a
    -- round so the camera+spawns settle into steady busy load)
    if rnd ~= prevRound then
      roundFpsM0[rnd] = {fr+150, nil}   -- arm; let post-transition shots drain first
      prevRound = rnd
    end
    if roundFpsM0[rnd] and roundFpsM0[rnd][2]==nil and fr>=roundFpsM0[rnd][1] then
      roundFpsM0[rnd] = {fr, rd32(FRAMECTR)}        -- window start
    elseif roundFpsM0[rnd] and roundFpsM0[rnd][2] and roundFps[rnd]==nil
           and fr >= roundFpsM0[rnd][1]+winLen then
      local gspan = rd32(FRAMECTR) - roundFpsM0[rnd][2]
      roundFps[rnd] = 60.0*gspan/winLen
    end

    if (fr%60)==0 then
      local ts={}
      for t,n in pairs(byType) do ts[#ts+1]=string.format("t%d=%d",t,n) end
      f:write(string.format("fr=%d round=%d cam=%d sched=%d total=%d [%s] fps_sofar=%s\n",
        fr, rnd, rds16(GGAME+CAM_OFF), sched, total, table.concat(ts," "),
        roundFps[rnd] and string.format("%.2f",roundFps[rnd]) or "..."))
      f:flush()
    end
  end

  if fr==1500 then manager.machine.video:snapshot() ; f:write("SNAP busy F1500\n") end
  if fr==3200 then manager.machine.video:snapshot() ; f:write("SNAP busy F3200\n") end

  if fr>=5200 then
    f:write("\n=== PER-ROUND (LEVEL) WORST-CASE ===\n")
    local rk={}; for k,_ in pairs(roundMaxSched) do rk[#rk+1]=k end; table.sort(rk)
    for _,r in ipairs(rk) do
      f:write(string.format("round %d : maxSchedOnScreen=%d  busyFPS=%s\n",
        r, roundMaxSched[r], roundFps[r] and string.format("%.2f",roundFps[r]) or "n/a"))
    end
    f:write(string.format("\nSCORE final=%d\n", rd32(GGAME+SCORE_OFF)))
    f:close(); manager.machine:exit()
  end
end)
print("sched_verify_fps running")
