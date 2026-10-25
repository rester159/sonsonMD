-- Original EE8E successful hit and queued 4CBE score consumer.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-reward-oracle.csv','w'))
f:write('subtype,variant,owner,hp,combat,after_hp,active,state,phase,credit,flash,shot_active,shot_state,shot_phase,event,sound,after_combat,points\n')
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 for subtype=0,2 do for variant=0,3 do for owner=0,1 do for _,hp in ipairs({0,1,2,7})do for _,combat in ipairs({0,0x1cff,0x3e7f,0x3e80,0x3eff,0x3f00,0xff80,0xffff})do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0xe38,0xe67 do m:write_u8(a,0)end
  for a=0x4d0,0x4ef do m:write_u8(a,0)end
  m:write_u8(0xe38,1);m:write_u16(0xe39,0x0101);m:write_u8(0xe3b,subtype);m:write_u8(0xe3c,1-owner);m:write_u16(0xe3d,100);m:write_u8(0xe40,128);m:write_u8(0xe50,hp);m:write_u8(0xe53,variant)
  m:write_u8(0x4d0,1);m:write_u16(0x4d1,0x0101);m:write_u16(0x4d5,100);m:write_u8(0x4d8,128)
  m:write_u8(0xd5,owner*128);m:write_u16(0x96,combat);m:write_u8(0x22,1);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local code={0x8e,0x0e,0x48,0x10,0x8e,4,0xe0,0xbd,0xee,0x8e}
  if hp==1 then for _,v in ipairs({0xf6,1,0x81,0xbd,0x4c,0xbe})do code[#code+1]=v end end
  local stop=0x220+#code;code[#code+1]=0x20;code[#code+1]=0xfe
  for k,v in ipairs(code)do m:write_u8(0x21f+k,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  pending={subtype,variant,owner,hp,combat,stop};coroutine.yield()
 end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   local p=pending;assert(cpu.state['CURPC'].value==p[6],'reward return');p[6]=nil
   local score=0;for a=0x6c+p[3]*4,0x6f+p[3]*4 do local v=m:read_u8(a);assert((v&15)<10 and(v>>4)<10);score=score*100+(v>>4)*10+(v&15)end
   for _,v in ipairs({m:read_u8(0xe50),m:read_u8(0xe38),m:read_u8(0xe39),m:read_u8(0xe3a),m:read_u8(0xe3c),m:read_u8(0xe51),m:read_u8(0x4d0),m:read_u8(0x4d1),m:read_u8(0x4d2),m:read_u16(0x180),m:read_u16(0x14)==0x1c2 and m:read_u8(0x1c1)or -1,m:read_u16(0x96),score})do p[#p+1]=v end
   f:write(table.concat(p,',')..'\n');pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_REWARD_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_REWARD_ERROR',err);manager.machine:exit()end
end)
