-- Original B922 initial redraw, including visibility and active-bit gates.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/food-redraw.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
local locations={}
local function hex(a,n)local s=''for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for food=-12,7 do for active=0,7 do for cameraIndex=1,2 do
 for _,delta in ipairs({255,256,257})do for _,y in ipairs({8,104,248})do
  local camera= cameraIndex==1 and 1000 or 65400
  local x=(camera+delta)&65535
  local base=0x1000+((x&248)>>3)+(31-((y&248)>>3))*32
  locations={base,base-32,base+1,base-31,base+1024,base+992,base+1025,base+993}
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0x560,0x627 do m:write_u8(a,0)end
  for a=0x3a0,0x3e0 do m:write_u8(a,0xa5)end
  for _,a in ipairs(locations)do m:write_u8(a,0xa5)end
  m:write_u16(0x90,camera)
  m:write_u8(0x560,active);m:write_u8(0x561,5);m:write_u16(0x562,0x1100)
  m:write_u16(0x564,x);m:write_u8(0x566,y);m:write_u8(0x567,food&255)
  for k,v in ipairs({0xbd,0xb9,0x22,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  pending={food&255,active,camera,delta,y}
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
   f:write(string.format('{"food":%d,"active":%d,"camera":%d,"delta":%d,"y":%d,"object":"%s","strip":"%s","patch":"%s"}\n',p[1],p[2],p[3],p[4],p[5],hex(0x560,10),hex(0x3a0,65),patch))
   pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('FOOD_REDRAW '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('FOOD_REDRAW_ERROR '..tostring(err));manager.machine:exit()end
end)
