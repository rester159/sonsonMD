-- arc_shot.lua : drive arcade sonson into mid-gameplay with enemies on screen and
-- take a video snapshot, then dump the spriteram so the snapshot can be correlated.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local vid = manager.machine.video
local function press(p, fld, v)
  local pt = mio.ports[p]; if pt then local fd=pt.fields[fld]; if fd then fd:set_value(v) end end
end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_shot.txt","w")
local fr, phase, t0, play0, shot = 0, "attract", 0, nil, 0
emu.register_frame_done(function()
  fr=fr+1; local md=r8(0x0002)
  if phase=="attract" then
    if md==0x01 and fr>700 then phase="coin"; t0=fr end
  elseif phase=="coin" then
    press(":SYSTEM","Coin 1",((fr-t0)%24<6) and 1 or 0)
    if fr-t0>=96 then phase="start"; t0=fr; press(":SYSTEM","Coin 1",0) end
  elseif phase=="start" then
    press(":SYSTEM","1 Player Start",((fr-t0)%24<6) and 1 or 0)
    if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0) end
    if fr-t0>600 then manager.machine:exit() end
  elseif phase=="play" then
    w8(0x0073,0xFF)
    local e=fr-play0
    press(":P1","P1 Right",1)
    press(":P1","P1 Button 1",((fr%8)<3) and 1 or 0)
    local ph=(fr//50)%4
    press(":P1","P1 Up",ph==1 and 1 or 0); press(":P1","P1 Down",ph==3 and 1 or 0)
    -- take several snapshots once enemies are present (e>1000), spaced out
    if e>=1000 and e%300==0 and shot<5 then
      shot=shot+1
      vid:snapshot()
      local parts={}
      for i=0,23 do local b=0x2020+i*4; local y=r8(b); if y~=0xF8 then
        parts[#parts+1]=string.format("#%d(y=%d x=%d c=%02X a=%02X)",i,y,r8(b+3),r8(b+1),r8(b+2)) end end
      f:write(string.format("SHOT %d fr=%d s96=%02X spr: %s\n", shot, fr, r8(0x0096), table.concat(parts," ")))
      f:flush()
    end
    if shot>=5 then phase="done" end
  elseif phase=="done" then f:close(); manager.machine:exit() end
  if fr>30000 then f:close(); manager.machine:exit() end
end)
print("arc_shot running")
