-- Original H then G family scans, both stable player-shot slots.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local out=assert(io.open("out/parity-audit/scan-oracle.jsonl","w"))
local frame=0;local pending=nil;local terminal=false;local taps={}
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local rng=12345
local function rand(n)rng=(rng*1103515245+12345)&0x7fffffff;return(rng>>8)%n end
local cases=coroutine.create(function()
 for case=0,1023 do
  local owner=case%2;local shotbase=owner==0 and 0x4d0 or 0x510
  for k=0,11 do
   local a=0xc18+k*32;for i=0,31 do m:write_u8(a+i,0)end
   m:write_u8(a,({0,1,1,1,2,128})[rand(6)+1]);m:write_u8(a+1,({0,1,2,6,128,255})[rand(6)+1])
   m:write_u16(a+5,100+rand(33)-16);m:write_u8(a+8,128+rand(33)-16)
  end
  for k=0,1 do local a=shotbase+k*32
   for i=0,31 do m:write_u8(a+i,0)end
   m:write_u8(a,({0,1,1,1,2})[rand(5)+1]);m:write_u8(a+1,rand(4));m:write_u16(a+5,100+rand(17)-8);m:write_u8(a+8,128+rand(17)-8)
  end
  m:write_u8(0xd5,owner*128);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0);m:write_u16(0x96,0);m:write_u8(0xb2,6);m:write_u8(0xb3,6)
  pending={case=case,owner=owner,shotbase=shotbase,actors=hex(0xc18,384),shots=hex(shotbase,64),hits={}}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  m:write_u8(0x220,0xbd);m:write_u16(0x221,owner==0 and 0xe8a8 or 0xe8b3)
  m:write_u8(0x223,0xbd);m:write_u16(0x224,owner==0 and 0xe951 or 0xe95c)
  m:write_u8(0x226,0x20);m:write_u8(0x227,0xfe)
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 if frame==60 then
  for _,address in ipairs({0xe906,0xe9af})do taps[#taps+1]=m:install_read_tap(address,address,'successful-source-hit',function()
   if pending then
    local actor=(cpu.state['X'].value-0xc28)//32;local shot=(cpu.state['Y'].value-pending.shotbase-16)//32
    pending.hits[#pending.hits+1]=actor*2+shot+1
   end
  end)end
 end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x226,'scan did not return')
   out:write(string.format('{"case":%d,"owner":%d,"actors":"%s","shots":"%s","hits":[%s]}\n',pending.case,pending.owner,pending.actors,pending.shots,table.concat(pending.hits,',')));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;out:close();print('SCAN_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;out:close();print('SCAN_ERROR',err);manager.machine:exit()end
end)
