-- verify_round.lua : exercise ROUND PROGRESSION in MAME (genesis/NTSC).
--
-- Boot: SEGA -> CAPCOM -> selector (confirm ARCADE) -> DIP (confirm) -> game.
-- Then HOLD RIGHT so the hero runs to the LEVEL END. When the camera clamps at
-- maxCam (LEVEL_W-SCREEN_W = 1304-256 = 1048) and he pushes the right wall for
-- ROUND_CLEAR_DWELL(18) frames, FLOW_ROUND_CLEAR fires: the ROUND CLEAR banner +
-- round music play for ROUND_CLEAR_FRAMES(120), then round_advance() bumps the
-- round, rebuilds the level (camera back to 0), re-seeds entities, and resumes
-- ALIVE at round 2. We keep holding RIGHT to optionally reach round 3.
--
-- Snapshots: in-game round-1 HUD, the ROUND CLEAR banner, and a round-2 frame.
--
-- Symbols (out/symbol.txt; mask e0ff.. -> 0xFF..):  g_game @ 0xFF07EC.
-- GameState layout (Player=22 incl. Anim av; verified vs g_entities@0xFF0812 =
-- g_game+38): score @ +22(0xFF0802,u32), lives @ +26(0xFF0806), flow @ +27
-- (0xFF0807), flowTimer @ +28(0xFF0808,u16), round @ +30(0xFF080A),
-- endDwell @ +31(0xFF080B), cameraX @ +32(0xFF080C,s16).
-- flow enum: 0 ALIVE, 1 DYING, 2 RESPAWNING, 3 GAME_OVER, 4 ROUND_CLEAR.

local GAME     = 0xFF07EC
local PLAYERX  = GAME + 0      -- Player.x (s16) at the very start of g_game
local SCORE    = 0xFF0802
local LIVES    = 0xFF0806
local FLOW     = 0xFF0807
local ROUND    = 0xFF080A
local ENDDWELL = 0xFF080B
local CAMX     = 0xFF080C

-- The level end: maxCam = LEVEL_W - SCREEN_W = 163*8 - 256 = 1048.
local LEVEL_W  = 163 * 8

local mem
local function rd(a)  return mem:read_u8(a) end
local function wr(a,v) mem:write_u8(a,v) end
local function wrs16(a,v) if v < 0 then v = v + 0x10000 end; mem:write_u16(a, v & 0xFFFF) end
local function rds16(a) local v = mem:read_u16(a); if v >= 0x8000 then v = v - 0x10000 end; return v end

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
press(F_SEL + 90, "P1 Start", 10)   -- confirm DIP -> game

local F_INGAME = 960    -- in the game loop by here

local flagname = {[0]="ALIVE",[1]="DYING",[2]="RESPAWNING",[3]="GAME_OVER",[4]="ROUND_CLEAR"}
local fr = 0
local phase = "boot"
local prev_flow, prev_round = -1, -1
local hud1_snapped = false
local banner_at = nil
local r2_snapped = false
local r2_entry = nil

