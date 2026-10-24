-- FF6A: original four-cell emission, including admission and byte wrap.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-render-oracle.csv','w'));f:write('case,record,camera,budget,output,remaining,count\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 for i=0,4095 do
  for k=0,47 do m:write_u8(0xe38+k,(i*17+k*37)&255)end
  m:write_u8(0xe38,({0,1,2,127,128,255})[i%6+1])
  m:write_u16(0xe3d,(i*257)&65535);m:write_u8(0xe40,i&255)
  local camera=(i*131)&65535;local budget=({0,1,3,4,5,24,255})[(i//6)%7+1]
  m:write_u16(0x90,camera);m:write_u8(0xa4,budget)
  for k=0,15 do m:write_u8(0x300+k,0xa5)end
  pending={i,hex(0xe38,48),camera,budget}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local code={0x8e,0x0e,0x48,0x10,0x8e,0x03,0x00,0xbd,0xff,0x6a,0x20,0xfe};for k,v in ipairs(code)do m:write_u8(0x21f+k,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x22a,'render return');pending[5]=hex(0x300,16);pending[6]=m:read_u8(0xa4);pending[7]=(cpu.state['Y'].value-0x300)//4;f:write(table.concat(pending,',')..'\n');pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_RENDER_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_RENDER_ERROR',err);manager.machine:exit()end
end)
