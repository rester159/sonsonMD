local cpu=manager.machine.devices[':maincpu']
local mem=cpu.spaces['program']
local frame=0
local expected=nil
emu.register_frame_done(function()
 frame=frame+1
 if frame==100 then expected=mem:read_u32(0xff0000);print(string.format('INITIAL_DATA %08X',expected)) end
 if frame==600 or frame==1800 then
  -- Model a cartridge entry with menu-inherited I/O and stale work RAM.
  for a=0xff0000,0xfffffc,4 do mem:write_u32(a,0xa5a5a5a5) end
  mem:write_u8(0xa10009,0x40)
  mem:write_u8(0xa1000b,0x40)
  mem:write_u8(0xa1000d,0x40)
  cpu.state['SR'].value=0x2700
  cpu.state['SP'].value=mem:read_u32(0)
  cpu.state['PC'].value=mem:read_u32(4)
  print('INHERITED_STATE_ENTRY '..frame)
 end
 if frame==720 or frame==1920 then
  local got=mem:read_u32(0xff0000)
  print(string.format('DATA_REINITIALIZED frame=%d expected=%08X actual=%08X pass=%s',frame,expected,got,tostring(got==expected)))
 end
 if frame==1200 or frame==2400 then manager.machine.video:snapshot() end
 if frame==2500 then manager.machine:exit() end
end)
