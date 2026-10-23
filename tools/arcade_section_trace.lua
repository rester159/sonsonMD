-- MC6809 A730, section boundary plus original pool load; masked interrupts.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/sections.csv','w'))
f:write('index,remaining,column,difficulty,camera,index_out,remaining_out,column_out,difficulty_out,speed,stop,next_stop,timer,next_timer,checkpoint,background,collision,collision_copy,source,destination\n')
local frame=0;local pending;local done=false;local count=0
local cases=coroutine.create(function()
 for index=0,25 do for _,remaining in ipairs({0,1,2,255})do
 for _,column in ipairs({0,1,31,32,64,255})do for _,difficulty in ipairs({0,27,28,255})do
  for a=0,0xfff do m:write_u8(a,0)end
  local camera=(index*256+65500)&65535
  m:write_u8(0x67,index);m:write_u8(0x98,remaining);m:write_u8(0x8a,column)
  m:write_u8(0x5a,difficulty);m:write_u16(0x96,0x1234);m:write_u16(0x90,camera)
  m:write_u8(0x9d,2);m:write_u8(0x9e,1);m:write_u8(0x9f,7);m:write_u8(0xa0,20)
  m:write_u16(0x65,0xabcd);m:write_u16(0x94,0x1111);m:write_u16(0x8e,0x2222);m:write_u16(0x6a,0x3333)
  m:write_u16(0xea,0x5587);m:write_u16(0xec,0x560)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  for k,v in ipairs({0xbd,0xa7,0x30,0x20,0xfe})do m:write_u8(0x21f+k,v)end
  pending={index,remaining,column,difficulty,camera}
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end end end
end)
emu.register_frame_done(function()
 if done then return end;frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x223,'return')
   for _,a in ipairs({0x67,0x98,0x8a,0x5a})do pending[#pending+1]=m:read_u8(a)end
   pending[#pending+1]=m:read_u16(0x96)
   for _,a in ipairs({0x9d,0x9e,0x9f,0xa0})do pending[#pending+1]=m:read_u8(a)end
   for _,a in ipairs({0x65,0x94,0x8e,0x6a,0xea,0xec})do pending[#pending+1]=m:read_u16(a)end
   f:write(table.concat(pending,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('SECTION_TRACE_PASS '..count);manager.machine:exit();return end
  local good,e=coroutine.resume(cases);assert(good,e)
 end)
 if not ok then done=true;f:close();print('SECTION_TRACE_ERROR '..tostring(err));manager.machine:exit()end
end)
