local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end end
local fr=0; local n=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr>=700 and fr<712 then p1("P1 Down",1) end
  if fr>=770 and fr<782 then p1("P1 Start",1) end
  if fr>=900 and fr<914 then p1("P1 A",1) end
  -- keep the player mid-field: drift (no L/R) so enemies appear on both sides
  if fr>=1080 then if (fr%30)<5 then p1("P1 B",1) end end
  if fr>=1300 and (fr%40)==0 and n<4 then manager.machine.video:snapshot(); n=n+1 end
  if fr>=1500 then manager.machine:exit() end
end)
print("running")
