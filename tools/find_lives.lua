-- find_lives.lua : locate the lives counter. Drive into gameplay, snapshot the
-- low DP/RAM, then provoke a death (stop pinning grace, walk idle) and diff which
-- byte decremented from its start value. Also log the HUD-ish counters.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local function press(p,f,v) local pt=mio.ports[p]; if pt then local fd=pt.fields[f]; if fd then fd:set_value(v) end end end
local function r8(a) return mem:read_u8(a) end
local OUT="D:/genesis ports/_capcom/son son/sonson_native/tools/find_lives.txt"
local out=io.open(OUT,"w")
local function log(s) print(s); out:write(s.."\n"); out:flush() end

local fr,phase,t0,play0=0,"attract",0,nil
local snap=nil
emu.register_frame_done(function()
  fr=fr+1
  local md=r8(0x0002)
  if phase=="attract" then
    if md==0x01 and fr>900 then phase="coin"; t0=fr end
  elseif phase=="coin" then
    press(":SYSTEM","Coin 1",((fr-t0)%24<6) and 1 or 0)
    if fr-t0>=96 then phase="start"; t0=fr; press(":SYSTEM","Coin 1",0) end
  elseif phase=="start" then
    press(":SYSTEM","1 Player Start",((fr-t0)%24<6) and 1 or 0)
    if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0) end
    if fr-t0>600 then log("STUCK"); manager.machine:exit() end
  elseif phase=="play" then
    -- first 200 frames: settle + snapshot whole DP page 0x00-0xFF and 0x40-0x80 scan
    if fr-play0==200 then
      snap={}
      for a=0,0xFF do snap[a]=r8(a) end
      log("snapshot taken at frame "..fr)
    end
    -- after snapshot: do NOTHING (no grace pin, no fire) and stand still so an
    -- enemy kills the hero; watch for any DP byte that decreases by 1 repeatedly.
    if snap and (fr-play0)>200 and (fr%30==0) then
      local diffs={}
      for a=0,0xFF do
        local v=r8(a)
        if v~=snap[a] then diffs[#diffs+1]=string.format("%02X:%02X->%02X",a,snap[a],v) end
      end
      -- only print a compact set of "candidate lives" (small values 0..7 that dropped)
      local cand={}
      for a=0,0xFF do
        local v=r8(a)
        if snap[a]>=1 and snap[a]<=7 and v==snap[a]-1 then cand[#cand+1]=string.format("%02X(%d->%d)",a,snap[a],v) end
      end
      log(string.format("fr=%d md=%02X life-cands:[%s]", fr, md, table.concat(cand,",")))
      if md~=0x02 then log("md left 02 -> game over reached at fr="..fr); manager.machine:exit() end
    end
    if fr-play0>3000 then log("done scan"); manager.machine:exit() end
  end
  if fr>30000 then manager.machine:exit() end
end)
emu.print_info("find_lives running")
