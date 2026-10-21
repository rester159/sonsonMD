local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function p2(n,v) local p=manager.machine.ioport.ports[":ctrl2:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel()
  for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end
  for _,n in ipairs({"P2 Up","P2 Down","P2 Left","P2 Right","P2 A","P2 B","P2 C"}) do p2(n,0) end
end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr<1000 then mem:write_u8(0xFF0000,2) end          -- g_numPlayers = 2
  if (fr>=720 and fr<732) or (fr>=820 and fr<832) or (fr>=920 and fr<932) then p1("P1 Start",1) end
  -- in-game: P1 advances + FIRES (scores kills); P2 advances but never fires
  if fr>=1080 then
    p1("P1 Right",1); if (fr%10)<4 then p1("P1 B",1) end  -- P1 shoots
    p2("P2 Right",1)                                       -- P2 just walks
    if (fr%240)<40 then p1("P1 Up",1); p2("P2 Down",1) end -- weave tiers, hit enemies
  end
  if fr==1500 then manager.machine.video:snapshot() end
  if fr==1900 then manager.machine.video:snapshot() end
  if fr==2300 then manager.machine.video:snapshot() end
  if fr>=2360 then manager.machine:exit() end
end)
print("running")
