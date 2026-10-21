-- spawn_sched_cap.lua : EMPIRICALLY capture SonSon's per-level enemy spawn
-- schedule from REAL md=02 gameplay (MAME `sonson`), confirming the static RE of
-- the $F0C9 spawn tables.  Uses the PROVEN ss_level_cap2 phase machine that keys
-- on $0002 (md): md==1 attract, md==2 in-game.
--
-- RE (decoded from ss.03e @ $C000): wave dispatch sub_EFED reads $0638 (phase) ->
-- sub_F065 computes list ptr u = [$F0C9 + ((stage&0x30)>>1) + (($23^$24)&6)],
-- stage=$0096.  List = 12 (Y_byte,type_byte) pairs; sub_F080 spawns at
-- worldX=scroll($90)+$F8 into $0828 (16 slots, stride $20). type_byte->[+3,u].
-- On-screen cap: F080 gates [$00A6] (live count) != $10 (=16, pool full).
--
-- $0828 record: active@u-0x10($0818+s*0x20), Y@u-0x08, type@u+0x03; stride 0x20.
-- We PIN $0067=SS_LVL each play-frame so that level's stage feeds $0096.
--
-- Output: tools/spawn_cap_L<N>.txt

local LVL = tonumber(os.getenv("SS_LVL") or "0")
local OUT = string.format("D:/genesis ports/_capcom/son son/sonson_native/tools/spawn_cap_L%d.txt", LVL)
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local f   = io.open(OUT, "w")
local function press(p, fld, v)
  local pt = mio.ports[p]; if pt then local fd = pt.fields[fld]; if fd then fd:set_value(v) end end
end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end

local POOL, NSLOTS, STRIDE = 0x0828, 16, 0x20

local fr, phase, t0, play0 = 0, "attract", 0, nil
local maxLive, firstStage = 0, nil
local typeSeen, ySeen = {}, {}

emu.register_frame_done(function()
  fr = fr + 1
  local md = r8(0x0002)

  if phase == "attract" then
    if md == 0x01 and fr > 1100 then phase = "coin"; t0 = fr end
  elseif phase == "coin" then
    press(":SYSTEM", "Coin 1", ((fr - t0) % 24 < 6) and 1 or 0)
    if fr - t0 >= 96 then phase = "start"; t0 = fr; press(":SYSTEM", "Coin 1", 0) end
  elseif phase == "start" then
    press(":SYSTEM", "1 Player Start", ((fr - t0) % 24 < 6) and 1 or 0)
    if md == 0x02 then phase = "play"; t0 = fr; play0 = fr; press(":SYSTEM", "1 Player Start", 0) end
    if fr - t0 > 600 then f:write("STUCK at start\n"); f:close(); manager.machine:exit() end
  elseif phase == "play" then
    w8(0x0067, LVL)        -- pin the level index
    w8(0x0073, 0xFF)       -- keep the player's grace byte hot so a hit won't end the run
    -- drive a forward run + fire so the camera scrolls and waves keep coming
    press(":P1", "P1 Up", 0); press(":P1", "P1 Down", 0)
    press(":P1", "P1 Left", 0); press(":P1", "P1 Right", 1)
    press(":P1", "P1 Button 1", ((fr % 12) < 3) and 1 or 0)
    -- occasional climb to ride different lanes
    if (fr // 90) % 3 == 1 then press(":P1", "P1 Up", 1) end

    if fr % 3 == 0 then
      local stage = r8(0x0096)
      local wphase = r8(0x0638)
      local live   = r8(0x00A6)
      if firstStage == nil then firstStage = stage end
      local n, types = 0, {}
      for s = 0, NSLOTS - 1 do
        local rb = POOL + s * STRIDE
        if r8(rb - 0x10) ~= 0 then
          n = n + 1
          local ty, yy = r8(rb + 0x03), r8(rb - 0x08)
          types[#types+1] = string.format("%02X@%02X", ty, yy)
          typeSeen[ty] = (typeSeen[ty] or 0) + 1
          ySeen[yy] = (ySeen[yy] or 0) + 1
        end
      end
      if n > maxLive then maxLive = n end
      f:write(string.format("fr=%d stage=%02X wphase=%02X liveA6=%02X poolN=%d [%s]\n",
                            fr, stage, wphase, live, n, table.concat(types, " ")))
      f:flush()
    end
    if md ~= 0x02 then
      f:write(string.format("left gameplay md=%02X at fr=%d\n", md, fr))
    end
    if play0 and fr - play0 >= 1400 then phase = "done" end
  elseif phase == "done" then
    f:write(string.format("\n=== SUMMARY level %d ===\n", LVL))
    f:write(string.format("firstStage(0096)=%s  MAX simultaneous 0828=%d\n", tostring(firstStage), maxLive))
    local tk = {}; for k,_ in pairs(typeSeen) do tk[#tk+1] = k end; table.sort(tk)
    for _,k in ipairs(tk) do f:write(string.format("  type %02X seen %d sample-frames\n", k, typeSeen[k])) end
    local yk = {}; for k,_ in pairs(ySeen) do yk[#yk+1] = k end; table.sort(yk)
    local ys = {}; for _,k in ipairs(yk) do ys[#ys+1] = string.format("%02X", k) end
    f:write("  Y positions seen: " .. table.concat(ys, " ") .. "\n")
    f:close(); manager.machine:exit()
  end
  if fr > 30000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
