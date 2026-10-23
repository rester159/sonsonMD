-- Original E55F/E565 food scan and queued BCD score consumer.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/food-contact.csv','w'))
f:write('owner,food,player_active,tile_active,visible,dx,dy,after_active,phase,count,total,event,sound,points\n')
local frame=0;local pending;local done=false;local count=0
local function fixture(owner,food,pa,ta,visible,dx,dy)
 for a=0,0x1ff do m:write_u8(a,0)end
 for a=0x480,0x757 do m:write_u8(a,0)end
 local p=0x480+owner*48
 m:write_u8(p,pa);m:write_u8(p+4,owner);m:write_u16(p+5,(4+dx)&65535);m:write_u8(p+8,(3+dy)&255)
 m:write_u8(0x560,ta);m:write_u8(0x561,3);m:write_u16(0x564,4);m:write_u8(0x566,3);m:write_u8(0x567,food)
 m:write_u16(0x90,visible==1 and 65500 or 500)
 m:write_u8(0xd5,owner*128);m:write_u8(0x22,1);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0)
 local fn=owner==0 and 0xe55f or 0xe565
 local code={0xbd,fn>>8,fn&255,0xb6,1,0x80,0x27,6,0xf6,1,0x81,0xbd,0x4c,0xbe,0x20,0xfe}
 for k,v in ipairs(code)do m:write_u8(0x1ff+k,v)end
 pending={owner,food,pa,ta,visible,dx,dy}
 cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
 coroutine.yield()
end
local cases=coroutine.create(function()
 for owner=0,1 do
  for food=-12,7 do fixture(owner,food&255,1,1,1,0,0)end
  for pa=0,2 do for ta=0,3 do for visible=0,1 do
   for _,dx in ipairs({-7,-6,0,6,7})do for _,dy in ipairs({-7,-6,0,6,7})do
    fixture(owner,244,pa,ta,visible,dx,dy)
   end end
  end end end
 end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x20e,'return')
   local score=0;for a=0x6c+pending[1]*4,0x6f+pending[1]*4 do local v=m:read_u8(a);assert((v&15)<10 and (v>>4)<10);score=score*100+(v>>4)*10+(v&15)end
   for _,v in ipairs({m:read_u8(0x560),m:read_u8(0x561),m:read_u8(0x9a),m:read_u8(0x9b),m:read_u16(0x180),m:read_u8(0x1c1),score})do pending[#pending+1]=v end
   f:write(table.concat(pending,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('FOOD_CONTACT '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('FOOD_CONTACT_ERROR '..tostring(err));manager.machine:exit()end
end)
