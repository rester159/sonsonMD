-- Source E1AF: missed-fruit carrier, continuous trajectories including X/Y wrap.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/orb-lifecycle.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
local function hex(a,n)local s=''for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for variant=0,1 do for _,y in ipairs({0,104,240,255})do for scenario=0,2 do
  for a=0,0xfff do m:write_u8(a,0)end
  m:write_u8(0xe18,1);m:write_u8(0xe1b,variant);m:write_u16(0xe36,0x708)
  m:write_u8(0x700,y);m:write_u8(0x6f9,3)
  local camera=scenario==2 and 65530 or 1024;local fraction=0
  for tick=0,511 do
   m:write_u16(0x90,camera)
   pending={variant,y,scenario,tick,camera,hex(0xe18,32),hex(0x6f8,32)}
   for k,v in ipairs({0xbd,0xe1,0xaf,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
   if scenario~=0 then fraction=fraction+141;camera=(camera+(fraction>>8))&65535;fraction=fraction&255 end
  end
 end end end
end)
emu.register_frame_done(function()
 if done then return end;frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return');local p=pending
   f:write(string.format('{"variant":%d,"y":%d,"scenario":%d,"tick":%d,"camera":%d,"before":"%s","parent_before":"%s","after":"%s","parent_after":"%s"}\n',p[1],p[2],p[3],p[4],p[5],p[6],p[7],hex(0xe18,32),hex(0x6f8,32)))
   pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('ORB_TRACE_PASS '..count);manager.machine:exit();return end
  local good,e=coroutine.resume(cases);assert(good,e)
 end)
 if not ok then done=true;f:close();print('ORB_TRACE_ERROR '..tostring(err));manager.machine:exit()end
end)
