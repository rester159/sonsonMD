-- Source B36F: kind2 barrier lifecycle, streamed and full background writes.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/barrier-lifecycle.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
local function hex(a,n)local s=''for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for variant=0,3 do for scenario=0,2 do
  for a=0,0xfff do m:write_u8(a,0)end
  local camera=scenario==2 and 65530 or 1024;local fraction=0
  m:write_u8(0x560,4);m:write_u8(0x567,variant);m:write_u8(0x569,2)
  m:write_u16(0x564,(camera+100)&65535);m:write_u8(0x566,104)
  for tick=0,511 do
   if tick%8==0 and m:read_u8(0x561)==3 then m:write_u8(0x561,4)end
   m:write_u16(0x90,camera);m:write_u8(0x24,tick&255);m:write_u8(0x8b,tick%3==0 and 1 or 0)
   for i=0x3a0,0x3df do m:write_u8(i,0x55)end
   for i=0x1000,0x17ff do m:write_u8(i,0x55)end
   pending={variant,scenario,tick,camera,hex(0x560,10)}
   for i=0x200,0x21f do m:write_u8(i,0x12)end
   for k,v in ipairs({0xbd,0xb3,0x6f,0x20,0xfe})do m:write_u8(0x21f+k,v)end
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
   if scenario~=0 then fraction=fraction+141;camera=(camera+(fraction>>8))&65535;fraction=fraction&255 end
  end
 end end
end)
emu.register_frame_done(function()
 if done then return end;frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x223,'return');local p=pending
   local pos=0x1000+18*32+((m:read_u8(0x565)&248)>>3)
   local patch='';for _,off in ipairs({0,-32,1,-31,1024,992,1025,993})do patch=patch..hex(pos+off,1)end
   f:write(string.format('{"variant":%d,"scenario":%d,"tick":%d,"camera":%d,"before":"%s","after":"%s","strip":"%s","patch":"%s"}\n',p[1],p[2],p[3],p[4],p[5],hex(0x560,10),hex(0x3ad,2)..hex(0x3cd,2),patch))
   pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('BARRIER_TRACE_PASS '..count);manager.machine:exit();return end
  local good,e=coroutine.resume(cases);assert(good,e)
 end)
 if not ok then done=true;f:close();print('BARRIER_TRACE_ERROR '..tostring(err));manager.machine:exit()end
end)
