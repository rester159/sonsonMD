local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 A","P1 B","P1 C","P1 Up","P1 Down"}) do sb(n,0) end end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- selector: move DOWN to HOME, then START; then START to confirm config -> HOME title
  if fr>=700 and fr<706 then sb("P1 Down",1) end          -- select HOME row
  if fr>=760 and fr<766 then sb("P1 Start",1) end          -- confirm HOME
  if fr>=980 and fr<986 then sb("P1 Start",1) end          -- confirm config
  if fr==1500 or fr==1700 then manager.machine.video:snapshot() end
  if fr>=1800 then manager.machine:exit() end
end)
print("running")
