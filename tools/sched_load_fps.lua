-- sched_load_fps.lua -- ISOLATE the FPS cost of the FAITHFUL scheduled enemy load
-- vs the test harness's shot stream, in the native port (genesis NTSC).
--
-- Three measured phases, all in round 1, each a 240-emu-frame deterministic FPS
-- window (g_frameCounter), logging the on-screen census during each:
--   A "enemies only"   : run RIGHT, NO firing -> pure scheduled-enemy load (the
--                        faithful worst case: max scheduled walkers marching in).
--   B "faithful fire"  : run RIGHT + fire one shot per ~10 frames (a realistic
--                        human mash, ~6/s) -> scheduled enemies + a few shots.
--   C "spam fire"      : run RIGHT + fire 3-of-5 frames (the unrealistic stream)
--                        -> shows the VDP per-scanline cost of MANY shot sprites.
-- Reports FPS + max(total sprites) + max(scheduled enemies) per phase, so we can
-- attribute any sub-60 to the faithful enemies vs the (non-faithful) shot spam.
local FRAMECTR = 0xFF0076
local GGAME    = 0xFF0838
local ENTITIES = 0xFF085E
local EMAX, STRIDE = 48, 0x2A
local OFF_TYPE, OFF_FROMSCHED = 1, 0x1C
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a)  return mem:read_u8(a)  end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/sched_load.txt","w")
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
local F_SEL=720; local F_DIP=930
-- phase schedule (emu frames): warmup then 3 measured windows
local phases = {
  {name="A_enemies_only", t0=1100, t1=1340, fire="none"},
  {name="B_faithful_fire",t0=1500, t1=1740, fire="slow"},
  {name="C_spam_fire",    t0=1900, t1=2140, fire="spam"},
}
local cur=nil
local mGame, maxTotal, maxSched = nil, 0, 0
local results={}

emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end

  -- pick active phase
  local p=nil
  for _,ph in ipairs(phases) do if fr>=ph.t0 and fr<ph.t1 then p=ph break end end

  -- always run right between/within phases so the scroll keeps spawning waves
  if fr>=1050 then setbtn("P1 Right",1) end

  if p then
    if p.fire=="slow" and (fr%10)==0 then setbtn("P1 B",1)
    elseif p.fire=="spam" and (fr%5)<3 then setbtn("P1 B",1) end
    -- window bookkeeping
    if cur~=p.name then
      cur=p.name; mGame=nil; maxTotal=0; maxSched=0
    end
    if fr==p.t0+1 then mGame=rd32(FRAMECTR) end
    local s,t=census()
    if t>maxTotal then maxTotal=t end
    if s>maxSched then maxSched=s end
    if fr==p.t1-1 and mGame then
      local span=(p.t1-1)-(p.t0+1)
      local gspan=rd32(FRAMECTR)-mGame
      results[#results+1]=string.format("%-16s FPS=%.2f  maxTotalSprites=%d  maxScheduledEnemies=%d",
        p.name, 60.0*gspan/span, maxTotal, maxSched)
    end
  else
    cur=nil
  end

  if fr==1200 then manager.machine.video:snapshot() end  -- A: enemies-only busy
  if fr==2000 then manager.machine.video:snapshot() end  -- C: spam

  if fr>=2300 then
    f:write("=== PHASE FPS (native genesis NTSC, round 1) ===\n")
    for _,r in ipairs(results) do f:write(r.."\n") end
    f:close(); manager.machine:exit()
  end
end)
print("sched_load_fps running")
