-- Entire original D55B dispatch: controlled entry -> pursuit -> retreat.
-- Shot children are recorded and freed each update, without running child AI.
-- Player poses are scripted inputs; this is not a natural-play capture.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-motion-oracle.csv','w'))
f:write('case,tick,subtype,difficulty,players,scroll,camera,clock,p1x,p1y,p2x,p2y,before,after,children,rng\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local cases=coroutine.create(function()
 local id=0
 for subtype=0,2 do for _,difficulty in ipairs({0,8,32,56})do for players=1,2 do for scroll=0,1 do
  clear(0,0x200);clear(0x480,96);clear(0xe38,144);clear(0xec8,128)
  m:write_u8(0x22,1);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0)
  m:write_u8(0x96,difficulty);m:write_u8(0x99,players==2 and 1 or 0);m:write_u8(0x9d,scroll)
  m:write_u8(0xe38,1);m:write_u8(0xe3b,subtype);m:write_u16(0xe3d,232)
  m:write_u8(0xe40,({176,160,96})[subtype+1]);m:write_u8(0x480,1);m:write_u8(0x4b0,players==2 and 1 or 0)
  for tick=0,767 do
   local camera=scroll==0 and (tick*141)//256 or 0
   m:write_u16(0x90,camera)
   local clock=tick&255;local p1x=camera+32+(tick//96%2)*120;local p2x=camera+144-(tick//128%2)*96
   local p1y=40+(tick//64%5)*32;local p2y=168-(tick//80%5)*32
   m:write_u8(0x24,clock);m:write_u16(0x485,p1x);m:write_u8(0x488,p1y);m:write_u16(0x4b5,p2x);m:write_u8(0x4b8,p2y)
   clear(0xec8,128)
   pending={id,tick,subtype,difficulty,players,scroll,camera,clock,p1x,p1y,p2x,p2y,hex(0xe38,48)}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   local code={0x8e,0x0e,0x48,0xbd,0xd5,0x5b,0x20,0xfe}
   for i,v in ipairs(code)do m:write_u8(0x21f+i,v)end
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
   coroutine.yield()
  end
  id=id+1
 end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x226,'dispatch did not return')
   pending[#pending+1]=hex(0xe38,48);pending[#pending+1]=hex(0xec8,128);pending[#pending+1]=hex(0x78,4)
   f:write(table.concat(pending,',')..'\n');pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_MOTION_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_MOTION_ERROR',err);manager.machine:exit()end
end)
