local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function p2(n,v) local p=manager.machine.ioport.ports[":ctrl2:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right"}) do p1(n,0) end; for _,n in ipairs({"P2 Up","P2 Down","P2 Left","P2 Right"}) do p2(n,0) end end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr<1000 then mem:write_u8(0xFF0000,2) end
  if (fr>=720 and fr<732) or (fr>=820 and fr<832) or (fr>=920 and fr<932) then p1("P1 Start",1) end
  -- separate them so both are clearly visible: P1 left, P2 right
  if fr>=1080 and fr<1200 then p1("P1 Left",1); p2("P2 Right",1) end
  if fr==1210 then manager.machine.video:snapshot() end
  if fr==1400 then manager.machine.video:snapshot() end
  if fr>=1450 then manager.machine:exit() end
end)
print("running")
