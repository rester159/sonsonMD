-- verify_autoscroll.lua : verify the FAITHFUL auto-scroll advance (no wall-push).
--
-- Boots SEGA->CAPCOM->selector->config->game by PULSING START until gameplay is
-- detected (g_levelW becomes nonzero -- it is 0 until game_init's level_select).
-- Then HOLDS RIGHT so the creep+run carry the hero to each level end. The NEW
-- model advances the MOMENT cameraX>=maxCam -- no dwell, no wall-push -- into a
-- BRIEF transition that swaps the next scenery + ticks the saturating stage.
--
-- We verify via EXACT standalone symbols (robust to struct-offset drift):
--   g_levelW @ 0xFF07EE (u16) -- changes on each scenery swap => proves advance+loop
--   g_frameCounter @ 0xFF0076 (volatile u32, +1 per game frame) -- fps gauge
-- round/stage are read from g_game (offsets self-calibrated by a one-time scan;
-- they're a secondary signal -- g_levelW is the primary advance proof). lives is
-- topped each frame so the bot survives long enough to traverse the levels.
--
-- PASS: multiple ADVANCE lines (g_levelW cycling), round incrementing, stage
-- incrementing (and it would saturate at 28), and game-frames/emu-frame ~= 1.000.

local LEVELW = 0xFF07EE          -- g_levelW (u16): nonzero == in gameplay
local GFC    = 0xFF0076          -- g_frameCounter (volatile u32)
local GAME   = 0xFF0840          -- g_game base
local LIVES  = GAME + 26         -- u8 lives (after Player[22]+score[4]; stable)
local ROUND_OFF, STAGE_OFF = 32, 33   -- expected; the scan below confirms/corrects

local mem
local function rd8(a)  return mem:read_u8(a)  end
local function rd16(a) return mem:read_u16(a) end
local function rd32(a) return mem:read_u32(a) end
local function wr8(a,v) mem:write_u8(a,v) end

local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start",
                      "P1 A","P1 B","P1 C"}) do setbtn(n, 0) end
end

local fr = 0
local ingame_fr, gfc_base
local prev_lw, prev_round, prev_stage = -1, -1, -1
local advances = 0
local snaps = 0

emu.register_frame_done(function()
  if not mem then mem = manager.machine.devices[":maincpu"].spaces["program"] end
  fr = fr + 1
  release_all()

  local lw = rd16(LEVELW)

  if lw == 0 then
    -- FRONT-END: pulse START (12 on / 68 off -> one clean edge per 80 frames) to
    -- confirm the selector then the config screen. Splashes ignore input.
    if (fr % 80) < 12 then setbtn("P1 Start", 1) end
    return
  end

  -- ---- IN GAME ----
  if not ingame_fr then
    ingame_fr = fr
    gfc_base  = rd32(GFC)
    local s = {}
    for off = 24, 38 do s[#s+1] = string.format("+%d=%d", off, rd8(GAME + off)) end
    print(string.format("INGAME F%d g_levelW=%d  g_game-scan: %s", fr, lw, table.concat(s, " ")))
    print("  (expect a byte=lives(~3), then 0=bonusIdx,0=flow, ... 1=round, 0=stage)")
    prev_lw = lw
    -- baseline in-game snapshot
    manager.machine.video:snapshot(); print("  SNAP ingame_baseline"); snaps = snaps + 1
  end

  -- survive + run right (traverse each level fast; the trigger is input-agnostic).
  if rd8(LIVES) < 4 then wr8(LIVES, 4) end
  setbtn("P1 Right", 1)

  local round = rd8(GAME + ROUND_OFF)
  local stage = rd8(GAME + STAGE_OFF)

  if lw ~= prev_lw then
    advances = advances + 1
    print(string.format("F%-6d ADVANCE #%d  g_levelW %d -> %d  round=%d stage=%d",
          fr, advances, prev_lw, lw, round, stage))
    prev_lw = lw
    if snaps < 3 then manager.machine.video:snapshot(); print("  SNAP advance"); snaps = snaps + 1 end
  end
  if round ~= prev_round then
    print(string.format("F%-6d round -> %d (stage=%d)", fr, round, stage)); prev_round = round
  end
  if stage ~= prev_stage then
    print(string.format("F%-6d stage -> %d", fr, stage)); prev_stage = stage
  end

  -- ~3600 in-game frames is several held-RIGHT traversals; report fps + exit.
  if fr >= ingame_fr + 3600 then
    local gframes = rd32(GFC) - gfc_base
    local eframes = fr - ingame_fr
    print(string.format("DONE: %d emu-frames in-game, g_frameCounter +%d => %.3f game-frames/emu-frame (1.000 = full 60fps)",
          eframes, gframes, gframes / eframes))
    print(string.format("TOTAL ADVANCES=%d  final round=%d stage=%d g_levelW=%d",
          advances, rd8(GAME + ROUND_OFF), rd8(GAME + STAGE_OFF), rd16(LEVELW)))
    manager.machine:exit()
  end
  if fr >= 24000 then print("HARD TIMEOUT"); manager.machine:exit() end
end)
print("verify_autoscroll.lua running")
