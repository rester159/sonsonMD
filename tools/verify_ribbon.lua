-- verify_ribbon.lua : VERIFY the SEAMLESS SINGLE-RIBBON scroll.
--
-- NOTE on snapshots: video:snapshot() needs a real render target. Run this with a
-- visible renderer to capture seam PNGs, e.g.
--   mame genesis -cart out/rom.bin -autoboot_script tools/verify_ribbon.lua \
--        -snapshot_directory tools/ribbon_snaps
-- Headless (-video none) the snapshot calls no-op/err, but the seam/loop/fps DATA
-- still logs. For a headless visual artifact use tools/render_seam.py (renders the
-- shipped ss_level maps -- the exact streamed tiles -- across every seam).
--
-- Boots SEGA->CAPCOM->selector->config->game (pulse START while g_levelW==0), then
-- HOLDS RIGHT for a long window and proves:
--   (1) SEAMLESS  -- the segment under the camera (g_levelIdx) advances 0->1->..->7
--                    continuously with no pause; snapshots taken AT several level
--                    seams (the frames either side of a segment change) show the
--                    scenery flowing across with no blank/garbage column.
--   (2) LOOPS     -- after segment 7 the ribbon returns to segment 0 (7->0 logged).
--   (3) 60fps     -- g_frameCounter advances 1:1 with emu frames across the whole
--                    held-RIGHT window, INCLUDING across seams (ratio ~1.000).
--
-- Symbols THIS build (out/symbol.txt):
--   g_levelW   @ 0xFF0072 (volatile u16)  -- 0 in front-end, ribbon width in-game
--   g_levelIdx @ 0xFF0075 (u8)            -- segment (baked level 0..7) under camera
--   g_frameCounter @ 0xFF007A (u32)       -- +1 per game frame (fps gauge)
--   g_game @ 0xFF07DC : lives@+30(u8) round@+36 stage@+37 cameraX@+38(s16)
--   (offsets confirmed empirically: lives drops on enemy contact @+30; cameraX low
--    byte @+39 climbs at run speed; round/stage step at each seam.)
local LEVELW  = 0xFF0072
local LEVELIDX= 0xFF0075
local GFC     = 0xFF007A
local GAME    = 0xFF07DC
local LIVES   = GAME + 30
local ROUND   = GAME + 36
local STAGE   = GAME + 37
local CAMX    = GAME + 38

local mem
local fr=0; local ig; local gfc0
local prevSeg=-1; local segChanges=0; local seq={}; local seenLoop=false
local snaps=0; local snapAtFr=nil; local lastSnapSeg=-2
local minRatioWin=2.0; local worstWinFr=0

local function rd16s(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end

-- rolling 120-frame fps window to catch any per-seam hitch
local winStart, winGfc
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; rel()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (fr%80)<12 then pad("P1 Start",1) end; return end
  if not ig then
    ig=fr; gfc0=mem:read_u32(GFC); winStart=fr; winGfc=mem:read_u32(GFC)
    print(string.format("INGAME f%d ribbonW=%d round=%d stage=%d seg=%d lives=%d",
      fr, lw, mem:read_u8(ROUND), mem:read_u8(STAGE), mem:read_u8(LEVELIDX), mem:read_u8(LIVES)))
  end
  if mem:read_u8(LIVES) < 6 then mem:write_u8(LIVES, 6) end   -- survive the traversal
  pad("P1 Right",1)

  local seg = mem:read_u8(LEVELIDX)

  -- detect a seam: schedule a "just after" snapshot 8 frames later (so the new
  -- segment's first columns have scrolled a few px in -> we see the join).
  if seg ~= prevSeg then
    segChanges = segChanges + 1
    seq[#seq+1]=seg
    print(string.format("f%-7d SEAM seg %d -> %d (#%d) camX=%d round=%d stage=%d",
      fr, prevSeg, seg, segChanges, rd16s(CAMX), mem:read_u8(ROUND), mem:read_u8(STAGE)))
    if prevSeg==7 and seg==0 then seenLoop=true; print("  >>> RIBBON LOOPED (seg 7 -> 0) <<<") end
    -- snapshot right AT the seam (this frame) and again 8 frames later
    if snaps < 8 and seg ~= lastSnapSeg then
      manager.machine.video:snapshot(); snaps=snaps+1; lastSnapSeg=seg
      print(string.format("  SNAP #%d at seam (seg %d, camX=%d)", snaps, seg, rd16s(CAMX)))
      snapAtFr = fr + 8
    end
    prevSeg=seg
  end
  if snapAtFr and fr==snapAtFr and snaps < 9 then
    manager.machine.video:snapshot(); snaps=snaps+1
    print(string.format("  SNAP #%d just-after-seam (seg %d, camX=%d)", snaps, seg, rd16s(CAMX)))
    snapAtFr=nil
  end

  -- rolling 120-frame fps window (worst-case across seams)
  if fr - winStart >= 120 then
    local g = mem:read_u32(GFC) - winGfc
    local r = g / (fr - winStart)
    if r < minRatioWin then minRatioWin = r; worstWinFr = fr end
    winStart=fr; winGfc=mem:read_u32(GFC)
  end

  -- run until we've LOOPED at least once (seg 7->0) plus a bit, or a hard cap.
  if (seenLoop and segChanges >= 9) or (fr >= ig + 40000) then
    local g=mem:read_u32(GFC)-gfc0; local e=fr-ig
    print(string.format("DONE %d emu-frames in-game, gfc+%d => %.4f game-frames/emu-frame", e, g, g/e))
    print("SEG sequence: "..table.concat(seq,","))
    print(string.format("seg-changes=%d looped(7->0)=%s", segChanges, tostring(seenLoop)))
    print(string.format("worst rolling-120 fps ratio = %.4f (at f%d)", minRatioWin, worstWinFr))
    print(string.format("final round=%d stage=%d", mem:read_u8(ROUND), mem:read_u8(STAGE)))
    manager.machine:exit()
  end
  if fr>=60000 then print("HARD TIMEOUT"); manager.machine:exit() end
end)
print("verify_ribbon running")
