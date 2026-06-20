-- verify_homemenu.lua -- drive the native boot flow to the HOME title MENU and
-- snapshot each selection. New flow (no upfront config): selector -> pick HOME
-- (DOWN + START) -> home_attract menu (1 PLAYER / 2 PLAYERS / OPTIONS). We then
-- navigate DOWN through the rows and snapshot each highlight. NEVER press START
-- inside the menu (that would start a game).
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 A","P1 B","P1 C","P1 Up","P1 Down"}) do sb(n,0) end end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- selector: hold DOWN to move to the HOME row, then START to confirm.
  if fr>=700 and fr<712 then sb("P1 Down",1) end
  if fr>=770 and fr<782 then sb("P1 Start",1) end
  -- now in the home menu (sel=0). snapshot "1 PLAYER".
  if fr==1050 then manager.machine.video:snapshot() end
  -- DOWN -> sel=1, snapshot "2 PLAYERS".
  if fr>=1100 and fr<1112 then sb("P1 Down",1) end
  if fr==1200 then manager.machine.video:snapshot() end
  -- DOWN -> sel=2, snapshot "OPTIONS".
  if fr>=1250 and fr<1262 then sb("P1 Down",1) end
  if fr==1350 then manager.machine.video:snapshot() end
  if fr>=1420 then manager.machine:exit() end
end)
print("running")
