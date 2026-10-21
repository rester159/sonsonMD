-- levelend_trace2.lua : LONG natural-progression trace of SonSon's stage/segment
-- advance, using the proven survival recipe (pin grace $0073 + don't run blindly
-- into contact: fire constantly, weave vertically). Logs seg/stage/phase/scroll
-- on every change + heartbeat. Goal: witness $67 wrap (25->0) and $5A climb toward
-- its $1C cap, and confirm whether a discrete "level end" event ever occurs.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local function press(p,f,v) local pt=mio.ports[p]; if pt then local fd=pt.fields[f]; if fd then fd:set_value(v) end end end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end
local OUT="D:/genesis ports/_capcom/son son/sonson_native/tools/levelend_trace2.txt"
local out=io.open(OUT,"w")
local function log(s) print(s); out:write(s.."\n"); out:flush() end

log("frame,md,st,seg67,stage5A,phase96,scrHi90,scrLo91,segdur98")
local fr,phase,t0,play0=0,"attract",0,nil
local last={}
local hb=0
local segwraps=0
local maxstage=0
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
    if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0); log("--- GAMEPLAY @ "..fr.." ---") end
    if fr-t0>600 then log("STUCK"); manager.machine:exit() end
  elseif phase=="play" then
    -- survival: keep grace hot; constant fire; gentle weave (don't barrel into mobs)
    w8(0x0073,0xFF)
    press(":P1","P1 Right", ((fr%4)<3) and 1 or 0)   -- mostly forward, brief pauses
    press(":P1","P1 B", 1)                            -- continuous fire
    local ph=(fr//90)%4
    press(":P1","P1 Up",   (ph==1) and 1 or 0)
    press(":P1","P1 Down", (ph==3) and 1 or 0)

    local seg=r8(0x0067); local stage=r8(0x005A); local p96=r8(0x0096)
    local sHi=r8(0x0090); local sLo=r8(0x0091); local dur=r8(0x0098)
    if stage>maxstage then maxstage=stage end
    if last.seg~=nil and seg==0 and last.seg==25 then segwraps=segwraps+1; log("*** SEG WRAP #"..segwraps.." (67:25->0) @ fr="..fr.." stage="..stage.." ***") end
    local changed=(seg~=last.seg) or (stage~=last.stage) or (p96~=last.p96) or (st~=last.st) or (md~=last.md)
    hb=hb+1
    local st=r8(0x0005)
    if changed or hb>=900 then
      log(string.format("%d,%02X,%02X,%d,%d,%02X,%02X,%02X,%02X",fr,md,st,seg,stage,p96,sHi,sLo,dur)); hb=0
    end
    last.seg=seg; last.stage=stage; last.p96=p96; last.st=st; last.md=md
    if md~=0x02 then log("LEFT gameplay (death/over) md="..md.." @ fr="..fr.." maxstage="..maxstage.." segwraps="..segwraps); manager.machine:exit() end
    if fr-play0>18000 then log("--- DONE 18000 frames; maxstage="..maxstage.." segwraps="..segwraps.." ---"); manager.machine:exit() end
  end
  if fr>50000 then log("TIMEOUT"); manager.machine:exit() end
end)
emu.print_info("levelend_trace2 running")
