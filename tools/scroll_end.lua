-- scroll_end.lua : hold RIGHT long past the end of the wide level, snapshot the
-- final CLAMPED frame to confirm (a) the camera clamps at LEVEL_W-SCREEN_W,
-- (b) the rightmost level columns render with no ring-buffer garbage / torn seam.
local function setfield(pname, fname, v)
  local p = manager.machine.ioport.ports[pname]
  if p then local fd = p.fields[fname]; if fd then fd:set_value(v) end end
end
local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  setfield(":ctrl1:mdpad:PAD", "P1 Right", 1)
  -- net ~2px/frame; level is 1304px, camera clamps at 1048px reached by ~frame 600.
  if fr == 1100 or fr == 1400 then     -- well past clamp: steady end-of-level view
    manager.machine.video:snapshot()
    print("SNAP F" .. fr)
  end
  if fr >= 1420 then manager.machine:exit() end
end)
