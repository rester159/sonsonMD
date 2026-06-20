local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end end
local fr=0; local rep=false
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- selector: pick HOME (down + start)
  if fr>=700 and fr<712 then p1("P1 Down",1) end
  if fr>=770 and fr<782 then p1("P1 Start",1) end
  -- home menu: sel stays 0 = "1 PLAYER"; just select with A
  if fr>=900 and fr<914 then p1("P1 A",1) end
  -- 1P game: drive + fire, report numplayers
  if fr>=1100 then p1("P1 Right",1); if (fr%10)<4 then p1("P1 B",1) end; if (fr%200)<30 then p1("P1 Up",1) end end
  if fr==1250 and not rep then rep=true; print("g_numPlayers="..mem:read_u8(0xFF0000)) end
  if fr==1400 then manager.machine.video:snapshot() end
  if fr==1800 then manager.machine.video:snapshot() end
  if fr>=1860 then manager.machine:exit() end
end)
print("running")
