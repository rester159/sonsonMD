-- Original normal state0 wrapper: AA03, clamp, firing pose and drift.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/player-init-oracle.jsonl','w'));local frame=0;local pending=nil;local terminal=false;local tap=nil
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local cases=coroutine.create(function()
 for owner=0,1 do for _,camera in ipairs({0,65520})do for _,x in ipairs({-20,0,8,207,208,216,300})do
 for _,input in ipairs({0,4,8,16,32,60})do for _,previous in ipairs({0,60})do for _,xf in ipairs({0,255})do for other=0,1 do
  local p=0x490+owner*48;local q=0x490+(1-owner)*48;clear(0x480,96)
  m:write_u8(p-16,1);m:write_u8(p-12,owner);m:write_u16(p-11,(camera+x)&65535);m:write_u8(p-9,xf);m:write_u8(p-8,104);m:write_u8(p-7,255);m:write_u8(p-6,1)
  m:write_u16(p-2,owner==0 and 0xafac or 0xafd0);m:write_u16(p+14,q)
  m:write_u8(q-16,other);m:write_u8(q-15,5);m:write_u8(p+8,input);m:write_u8(p+9,previous)
  m:write_u16(0x90,camera);m:write_u8(0x9d,0)
  pending={camera=camera,input=input,previous=previous,other=other,before=hex(p-16,48),base=p,fire_x=0,fire_y=0,fire_facing=0}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  m:write_u8(0x220,0x8e);m:write_u16(0x221,p);m:write_u8(0x223,0xbd);m:write_u16(0x224,0xa9a7);m:write_u8(0x226,0x20);m:write_u8(0x227,0xfe)
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 if frame==60 then tap=m:install_read_tap(0xad10,0xad10,'initial-fire-pose',function()
  if pending then local p=pending;p.fire_x=m:read_u16(p.base-11);p.fire_y=m:read_u8(p.base-8);p.fire_facing=m:read_u8(p.base-6)end
 end)end
 local ok,err=pcall(function()
  if pending then local p=pending;assert(cpu.state['CURPC'].value==0x226,'initialization did not return')
   f:write(string.format('{"camera":%d,"input":%d,"previous":%d,"other":%d,"before":"%s","after":"%s","fire_x":%d,"fire_y":%d,"fire_facing":%d}\n',p.camera,p.input,p.previous,p.other,p.before,hex(p.base-16,48),p.fire_x,p.fire_y,p.fire_facing));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('PLAYER_INIT_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('PLAYER_INIT_ERROR',err);manager.machine:exit()end
end)
