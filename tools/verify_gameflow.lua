-- verify_gameflow.lua : exercise the Phase-8c GAME-FLOW in MAME (genesis/NTSC).
--
-- Boot: SEGA splash -> CAPCOM logo -> selector (confirm ARCADE) -> DIP screen
-- (confirm with Start) -> game. Then we simulate lethal enemy hits by poking
-- g_player_dead_request=1 in RAM (the same byte the collision pass raises on a
-- real touch), and watch the life economy + flow state machine react:
--   hit -> lose a life -> DYING -> RESPAWNING (invuln) -> ALIVE   (repeat)
--   last life lost -> GAME_OVER overlay -> game_run returns -> SELECTOR.
--
-- We poke the request rather than navigating the player into an enemy so the
-- test is deterministic. Snapshots: in-game HUD (lives), GAME OVER overlay, and
-- the re-entered selector. Symbols (out/symbol.txt, mask e0ff.. -> 0xFF..):
--   g_player_dead_request @ 0xFF0072   g_player_invuln @ 0xFF0073
--   g_game @ 0xFF0794  ->  score +18 (0xFF07A6), lives +22 (0xFF07AA),
--                          flow +23 (0xFF07AB), flowTimer +24 (0xFF07AC,u16)
-- flow enum: 0 ALIVE, 1 DYING, 2 RESPAWNING, 3 GAME_OVER.

local DEAD_REQ = 0xFF0072
local INVULN   = 0xFF0073
local LIVES    = 0xFF07AA
local FLOW     = 0xFF07AB

local mem
local function rd(a) return mem:read_u8(a) end
local function wr(a,v) mem:write_u8(a,v) end

-- pad helpers (3-button pad port, same as verify_8b.lua)
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

-- Boot timeline (frames @60fps). Splashes: SEGA ~few s, CAPCOM ~6 s -> be generous.
local F_SEL = 720          -- selector up
press(F_SEL,      "P1 Start", 10)   -- confirm selector (ARCADE default) -> DIP
press(F_SEL + 90, "P1 Start", 10)   -- confirm DIP -> game

local F_INGAME = 960       -- by here we are in the game loop (FPS 60)

local flagname = {[0]="ALIVE",[1]="DYING",[2]="RESPAWNING",[3]="GAME_OVER"}
local fr = 0
local phase = "boot"
local snaps = {}
local hit_armed = false
local prev_flow = -1
local prev_lives = -1
local seen = {}            -- log of flow transitions
local reentry_seen = false
local start_lives = nil

emu.register_frame_done(function()
  if not mem then
    mem = manager.machine.devices[":maincpu"].spaces["program"]
  end
  fr = fr + 1
  release_all()
  for _, p in ipairs(presses) do
    if fr >= p.at and fr < p.off then setbtn(p.name, 1) end
  end

  if fr < F_INGAME then return end

  local lives = rd(LIVES)
  local flow  = rd(FLOW)
  local invu  = rd(INVULN)

  -- log every flow change with the life count
  if flow ~= prev_flow then
    print(string.format("F%-5d flow %-10s lives=%d invuln=%d", fr, flagname[flow] or ("?"..flow), lives, invu))
    seen[#seen+1] = {fr=fr, flow=flow, lives=lives}
    prev_flow = flow
  end
  if lives ~= prev_lives then prev_lives = lives end

  if phase == "boot" then
    -- confirm we reached the game (flow ALIVE, lives = starting count from DIP)
    if flow == 0 and lives > 0 then
      start_lives = lives
      print(string.format("F%-5d IN GAME: starting lives=%d (DIP LIVES)", fr, lives))
      manager.machine.video:snapshot()        -- HUD snapshot (lives shown)
      print("SNAP hud_lives F"..fr)
      phase = "play"
    end
  elseif phase == "play" then
    -- when ALIVE and not already invulnerable, deal a lethal hit
    if flow == 0 and invu == 0 then
      if not hit_armed then
        wr(DEAD_REQ, 1)                        -- simulate enemy contact
        hit_armed = true
        print(string.format("F%-5d POKE dead_request (lives before=%d)", fr, lives))
      end
    else
      hit_armed = false                        -- re-arm once back through a phase
    end
    -- once GAME OVER shows, wait a few frames for the overlay text DMA to land,
    -- then snapshot it and move to watch for re-entry.
    if flow == 3 then
      go_at = fr + 30                          -- snapshot 30 frames into the beat
      phase = "gameover_hold"
    end
  elseif phase == "gameover_hold" then
    if flow == 3 and fr >= go_at then
      manager.machine.video:snapshot()
      print(string.format("SNAP game_over F%d  lives=%d", fr, lives))
      phase = "wait_reentry"
    elseif flow ~= 3 then
      phase = "wait_reentry"                   -- beat ended early; fall through
    end
  elseif phase == "wait_reentry" then
    -- the GAME OVER beat then returns to the selector. Detect re-entry: the
    -- screen width flips back to H40 (320) for the front-end. (We also see the
    -- game RAM reset on the next game_init, but the selector is the cue.)
    local w = 0
    pcall(function() w = manager.machine.screens:at(1).width end)
    if not reentry_seen and w and w >= 320 then
      reentry_seen = true
      print(string.format("F%-5d RE-ENTRY: screen width=%d (back to H40 selector)", fr, w))
      manager.machine.video:snapshot()
      print("SNAP selector_reentry F"..fr)
      phase = "done_wait"
      done_at = fr + 30
    end
  elseif phase == "done_wait" then
    if fr >= done_at then manager.machine:exit() end
  end

  if fr >= 4000 then
    print("TIMEOUT (no completion by F"..fr..")")
    manager.machine:exit()
  end
end)

print("verify_gameflow.lua running")
