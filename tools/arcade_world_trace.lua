local m=manager.machine.devices[":maincpu"].spaces.program
local f=assert(io.open("out/parity-audit/world-oracle.csv","w"));f:write("stage,col,codes,attributes,collision\n")
local fr=0;local stage=0;local col=0;local started=false;local pending=false
local function sys(n,v)manager.machine.ioport.ports[":SYSTEM"].fields[n]:set_value(v)end
local function initialize()
 local rec=0x6485+stage*7
 m:write_u8(0x67,stage);m:write_u16(0x94,m:read_u16(rec+3));m:write_u16(0x8e,m:read_u16(rec+5));m:write_u8(0x8a,0);m:write_u8(0x98,5)
end
emu.register_frame_done(function()
 fr=fr+1;sys("Coin 1",0);sys("1 Player Start",0)
 if not started then
  if m:read_u8(2)==1 then if fr%140<25 then sys("Coin 1",1)elseif fr%140>=60 and fr%140<90 then sys("1 Player Start",1)end end
  if m:read_u8(2)==2 and m:read_u16(0x48b)==0xaa16 then started=true;initialize()end
 else
  if pending then
   local codes={};local attrs={};local collision={}
   for i=0,26 do codes[#codes+1]=string.format("%02x",m:read_u8(0x3a0+i));attrs[#attrs+1]=string.format("%02x",m:read_u8(0x3c0+i))end
   for i=0,3 do collision[#collision+1]=string.format("%02x",m:read_u8(0x100+col*4+i))end
   f:write(string.format("%d,%d,%s,%s,%s\n",stage,col,table.concat(codes),table.concat(attrs),table.concat(collision)))
   col=col+1
   if col==32 then stage=stage+1;col=0;if stage==26 then f:close();manager.machine:exit();return end;initialize()end
  end
 end
 if started then
  -- Accelerated source column-stream fixture. Original CPU expands every column.
  m:write_u16(0x91,0x7ff);m:write_u8(0x93,8);m:write_u8(0x9d,0)
  for a=0x818,0xdf8,0x20 do m:write_u8(a,0)end
  pending=true
 end
 if fr>5000 then f:close();manager.machine:exit()end
end)
