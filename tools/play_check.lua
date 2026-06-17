-- play_check.lua : drive a realistic play session (run right + climb up/down +
-- fire) on the current build and snapshot, to assess gameplay after the terrain-
-- collision rewrite and spot the next behavior gap vs the arcade. No life-poke
-- (short capture); if the hero dies we see the death/respawn too.
local LEVELW=0xFF078A
local mem; local em=0; local ig
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1; rel()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (em%80)<12 then pad("P1 Start",1) end; return end
  if not ig then ig=em end
  local t=em-ig
  pad("P1 Right",1)                                   -- always running right
  if (t%200) < 45 then pad("P1 Up",1) end             -- climb-up bursts
  if (t%200) >= 110 and (t%200) < 150 then pad("P1 Down",1) end  -- climb-down bursts
  if (t%48) < 6 then pad("P1 B",1); pad("P1 C",1); pad("P1 A",1) end  -- fire taps
  if t==150 or t==380 or t==620 then manager.machine.video:snapshot(); print("SNAP play t="..t.." em="..em) end
  if t>=650 then manager.machine:exit() end
end)
print("play_check.lua running")
