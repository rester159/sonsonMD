-- Original player-shot dispatcher B03A, preserving parent availability effects.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open("out/parity-audit/shot-oracle.jsonl","w"))
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 for facing=0,3 do for scenario=0,11 do
  local camera= scenario>=6 and 0xffc0 or 0x2345
  local delta=({0,8,100,232,240,255})[scenario%6+1]
  for a=0x4d0,0x4ef do m:write_u8(a,0)end
  m:write_u8(0x4d0,1);m:write_u16(0x4d5,(camera+delta)&65535);m:write_u8(0x4d7,255);m:write_u8(0x4d8,136);m:write_u8(0x4d9,127);m:write_u8(0x4da,facing)
  m:write_u8(0x4e0,0x46);m:write_u8(0x4e1,0xc2);m:write_u16(0x4de,0xafec);m:write_u8(0x49c,scenario%3)
  for tick=0,39 do
   if tick==8 and scenario%3==1 and m:read_u8(0x4d0)~=0 then m:write_u8(0x4d0,2);m:write_u16(0x4d1,0x0200)end
   if tick==12 and scenario%3==2 then camera=(camera+200)&65535 end
   m:write_u16(0x90,camera)
   pending={facing=facing,scenario=scenario,tick=tick,camera=camera,before=hex(0x4d0,32),available=m:read_u8(0x49c)}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   local code={0x8e,4,0xe0,0x10,0x8e,4,0x90,0xbd,0xb0,0x3a,0x20,0xfe}
   for i,v in ipairs(code)do m:write_u8(0x21f+i,v)end
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
   coroutine.yield()
  end
 end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x22a,'shot did not return');local p=pending
   f:write(string.format('{"facing":%d,"scenario":%d,"tick":%d,"camera":%d,"available":%d,"after_available":%d,"before":"%s","after":"%s"}\n',p.facing,p.scenario,p.tick,p.camera,p.available,m:read_u8(0x49c),p.before,hex(0x4d0,32)));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('SHOT_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('SHOT_ERROR',err);manager.machine:exit()end
end)
