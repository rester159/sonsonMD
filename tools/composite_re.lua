-- composite_re.lua : DEFINITIVE check for the LARGE/COMPOSITE_E composite question.
--
-- Two goals, over a LONG driven arcade `sonson` playthrough that forces the stage
-- counter deep (past the natural ~stage 9 the prior census reached):
--   (1) POOL CENSUS: does classC ($0A28, the F2B4 formation-spawner target),
--       classE ($0B68), classD ($0AE8), classG ($0C28), obj16x16 ($0E48) EVER
--       become active in real md=02 gameplay?  Track per-stage + global max.
--   (2) SPRITERAM CLUSTER: scan the live sprite list ($2020..$207F, 24 sprites x
--       4 bytes = Y,code,attr,X) every frame.  A multi-sprite COMPOSITE body would
--       show >=3 hardware sprites sharing (nearly) the same X within a 16px column
--       AND a contiguous code run.  Record the worst (largest) such cluster seen
--       and a sample dump.  If max cluster is <=2 sprites everywhere, no on-screen
--       entity is a multi-sprite metasprite.
--
-- Drive: coin+start out of attract, then run RIGHT + fire + weave lanes, keep the
-- player's grace byte hot, and PIN the spawn-stage selector high to push deep
-- waves: poke $0096 (spawn stage, cap 0x1C) up over time, and $005A (stage) too.
--
-- $0096 = spawn stage (the wave selector F2B4 XORs into its layout pick).
-- $005A = stage counter (cap 0x1C=28).   $0067 = gfx level.   $0002 = mode (2=play).

local OUT = "D:/genesis ports/_capcom/son son/sonson_native/tools/composite_re.txt"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local f   = io.open(OUT, "w")
local function press(p, fld, v)
  local pt = mio.ports[p]; if pt then local fd = pt.fields[fld]; if fd then fd:set_value(v) end end
end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end

local POOLS = {
  {"enemyMain", 0x0828, 16, 0x20},
  {"classC",    0x0A28, 6,  0x20},   -- F2B4 formation-spawn target
  {"classD",    0x0AE8, 6,  0x20},
  {"classE",    0x0B68, 2,  0x20},   -- COMPOSITE_E
  {"classF",    0x0BA8, 5,  0x20},
  {"classG",    0x0C28, 6,  0x20},
  {"classH",    0x0CE8, 6,  0x20},
  {"classI",    0x0DA8, 6,  0x20},
  {"objSingle", 0x0E28, 4,  0x20},
  {"obj16x16",  0x0E48, 3,  0x30},   -- LARGE
  {"classK",    0x0ED8, 6,  0x20},
}

