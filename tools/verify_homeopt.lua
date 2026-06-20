-- verify_homeopt.lua -- HOME menu -> OPTIONS round-trip. Navigate to OPTIONS,
-- select it (enter the H40 options screen), confirm options (START), and verify
-- we land back on the H32 home menu. Snapshots: the options screen, then the
-- restored home menu.
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 A","P1 B","P1 C","P1 Up","P1 Down"}) do sb(n,0) end end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr>=700 and fr<712 then sb("P1 Down",1) end          -- selector: HOME row
  if fr>=770 and fr<782 then sb("P1 Start",1) end          -- confirm HOME
  -- home menu: DOWN x2 -> OPTIONS
  if fr>=1100 and fr<1112 then sb("P1 Down",1) end
  if fr>=1160 and fr<1172 then sb("P1 Down",1) end
  if fr==1250 then manager.machine.video:snapshot() end     -- OPTIONS highlighted
  if fr>=1300 and fr<1312 then sb("P1 A",1) end             -- select OPTIONS
  if fr==1450 then manager.machine.video:snapshot() end     -- options screen (H40)
  if fr>=1550 and fr<1562 then sb("P1 Start",1) end         -- confirm options
  if fr==1750 then manager.machine.video:snapshot() end     -- back on home menu (H32)
  if fr>=1820 then manager.machine:exit() end
end)
print("running")
