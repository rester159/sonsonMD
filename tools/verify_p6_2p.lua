local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end end
local fr=0; local reported=false
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- selector: pick HOME (down + start)
  if fr>=700 and fr<712 then p1("P1 Down",1) end
  if fr>=770 and fr<782 then p1("P1 Start",1) end
  -- home menu: DOWN to "2 PLAYERS" (sel 0->1), then A to select
  if fr>=900 and fr<914 then p1("P1 Down",1) end
  if fr>=980 and fr<994 then p1("P1 A",1) end
  -- in 2P game now: report g_numPlayers, drive both a little for the HUD
  if fr>=1150 then p1("P1 Right",1); if (fr%10)<4 then p1("P1 B",1) end end
  if fr==1250 and not reported then reported=true; print("g_numPlayers="..mem:read_u8(0xFF0000)) end
  if fr==1400 then manager.machine.video:snapshot() end
  if fr>=1460 then manager.machine:exit() end
end)
print("running")
