-- Original AD9B death dispatcher through entry-state initialization.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/life-oracle.jsonl','w'));local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local cases=coroutine.create(function()
 for owner=0,1 do for _,reserves in ipairs({0,1,2,255})do for stopped=0,1 do
 for _,y in ipairs({0,1,72,136,200,248,255})do for _,xf in ipairs({0,255})do
  local p=0x490+owner*48;clear(p-16,48);m:write_u8(p-16,1);m:write_u8(p-13,1);m:write_u8(p-12,owner)
  m:write_u16(p-11,65528);m:write_u8(p-9,xf);m:write_u8(p-8,y);m:write_u8(p-7,255);m:write_u8(p-6,3)
  m:write_u8(0x58+owner,reserves);m:write_u8(0x88+owner,0)
  local camera=65520;local frac=0
  for tick=0,219 do
   clear(0x180,128);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0)
   m:write_u16(0x90,camera);m:write_u8(0x9d,stopped)
   pending={owner=owner,tick=tick,camera=camera,stopped=stopped,before=hex(p-16,48),reserves=m:read_u8(0x58+owner),gate=m:read_u8(0x88+owner),base=p}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   m:write_u8(0x220,0x8e);m:write_u16(0x221,p);m:write_u8(0x223,0xbd);m:write_u16(0x224,0xad9b);m:write_u8(0x226,0x20);m:write_u8(0x227,0xfe)
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
   coroutine.yield()
   if m:read_u8(p-15)==7 or m:read_u8(p-13)==0 then break end
   if stopped==0 then frac=frac+141;camera=(camera+frac//256)%65536;frac=frac%256 end
   assert(tick<219,'death trajectory failed to terminate')
  end
 end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x226,'death routine did not return');local p=pending
   local event=m:read_u16(0x10)==0x180 and 65535 or m:read_u16(0x180)
   f:write(string.format('{"owner":%d,"tick":%d,"camera":%d,"stopped":%d,"before":"%s","after":"%s","reserves":%d,"after_reserves":%d,"gate":%d,"after_gate":%d,"event":%d,"sound":"%s"}\n',p.owner,p.tick,p.camera,p.stopped,p.before,hex(p.base-16,48),p.reserves,m:read_u8(0x58+p.owner),p.gate,m:read_u8(0x88+p.owner),event,hex(0x1c0,8)));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LIFE_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LIFE_ERROR',err);manager.machine:exit()end
end)
