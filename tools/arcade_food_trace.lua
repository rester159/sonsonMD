-- Original E607: food aggregate and ordinary-pickup request, isolated fixtures.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/food-trigger.csv','w'))
f:write('count,total,food,grade,after_count,after_total,kind,latch,active,phase,placement,row\n')
local frame=0;local pending;local done=false;local count=0
local cases=coroutine.create(function()
 for n=0,5 do for total=n,n*10 do for food=-12,7 do for _,grade in ipairs({0,1,2,3,4,5,6,7,8,255})do
  for a=0,0x1ff do m:write_u8(a,0)end
  m:write_u8(0x99,0);m:write_u8(0x9a,n);m:write_u8(0x9b,total);m:write_u8(0x7e,grade)
  m:write_u8(0x566,0x68);m:write_u8(0x567,food&255)
  m:write_u8(0x6fb,99);m:write_u8(0x70b,88)
  m:write_u8(0x550,77);m:write_u8(0x551,66);m:write_u8(0x554,55);m:write_u8(0x555,44)
  for k,v in ipairs({0x8e,0x05,0x60,0xbd,0xe6,0x07,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  pending={n,total,food&255,grade}
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x206,'return')
   for _,v in ipairs({m:read_u8(0x9a),m:read_u8(0x9b),m:read_u8(0x6fb),m:read_u8(0x70b),m:read_u8(0x550),m:read_u8(0x551),m:read_u8(0x554),m:read_u8(0x555)})do pending[#pending+1]=v end
   f:write(table.concat(pending,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('FOOD_TRACE '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('FOOD_ERROR '..tostring(err));manager.machine:exit()end
end)
