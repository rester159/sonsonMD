local m=manager.machine.devices[":maincpu"].spaces.program
local f=io.open("/Users/rester159/genesis ports/_capcom/son son/itch-screenshots/work/menu-results.txt","w");local fr=0;local fails=0;local checks=0;local tick=0
local function check(ok,n) checks=checks+1;if not ok then fails=fails+1 end;f:write((ok and "PASS " or "FAIL ")..n.."\n");f:flush() end
local function held(a,b,n) if fr>=a and fr<b then manager.machine.ioport.ports[":ctrl1:mdpad:PAD"].fields["P1 "..n]:set_value(1) end end
emu.register_frame_done(function()
 fr=fr+1
 if fr==1000 or fr==1150 then manager.machine.video:snapshot() end
 for _,n in ipairs({"Down","Up","Start","Right","Left"}) do manager.machine.ioport.ports[":ctrl1:mdpad:PAD"].fields["P1 "..n]:set_value(0) end
 held(920,932,"Down");held(950,965,"Start")
 held(1040,1052,"Down");held(1080,1092,"Down");held(1120,1132,"Start")
 held(1200,1212,"Down");held(1240,1252,"Down");held(1280,1292,"Down");held(1320,1332,"Down")
 held(1360,1372,"Right");held(1400,1412,"Right");held(1440,1452,"Right");held(1480,1492,"Left");held(1520,1532,"Left")
 if fr==1390 then check(m:read_u8(16711680+7)==5,"Options CREDITS increases from 3 to 5");manager.machine.video:snapshot() end
 if fr==1430 then check(m:read_u8(16711680+7)==9,"Options CREDITS increases to 9") end
 if fr==1470 then check(m:read_u8(16711680+7)==0,"Options CREDITS wraps to zero") end
 if fr==1550 then check(m:read_u8(16711680+7)==5,"Options CREDITS changes left independently of lives") end
 held(1560,1575,"Start");held(1600,1612,"Up");held(1640,1680,"Start")
 if fr==1700 then
  check(m:read_u8(16714152)==1 and m:read_u16(16714194)>0 and m:read_u8(16714196+84)==2,"Home two-player menu launches co-op")
  check(m:read_u8(16714297)==5,"Selected credits apply to new Home game")
  check(m:read_u8(16714296)==0,"Held menu Start does not immediately pause gameplay")
  manager.machine.video:snapshot()
 end
 held(1740,1770,"Start")
 if fr==1760 then tick=m:read_u32(16714182);check(m:read_u8(16714296)==1,"Fresh Home Start pauses") end
 if fr==1790 then check(m:read_u32(16714182)==tick,"Home pause freezes the actual game loop") end
 held(1820,1832,"Start")
 if fr==1860 then check(m:read_u8(16714296)==0 and m:read_u32(16714182)>tick,"Fresh Home Start resumes") end
 if fr>=1880 then f:write(string.format("CHECKS %d FAILURES %d\n",checks,fails));f:close();manager.machine:exit() end
end)
