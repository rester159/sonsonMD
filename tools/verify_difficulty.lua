-- verify_difficulty.lua : prove g_config.difficulty scales enemy march speed.
--
-- The difficulty hook (entity.c k_enemy_speed_by_diff) sets a ground enemy's
-- spawn vx to -ENEMY_SPEED * {0.75,1.0,1.25,1.5} for difficulty {Easy,Normal,
-- Hard,VeryHard}. ENEMY_SPEED=0x100=256, so the leftward vx (negative) is:
--   Easy -192(0xFF40)  Normal -256(0xFF00)  Hard -320(0xFEC0)  VHard -384(0xFE80)
--
-- We set the DSW2 Difficulty field (0x60) to VERY HARD on the DIP screen (clear
-- bits 5-6: difficulty = 3 - ((dsw2>>5)&3) = 3), then read slot-0 enemy vx in
-- game. vx == -384 proves the hook changed it from the Normal default -256.
--
-- Symbols (layout updated 2026-06-17 for round progression; g_entities @ 0xFF0812):
--   g_config.dsw2 @ 0xFF0001. g_entities @ 0xFF0812, sizeof(Entity)=20,
--   type @ +1 (1=ET_ENEMY_A), vx @ +10 (s16). g_game.flow @ 0xFF0807.
-- NOTE: ROUND 1 adds zero per-round speed bonus, so the VeryHard expectation
-- (-384) is unchanged by the round ramp (the ramp only kicks in from round 2+).

local DSW2  = 0xFF0001
local ENT0  = 0xFF0812
local FLOW  = 0xFF0807

local mem
local function rds16(a)
  local v = mem:read_u16(a)
  if v >= 0x8000 then v = v - 0x10000 end
  return v
end
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
press(F_SEL,      "P1 Start", 10)
press(F_SEL + 90, "P1 Start", 10)

local fr = 0
local poked, reported = false, false
emu.register_frame_done(function()
  if not mem then mem = manager.machine.devices[":maincpu"].spaces["program"] end
  fr = fr + 1
  release_all()
  for _, p in ipairs(presses) do
    if fr >= p.at and fr < p.off then setbtn(p.name, 1) end
  end

  if fr >= F_SEL + 30 and fr < F_SEL + 90 and not poked then
    local v = mem:read_u8(DSW2)
    mem:write_u8(DSW2, (v & 0x9F))     -- clear bits 5-6 -> difficulty VERY HARD
    print(string.format("F%-5d DIP: dsw2 %02X -> %02X (DIFFICULTY=VERY HARD)", fr, v, v & 0x9F))
    poked = true
  end

  -- read slot-0 enemy vx right as the game starts (before the seek flips much)
  if fr >= 952 and not reported then
    local flow = mem:read_u8(FLOW)
    local typ0 = mem:read_u8(ENT0 + 1)
    if flow == 0 and typ0 == 1 then
      local vx = rds16(ENT0 + 10)
      print(string.format("F%-5d slot0 enemy vx=%d  (Normal default=-256; VeryHard expect=-384)", fr, vx))
      if vx == -384 then print("PASS: VeryHard scaled the enemy march speed (-384)")
      elseif vx == 384 then print("PASS(seek-flipped): magnitude 384 = VeryHard scale")
      else print("CHECK: vx="..vx.." (|vx| should be 384 for VeryHard)") end
      reported = true
      done_at = fr + 10
    end
  end
  if reported and fr >= done_at then manager.machine:exit() end
  if fr >= 1300 then print("TIMEOUT"); manager.machine:exit() end
end)
print("verify_difficulty.lua running")
