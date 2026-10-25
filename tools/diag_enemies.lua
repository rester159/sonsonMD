local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end end
local fr=0; local shots=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- 1P game: selector HOME + start
  if fr>=700 and fr<712 then p1("P1 Down",1) end
  if fr>=770 and fr<782 then p1("P1 Start",1) end
  if fr>=900 and fr<914 then p1("P1 A",1) end
  -- advance to provoke enemies; occasional fire
  if fr>=1080 then p1("P1 Right",1); if (fr%24)<4 then p1("P1 B",1) end end
  -- burst capture during combat: 8 frames, 24 apart (~0.4s)
  if fr>=1400 and fr<=1400+24*7 and ((fr-1400)%24)==0 and shots<8 then
    manager.machine.video:snapshot(); shots=shots+1
  end
  if fr>=1620 then manager.machine:exit() end
end)
print("running")
