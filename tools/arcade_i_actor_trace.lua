-- Original D317 four-actor dispatch. Isolated ordinary lifecycles, no ROM edits.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open("out/parity-audit/i-actor-oracle.jsonl","w"));local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for i=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+i))end;return table.concat(t)end
local cases=coroutine.create(function()
 for _,slot in ipairs({0,3})do for stopped=0,1 do for scenario=0,5 do
  for a=0xd98,0xe17 do m:write_u8(a,0)end
  local base=0xda8+slot*32;local camera=0x2345;local frac=0
  m:write_u8(base-16,128);m:write_u8(base-9,scenario%2==0 and 0 or 255);m:write_u8(base-7,scenario%2==0 and 0 or 255)
  m:write_u8(base-6,scenario%2==0 and 3 or 1);m:write_u8(0xac,0)
  for tick=0,255 do
   m:write_u16(0x90,camera);m:write_u8(0x9d,stopped);m:write_u8(0xa1,0)
   if tick==80 and scenario>=2 and m:read_u8(base-16)~=0 then m:write_u8(base-15,4);m:write_u8(base-14,scenario>=4 and 4 or 0);m:write_u8(base-12,scenario%2)end
   pending={slot,stopped,scenario,tick,camera,hex(base-16,32),m:read_u8(0xac),base}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   m:write_u8(0x220,0xbd);m:write_u16(0x221,0xd317);m:write_u8(0x223,0x20);m:write_u8(0x224,0xfe)
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
   coroutine.yield()
   if stopped==0 then frac=frac+141;camera=(camera+frac//256)%65536;frac=frac%256 end
  end
 end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x223,'actor did not return')
   local p=pending
   f:write(string.format('{"slot":%d,"stopped":%d,"scenario":%d,"tick":%d,"camera":%d,"before":"%s","live_before":%d,"after":"%s","live_after":%d}\n',p[1],p[2],p[3],p[4],p[5],p[6],p[7],hex(p[8]-16,32),m:read_u8(0xac)));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('I_ACTOR_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('I_ACTOR_ERROR',err);manager.machine:exit()end
end)
