-- sega_snap.lua : snapshot the SEGA opening splash (the FIRST boot stage) to
-- confirm it's now the Revenge-of-Shinobi logo (small blue logotype + rolling
-- sheen on black), not the Sonic-1 white card. The splash holds ~210 frames.
local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  if fr == 90  then manager.machine.video:snapshot(); print("SNAP sega F90") end
  if fr == 170 then manager.machine.video:snapshot(); print("SNAP sega F170"); manager.machine:exit() end
end)
print("sega_snap.lua running")
