-- verify_8b_scroll.lua : ARCADE branch, then HOLD RIGHT in-game to prove the
-- confirm-into-game path lands in the LIVE scrolling 60fps loop (camera moves),
-- not a frozen frame. Snapshots an early in-game frame and a later one after
-- the camera has scrolled.
local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start"}) do setbtn(n, 0) end
end
local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  release_all()
  if fr >= 720 and fr < 728 then setbtn("P1 Start", 1) end   -- confirm selector (ARCADE)
  if fr >= 930 and fr < 938 then setbtn("P1 Start", 1) end   -- confirm DIP -> game
  if fr >= 960 then setbtn("P1 Right", 1) end                -- run right in-game
  if fr == 980  then manager.machine.video:snapshot(); print("SNAP game_early F"..fr) end
  if fr == 1300 then manager.machine.video:snapshot(); print("SNAP game_late  F"..fr) end
  if fr >= 1330 then manager.machine:exit() end
end)
print("verify_8b_scroll running")
