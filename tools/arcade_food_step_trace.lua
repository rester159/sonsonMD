-- Original B36F kind0 food phases and actual character/attribute writes.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/food-step.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
local locations={0x1100,0x10e0,0x1101,0x10e1,0x1500,0x14e0,0x1501,0x14e1}
local function hex(a,n)local s=''for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for food=-12,7 do for phase=0,5 do for advance=0,1 do
 for _,delta in ipairs({255,256,257})do for _,y in ipairs({0,104,255})do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0x560,0x627 do m:write_u8(a,0)end
  for a=0x3a0,0x3e0 do m:write_u8(a,0xa5)end
  for _,a in ipairs(locations)do m:write_u8(a,0xa5)end
  m:write_u16(0x90,1000);m:write_u8(0x8b,advance)
  m:write_u8(0x560,4);m:write_u8(0x561,phase);m:write_u16(0x562,0x1100)
  m:write_u16(0x564,1000+delta);m:write_u8(0x566,y);m:write_u8(0x567,food&255)
  for k,v in ipairs({0xbd,0xb3,0x6f,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  pending={food&255,phase,advance,delta,y}
  cpu.state['S'].value=0x330;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end end end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return')
   local p=pending;local patch='';for _,a in ipairs(locations)do patch=patch..hex(a,1)end
   f:write(string.format('{"food":%d,"phase":%d,"advance":%d,"delta":%d,"y":%d,"object":"%s","strip":"%s","patch":"%s"}\n',p[1],p[2],p[3],p[4],p[5],hex(0x560,10),hex(0x3a0,65),patch))
   pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('FOOD_STEP '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('FOOD_STEP_ERROR '..tostring(err));manager.machine:exit()end
end)
