-- Real input probe: start 1P, insert a coin during gameplay, press 2P Start.
local m=manager.machine.devices[":maincpu"].spaces.program
local f=assert(io.open('out/parity-audit/join-probe.csv','w'));f:write('frame,tick,mode,out1,out2,active1,active2,credits,system\n')
local fr=0;local tick=-1
local function system(n,v)manager.machine.ioport.ports[":SYSTEM"].fields[n]:set_value(v)end
emu.register_frame_done(function()
 fr=fr+1;system('Coin 1',0);system('1 Player Start',0);system('2 Players Start',0)
 if tick<0 then
  if m:read_u8(2)==1 then
   local cycle=fr%240
   if cycle<20 then system('Coin 1',1)elseif cycle>=130 and cycle<150 then system('1 Player Start',1)end
  end
  if m:read_u8(2)==2 and m:read_u8(0x481)==1 and m:read_u16(0x48b)==0xaa16 then tick=0 end
 else tick=tick+1 end
 if tick>=0 then
  -- Isolate start-button behavior from enemy deaths; leave players unmodified.
  for a=0x818,0xdf8,0x20 do m:write_u8(a,0)end
  if tick>=40 and tick<60 then system('Coin 1',1)end
  if tick>=100 and tick<120 or tick>=200 and tick<220 then system('2 Players Start',1)end
  f:write(string.format('%d,%d,%d,%d,%d,%d,%d,%d,%d\n',fr,tick,m:read_u8(0x99),m:read_u8(0x88),m:read_u8(0x89),m:read_u8(0x480),m:read_u8(0x4b0),m:read_u16(0x25),m:read_u8(0x3c)))
  if tick==300 then f:close();print('JOIN_PROBE_DONE');manager.machine:exit()end
 end
 if fr>3000 then f:close();print('JOIN_PROBE_TIMEOUT');manager.machine:exit()end
end)
