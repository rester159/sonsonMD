-- verify_enemy_tier.lua : prove the FAITHFUL ground-enemy vertical behavior.
--
-- RE verdict (cited in entity.c seat_on_tier): arcade ground enemies do NOT chase
-- the player's tier -- they march horizontally (tracking the player on X) and ride
-- their spawn tier (sub_A847 reseat). This harness boots to the game, drives the
-- player UP/DOWN across tiers (so his Y sweeps the full 40..200 band) while ground
-- enemies are on screen, and confirms every ground-enemy Y stays SNAPPED to one of
-- the 6 SS_TIER_Y bands {40,72,104,136,168,200} and does NOT converge on the
-- player's Y. Also measures the 60fps ratio (g_frameCounter / emu-frame ~1.000).
--
-- Addresses re-derived from out/symbol.txt (this build):
--   g_frameCounter@0xFF0076  g_levelW@0xFF078A  g_game@0xFF07E0  g_entities@0xFF080C
-- GameState (Player first): player.y@+2  vstate@+12  tier@+13  round@+38 stage@+39
--   cameraX@+40.  Entity stride 0x28: active@+0 type@+1 y@+4 state@+15.

local GFC    = 0xFF0076
local LEVELW = 0xFF078A
local GAME   = 0xFF07E0
local PY     = GAME + 2          -- player.y  (confirmed: reads 136 = tier 2 at start)
local PVST   = GAME + 12         -- player.vstate
local PTIER  = GAME + 13         -- player.tier
local ENTS   = 0xFF080C          -- g_entities (stride 0x2A; active@0 type@1 x@2 y@4 state@15)
                                 -- (stride VERIFIED 0x2A via the 4 known ambient fruits)
local ESTRIDE= 0x2A
local EMAX   = 48

-- EntityType ids (entity.h enum order): NONE=0 ENEMY_A=1 SHOT=2 ITEM=3 FLYER=4
--   THROWER=5 ENEMY_SHOT=6 CREATURE_G=7 CREATURE_C=8 COMPOSITE_E=9 HOPPER=10
--   LARGE=11 EMITTER=12 ORB=13 POPUP=14.
-- The GROUND MARCHERS we fixed (must stay tier-seated): ENEMY_A, THROWER,
--   CREATURE_G, CREATURE_C, COMPOSITE_E, LARGE.  (FLYER/HOPPER/ORB/shots excluded:
--   flyer dive + hopper leap are ballistic/faithful; orb bobs around its own row.)
local GROUND = {[1]=true,[5]=true,[7]=true,[8]=true,[9]=true,[11]=true}
local TNAME  = {[1]="ENEMY_A",[4]="FLYER",[5]="THROWER",[7]="CREAT_G",[8]="CREAT_C",
                [9]="COMPOS_E",[10]="HOPPER",[11]="LARGE",[13]="ORB"}
local TIER_Y = {40,72,104,136,168,200}

local function is_tier(y) for _,t in ipairs(TIER_Y) do if y==t then return true end end return false end

local mem
local function rd8(a)  return mem:read_u8(a) end
local function rd16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function rd32(a) return mem:read_u32(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end

local fr=0
local ig0, gfc0
local snaps=0
-- offences = ground enemies whose Y is NOT on a tier band, OR that converge on player Y
local off_count=0           -- frames with an off-tier ground enemy
local off_examples={}
local seen_ground=0          -- max ground enemies seen at once
local min_gap_to_player=999  -- min |enemyY - playerY| over all ground enemies (should NOT trend to 0)
local converge_frames=0      -- frames where a ground enemy's Y == player Y AND player off the enemy's spawn tier

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1
  release_all()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (fr%80)<12 then setbtn("P1 Start",1) end return end
  if not ig0 then ig0=fr; gfc0=rd32(GFC)
    print(string.format("INGAME F%d playerY=%d vstate=%d tier=%d",fr,rd16(PY),rd8(PVST),rd8(PTIER)))
    manager.machine.video:snapshot(); snaps=snaps+1
  end
  local rel=fr-ig0

  -- DRIVE: sweep the player across tiers so his Y covers the full band while ground
  -- enemies march. RIGHT keeps the scroll + waves coming; UP/DOWN climb between tiers.
  setbtn("P1 Right",1)
  if     rel>= 60 and rel<140 then setbtn("P1 Up",1)
  elseif rel>=160 and rel<260 then setbtn("P1 Down",1)
  elseif rel>=300 and rel<380 then setbtn("P1 Up",1)
  elseif rel>=420 and rel<520 then setbtn("P1 Down",1)
  elseif rel>=560 and rel<640 then setbtn("P1 Up",1)
  end

  local py=rd16(PY)
  -- scan the entity pool for ground enemies
  local nground=0
  for i=0,EMAX-1 do
    local base=ENTS+i*ESTRIDE
    local act=rd8(base+0)
    if act~=0 then
      local ty=rd8(base+1)
      local st=rd8(base+15)
      if GROUND[ty] and st~=1 and st~=2 then    -- alive (not dying) ground marcher
        nground=nground+1
        local ey=rd16(base+4)
        if not is_tier(ey) then
          off_count=off_count+1
          if #off_examples<8 then
            off_examples[#off_examples+1]=string.format("rel%d %s Y=%d (off-tier) playerY=%d",rel,TNAME[ty] or ty,ey,py)
          end
        end
        local gap=math.abs(ey-py)
        if gap<min_gap_to_player then min_gap_to_player=gap end
        -- convergence test: enemy sitting exactly on player's Y while player is off
        -- a tier band (i.e. the enemy followed him there) -> the old unfaithful chase
        if ey==py and not is_tier(py) then converge_frames=converge_frames+1 end
      end
    end
  end
  if nground>seen_ground then seen_ground=nground end

  if rel==130 or rel==350 or rel==600 then
    print(string.format("rel%-4d playerY=%d vstate=%d tier=%d groundEnemies=%d",rel,py,rd8(PVST),rd8(PTIER),nground))
    if snaps<5 then manager.machine.video:snapshot(); snaps=snaps+1 end
  end

  if rel>=720 then
    local gf=rd32(GFC)-gfc0; local ef=fr-ig0
    print("=========== ENEMY-TIER VERIFY RESULT ===========")
    print(string.format("max ground enemies on screen at once: %d",seen_ground))
    print(string.format("frames with an OFF-TIER ground enemy : %d  (want 0)",off_count))
    print(string.format("frames a ground enemy SAT ON the player Y while he was off-tier (chase): %d  (want 0)",converge_frames))
    print(string.format("min |enemyY-playerY| seen           : %d  (a chaser would trend to 0; tier-bound stays quantised)",min_gap_to_player))
    if #off_examples>0 then print("OFF-TIER EXAMPLES:"); for _,s in ipairs(off_examples) do print("  "..s) end end
    print(string.format("FPS: %d emu-frames, gfc+%d => %.3f game-frames/emu-frame (1.000=60fps)",ef,gf,gf/ef))
    manager.machine.video:snapshot()
    manager.machine:exit()
  end
  if fr>=24000 then print("HARD TIMEOUT"); manager.machine:exit() end
end)
print("verify_enemy_tier running")
