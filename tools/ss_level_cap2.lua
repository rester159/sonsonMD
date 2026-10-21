-- ss_level_cap2.lua : reconstruct a WIDE SonSon level-N background from REAL
-- GAMEPLAY (md=02, NOT the attract demo), so the playfield is CLEAN -- no
-- "PLEASE DEPOSIT COIN" / title art (those exist only in the md=01 attract demo
-- that the original ss_level_cap.lua stitched).
--
-- LEVEL SELECT: $0067 is the round/level index (RE: sub_8FBC `ldb <$67; lda #7;
-- mul; ldx #$6485; leax d,x; ldd 3,x -> $94/$95 level-source ptr`). The arcade
-- always starts a fresh game at round 0; we PIN $0067 = SS_LVL (env) every play
-- frame so the column streamer feeds level SS_LVL's ROM data into videoram, and
-- we stitch that. (Verified: pinning $67 makes $94/$95 track leveldata[SS_LVL].)
--
-- STITCH (same mechanism as ss_level_cap.lua, confirmed by ss_scroll_probe):
--   BG = 32-col window (videoram 0x1000, colorram 0x1400, row-major 32x32). The
--   scroll word $0090/$0091 (px) slides it (wraps mod 32 cols). At scroll s,
--   display column vc shows absolute level column absC=(s>>3)+vc, read from
--   videoram wrap-column (absC % 32). We sample every frame, writing each observed
--   cell into wide[absC][row].
--
-- CLEAN-CAPTURE RULE: the round-intro banner ("LET'S GO TENJIKU" / "GOOD LUCK")
-- is an overlay drawn over the CENTER of the screen at the level start. It stays
-- pinned to the SCREEN while the level columns scroll under it, then auto-clears.
-- Eyewitnessed: present through scroll~119, GONE by scroll~152. So we begin the
-- stitch only once scroll >= CLEAN_SCROLL (banner fully gone) and from then on
-- every visible column is clean level data -> capture the FULL window each frame.
-- The leftmost columns that scrolled past UNDER the banner (abs cols 0..~CLEAN
-- /8+31) are intentionally dropped; write_json RENUMBERS the first clean column to
-- level col 0, so the baked level is a contiguous, fully-clean playfield that
-- starts a little way into the arcade level (the hero always starts at the band
-- left and runs right -- seamless). CLEAN over count, per the task.
--
-- Output: tools/levelN_capture.json (N = SS_LVL) with the wide (code,attr) map.

local LVL = tonumber(os.getenv("SS_LVL") or "0")
local OUT = string.format("D:/genesis ports/_capcom/son son/sonson_native/tools/level%d_capture.json", LVL+1)
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local function press(p,f,v) local pt=mio.ports[p]; if pt then local fd=pt.fields[f]; if fd then fd:set_value(v) end end end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end

local CLEAN_SCROLL = 170     -- begin stitching once scrolled past the banner (gone by ~152)

local wideV, wideC, seen = {}, {}, {}
local minAbs, maxAbs = 1e9, -1

-- capture the FULL 32-col window (all clean once scroll>=CLEAN_SCROLL)
local function capture_window(s)
  local base = s >> 3
  for vc = 0, 31 do
    local absC = base + vc
    local wrap = absC % 32
    if not seen[absC] then
      local cv, cc = {}, {}
      for row = 0, 31 do
        cv[row+1] = r8(0x1000 + row*32 + wrap)
        cc[row+1] = r8(0x1400 + row*32 + wrap)
      end
      wideV[absC] = cv; wideC[absC] = cc; seen[absC] = true
      if absC > maxAbs then maxAbs = absC end
      if absC < minAbs then minAbs = absC end
    end
  end
end

local function write_json()
  -- find the contiguous covered run starting at the first captured column (minAbs)
  local start = minAbs
  local cols = 0
  while seen[start + cols] do cols = cols + 1 end
  -- helper: renumbered column c (0..cols-1) maps to absolute (start + c)
  local function A(c) return start + c end
  local f = io.open(OUT, "w")
  f:write("{\n")
  f:write(string.format('  "source": "sonson arcade REAL GAMEPLAY md=02 level index %d ($0067=%d, clean playfield)",\n', LVL, LVL))
  f:write('  "method": "32-col window, right-edge columns stitched by absolute column from live md=02 play; $0067 pinned to select the level; intro-banner frames skipped (scroll>=200)",\n')
  f:write('  "level_cols": ' .. cols .. ',\n')
  f:write('  "rows": 32,\n')
  f:write(string.format('  "abs_start_col": %d,\n', start))
  f:write('  "videoram": [')
  local first = true
  for row = 0, 31 do for c = 0, cols-1 do
    if not first then f:write(",") end; first=false; f:write(tostring(wideV[A(c)][row+1]))
  end end
  f:write("],\n")
  f:write('  "colorram": [')
  first = true
  for row = 0, 31 do for c = 0, cols-1 do
    if not first then f:write(",") end; first=false; f:write(tostring(wideC[A(c)][row+1]))
  end end
  f:write("],\n")
  local cset = {}
  for row = 0, 31 do for c = 0, cols-1 do
    local v=wideV[A(c)][row+1]; local a=wideC[A(c)][row+1]
    cset[(v + 256*(a%4)) .. ":" .. math.floor(a/4)] = true
  end end
  local nb=0; for _ in pairs(cset) do nb=nb+1 end
  f:write('  "char_used_count": ' .. nb .. '\n')
  f:write("}\n")
  f:close()
  print(string.format("WROTE %s : %d cols (%.2f screens), absStart=%d, absMax=%d, distinct char=%d",
    OUT, cols, cols/32.0, start, maxAbs, nb))
end

local fr=0; local phase="attract"; local t0=0; local play0=nil
emu.register_frame_done(function()
  fr=fr+1
  local st=r8(0x0005); local md=r8(0x0002)
  if phase=="attract" then
    if md==0x01 and fr>1100 then phase="coin"; t0=fr end
  elseif phase=="coin" then
    press(":SYSTEM","Coin 1",((fr-t0)%24<6) and 1 or 0)
    if fr-t0>=96 then phase="start"; t0=fr; press(":SYSTEM","Coin 1",0) end
  elseif phase=="start" then
    press(":SYSTEM","1 Player Start",((fr-t0)%24<6) and 1 or 0)
    if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0) end
    if fr-t0>600 then print("STUCK start"); manager.machine:exit() end
  elseif phase=="play" then
    w8(0x0067, LVL)                      -- PIN the level index every frame
    press("P1 Right", 1)                  -- run forward (drives the scroll)
    press("P1 B", ((fr%12)<3) and 1 or 0) -- fire to clear a path forward
    -- keep invuln-ish: re-pin the grace byte so a stray hit doesn't end the run
    w8(0x0073, 0xFF)

    local scroll = (r8(0x0090) << 8) | r8(0x0091)
    if st == 0x01 and scroll >= CLEAN_SCROLL then
      capture_window(scroll)
    end

    -- finish when the scroll stops advancing near the end (held >= ~1300) for a
    -- while, or on leaving gameplay -- whichever first.
    if scroll >= 1300 then
      if not _end_at then _end_at = fr + 120 end   -- let the last columns settle
      if fr >= _end_at then write_json(); manager.machine:exit() end
    end
    if md ~= 0x02 then
      print("left gameplay early md="..md.." scroll="..scroll)
      write_json(); manager.machine:exit()
    end
  end
  if fr > 30000 then print("TIMEOUT"); write_json(); manager.machine:exit() end
end)
emu.print_info("ss_level_cap2 running lvl="..LVL)
