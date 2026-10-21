-- play -> die (run right into enemies, no fire) -> game over -> back to selector;
-- confirm no crash + g_frameCounter stops advancing after we leave the game, and
-- the selector is reachable again. Also snapshot the returned selector.
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local FC=0xFF0074
local fr=0; local last=-1; local samples={}
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr>=980 and fr<988 then setbtn("P1 Start",1) end       -- selector -> arcade
  if fr>=1180 and fr<1188 then setbtn("P1 Start",1) end     -- dip -> game
  -- die fast: run right into the enemy wave repeatedly, never fire
  if fr>=1300 and fr<2600 then setbtn("P1 Right",1) end
  if fr%200==0 then emu.print_info(string.format("fr=%d gframe=%d", fr, rd32(FC))) end
  -- after a while, try to re-confirm selector (in case we returned)
  if fr>=3000 and fr<3008 then setbtn("P1 Start",1) end
  if fr==2900 then manager.machine.video:snapshot(); emu.print_info("SNAP post-gameover fr2900") end
  if fr>=3200 then manager.machine:exit() end
end)
emu.print_info("reentry running")
