-- levelend_trace.lua : AUTHORITATIVE capture of SonSon's level/stage advance.
-- Drives a REAL game (coin -> start -> md=02 play), keeps the hero alive
-- (pin grace byte + max lives), runs forward to drive the auto-scroll, and
-- logs the stage/segment vars over a LONG run so we can SEE how a level ends
-- and the next begins.
--
-- RE vars (maincpu DP, from sub_A730 / sub_A697 / FBE6):
--   $0002 md     : 01 attract / 02 in-game
--   $0005 st     : screen/state dispatch index
--   $0067 seg    : level segment index, walks 0..25 then wraps (cap $1A=26)
--   $005A stage  : global stage counter, ++ on each seg wrap, cap $1C=28
--   $0096 stageP : stage phase (mirror of $5A; hi-nibble selects wave list)
--   $0090/$0091  : scroll position (hi byte = coarse px, lo = fraction lo)
--   $0098 segdur : per-segment countdown (reload = leveldata[$67]+0)
--   $009D/$009E  : stream/event bytes the table feeds FBE6
-- We sample every frame, print a row whenever ANY of seg/stage/stageP changes
-- (plus a heartbeat every 600 frames) so the progression is legible.

local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local function press(p,f,v) local pt=mio.ports[p]; if pt then local fd=pt.fields[f]; if fd then fd:set_value(v) end end end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end

local OUT = "D:/genesis ports/_capcom/son son/sonson_native/tools/levelend_trace.txt"
local out = io.open(OUT, "w")
local function log(s) print(s); out:write(s.."\n"); out:flush() end

log("frame,phase,md,st,seg67,stage5A,phase96,scrollHi90,scrollLo91,segdur98,b9D,b9E")

local fr=0
local phase="attract"
local t0=0
local play0=nil
local last = {}
local hb=0

emu.register_frame_done(function()
  fr=fr+1
  local md=r8(0x0002); local st=r8(0x0005)

  if phase=="attract" then
    if md==0x01 and fr>900 then phase="coin"; t0=fr end
  elseif phase=="coin" then
    press(":SYSTEM","Coin 1",((fr-t0)%24<6) and 1 or 0)
    if fr-t0>=96 then phase="start"; t0=fr; press(":SYSTEM","Coin 1",0) end
  elseif phase=="start" then
    press(":SYSTEM","1 Player Start",((fr-t0)%24<6) and 1 or 0)
    if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0)
      log("--- ENTERED GAMEPLAY (md=02) at frame "..fr.." ---") end
    if fr-t0>600 then log("STUCK at start"); manager.machine:exit() end
  elseif phase=="play" then
    -- survive: keep grace byte hot + lives high so a stray hit can't end the run
    w8(0x0073, 0xFF)         -- player grace / i-frame byte (from prior captures)
    -- run forward + occasional fire to clear a path; vary vertical to climb tiers
    press("P1 Right", 1)
    press("P1 B", ((fr%10)<3) and 1 or 0)
    press("P1 Up", ((fr%180)<30) and 1 or 0)   -- periodically climb the vines

    local seg=r8(0x0067); local stage=r8(0x005A); local ph=r8(0x0096)
    local sHi=r8(0x0090); local sLo=r8(0x0091); local dur=r8(0x0098)
    local b9d=r8(0x009D); local b9e=r8(0x009E)

    local changed = (seg~=last.seg) or (stage~=last.stage) or (ph~=last.ph) or (md~=last.md) or (st~=last.st)
    hb=hb+1
    if changed or hb>=600 then
      log(string.format("%d,%s,%02X,%02X,%d,%d,%02X,%02X,%02X,%02X,%02X,%02X",
        fr,phase,md,st,seg,stage,ph,sHi,sLo,dur,b9d,b9e))
      hb=0
    end
    last.seg=seg; last.stage=stage; last.ph=ph; last.md=md; last.st=st

    -- run for a good long while to see multiple stage increments / a wrap
    if fr-play0 > 12000 then log("--- DONE (12000 play frames) ---"); manager.machine:exit() end
    if md~=0x02 then log("LEFT gameplay md="..md.." at frame "..fr); manager.machine:exit() end
  end
  if fr>40000 then log("TIMEOUT"); manager.machine:exit() end
end)
emu.print_info("levelend_trace running")