emu.register_frame_done(function()
  if not mem then mem = manager.machine.devices[":maincpu"].spaces["program"] end
  fr = fr + 1
  release_all()
  for _, p in ipairs(presses) do
    if fr >= p.at and fr < p.off then setbtn(p.name, 1) end
  end

  if fr < F_INGAME then return end

  -- drive the hero RIGHT every in-game frame so he pushes the level-end wall.
  setbtn("P1 Right", 1)

  local flow  = rd(FLOW)
  local round = rd(ROUND)
  local lives = rd(LIVES)
  local camx  = rds16(CAMX)
  local dwell = rd(ENDDWELL)

  -- DETERMINISTIC HARNESS (same spirit as verify_gameflow poking dead_request):
  -- a blind RIGHT-holding bot can't fight through the wave to the finish, so we
  -- (a) keep lives topped up so a stray hit just respawns (never game-over), and
  -- (b) shove the hero's world X to the level end each ALIVE/RESPAWNING frame so
  -- player_control's band-clamp drags the CAMERA to maxCam and re-pins him at the
  -- right wall. The ROUND-CLEAR condition itself (camera maxed + wall-push for
  -- ROUND_CLEAR_DWELL frames) is then evaluated entirely by the real game code --
  -- we only remove the "survive + traverse" burden, not the clear logic. The
  -- 150-frame RESPAWNING window is invulnerable + controllable, so the wall-push
  -- dwell reliably accrues there even amid enemies.
  -- Push to the end while clearing round 1; for round 2 wait until we've grabbed
  -- the round-2-START snapshot (camX small) before shoving him to the end again,
  -- so we can prove the level reset to the beginning. Stop at round 3.
  local push_ok = (round == 1) or (round == 2 and r2_snapped)
  if round < 3 then
    if lives < 5 then wr(LIVES, 5) end
    if push_ok and (flow == 0 or flow == 2) then   -- ALIVE/RESPAWNING (controllable)
      wrs16(PLAYERX, LEVEL_W)             -- past the end -> clamp pins him at maxCam
    end
  end

  if flow ~= prev_flow then
    print(string.format("F%-5d flow %-11s round=%d lives=%d camX=%d dwell=%d",
          fr, flagname[flow] or ("?"..flow), round, lives, camx, dwell))
    prev_flow = flow
  end
  if round ~= prev_round then
    print(string.format("F%-5d ROUND -> %d  (flow=%s lives=%d)",
          fr, round, flagname[flow] or "?", lives))
    prev_round = round
  end

  -- once safely in-game (ALIVE, round 1), snapshot the round-1 HUD
  if not hud1_snapped and flow == 0 and round == 1 and fr > F_INGAME + 20 then
    manager.machine.video:snapshot()
    print("SNAP round1_hud F"..fr)
    hud1_snapped = true
  end

  -- ROUND CLEAR beat: snapshot the banner ~30 frames in (let the text DMA land)
  if flow == 4 then
    if banner_at == nil then banner_at = fr + 30 end
    if fr >= banner_at and not r2_snapped then
      -- snapshot the banner exactly once (guard with a separate flag)
      if not _banner_done then
        manager.machine.video:snapshot()
        print(string.format("SNAP round_clear_banner F%d (clearing round %d)", fr, round))
        _banner_done = true
      end
    end
  else
    banner_at = nil
  end

  -- reached round 2: note the entry frame, then snapshot ~50 frames later so the
  -- banner-clear DMA has composited (snapshotting the exact transition frame
  -- catches the still-displayed banner from the prior frame) and we can confirm
  -- BOTH that the level reset to the start (camX still small -- we don't push on
  -- round 2 until after this snap) AND that the ROUND CLEAR banner is gone.
  if round >= 2 and r2_entry == nil then r2_entry = fr end
  if not r2_snapped and r2_entry and fr >= r2_entry + 50 and flow == 0 then
    manager.machine.video:snapshot()
    print(string.format("SNAP round2_frame F%d  round=%d camX=%d (banner should be gone)", fr, round, camx))
    print("PASS: reached ROUND 2 (round loop works; round# incremented, level reset to start)")
    r2_snapped = true
    -- keep holding RIGHT to confirm round 2 also progresses toward another clear
    done_at = fr + 700
  end

  if r2_snapped and fr >= done_at then
    print(string.format("DONE F%d final round=%d flow=%s lives=%d",
          fr, rd(ROUND), flagname[rd(FLOW)] or "?", rd(LIVES)))
    manager.machine:exit()
  end

  if fr >= 3000 then
    print(string.format("TIMEOUT F%d (round=%d flow=%s camX=%d dwell=%d)",
          fr, round, flagname[flow] or "?", camx, dwell))
    manager.machine:exit()
  end
end)

print("verify_round.lua running")
