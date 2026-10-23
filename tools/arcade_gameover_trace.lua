-- Source A98A after AE41 sets the player's out flag and state0400/kind0.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/gameover-oracle.jsonl','w'));local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 for teammate=0,1 do for owner=0,1 do for _,camera in ipairs({0,65520})do for _,xf in ipairs({0,255})do for _,timer in ipairs({0,4})do
  for a=0x480,0x4df do m:write_u8(a,0)end
  local p=0x490+owner*48;local q=0x490+(1-owner)*48
  m:write_u8(p-16,2);m:write_u8(p-15,4);m:write_u8(p-12,owner)
  m:write_u16(p-11,(camera+32)&65535);m:write_u8(p-9,xf);m:write_u8(p-8,248);m:write_u8(p-7,255);m:write_u8(p-6,1)
  m:write_u16(p-2,(owner==0 and 0xafac or 0xafd0)+3);m:write_u8(p-3,timer);m:write_u8(p,64+owner*8);m:write_u8(p+1,192+owner)
  m:write_u16(p+14,q);m:write_u8(q-13,1)
  if teammate==1 then
   m:write_u8(q-16,1);m:write_u8(q-13,0);m:write_u8(q-12,1-owner)
   m:write_u16(q-11,(camera+100)&65535);m:write_u8(q-8,104);m:write_u8(q-6,1)
   m:write_u16(q-2,owner==0 and 0xafd0 or 0xafac);m:write_u16(q+14,p)
  end
  m:write_u16(0x88,0);m:write_u8(0x88+owner,1);m:write_u16(0x90,camera);m:write_u8(0x9d,0)
  m:write_u16(0x44,0);m:write_u16(0x46,0)
  for tick=0,2 do
   pending={teammate=teammate,owner=owner,camera=camera,tick=tick,base=p,before=hex(p-16,48)}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   m:write_u8(0x220,0xbd);m:write_u16(0x221,0xa98a);m:write_u8(0x223,0x20);m:write_u8(0x224,0xfe)
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
   coroutine.yield()
  end
 end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then local p=pending;assert(cpu.state['CURPC'].value==0x223,'cleanup did not return')
   f:write(string.format('{"teammate":%d,"owner":%d,"camera":%d,"tick":%d,"before":"%s","after":"%s"}\n',p.teammate,p.owner,p.camera,p.tick,p.before,hex(p.base-16,48)));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('GAMEOVER_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('GAMEOVER_ERROR',err);manager.machine:exit()end
end)
