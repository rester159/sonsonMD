-- Original ordinary G/H/I player-contact scans. Bonus flag64 is a separate contract.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/contact-oracle.csv','w'))
f:write('family,owner,player_active,enemy_active,enemy_state,player_x,player_y,enemy_x,enemy_y,hit\n')
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 local function run(family,owner,pa,ea,es,px,py,ex,ey)
  local base=({0xc28,0xce8,0xda8})[family+1];local p=0x490+owner*48
  for a=0xc18,0xe17 do m:write_u8(a,0)end
  m:write_u8(base-16,ea);m:write_u8(base-15,es);m:write_u16(base-11,ex&65535);m:write_u8(base-8,ey&255)
  m:write_u8(p-16,pa);m:write_u16(p-15,0x0201);m:write_u8(p-13,0);m:write_u16(p-11,px&65535);m:write_u8(p-8,py&255)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local entries={{0xeac0,0xeacb},{0xeaa3,0xeaae},{0xeade,0xeae9}}
  m:write_u8(0x220,0xbd);m:write_u16(0x221,entries[family+1][owner+1]);m:write_u8(0x223,0x20);m:write_u8(0x224,0xfe)
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  pending={family,owner,pa,ea,es,px&65535,py&255,ex&65535,ey&255,p};coroutine.yield()
 end
 for family=0,2 do for owner=0,1 do
  for _,center in ipairs({0,65528})do for dx=-10,10 do for dy=-10,10 do run(family,owner,1,1,1,center+dx,center+dy,center,center)end end end
  for _,pa in ipairs({0,1,2,128})do for _,ea in ipairs({0,1,2,3,128,129,130,255})do for _,state in ipairs({0,4,128,255})do run(family,owner,pa,ea,state,100,128,100,128)end end end
 end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then local p=pending;assert(cpu.state['CURPC'].value==0x223,'contact did not return');local hit=m:read_u8(p[10]-13);assert(hit==0 or hit==1);assert(m:read_u16(p[10]-15)==(hit==1 and 0 or 0x0201));p[10]=hit;f:write(table.concat(p,',')..'\n');pending=nil end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('CONTACT_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('CONTACT_ERROR',err);manager.machine:exit()end
end)
