-- composite_focus.lua : isolate genuine multi-sprite METASPRITE bodies on the
-- playfield (exclude HUD), and correlate each with the owning entity pool slot.
--
-- A SonSon hardware sprite is 16x16 (the board's spritelayout). A composite body
-- = several 16x16 sprites placed adjacent. We look for groups of >=2 active
-- sprites forming a tight block (X within 20px, Y within 20px). For each such
-- block we print the members AND scan every entity pool for a slot whose screen
-- position (x_int - scroll, y_int) matches the block anchor -> names the owner.
--
-- Playfield only: ignore sprites with Y >= 0xAC (172) -- that's the bottom HUD/
-- score band where digit/letter sprites cluster (codes DD/FE/FF/8x).

local OUT = "D:/genesis ports/_capcom/son son/sonson_native/tools/composite_focus.txt"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local f   = io.open(OUT, "w")
local function press(p, fld, v)
  local pt = mio.ports[p]; if pt then local fd = pt.fields[fld]; if fd then fd:set_value(v) end end
end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end

local SPR_BASE = 0x2020
local SCROLL = 0x0090   -- camera scroll word (hi at 0x90)

-- entity pools to correlate (name, base, nslots, stride). Player is at $0490.
local POOLS = {
  {"PLAYER",    0x0490, 2,  0x30},
  {"enemyMain", 0x0828, 16, 0x20},
  {"classC",    0x0A28, 6,  0x20},
  {"classD",    0x0AE8, 6,  0x20},
  {"classE",    0x0B68, 2,  0x20},
  {"classF",    0x0BA8, 5,  0x20},
  {"classG",    0x0C28, 6,  0x20},
  {"classH",    0x0CE8, 6,  0x20},
  {"classI",    0x0DA8, 6,  0x20},
  {"objSingle", 0x0E28, 4,  0x20},
  {"obj16x16",  0x0E48, 3,  0x30},
  {"classK",    0x0ED8, 6,  0x20},
}

-- read entity screen pos. The record start = 6809base; x register points base+0x10.
-- x_int @ base+0x05, x_sub @ base+0x06, y_int @ base+0x08, spr_code @ base+0x10.
local function entScreenInfo(base)
  if r8(base) == 0 then return nil end           -- inactive (active @ +0x00)
  local xint = r8(base+0x05); local xsub = r8(base+0x06)
  local yint = r8(base+0x08)
  local code = r8(base+0x10)
  local scroll = (r8(SCROLL) << 8) | r8(SCROLL+1)
  local worldx = (xint << 8) | xsub
  local sx = (worldx - scroll) & 0xFFFF
  if sx > 0x8000 then sx = sx - 0x10000 end
  return {sx=sx, sy=yint, code=code}
end

-- find the owner pool/slot nearest to a given screen (bx,by)
local function findOwner(bx, by)
  local bestName, bestDist, bestCode = "?", 9999, -1
  for _,p in ipairs(POOLS) do
    for s = 0, p[3]-1 do
      local base = p[2] + s*p[4]
      local info = entScreenInfo(base)
      if info then
        -- sprite top-left is roughly entity (sx,sy); allow some slop
        local dx = info.sx - bx; if dx<0 then dx=-dx end
        local dy = info.sy - by; if dy<0 then dy=-dy end
        local d = dx + dy
        if d < bestDist then bestDist=d; bestName=string.format("%s[%d]",p[1],s); bestCode=info.code end
      end
    end
  end
  return bestName, bestDist, bestCode
end

local fr, phase, t0, play0 = 0, "attract", 0, nil
local bestBlock = 0
local logged = 0

emu.register_frame_done(function()
  fr = fr + 1
  local md = r8(0x0002)
  if phase == "attract" then
    if md == 0x01 and fr > 700 then phase = "coin"; t0 = fr end
  elseif phase == "coin" then
    press(":SYSTEM", "Coin 1", ((fr - t0) % 24 < 6) and 1 or 0)
    if fr - t0 >= 96 then phase = "start"; t0 = fr; press(":SYSTEM", "Coin 1", 0) end
  elseif phase == "start" then
    press(":SYSTEM", "1 Player Start", ((fr - t0) % 24 < 6) and 1 or 0)
    if md == 0x02 then phase = "play"; t0 = fr; play0 = fr; press(":SYSTEM", "1 Player Start", 0) end
    if fr - t0 > 600 then f:write("STUCK\n"); f:close(); manager.machine:exit() end
  elseif phase == "play" then
    w8(0x0073, 0xFF)
    press(":P1", "P1 Right", 1)
    press(":P1", "P1 Button 1", ((fr % 8) < 3) and 1 or 0)
    local ph = (fr // 50) % 4
    press(":P1", "P1 Up", ph==1 and 1 or 0); press(":P1", "P1 Down", ph==3 and 1 or 0)

    -- collect playfield sprites (Y < 0xAC, Y != 0xF8 clear)
    local act = {}
    for i = 0, 23 do
      local b = SPR_BASE + i*4
      local y = r8(b+0)
      if y ~= 0xF8 and y < 0xAC then
        act[#act+1] = {y=y, code=r8(b+1), attr=r8(b+2), x=r8(b+3), idx=i}
      end
    end
    -- find largest tight block (20px X, 20px Y)
    for _,a in ipairs(act) do
      local grp = {}
      for _,o in ipairs(act) do
        local dx=o.x-a.x; if dx<0 then dx=-dx end
        local dy=o.y-a.y; if dy<0 then dy=-dy end
        if dx<=20 and dy<=20 then grp[#grp+1]=o end
      end
      if #grp >= 3 and logged < 40 then
        -- anchor = min x, min y of the group
        local minx,miny=999,999
        for _,o in ipairs(grp) do if o.x<minx then minx=o.x end; if o.y<miny then miny=o.y end end
        local owner, dist, ocode = findOwner(minx, miny)
        local parts={}
        for _,o in ipairs(grp) do parts[#parts+1]=string.format("(y=%d x=%d c=%02X a=%02X)",o.y,o.x,o.code,o.attr) end
        f:write(string.format("fr=%d block=%d anchor=(%d,%d) owner=%s(d=%d,code=%02X) | %s\n",
          fr, #grp, minx, miny, owner, dist, ocode, table.concat(parts," ")))
        f:flush()
        logged = logged + 1
      end
      if #grp > bestBlock then bestBlock = #grp end
    end

    if play0 and fr - play0 >= 6000 then phase="done" end
  elseif phase == "done" then
    f:write(string.format("\nMAX playfield block size = %d\n", bestBlock))
    f:close(); manager.machine:exit()
  end
  if fr > 30000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
print("composite_focus running")
