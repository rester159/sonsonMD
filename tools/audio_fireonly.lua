-- single isolated fire presses over the BGM; capture to WAV to detect SFX transients
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr>=980 and fr<988 then setbtn("P1 Start",1) end
  if fr>=1180 and fr<1188 then setbtn("P1 Start",1) end
  -- stand still (don't run into enemies); fire one shot every 120f (=2s)
  if fr>=1400 then
    local ph=(fr-1400)%120
    if ph<5 then setbtn("P1 B",1) end
  end
  if fr>=2360 then manager.machine:exit() end
end)
emu.print_info("fireonly running")
