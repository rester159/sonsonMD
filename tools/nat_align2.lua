-- nat_align2.lua : verify the lane-grid fix -- the hero should stand ON the vine
-- tiers now (lane0 y=200=bottom tier, each UP step = one 32px tier). Snapshot the
-- hero at lane 0, then after climbing ~3 tiers, printing p->y each time.
local LEVELW = 0xFF07EE
local PY     = 0xFF0842        -- g_game+2 = Player.y (s16)
local mem
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local fr, ingame = 0, nil
local did = {}
emu.register_frame_done(function()
  if not mem then mem = manager.machine.devices[":maincpu"].spaces["program"] end
  fr = fr + 1; rel()
  if mem:read_u16(LEVELW) == 0 then if (fr%80)<12 then setbtn("P1 Start",1) end; return end
  if not ingame then ingame = fr end
  local t = fr - ingame
  if not did.a and t>=30 then did.a=true
     print(string.format("LANE0  F%d playerY=%d (sprite screenY %d..%d; bottom tier row=200)", fr, rds16(PY), rds16(PY), rds16(PY)+16))
     manager.machine.video:snapshot(); print("  SNAP lane0") end
  if t>=50 and t<110 then setbtn("P1 Up",1) end          -- climb ~ a few tiers
  if not did.b and t>=130 then did.b=true
     print(string.format("CLIMBED F%d playerY=%d (should be a tier row: 168/136/104/72/40)", fr, rds16(PY)))
     manager.machine.video:snapshot(); print("  SNAP climbed"); manager.machine:exit() end
  if fr>=4000 then manager.machine:exit() end
end)
print("nat_align2.lua running")
