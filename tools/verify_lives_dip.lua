-- verify_lives_dip.lua : prove the arcade LIVES DIP changes the starting count.
--
-- Boot to the DIP screen, poke g_config.dsw2's LIVES field (0x03) to the "7
-- lives" setting (raw value 0 -> LIVES_TBL[0]=7), then confirm the DIP. The
-- screen's confirm path calls sync_from_dsw(), which decodes dsw2&0x03 into
-- g_config.lives; game_init then seeds g_game.lives from it. We read g_game.lives
-- in-game and assert it is 7 (vs the default 3) -- closing the DIP->lives loop.
--
-- Symbols: g_config @ 0xFF0000 (dsw1=+0, dsw2=+1). g_game.lives @ 0xFF07AA.
-- DSW2 default = 0xCB (lives field 0x03 -> 3). Setting field to 0 -> 0xC8 -> 7.

local DSW2  = 0xFF0001
local LIVES = 0xFF07AA
local FLOW  = 0xFF07AB

local mem
local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start",
                      "P1 A","P1 B","P1 C"}) do setbtn(n, 0) end
end
local presses = {}
local function press(at, name, dur) presses[#presses+1] = {at=at, name=name, off=at+(dur or 8)} end

local F_SEL = 720
press(F_SEL,      "P1 Start", 10)   -- confirm selector (ARCADE) -> DIP
press(F_SEL + 90, "P1 Start", 10)   -- confirm DIP -> game (after we poke dsw2)

local fr = 0
local poked = false
local reported = false
emu.register_frame_done(function()
  if not mem then mem = manager.machine.devices[":maincpu"].spaces["program"] end
  fr = fr + 1
  release_all()
  for _, p in ipairs(presses) do
    if fr >= p.at and fr < p.off then setbtn(p.name, 1) end
  end

  -- on the DIP screen (between the two Start presses), force the LIVES field to
  -- the "7" setting by clearing dsw2 bits 0-1. (The screen edits dsw2 on input;
  -- we set it directly so the test is independent of cursor navigation.)
  if fr >= F_SEL + 30 and fr < F_SEL + 90 and not poked then
    local v = mem:read_u8(DSW2)
    mem:write_u8(DSW2, (v & 0xFC))      -- lives field -> 0 -> 7 lives
    print(string.format("F%-5d DIP: dsw2 %02X -> %02X (LIVES=7)", fr, v, v & 0xFC))
    poked = true
  end

  -- in game: read the starting life count
  if fr >= 960 and not reported then
    local flow = mem:read_u8(FLOW)
    local lives = mem:read_u8(LIVES)
    if flow == 0 and lives > 0 then
      print(string.format("F%-5d IN GAME: lives=%d  (expect 7 from the DIP)", fr, lives))
      if lives == 7 then print("PASS: DIP LIVES=7 took effect")
      else print("FAIL: expected 7, got "..lives) end
      manager.machine.video:snapshot()
      print("SNAP hud_lives7 F"..fr)
      reported = true
      done_at = fr + 20
    end
  end
  if reported and fr >= done_at then manager.machine:exit() end
  if fr >= 1400 then print("TIMEOUT"); manager.machine:exit() end
end)
print("verify_lives_dip.lua running")
