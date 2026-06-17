-- nat_align.lua : diagnose the "sprites float on the platforms" bug.
-- Boot to gameplay (pulse START while g_levelW==0), then DON'T move so the hero
-- stays at the level start on lane 0 (bottom). Snapshot a clean early frame + a
-- slightly-scrolled frame (enemies present), and print the player Y + the first
-- few live entity Ys so we can compare sprite world-Y vs the baked platform rows.
-- Player.y = g_game+2 (struct: s16 x@+0, y@+2). Entities: g_entities, stride 0x28,
-- +0 active / +1 type / +4 x(s16) / +6 y(s16)  (RE-derive if the print looks off).

local LEVELW = 0xFF07EE
local GAME   = 0xFF0840
local PX     = GAME + 0       -- Player.x (s16)
local PY     = GAME + 2       -- Player.y (s16)
local ENTS   = 0xFF085A       -- g_entities (prior-build addr; scan-confirmed below)
local STRIDE = 0x28

local mem
local function rd8(a)  return mem:read_u8(a) end
local function rds16(a) local v = mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end

local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

local fr, ingame = 0, nil
local snapped = {}
emu.register_frame_done(function()
  if not mem then mem = manager.machine.devices[":maincpu"].spaces["program"] end
  fr = fr + 1
  release_all()
  local lw = mem:read_u16(LEVELW)
  if lw == 0 then
    if (fr % 80) < 12 then setbtn("P1 Start", 1) end
    return
  end
  if not ingame then ingame = fr; print(string.format("INGAME F%d g_levelW=%d playerY=%d", fr, lw, rds16(PY))) end

  -- NO movement input: the hero idles at lane 0 (bottom). (The creep still drifts
  -- him right slowly, but Y/lane is untouched -- exactly what we want to inspect.)

  local function dump(tag)
    local px, py = rds16(PX), rds16(PY)
    print(string.format("%s F%d  PLAYER x=%d y=%d (lane0; sprite occupies screenY %d..%d)", tag, fr, px, py, py, py+16))
    local n = 0
    for i=0,15 do
      local b = ENTS + i*STRIDE
      if rd8(b) ~= 0 then
        local t = rd8(b+1); local ex, ey = rds16(b+4), rds16(b+6)
        print(string.format("   ent[%d] type=%d x=%d y=%d (screenX=%d screenY=%d..%d)", i, t, ex, ey, ex-rds16(0xFF0840+34), ey, ey+16))
        n = n + 1; if n>=8 then break end
      end
    end
    manager.machine.video:snapshot(); print("   SNAP "..tag)
  end

  if not snapped.a and fr >= ingame + 50  then snapped.a = true; dump("EARLY") end
  if not snapped.b and fr >= ingame + 220 then snapped.b = true; dump("SCROLLED"); manager.machine:exit() end
  if fr >= 4000 then manager.machine:exit() end
end)
print("nat_align.lua running")
