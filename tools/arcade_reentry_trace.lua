-- Original AD9B/AE99 automatic reentry and input/fire takeover.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/reentry-oracle.jsonl','w'));local frame=0;local pending=nil;local terminal=false;local tap=nil
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local cases=coroutine.create(function()
 for owner=0,1 do for stopped=0,1 do for _,xf in ipairs({0,255})do for scenario=0,9 do
  local p=0x490+owner*48;local shot=owner==0 and 0x4e0 or 0x520
  clear(p-16,48);m:write_u16(p-18,shot);m:write_u8(p-16,2);m:write_u8(p-15,7);m:write_u8(p-13,1);m:write_u8(p-12,owner)
  m:write_u16(p-11,65512);m:write_u8(p-9,xf);m:write_u8(p-8,scenario==8 and 40 or 104);m:write_u8(p-7,255);m:write_u8(p-6,1)
  m:write_u16(p-2,owner==0 and 0xaf9a or 0xaf9f);m:write_u8(p-3,127);m:write_u8(p,owner==0 and 0x53 or 0x54);m:write_u8(p+1,0xc0+owner);m:write_u8(p+11,16)
  local camera=65520;local frac=0;local previous=0
  for tick=0,799 do
   local input=0
   if scenario==1 then input=16
   elseif tick>=16 and scenario>=2 and scenario<=4 then input=({16,32,4})[scenario-1]
   elseif scenario==5 then input=tick%8==0 and 1 or 0
   elseif tick>=16 and scenario==6 then input=9
   elseif tick>=16 and scenario==7 then input=48
   elseif tick>=16 and scenario==8 then input=32
   elseif tick>=430 and scenario==9 then input=17 end
   clear(shot-16,64);clear(0x180,128);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0)
   m:write_u16(0x90,camera);m:write_u8(0x9d,stopped);m:write_u8(0x24,(tick+scenario*17)&255)
   m:write_u8(p+8,input);m:write_u8(p+9,previous);m:write_u8(p+12,2)
   pending={owner=owner,scenario=scenario,tick=tick,clock=(tick+scenario*17)&255,camera=camera,stopped=stopped,before=hex(p-16,48),base=p,shot=shot,fire=0,fire_x=0,fire_y=0,fire_facing=0}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   m:write_u8(0x220,0x8e);m:write_u16(0x221,p);m:write_u8(0x223,0xbd);m:write_u16(0x224,0xad9b);m:write_u8(0x226,0x20);m:write_u8(0x227,0xfe)
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
   coroutine.yield();previous=input
   if m:read_u8(p-13)==0 then break end
   if stopped==0 then frac=frac+141;camera=(camera+frac//256)%65536;frac=frac%256 end
   assert(tick<799,'reentry did not release')
  end
 end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 if frame==60 then tap=m:install_read_tap(0xad10,0xad10,'reentry-fire-pose',function()
  if pending then local p=pending;assert(p.fire==0);p.fire=1;p.fire_x=m:read_u16(p.base-11);p.fire_y=m:read_u8(p.base-8);p.fire_facing=m:read_u8(p.base-13)~=0 and 1 or m:read_u8(p.base-6)end
 end)end
 local ok,err=pcall(function()
  if pending then local p=pending;assert(cpu.state['CURPC'].value==0x226,'reentry did not return')
   f:write(string.format('{"owner":%d,"scenario":%d,"tick":%d,"clock":%d,"camera":%d,"stopped":%d,"before":"%s","after":"%s","fire":%d,"fire_x":%d,"fire_y":%d,"fire_facing":%d,"shots":"%s"}\n',p.owner,p.scenario,p.tick,p.clock,p.camera,p.stopped,p.before,hex(p.base-16,48),p.fire,p.fire_x,p.fire_y,p.fire_facing,hex(p.shot-16,64)));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('REENTRY_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('REENTRY_ERROR',err);manager.machine:exit()end
end)
