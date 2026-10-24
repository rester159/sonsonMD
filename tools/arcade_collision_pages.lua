-- Independent source observation. Original arcade CPU executes all player logic.
-- Optional fixture clears hostile records to isolate locomotion; no player writes.
local m=manager.machine.devices[":maincpu"].spaces.program
local players=2
local path=os.getenv("SONSON_TRACE_OUT") or "out/parity-audit/arcade-players.csv"
local out=assert(io.open("out/parity-audit/collision-pages.csv","w"));out:write("clock,page\n")
local fr=0;local tick=-1;local phase=nil;local enter=nil;local selected=nil;local calls=0
local function pad(k,n,v)manager.machine.ioport.ports[":P"..k].fields["P"..k.." "..n]:set_value(v)end
local function system(n,v)manager.machine.ioport.ports[":SYSTEM"].fields[n]:set_value(v)end
emu.register_frame_done(function()
 fr=fr+1
 if fr==60 then
  enter=m:install_read_tap(0xe2b3,0xe2b3,"original-collision-entry",function()phase=m:read_u8(0x24)end)
  selected=m:install_write_tap(0xd5,0xd5,"original-collision-page",function(o,data,mask)
   if tick>=0 and phase and (manager.machine.devices[":maincpu"].state["CURPC"].value==0xe2bd or manager.machine.devices[":maincpu"].state["CURPC"].value==0xe311)then out:write(string.format("%d,%d\n",phase,data));calls=calls+1 end
  end)
 end
 for k=1,2 do for _,n in ipairs({"Left","Right","Up","Down","Button 1"})do pad(k,n,0)end end
 system("Coin 1",0);system("2 Players Start",0);system("1 Player Start",0)
 if tick<0 then
  if m:read_u8(2)==1 then
   local cycle=fr%240
   if cycle<20 or (cycle>=50 and cycle<70)then system("Coin 1",1)
   elseif cycle>=130 and cycle<150 then system(players==2 and "2 Players Start" or "1 Player Start",1)end
  end
  if m:read_u8(2)==2 and m:read_u8(0x481)==1 and m:read_u16(0x48b)==0xAA16 and (players==1 or m:read_u8(0x4b0)==1)then tick=0 end
 else tick=tick+1 end
 if tick>=0 then
  for a=0x818,0xdf8,0x20 do m:write_u8(a,0)end
  -- Independent directions, held climbs, reverse input and screen-boundary cases.
  local phase=tick%640
  if phase>=40 and phase<100 then pad(1,"Right",1)end
  if phase>=120 and phase<160 then pad(1,"Left",1)end
  if phase>=180 and phase<280 then pad(1,"Up",1)end
  if phase>=300 and phase<400 then pad(1,"Down",1)end
  if phase>=420 and phase<510 then pad(1,"Left",1)end
  if phase>=530 and phase<630 then pad(1,"Right",1)end
  if players==2 then
   if phase>=20 and phase<130 then pad(2,"Left",1)end
   if phase>=150 and phase<230 then pad(2,"Up",1)end
   if phase>=250 and phase<330 then pad(2,"Right",1)end
   if phase>=350 and phase<450 then pad(2,"Down",1)end
   if phase>=470 and phase<630 then pad(2,"Right",1)end
  end

 end
 if tick>=128 or fr>4500 then out:close();manager.machine:exit()end
end)
