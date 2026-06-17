-- sched_kill_check.lua -- confirm the scheduler's wave enemies are KILLABLE and
-- gameplay scores (the faithful enemies engage + collide). Hold a single lane,
-- run RIGHT into the incoming wave, fire steadily, watch the score climb and the
-- scheduled-enemy count churn (spawn -> approach -> killed). Native genesis NTSC.
local FRAMECTR = 0xFF0076
local GGAME    = 0xFF0838
local ENTITIES = 0xFF085E
local EMAX, STRIDE = 48, 0x2A
local OFF_FROMSCHED, SCORE_OFF = 0x1C, 0x16
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a)  return mem:read_u8(a)  end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/sched_kill.txt","w")
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function sched_count() local s=0; for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE; if rd8(b)~=0 and rd8(b+OFF_FROMSCHED)~=0 then s=s+1 end end; return s end
local fr=0
local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local scoreAt=nil
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then
    -- Hold ONE lane (no weaving) and run right into the wave, firing steadily but
    -- realistically (every 8 frames). Enemies lane-home to the player -> they line
    -- up with the shots and die.
    setbtn("P1 Right",1)
    if (fr%8)==0 then setbtn("P1 B",1) end
  end
  if fr==F_PLAY+30 then scoreAt=rd32(GGAME+SCORE_OFF) end
  if fr>=F_PLAY and (fr%120)==0 then
    f:write(string.format("fr=%d score=%d sched=%d\n", fr, rd32(GGAME+SCORE_OFF), sched_count()))
    f:flush()
  end
  if fr>=2800 then
    local sc=rd32(GGAME+SCORE_OFF)
    f:write(string.format("\nscore start=%d final=%d  KILLS-HAPPENED=%s\n",
      scoreAt or -1, sc, tostring(sc > (scoreAt or 0))))
    f:close(); manager.machine:exit()
  end
end)
print("sched_kill_check running")
