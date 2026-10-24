-- Original ss.02e MC6809 $9094: both initial player records, including retained bytes.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/start-oracle.jsonl','w'))
local frame=0;local pending=nil;local terminal=false
local function hex()local t={}for a=0x47e,0x4df do t[#t+1]=string.format('%02x',m:read_u8(a))end;return table.concat(t)end
local cases=coroutine.create(function()
 for _,camera in ipairs({0,255,65520})do for _,mode in ipairs({0,1,255})do
 for _,p1out in ipairs({0,1,255})do for _,p2out in ipairs({0,1,255})do for _,seed in ipairs({0,165})do
  for a=0x47e,0x4df do m:write_u8(a,seed)end
  m:write_u16(0x90,camera);m:write_u8(0x99,mode);m:write_u8(0x88,p1out);m:write_u8(0x89,p2out)
  pending={camera=camera,mode=mode,p1out=p1out,p2out=p2out,before=hex()}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  m:write_u8(0x220,0xbd);m:write_u16(0x221,0x9094);m:write_u8(0x223,0x20);m:write_u8(0x224,0xfe)
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then local p=pending;assert(cpu.state['CURPC'].value==0x223,'start did not return')
   f:write(string.format('{"camera":%d,"mode":%d,"p1out":%d,"p2out":%d,"before":"%s","after":"%s"}\n',p.camera,p.mode,p.p1out,p.p2out,p.before,hex()));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('START_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('START_ERROR',err);manager.machine:exit()end
end)
