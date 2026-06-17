-- verify_death.lua : verify the new DEATH animations in the native port (MAME
-- genesis / NTSC). Boots SEGA->CAPCOM->selector->DIP->game, then:
--  (1) dumps g_game[0..44] once in-game to confirm field offsets,
--  (2) forces a PLAYER death (poke g_player_dead_request=1) and logs the player's
--      anim id + tile + flow through DYING->RESPAWNING (player tile should be the
--      PLAYER_DEATH frame, av.id should be 3),
--  (3) forces an ENEMY death (poke the first active scheduled enemy into its death
--      state) and logs its tile stepping through the death-burst frames,
--  (4) measures FPS via g_frameCounter over a busy window.
-- Symbols (mask e0ff..->0xFF..): g_game 0xFF0840, g_player_dead_request 0xFF083D,
--   g_player_invuln 0xFF083E, g_entities 0xFF0866, g_frameCounter 0xFF0076.
-- Player @ g_game+0: tile @ +20 (u16), av.id @ +16. GameState: flow @ +29 (after
--   player(22)+pad+score(u32)@24+lives@28). Entity stride 0x2A; +0 active, +1 type,
--   +14 state? -- we dump to confirm. PLAYER_DEATH tile=374, ENEMY_DEATH first=390.

local GGAME=0xFF0840; local DEADREQ=0xFF083D; local INVULN=0xFF083E
local ENT=0xFF0866; local FCTR=0xFF0076
local ESTRIDE=0x2A
local mem
local function r8(a) return mem:read_u8(a) end
local function r16(a) return mem:read_u16(a) end
local function r32(a) return mem:read_u32(a) end
local function w8(a,v) mem:write_u8(a,v) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end

-- candidate field offsets within Player/GameState (confirmed by the dump)
local OFF_PTILE=20      -- player.tile
local OFF_PAVID=16      -- player.av.id
local OFF_FLOW=27       -- g_game.flow (score u32 @+22 packed, lives@+26, flow@+27)
local OFF_ELIVES=26

local fr=0; local mem_ok=false; local dumped=false
local F_SEL=720
local presses={{at=F_SEL,off=F_SEL+10},{at=F_SEL+90,off=F_SEL+100}}
local F_INGAME=1000
local p_death_done=false; local e_death_done=false
local fctr0,fr0
local elog={}
local function log(s) print(s); elog[#elog+1]=s end

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"]; mem_ok=true end
  fr=fr+1; rel()
  for _,p in ipairs(presses) do if fr>=p.at and fr<p.off then setbtn("P1 Start",1) end end
  if fr<F_INGAME then return end

  if not dumped then
    dumped=true
    local s="g_game[0..44]:"
    for i=0,44 do s=s..string.format(" %02X",r8(GGAME+i)) end
    log(s)
    log(string.format("guess flow@+%d=%d lives@+%d=%d ptile@+%d=%d avid@+%d=%d",
      OFF_FLOW, r8(GGAME+OFF_FLOW), OFF_ELIVES, r8(GGAME+OFF_ELIVES),
      OFF_PTILE, r16(GGAME+OFF_PTILE), OFF_PAVID, r8(GGAME+OFF_PAVID)))
    -- dump first 3 entity slots
    for k=0,2 do
      local b=ENT+k*ESTRIDE; local s2=string.format("ent%d[0..42]:",k)
      for i=0,42 do s2=s2..string.format(" %02X",r8(b+i)) end
      log(s2)
    end
    fctr0=r32(FCTR); fr0=fr
  end

  -- (2) force a PLAYER death at fr 1100, then sample for 70 frames
  if fr==1100 then w8(DEADREQ,1); log(string.format("F%d POKE player dead_request=1",fr)) end
  if fr>=1100 and fr<=1175 and (fr%4==0) then
    log(string.format("F%-5d PLAYER flow=%d avid=%d tile=%d invuln=%d",
      fr, r8(GGAME+OFF_FLOW), r8(GGAME+OFF_PAVID), r16(GGAME+OFF_PTILE), r8(INVULN)))
  end

  -- (3) force an ENEMY death. Entity offsets (from the dump): state @ +15,
  -- timer @ +16, hp @ +17, tile (u16) @ +40. ET_ENEMY_A=1, FLYER=4, THROWER=5,
  -- CREATURE_G=7, CREATURE_C=8, COMPOSITE_E=9, HOPPER=10. Walkers/creatures use
  -- death state 1; flyer/thrower/hopper use state 2. Find an ENEMY_A walker and
  -- poke it into death state 1, then watch its tile step through ENEMY_DEATH
  -- (390,394,398,402,406,410,414,418).
  if fr==1300 then
    for k=0,47 do
      local b=ENT+k*ESTRIDE
      if r8(b)~=0 and r8(b+1)==1 then    -- active ET_ENEMY_A walker
        w8(b+17,1)                        -- hp=1
        w8(b+15,1)                        -- state=1 (dying)
        STATE_ENT=b; STATE_K=k
        log(string.format("F%d kill ent%d (ENEMY_A): poked state=1",fr,k))
        break
      end
    end
    if not STATE_ENT then log("F1300 no ENEMY_A walker active to kill") end
  end
  if fr>=1300 and STATE_ENT and not e_death_done then
    if (fr%3==0) and fr<=1380 then
      log(string.format("F%-5d ENEMY_A active=%d state=%d tile=%d",
        fr, r8(STATE_ENT), r8(STATE_ENT+15), r16(STATE_ENT+40)))
    end
    if fr>1380 then e_death_done=true end
  end

  -- (4) FPS over the busy window
  if fr==2200 then
    local g=r32(FCTR); local s=60.0*(g-fctr0)/(fr-fr0)
    log(string.format("F%d FPS over busy window = %.2f (gframes=%d over %d)",fr,s,g-fctr0,fr-fr0))
    manager.machine:exit()
  end
end)
print("verify_death running")