local function poolCount(base, n, stride)
  local c, idxs = 0, {}
  for s = 0, n-1 do
    local rb = base + s*stride
    if r8(rb - 0x10) ~= 0 then
      c = c + 1
      idxs[#idxs+1] = r8(rb - 0xF)
    end
  end
  return c, idxs
end

-- Spriteram cluster analysis. Read 24 sprites; an active sprite has Y != 0xF8
-- (the clear value). Find the largest group of active sprites whose X values fall
-- within a 24px window (a composite body spans ~16-32px) AND whose Y values fall
-- within a 32px window. Return {size, sampleString}.
local SPR_BASE = 0x2020
local function spriteCluster()
  local act = {}
  for i = 0, 23 do
    local b = SPR_BASE + i*4
    local y = r8(b+0); local code = r8(b+1); local attr = r8(b+2); local x = r8(b+3)
    if y ~= 0xF8 then act[#act+1] = {y=y, code=code, attr=attr, x=x, idx=i} end
  end
  -- brute force: for each sprite as anchor, count others within 24px x AND 32px y
  local best, bestList = 0, nil
  for _,a in ipairs(act) do
    local grp = {}
    for _,o in ipairs(act) do
      local dx = o.x - a.x; if dx < 0 then dx = -dx end
      local dy = o.y - a.y; if dy < 0 then dy = -dy end
      if dx <= 24 and dy <= 32 then grp[#grp+1] = o end
    end
    if #grp > best then best = #grp; bestList = grp end
  end
  local s = ""
  if bestList then
    local parts = {}
    for _,o in ipairs(bestList) do
      parts[#parts+1] = string.format("[#%d y=%d x=%d code=%02X attr=%02X]", o.idx, o.y, o.x, o.code, o.attr)
    end
    s = table.concat(parts, " ")
  end
  return best, s, #act
end

local stageSeen = {}
local poolMax   = {}
for _,p in ipairs(POOLS) do poolMax[p[1]] = 0 end
local clusterMax, clusterSample, clusterStage = 0, "", -1

local fr, phase, t0, play0 = 0, "attract", 0, nil
local lastStage = nil
local stageLog = {}

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
    w8(0x0073, 0xFF)   -- keep grace hot (stay alive)
    press(":P1", "P1 Up", 0); press(":P1", "P1 Down", 0)
    press(":P1", "P1 Left", 0); press(":P1", "P1 Right", 1)
    press(":P1", "P1 Button 1", ((fr % 8) < 3) and 1 or 0)
    local ph = (fr // 50) % 4
    if ph == 1 then press(":P1", "P1 Up", 1) elseif ph == 3 then press(":P1", "P1 Down", 1) end

    -- FORCE deep stage progression: every ~900 frames bump $0096 and $005A so the
    -- wave selector cycles through ALL 8 F2F8 layouts and reaches the deep waves.
    local elapsed = fr - play0
    if elapsed > 1200 and (elapsed % 900) == 0 then
      local s96 = r8(0x0096); if s96 < 0x1C then w8(0x0096, s96 + 1) end
      local s5a = r8(0x005A); if s5a < 0x1C then w8(0x005A, s5a + 1) end
    end

    local stage = r8(0x0096)
    local s5a   = r8(0x005A)
    local lvl67 = r8(0x0067)
    if stage ~= lastStage then
      stageLog[#stageLog+1] = string.format("fr=%d stage96=%02X 5A=%02X gfx67=%02X", fr, stage, s5a, lvl67)
      lastStage = stage
    end
    stageSeen[stage] = stageSeen[stage] or {}
    for _,p in ipairs(POOLS) do
      local c = poolCount(p[2], p[3], p[4])
      if c > (stageSeen[stage][p[1]] or 0) then stageSeen[stage][p[1]] = c end
      if c > poolMax[p[1]] then poolMax[p[1]] = c end
    end

    -- spriteram cluster (every frame; record worst)
    local csz, csample = spriteCluster()
    if csz > clusterMax then clusterMax = csz; clusterSample = csample; clusterStage = stage end

    if fr % 120 == 0 then
      local parts = {}
      for _,p in ipairs(POOLS) do
        local c, idxs = poolCount(p[2], p[3], p[4])
        if c > 0 then parts[#parts+1] = string.format("%s=%d", p[1], c) end
      end
      local _, _, nact = spriteCluster()
      f:write(string.format("fr=%d s96=%02X 5A=%02X g67=%02X spr_active=%d clusterMaxSoFar=%d | %s\n",
        fr, stage, s5a, lvl67, nact, clusterMax, table.concat(parts, " ")))
      f:flush()
    end

    if play0 and fr - play0 >= 20000 then phase = "done" end
  elseif phase == "done" then
    f:write("\n=== STAGE PROGRESSION ===\n")
    for _,l in ipairs(stageLog) do f:write(l.."\n") end
    f:write("\n=== PER-STAGE POOLS (max simultaneous active) ===\n")
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
    f:write("\n=== SPRITERAM CLUSTER ===\n")
    f:write(string.format("max co-located hardware sprites (within 24px X, 32px Y): %d  (at stage %02X)\n", clusterMax, clusterStage))
    f:write("sample: " .. clusterSample .. "\n")
    f:write("(>=3 with a contiguous code run would indicate a multi-sprite composite body;\n")
    f:write(" <=2 means every on-screen entity is a single 16x16 sprite.)\n")
    f:close(); manager.machine:exit()
  end
  if fr > 60000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
print("composite_re running")
