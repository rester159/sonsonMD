-- scroll_test.lua : drive the native ROM with RIGHT held, snapshot mid-scroll,
-- and log the VDP Plane-A horizontal-scroll value over time so we can confirm
-- the camera advances smoothly through the wide level (and wraps mod 512 as the
-- 64-col ring scrolls).  genesis driver: pad at :ctrl1:mdpad:PAD, field "P1 Right".
local function setfield(pname, fname, v)
  local p = manager.machine.ioport.ports[pname]
  if p then local fd = p.fields[fname]; if fd then fd:set_value(v) end end
end

-- VRAM HScroll table lives at the plane's HScroll address; easier: read the 68k
-- work RAM where SGDK keeps nothing stable for us, so we rely on snapshots +
-- the VDP scroll write is not directly memory-mapped. Instead we just snapshot.
local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  -- Hold RIGHT for the whole run so the player's run + forward creep scrolls.
  setfield(":ctrl1:mdpad:PAD", "P1 Right", 1)
  if fr == 60 or fr == 240 or fr == 480 or fr == 900 then
    manager.machine.video:snapshot()
    print("SNAP F" .. fr)
  end
  if fr >= 920 then manager.machine:exit() end
end)
