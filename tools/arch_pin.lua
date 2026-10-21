-- arch_pin.lua : pinpoint WHERE the 59fps 1s-window occurs (round transition vs
-- steady dense scene). Logs every 1s window's fps + total sprites + cam delta +
-- round, for the whole pure-right run. No game RAM written.
local FRAMECTR=0xFF0076; local GGAME=0xFF0840; local ENTITIES=0xFF0866
local EMAX,STRIDE=48,0x2A; local OFF_FROMSCHED,ROUND_OFF,CAM_OFF=0x1C,0x1E,0x20
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a) return mem:read_u8(a) end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arch_pin.txt","w")
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function census() local sched,total=0,0; for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE; if rd8(b)~=0 then total=total+1; if rd8(b+OFF_FROMSCHED)~=0 then sched=sched+1 end end end; return sched,total end
local fr=0; local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local subG,subF,prevCam,prevRound=nil,nil,0,1
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then
    setbtn("P1 Right",1)
    if (fr%8)==0 then setbtn("P1 B",1) end
    local rnd=rd8(GGAME+ROUND_OFF); local cam=rds16(GGAME+CAM_OFF); local sched,total=census()
    if subG==nil then subG=rd32(FRAMECTR); subF=fr; prevCam=cam; prevRound=rnd end
    if (fr%60)==0 then
      local g=rd32(FRAMECTR); local s=60.0*(g-subG)/(fr-subF)
      local transition = (rnd~=prevRound) and "ROUND-CHANGE" or ""
      if s < 59.9 then
        f:write(string.format("fr=%d round=%d cam=%d camMoved=%s total=%d FPS=%.2f %s\n",
          fr, rnd, cam, tostring(cam~=prevCam), total, s, transition))
        f:flush()
      end
      prevCam=cam; subG=g; subF=fr; prevRound=rnd
    end
  end
  if fr>=9000 then f:write("=== done (only <59.9 windows logged) ===\n"); f:close(); manager.machine:exit() end
end)
print("arch_pin running")
