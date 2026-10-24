local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr>=700 and fr<712 then p1("P1 Down",1) end
  if fr>=770 and fr<782 then p1("P1 Start",1) end
  if fr>=900 and fr<914 then p1("P1 A",1) end
  -- hold LEFT, snapshot; then hold RIGHT, snapshot (isolate player facing)
  if fr>=1000 and fr<1120 then p1("P1 Left",1) end
  if fr==1110 then manager.machine.video:snapshot() end   -- player should face LEFT
  if fr>=1160 and fr<1300 then p1("P1 Right",1) end
  if fr==1290 then manager.machine.video:snapshot() end   -- player should face RIGHT
  if fr>=1340 then manager.machine:exit() end
end)
print("running")
