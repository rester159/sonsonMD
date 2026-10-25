-- Original AD53 target selection then D6F3 low-HP steering, including real 95F4.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-steering-oracle.csv','w'))
f:write('case,tracking,a0,a1,k0,k1,x0,x1,y,y0,y1,hp,previous,clock,seed,sample,target,eligible,direction,after_seed,after_sample\n')
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 for i=0,8191 do
  local tracking=i&1;local flags={0,1,2,255};local a0=flags[((i>>1)&3)+1];local a1=flags[((i>>3)&3)+1]
  local k0=(i>>5)&1;local k1=(i>>6)&1;local x0=32;local x1=({0,32,65535})[(i//128)%3+1]
  local y=(i*37)&255;local y0=(i*29)&255;local y1=(i*43)&255
  local hp=({0,1,2,7})[(i//512)%4+1];local previous=(i//7)%5;local clock=(i//17)%256;local seed=(i*40503+17)&65535;local sample=(i*13)&255
  m:write_u8(0x99,tracking);m:write_u8(0x480,a0);m:write_u8(0x4b0,a1);m:write_u8(0x483,k0);m:write_u8(0x4b3,k1)
  m:write_u16(0x485,x0);m:write_u16(0x4b5,x1);m:write_u8(0x488,y0);m:write_u8(0x4b8,y1)
  m:write_u8(0xe40,y);m:write_u8(0xe50,hp);m:write_u8(0xe42,previous);m:write_u8(0x24,clock);m:write_u16(0x78,seed);m:write_u8(0x7a,sample)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local code={0x8e,0x0e,0x48,0xbd,0xad,0x53,0x10,0xbf,1,0x80,0x1f,0xa9,0xf7,1,0x82,0xbd,0xd6,0xf3,0xf7,1,0x83,0x20,0xfe}
  for n,v in ipairs(code)do m:write_u8(0x21f+n,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  pending={i,tracking,a0,a1,k0,k1,x0,x1,y,y0,y1,hp,previous,clock,seed,sample};coroutine.yield()
 end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x235,'steering return')
   pending[#pending+1]=(m:read_u16(0x180)-0x490)//48;pending[#pending+1]=1-(m:read_u8(0x182)&1)
   pending[#pending+1]=m:read_u8(0x183);pending[#pending+1]=m:read_u16(0x78);pending[#pending+1]=m:read_u8(0x7a)
   f:write(table.concat(pending,',')..'\n');pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_STEERING_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_STEERING_ERROR',err);manager.machine:exit()end
end)
