-- Original D52A four-cell animation and D924 damage refresh.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-animation-oracle.csv','w'));f:write('operation,before,after\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local function run(pc,op)
 pending={op,hex(0xe38,48)}
 for a=0x200,0x21f do m:write_u8(a,0x12)end
 local code={0x8e,0x0e,0x48,0xbd,pc>>8,pc&255,0x20,0xfe};for i,v in ipairs(code)do m:write_u8(0x21f+i,v)end
 cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
 coroutine.yield()
end
local cases=coroutine.create(function()
 local roots={0xd5ce,0xd848,0xd8bc}
 for _,base in ipairs({0xd987,0xd997})do for hp=0,7 do roots[#roots+1]=m:read_u16(base+hp*2)end end
 for _,root in ipairs(roots)do
  for a=0xe38,0xe67 do m:write_u8(a,0x5a)end
  m:write_u16(0xe46,root);m:write_u8(0xe45,0)
  for tick=0,255 do run(0xd52a,0)end
 end
 for hp=0,7 do for facing=1,4 do for subtype=0,2 do for flag=0,1 do for _,timer in ipairs({0,255})do
  for a=0xe38,0xe67 do m:write_u8(a,(a*7+hp)&255)end
  m:write_u16(0xe46,0xd5ce);m:write_u8(0xe45,timer);m:write_u8(0xe50,hp);m:write_u8(0xe51,flag);m:write_u8(0xe52,facing);m:write_u8(0xe3b,subtype)
  run(0xd924,1)
 end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then assert(cpu.state['CURPC'].value==0x226,'animation return');f:write(pending[1]..','..pending[2]..','..hex(0xe38,48)..'\n');pending=nil end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_ANIMATION_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_ANIMATION_ERROR',err);manager.machine:exit()end
end)
