-- Original54E7 loader: all87 source blocks, end sentinel and five pool quarters.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/world-objects/boot-masked.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
--54B0 unmasks IRQ between copy chunks. The isolated copy fixture overrides
--only operand54E1 (ANDCC EF -> ANDCC FF), retaining the IRQ mask. Original
--copy instructions/data are unchanged. This does not validate board IRQs.
loader_mask_tap=m:install_read_tap(0x54e1,0x54e1,'object_loader_mask',function(address,data,mask)
 if pending and pending[1]==-1 then assert(data==0xef);print("MASK_OPERAND");return 0xff end
 return data
end)
local function hex(a,n)local s=''for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for block=-1,-1 do for quarter=0,4 do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0x560,0x627 do m:write_u8(a,0xa5)end
  local source=0x5587+block*40;local destination=0x560+quarter*40
  m:write_u16(0xea,source);m:write_u16(0xec,destination)
  local code={0xbd,0x54,0xe7,0x20,0xfe}
  if block==-1 then
   m:write_u8(0x320,0x3b)
   for a=0x560,0x627,10 do m:write_u8(a,0)end
   code={0xc6,0,0xbd,0x54,0xb0,0x1a,0x50,0x20,0xfe}
  end
  for k,v in ipairs(code)do m:write_u8(0x1ff+k,v)end
  pending={block,quarter}
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==(pending[1]==-1 and 0x207 or 0x203),string.format('return PC=%04x',cpu.state['CURPC'].value))
   f:write(string.format('{"block":%d,"quarter":%d,"source":%d,"destination":%d,"pool":"%s"}\n',pending[1],pending[2],m:read_u16(0xea),m:read_u16(0xec),hex(0x560,200)))
   pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('OBJECT_LOADER '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('OBJECT_LOADER_ERROR '..tostring(err));manager.machine:exit()end
end)
