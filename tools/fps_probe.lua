-- fps_probe.lua -- minimal busy-scene FPS probe (drive selector->DIP->game, hold
-- RIGHT + mash B + weave lanes, measure game-frames via g_frameCounter over a
-- fixed emu-frame window). Address passed via env FPS_FRAMECTR (hex) or default.
local FRAMECTR = tonumber(os.getenv("FPS_FRAMECTR") or "0xFF0076")
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end
local F_SEL=720; local F_DIP=930; local F_BUSY0=1000
local F_M0=1080; local F_M1=1980; local F_END=2010
local m0,m1; local fr=0
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_BUSY0 then
    setbtn("P1 Right",1)
    if (fr%7)<3 then setbtn("P1 B",1) end
    local ph=(fr//35)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end
  if fr==F_M0 then m0=rd32(FRAMECTR) end
  if fr==F_M1 then m1=rd32(FRAMECTR)
    print(string.format("FPS-RESULT game-frames=%d emu-frames=%d => FPS=%.2f",
      m1-m0, F_M1-F_M0, 60.0*(m1-m0)/(F_M1-F_M0))) end
  if fr>=F_END then manager.machine:exit() end
end)
print("fps_probe running ctr=0x"..string.format("%X",FRAMECTR))
