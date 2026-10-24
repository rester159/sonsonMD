-- Independent source observation. Original arcade CPU executes all player logic.
-- Optional fixture clears hostile records to isolate locomotion; no player writes.
local m=manager.machine.devices[":maincpu"].spaces.program
local players=tonumber(os.getenv("SONSON_TRACE_PLAYERS") or "1")
local path=os.getenv("SONSON_TRACE_OUT") or "out/parity-audit/arcade-players.csv"
local out=assert(io.open(path,"w"));local fr=0;local tick=-1
local fields={"active","state","phase","kind","x","xf","y","yf","face","held","previous","table","attached","timer","stamp","anim_cursor","anim_timer","sprite_code","sprite_attr"}
local header={"frame","tick","scroll","frac","ring"}
for k=1,players do for _,name in ipairs(fields) do header[#header+1]="p"..k.."_"..name end end
header[#header+1]="collision";out:write(table.concat(header,",").."\n")
local function pad(k,n,v)manager.machine.ioport.ports[":P"..k].fields["P"..k.." "..n]:set_value(v)end
local function system(n,v)manager.machine.ioport.ports[":SYSTEM"].fields[n]:set_value(v)end
local function actor(base,v)
 for _,o in ipairs({-16,-15,-14,-13})do v[#v+1]=m:read_u8(base+o)end
 v[#v+1]=m:read_u16(base-11)
 for _,o in ipairs({-9,-8,-7,-6,8,9})do v[#v+1]=m:read_u8(base+o)end
 v[#v+1]=m:read_u16(base-5)
 for _,o in ipairs({13,10,10})do v[#v+1]=m:read_u8(base+o)end
 v[#v+1]=m:read_u16(base-2)
 for _,o in ipairs({-3,0,1})do v[#v+1]=m:read_u8(base+o)end
end
emu.register_frame_done(function()
 fr=fr+1
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
  local v={fr,tick,m:read_u16(0x90),m:read_u8(0x92),m:read_u8(0x8a)}
  actor(0x490,v);if players==2 then actor(0x4c0,v)end
  local bytes={}for a=0x100,0x17f do bytes[#bytes+1]=string.format("%02x",m:read_u8(a))end
  v[#v+1]=table.concat(bytes);out:write(table.concat(v,",").."\n")
 end
 if tick>=1280 or fr>4500 then out:close();manager.machine:exit()end
end)
