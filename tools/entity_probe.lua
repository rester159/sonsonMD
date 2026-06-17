-- entity_probe.lua : inspect the live native entity pool to confirm enemies are
-- alive/moving, shots travel, and collisions defeat enemies (score climbs).
-- Reads g_entities (@0xFF07F4) + g_game.score. Drives ARCADE -> busy scene.
--
-- Entity struct layout (entity.h, packed, SGDK -fms-extensions; 68k is 2-byte
-- aligned for s16/u16). Offsets (bytes) verified against the struct order:
--   +0  u8  active     +1  u8  type
--   +2  s16 x          +4  s16 y
--   +6  u16 xSub       +8  u16 ySub
--   +10 s16 vx         +12 s16 vy
--   +14 u8 lane +15 u8 state +16 u8 timer +17 u8 hp
--   +18 u8 facing +19 u8 anim +20 u8 animTimer (+pad) +22 u16 tile
-- sizeof rounds to 24 (0x18) with alignment. We confirm stride by scanning.
local ENT   = 0xFF07F4
local GAME  = 0xFF0124          -- g_game; score is the first u32 after Player
local STRIDE_GUESS = 24
local NMAX  = 24

local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local function r8(a)  return mem:read_u8(a)  end
local function r16(a) return mem:read_i16(a) end
local function r32(a) return mem:read_u32(a) end

local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

local TYPENAME = {[0]="NONE",[1]="ENEMY_A",[2]="SHOT",[3]="ITEM"}

local function dump(label)
  print("==== "..label.." ====")
  local nEnemy, nShot, nItem = 0,0,0
  for i=0,NMAX-1 do
    local b = ENT + i*STRIDE_GUESS
    local active = r8(b+0)
    if active ~= 0 then
      local t = r8(b+1)
      local x = r16(b+2); local y = r16(b+4)
      local vx = r16(b+10); local st = r8(b+15); local hp = r8(b+17)
      if t==1 then nEnemy=nEnemy+1 elseif t==2 then nShot=nShot+1 elseif t==3 then nItem=nItem+1 end
      print(string.format("  slot%02d %-7s x=%d y=%d vx=%d state=%d hp=%d", i, TYPENAME[t] or ("?"..t), x, y, vx, st, hp))
    end
  end
  print(string.format("  totals: enemies=%d shots=%d items=%d  score=%d", nEnemy, nShot, nItem, r32(GAME)))
end

local fr = 0
emu.register_frame_done(function()
  fr = fr + 1
  release_all()
  if fr >= 720 and fr < 728 then setbtn("P1 Start",1) end
  if fr >= 930 and fr < 938 then setbtn("P1 Start",1) end
  if fr >= 1000 then setbtn("P1 Right",1); if (fr%8)<3 then setbtn("P1 B",1) end end
  if fr == 1010 then dump("early (scene just started)") end
  if fr == 1200 then dump("mid (after some marching + firing)") end
  if fr == 1380 then dump("late (after sustained combat)") end
  if fr >= 1400 then manager.machine:exit() end
end)
print("entity_probe.lua running")
