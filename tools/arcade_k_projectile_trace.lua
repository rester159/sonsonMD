-- CBC8 kind2 child dispatcher; isolated owned slot, original code unpatched.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/k-projectile-oracle.csv','w'));f:write('case,tick,camera,stopped,purge,before,after\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 local id=0
 for _,facing in ipairs({1,3})do for _,xf in ipairs({0,255})do for _,offset in ipairs({0,120,247})do for stopped=0,1 do for _,camera0 in ipairs({0,65530})do for mode=0,2 do
  for a=0xec8,0xf47 do m:write_u8(a,0)end
  for k=0,31 do m:write_u8(0xec8+k,(k*13+id)&255)end
  m:write_u8(0xec8,1);m:write_u16(0xec9,0);m:write_u8(0xecb,2);m:write_u16(0xecd,(camera0+offset)&65535);m:write_u8(0xecf,xf);m:write_u8(0xed2,facing);m:write_u16(0xed6,0xcec7)
  for tick=0,159 do
   local camera=(camera0+(stopped==0 and (tick*141)//256 or 0))&65535
   local purge=mode==2 and tick>=24 and tick<=26 and 1 or 0
   if mode==1 and tick==24 then m:write_u16(0xec9,0x0300)end
   m:write_u16(0x90,camera);m:write_u8(0x9d,stopped);m:write_u8(0xa1,purge)
   pending={id,tick,camera,stopped,purge,hex(0xec8,32)}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   local code={0xbd,0xcb,0xc8,0x20,0xfe};for k,v in ipairs(code)do m:write_u8(0x21f+k,v)end
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
  end
  id=id+1
 end end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then assert(cpu.state['CURPC'].value==0x223,'K return');pending[#pending+1]=hex(0xec8,32);f:write(table.concat(pending,',')..'\n');pending=nil end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('K_PROJECTILE_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('K_PROJECTILE_ERROR',err);manager.machine:exit()end
end)
