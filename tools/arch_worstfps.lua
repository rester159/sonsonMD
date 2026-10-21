-- arch_worstfps.lua : measure the NATIVE port's WORST-CASE deterministic FPS at
-- the busiest FAITHFUL load. Proven pattern (sched_verify_fps): coin/start, then
-- hold RIGHT (continuous scroll -> continuous spawning) + realistic fire + lane
-- weave. Round 1 scrolls the full wide level continuously (the busy + scrolling
-- worst case). Measure FPS via g_frameCounter (one tick per game_update = one per
-- V-blank frame; a dropped frame makes it advance slower), reporting the WORST 1s
-- sub-window that is BOTH busy (>=8 total sprites) AND scrolling (camera moving),
-- plus the overall busy-window average. No game RAM is written.
--
-- Symbols (out/symbol.txt): g_frameCounter@0xFF0076, g_game@0xFF0840
--   round u8 @+0x1E, cameraX s16 @+0x20.  g_entities@0xFF0866 stride 0x2A.
local FRAMECTR = 0xFF0076
local GGAME    = 0xFF0840
local ENTITIES = 0xFF0866
local EMAX, STRIDE = 48, 0x2A
local OFF_FROMSCHED, ROUND_OFF, CAM_OFF = 0x1C, 0x1E, 0x20
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a)  return mem:read_u8(a) end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arch_worstfps.txt","w")
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function census()
  local sched,total=0,0
  for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE
    if rd8(b)~=0 then total=total+1; if rd8(b+OFF_FROMSCHED)~=0 then sched=sched+1 end end
  end
  return sched,total
end

local fr=0
local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local winG0, winF0 = nil, nil
local subG, subF = nil, nil
local worstSub, worstAt, worstTot = 999, 0, 0
local busyFrames, busyG0, busyGcnt = 0, nil, 0
local maxTotal, maxSched = 0, 0
local prevCam = -1
local SUB = 60

emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then
    setbtn("P1 Right",1)                       -- continuous forward scroll
    if (fr%8)==0 then setbtn("P1 B",1) end      -- realistic fire ~7.5/s
    local ph=(fr//40)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end

    local cam = rds16(GGAME+CAM_OFF)
    local sched,total = census()
    if total>maxTotal then maxTotal=total end
    if sched>maxSched then maxSched=sched end

    if fr==F_PLAY+60 then subG=rd32(FRAMECTR); subF=fr; prevCam=cam; winG0=rd32(FRAMECTR); winF0=fr end
    if subG and (fr%SUB)==0 then
      local g=rd32(FRAMECTR)
      local s=60.0*(g-subG)/(fr-subF)
      if total>=8 and cam~=prevCam then         -- busy AND scrolling
        if s<worstSub then worstSub=s; worstAt=fr; worstTot=total end
        busyGcnt = busyGcnt + (g-subG)          -- accumulate busy+scroll game-frames
        busyFrames = busyFrames + (fr-subF)
      end
      prevCam=cam; subG=g; subF=fr
    end
    if (fr%180)==0 and subG then
      f:write(string.format("fr=%d round=%d cam=%d sched=%d total=%d worstSub=%.2f@%d(tot%d)\n",
        fr, rd8(GGAME+ROUND_OFF), cam, sched, total, worstSub, worstAt, worstTot))
      f:flush()
    end
  end
  if fr==1500 then manager.machine.video:snapshot() end
  if fr==1700 then manager.machine.video:snapshot() end
  if fr==5200 then manager.machine.video:snapshot() end
  if fr>=7000 then
    f:write("\n=== WORST-CASE FAITHFUL FPS (hold RIGHT, realistic fire, lane weave) ===\n")
    f:write(string.format("busy+scrolling frames sampled = %d ; avg FPS over them = %.3f\n",
      busyFrames, busyFrames>0 and 60.0*busyGcnt/busyFrames or 0))
    f:write(string.format("WORST 1s busy+scrolling sub-window FPS = %.3f (at fr=%d, total=%d sprites)\n",
      worstSub, worstAt, worstTot))
    f:write(string.format("max total sprites = %d ; max scheduled bodies = %d\n", maxTotal, maxSched))
    f:close(); manager.machine:exit()
  end
end)
print("arch_worstfps running")
