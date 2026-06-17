-- arch_stageforce.lua : FORCE the spawn-stage $0096/$005A to a target value during
-- md=02 gameplay and observe which entity POOLS populate at that stage, to map the
-- full enemy variety across ALL stage groups (incl. the deep ones the AI player
-- can't naturally reach).  Set SS_STAGE env (hex) to the stage to force.
--
-- We write $0096 + $005A every play-frame (the wave dispatch reads $0096; $005A is
-- the round/stage counter). $0067 (gfx level) is left for the engine. We log per
-- pool active counts + dispatch indices, and the spawn-list pointer the wave dispatch
-- selected (so we can confirm the type/idx mix).

local STG = tonumber(os.getenv("SS_STAGE") or "10", 16)
local OUT = string.format("D:/genesis ports/_capcom/son son/sonson_native/tools/arch_stage_%02X.txt", STG)
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
  {"classC",    0x0A28, 6,  0x20},
  {"classD",    0x0AE8, 6,  0x20},
  {"classE",    0x0B68, 2,  0x20},
  {"classF",    0x0BA8, 5,  0x20},
  {"classG",    0x0C28, 6,  0x20},
  {"classH",    0x0CE8, 6,  0x20},
  {"classI",    0x0DA8, 6,  0x20},
  {"objSingle", 0x0E28, 4,  0x20},
  {"obj16x16",  0x0E48, 3,  0x30},
}
local function poolCount(base, n, stride)
  local c, idxs = 0, {}
  for s = 0, n-1 do
    local rb = base + s*stride
    if r8(rb - 0x10) ~= 0 then c = c + 1; idxs[#idxs+1] = r8(rb - 0xF) end
  end
  return c, idxs
end
local poolMax = {}
for _,p in ipairs(POOLS) do poolMax[p[1]] = 0 end

local fr, phase, t0, play0 = 0, "attract", 0, nil
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
    w8(0x0073, 0xFF)          -- keep grace hot
    w8(0x0096, STG)           -- FORCE spawn stage
    w8(0x005A, STG)           -- and the stage counter
    press(":P1", "P1 Up", 0); press(":P1", "P1 Down", 0)
    press(":P1", "P1 Left", 0); press(":P1", "P1 Right", 1)
    press(":P1", "P1 Button 1", ((fr % 10) < 3) and 1 or 0)
    local ph = (fr // 70) % 4
    if ph == 1 then press(":P1", "P1 Up", 1) elseif ph == 3 then press(":P1", "P1 Down", 1) end

    for _,p in ipairs(POOLS) do
      local c = poolCount(p[2], p[3], p[4])
      if c > poolMax[p[1]] then poolMax[p[1]] = c end
    end
    if fr % 60 == 0 then
      local parts = {}
      for _,p in ipairs(POOLS) do
        local c, idxs = poolCount(p[2], p[3], p[4])
        if c > 0 then parts[#parts+1] = string.format("%s=%d{%s}", p[1], c, table.concat(idxs, ",")) end
      end
      f:write(string.format("fr=%d stage=%02X gfx67=%02X | %s\n", fr, r8(0x0096), r8(0x0067), table.concat(parts, " ")))
      f:flush()
    end
    if play0 and fr - play0 >= 3000 then phase = "done" end
  elseif phase == "done" then
    f:write(string.format("\n=== STAGE %02X GLOBAL MAX per pool ===\n", STG))
    for _,p in ipairs(POOLS) do f:write(string.format("  %-10s max=%d\n", p[1], poolMax[p[1]])) end
    f:close(); manager.machine:exit()
  end
  if fr > 12000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
print(string.format("arch_stageforce stage=%02X running", STG))
