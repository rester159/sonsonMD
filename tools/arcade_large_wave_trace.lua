-- Original FAFA controller; reuse poisoned actors, including occupied slots.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-wave-oracle.csv','w'));f:write('case,tick,camera,difficulty,control_before,actors_before,control_after,actors_after\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 local id=0
 for enabled=0,1 do for mask=0,7 do for variant=0,3 do for _,camera in ipairs({0,65530})do for _,difficulty in ipairs({0,16,32,48})do
  for a=0x6c8,0x6d7 do m:write_u8(a,(a*7+id)&255)end
  m:write_u8(0x6c8,enabled);m:write_u8(0x6ca,0);m:write_u8(0x6cb,17);m:write_u16(0x6cc,0xea8);m:write_u8(0x6ce,197);m:write_u8(0x6d6,variant)
  m:write_u16(0x90,camera);m:write_u8(0x96,difficulty);m:write_u8(0x9d,1)
  for k=0,2 do
   local a=0xe38+k*48
   for j=0,47 do m:write_u8(a+j,(j*13+id+k)&255)end
   m:write_u8(a,(mask&(1<<k))~=0 and ({1,2,255})[k+1]or 0)
   m:write_u8(a+1,3);m:write_u8(a+2,1);m:write_u8(a+3,k);m:write_u8(a+27,variant)
   m:write_u16(a+14,0xd5ce);m:write_u16(a+30,0x490);m:write_u16(a+11,0xdb51+k*25)
  end
  for tick=0,5 do
   pending={id,tick,camera,difficulty,hex(0x6c8,16),hex(0xe38,144)}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   local code={0xbd,0xfa,0xfa,0x20,0xfe};for k,v in ipairs(code)do m:write_u8(0x21f+k,v)end
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
  end
  id=id+1
 end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then assert(cpu.state['CURPC'].value==0x223,'wave return');pending[#pending+1]=hex(0x6c8,16);pending[#pending+1]=hex(0xe38,144);f:write(table.concat(pending,',')..'\n');pending=nil end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_WAVE_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_WAVE_ERROR',err);manager.machine:exit()end
end)
