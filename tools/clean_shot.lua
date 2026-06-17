-- clean_shot.lua : natural run, snapshot at a clean mid-level moment (scrolled,
-- before any death) for spec evidence. Saves to tools/arcade_clean_midlevel.png.
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local mio=manager.machine.ioport
local function press(p,f,v) local pt=mio.ports[p]; if pt then local fd=pt.fields[f]; if fd then fd:set_value(v) end end end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end
local fr,phase,t0,play0=0,"attract",0,nil
local snapped=false
emu.register_frame_done(function()
  fr=fr+1
  local md=r8(0x0002)
  if phase=="attract" then if md==0x01 and fr>900 then phase="coin"; t0=fr end
  elseif phase=="coin" then press(":SYSTEM","Coin 1",((fr-t0)%24<6) and 1 or 0); if fr-t0>=96 then phase="start"; t0=fr; press(":SYSTEM","Coin 1",0) end
  elseif phase=="start" then press(":SYSTEM","1 Player Start",((fr-t0)%24<6) and 1 or 0); if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0) end; if fr-t0>600 then manager.machine:exit() end
  elseif phase=="play" then
    w8(0x0073,0xFF)
    press(":P1","P1 Right",1); press(":P1","P1 B",1)
    -- snap once well into the scroll (scrHi>=2 => ~512px in) while still alive
    if not snapped and r8(0x0090)>=2 and md==0x02 then
      manager.machine.video:snapshot(); snapped=true
      print(string.format("SNAP @ fr=%d seg=%d stage=%d scrHi=%02X", fr, r8(0x0067), r8(0x005A), r8(0x0090)))
      manager.machine:exit()
    end
    if md~=0x02 then print("died fr="..fr); manager.machine:exit() end
  end
  if fr>30000 then manager.machine:exit() end
end)
