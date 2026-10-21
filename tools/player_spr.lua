-- player_spr.lua : determine whether the PLAYER (SonSon) is drawn as a 2x2
-- metasprite, and identify the owner of the code-BA 32x32 block.
-- Reads the player record at $0490 (and $04C0) AND the live spriteram, and prints
-- the player's spr_code + screen pos alongside the playfield sprite list, every
-- ~30 frames, for the first few seconds of play.

local OUT = "D:/genesis ports/_capcom/son son/sonson_native/tools/player_spr.txt"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local f   = io.open(OUT, "w")
local function press(p, fld, v)
  local pt = mio.ports[p]; if pt then local fd = pt.fields[fld]; if fd then fd:set_value(v) end end
end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end
local SPR_BASE = 0x2020
local SCROLL = 0x0090

local fr, phase, t0, play0 = 0, "attract", 0, nil

local function dumpFrame(tag)
  local scroll = (r8(SCROLL)<<8)|r8(SCROLL+1)
  -- player records at $0490 stride $30 (2 players). record start = base; the
  -- player x register also points base+0x10 per the RE header.
  for pi=0,1 do
    local base = 0x0490 + pi*0x30
    local act  = r8(base)
    local xint = r8(base+0x05); local xsub=r8(base+0x06); local yint=r8(base+0x08)
    local code = r8(base+0x10); local attr=r8(base+0x11)
    local worldx=(xint<<8)|xsub
    local sx=(worldx-scroll)&0xFFFF; if sx>0x8000 then sx=sx-0x10000 end
    f:write(string.format("  PLAYER[%d] act=%02X scr=(%d,%d) code=%02X attr=%02X worldx=%04X\n",
      pi, act, sx, yint, code, attr, worldx))
  end
  -- full active spriteram (Y!=F8)
  local parts={}
  for i=0,23 do
    local b=SPR_BASE+i*4; local y=r8(b)
    if y~=0xF8 then parts[#parts+1]=string.format("#%d(y=%d x=%d c=%02X a=%02X)",i,y,r8(b+3),r8(b+1),r8(b+2)) end
  end
  f:write(string.format("fr=%d %s scroll=%04X spr: %s\n", fr, tag, scroll, table.concat(parts," ")))
  f:flush()
end

emu.register_frame_done(function()
  fr = fr + 1
  local md = r8(0x0002)
  if phase=="attract" then
    if md==0x01 and fr>700 then phase="coin"; t0=fr end
  elseif phase=="coin" then
    press(":SYSTEM","Coin 1",((fr-t0)%24<6) and 1 or 0)
    if fr-t0>=96 then phase="start"; t0=fr; press(":SYSTEM","Coin 1",0) end
  elseif phase=="start" then
    press(":SYSTEM","1 Player Start",((fr-t0)%24<6) and 1 or 0)
    if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0) end
    if fr-t0>600 then f:write("STUCK\n"); f:close(); manager.machine:exit() end
  elseif phase=="play" then
    w8(0x0073,0xFF)
    -- HOLD STILL the first 200 frames (see the spawn pose), then move right
    local e = fr-play0
    if e>200 then press(":P1","P1 Right",1) end
    if e%30==0 then dumpFrame(e<=200 and "[idle]" or "[run]") end
    if e>=1200 then phase="done" end
  elseif phase=="done" then
    f:close(); manager.machine:exit()
  end
  if fr>30000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
print("player_spr running")
