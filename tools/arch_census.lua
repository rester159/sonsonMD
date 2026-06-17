-- arch_census.lua : ground-truth which ENTITY POOLS (archetypes) populate per
-- arcade level/stage in REAL md=02 gameplay (MAME `sonson`), WITHOUT pinning
-- $0067, so the spawn-stage $0096 advances naturally as the game progresses.
--
-- Pools (base = first slot; active byte @ base-0x10; type/idx @ base+3 for the
-- enemyMain-style, or via -$F hi for the table-dispatched classes; for census we
-- just read the slot's index byte at -$F hi (the dispatch index) + the type/$3):
--   $0828 enemyMain  (walkers/thrower)        idx byte @ +3
--   $0A28 classC     (creature families)
--   $0AE8 classD     (projectiles/emitters)
--   $0B68 classE     (large composite, 2 slots)
--   $0BA8 classF     (flyers, ~5)
--   $0C28 classG     (ground creatures, ~6)
--   $0CE8 classH     (hoppers/actors, ~6)
--   $0DA8 classI     (lobbed projectiles)
--   $0E28 objSingle  (unique)
--   $0E48 obj16x16   (large, 3 slots stride $30)
--   $0ED8 classK     (score children)
-- For each pool we report MAX simultaneous active over the run + per-stage which
-- pools were non-empty.  We log $0096 (spawn stage), $005A, $0067 (gfx level).

local OUT = "D:/genesis ports/_capcom/son son/sonson_native/tools/arch_census.txt"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local f   = io.open(OUT, "w")
local function press(p, fld, v)
  local pt = mio.ports[p]; if pt then local fd = pt.fields[fld]; if fd then fd:set_value(v) end end
end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end

-- pool descriptors: {name, base, nslots, stride}
local POOLS = {
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

local function poolCount(base, n, stride)
  local c, idxs = 0, {}
  for s = 0, n-1 do
    local rb = base + s*stride
    if r8(rb - 0x10) ~= 0 then
      c = c + 1
      -- dispatch index = hi byte of word @ rb-0xF ; also slot type @ rb+3
      local idx = r8(rb - 0xF)
      idxs[#idxs+1] = idx
    end
  end
  return c, idxs
end

-- per-stage seen: stageSeen[stage][poolname] = max count
local stageSeen = {}
local poolMax   = {}   -- global max per pool
for _,p in ipairs(POOLS) do poolMax[p[1]] = 0 end

local fr, phase, t0, play0 = 0, "attract", 0, nil
local lastStage = nil
local stageLog = {}    -- ordered list of stages encountered with frame

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
    if fr - t0 > 600 then f:write("STUCK at start\n"); f:close(); manager.machine:exit() end
  elseif phase == "play" then
    -- keep the player alive (grace byte hot) and drive a forward run + fire so the
    -- camera scrolls, stages advance, and waves keep coming. Do NOT pin $0067.
    w8(0x0073, 0xFF)
    press(":P1", "P1 Up", 0); press(":P1", "P1 Down", 0)
    press(":P1", "P1 Left", 0); press(":P1", "P1 Right", 1)
    press(":P1", "P1 Button 1", ((fr % 10) < 3) and 1 or 0)
    -- weave lanes so all rows engage + the climb/finish triggers fire
    local ph = (fr // 70) % 4
    if ph == 1 then press(":P1", "P1 Up", 1) elseif ph == 3 then press(":P1", "P1 Down", 1) end

    local stage = r8(0x0096)
    local s5a   = r8(0x005A)
    local lvl67 = r8(0x0067)
    if stage ~= lastStage then
      stageLog[#stageLog+1] = string.format("fr=%d stage96=%02X 5A=%02X gfx67=%02X", fr, stage, s5a, lvl67)
      lastStage = stage
    end
    stageSeen[stage] = stageSeen[stage] or {}
    for _,p in ipairs(POOLS) do
      local c, idxs = poolCount(p[2], p[3], p[4])
      if c > (stageSeen[stage][p[1]] or 0) then stageSeen[stage][p[1]] = c end
      if c > poolMax[p[1]] then poolMax[p[1]] = c end
    end

    if fr % 60 == 0 then
      local parts = {}
      for _,p in ipairs(POOLS) do
        local c, idxs = poolCount(p[2], p[3], p[4])
        if c > 0 then parts[#parts+1] = string.format("%s=%d{%s}", p[1], c, table.concat(idxs, ",")) end
      end
      f:write(string.format("fr=%d stage96=%02X gfx67=%02X | %s\n", fr, stage, lvl67, table.concat(parts, " ")))
      f:flush()
    end

    if play0 and fr - play0 >= 9000 then phase = "done" end
  elseif phase == "done" then
    f:write("\n=== STAGE PROGRESSION (stage96 changes) ===\n")
    for _,l in ipairs(stageLog) do f:write(l.."\n") end
    f:write("\n=== PER-STAGE POOLS POPULATED (max simultaneous) ===\n")
    local sk = {}; for k,_ in pairs(stageSeen) do sk[#sk+1]=k end; table.sort(sk)
    for _,st in ipairs(sk) do
      local parts = {}
      for _,p in ipairs(POOLS) do
        local m = stageSeen[st][p[1]]
        if m and m > 0 then parts[#parts+1] = string.format("%s=%d", p[1], m) end
      end
      f:write(string.format("stage %02X : %s\n", st, table.concat(parts, "  ")))
    end
    f:write("\n=== GLOBAL MAX per pool ===\n")
    for _,p in ipairs(POOLS) do f:write(string.format("  %-10s max=%d\n", p[1], poolMax[p[1]])) end
    f:close(); manager.machine:exit()
  end
  if fr > 40000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
print("arch_census running")
