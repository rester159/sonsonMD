-- SonSon main MC6809: original D58B initializer and EE8E shot-hit leaf.
-- Injected records establish local contracts, not natural encounter reachability.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-damage-oracle.csv','w'))
f:write('kind,owner,input_hp,dx,dy,hp,active,major,minor,shot_active,shot_major,shot_minor,flash,credit,carry\n')
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 for kind=0,1 do for owner=0,1 do
 for hp=0,(kind==0 and 0 or 255)do
 local offsets=kind==0 and {{0,0}}or {{0,0},{-29,0},{-28,0},{12,0},{13,0},{0,-29},{0,-28},{0,11},{0,12}}
 for _,offset in ipairs(offsets)do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0xe38,0xe67 do m:write_u8(a,0)end
  for a=0x4d0,0x4ef do m:write_u8(a,0)end
  local x=0xe48;local y=0x4e0
  m:write_u8(x-16,1);m:write_u16(x-15,0x0100);m:write_u16(x-11,100+offset[1]);m:write_u8(x-8,128+offset[2]);m:write_u8(x+8,hp)
  m:write_u8(y-16,1);m:write_u16(y-15,0x0100);m:write_u16(y-11,100);m:write_u8(y-8,128)
  m:write_u8(0xd5,owner*128);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0);m:write_u8(0x22,1)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local pc=kind==0 and 0xd58b or 0xee8e
  local code={0x8e,0x0e,0x48,0x10,0x8e,4,0xe0,0xbd,pc>>8,pc&255,0x20,0xfe}
  for i,v in ipairs(code)do m:write_u8(0x21f+i,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  pending={kind,owner,hp,offset[1],offset[2]};coroutine.yield()
 end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x22a,'routine did not return')
   local p=pending;local vals={p[1],p[2],p[3],p[4],p[5],m:read_u8(0xe50),m:read_u8(0xe38),m:read_u8(0xe39),m:read_u8(0xe3a),m:read_u8(0x4d0),m:read_u8(0x4d1),m:read_u8(0x4d2),m:read_u8(0xe51),m:read_u8(0xe3c),cpu.state['CC'].value&1}
   f:write(table.concat(vals,',')..'\n');pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_DAMAGE_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_DAMAGE_ERROR',err);manager.machine:exit()end
end)
