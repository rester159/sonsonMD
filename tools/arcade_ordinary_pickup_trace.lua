-- Original B19A FE/FF lifetimes; controller/pickup triggering is separate.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/ordinary-pickup-lifecycle.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
local function hex(a,n)local s=''for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for kind=0,9 do for owner=0,1 do local placement=13;for scenario=0,2 do
  for a=0x6f8,0x757 do m:write_u8(a,0)end
  m:write_u8(0x6fc,owner);m:write_u8(0x6f8,1);m:write_u8(0x6fb,kind);m:write_u8(0x714,placement)
  local camera=65500;local fraction=0
  for tick=0,511 do
   for a=0,0x1ff do m:write_u8(a,0)end
   for a=0xe18,0xe37 do m:write_u8(a,0)end
   if tick==64 and scenario==2 then m:write_u8(0x6f8,2);m:write_u8(0x6f9,1);m:write_u8(0x6fa,0)end
   m:write_u16(0x90,camera)
   pending={kind,placement,scenario,tick,camera,hex(0x6f8,32)}
   for k,v in ipairs({0xbd,0xb1,0x9a,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
   cpu.state['S'].value=0x330;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
   if scenario~=1 then fraction=fraction+141;camera=(camera+(fraction>>8))&65535;fraction=fraction&255 end
  end
 end end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,string.format('return PC=%04x case=%d',cpu.state['CURPC'].value,count));local p=pending
   f:write(string.format('{"kind":%d,"placement":%d,"scenario":%d,"tick":%d,"camera":%d,"before":"%s","after":"%s","orb":"%s"}\n',p[1],p[2],p[3],p[4],p[5],p[6],hex(0x6f8,32),hex(0xe18,32)))
   pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('ORDINARY_TRACE '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('ORDINARY_ERROR '..tostring(err));manager.machine:exit()end
end)
