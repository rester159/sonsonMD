-- arc_attract.lua -- capture the arcade attract sequence (NO coin; stay in attract).
-- Snapshot at intervals to map the full title/demo/score/instructions cycle.
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r8(a) return mem:read_u8(a) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_attract.txt","w")
local fr=0; local shot=0
emu.register_frame_done(function()
  fr=fr+1
  local gs=r8(0x0002)
  -- snapshot every 480 frames once boot is done, logging the game-state byte
  if fr>=1100 and (fr%480)==0 and shot<16 then
    manager.machine.video:snapshot()
    f:write(string.format("snap %02d: fr=%d  gs($0002)=%d\n", shot, fr, gs)); f:flush()
    shot=shot+1
  end
  if fr>=9000 then f:close(); manager.machine:exit() end
end)
print("running")
