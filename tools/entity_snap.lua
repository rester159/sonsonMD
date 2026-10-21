-- entity_snap.lua : capture a POPULATED combat scene for the Phase-4 deliverable.
-- ARCADE -> game; climb up a couple of lanes so enemies share the player's row,
-- hold RIGHT (scroll + enemies marching in), mash B (shot stream). Snapshot when
-- the most enemies are on-screen.
local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end
local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  release_all()
  if fr >= 720 and fr < 728 then setbtn("P1 Start",1) end   -- confirm selector
  if fr >= 930 and fr < 938 then setbtn("P1 Start",1) end   -- confirm DIP -> game
  -- in-game: hold RIGHT; pulse B for a shot stream
  if fr >= 980 then
    setbtn("P1 Right",1)
    if (fr%6)<2 then setbtn("P1 B",1) end
  end
  -- a few snapshots across the early combat window to catch peak density
  if fr == 1040 or fr == 1080 or fr == 1120 then
    manager.machine.video:snapshot(); print("SNAP combat F"..fr)
  end
  if fr >= 1140 then manager.machine:exit() end
end)
print("entity_snap.lua running")
