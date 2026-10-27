local fr=0
local second=os.getenv('SS_GAME')=='2'
emu.register_frame_done(function()
 fr=fr+1
 local age=fr
 if fr>2500 then age=fr-2500 end
 for _,port in pairs(manager.machine.ioport.ports) do
  for _,name in ipairs({'P1 Start','P1 Right','P1 B','P1 A'}) do
   local field=port.fields[name]
   if field then
    local on=false
    if name=='P1 Start' then on=(age>=60 and age<68) or (age>=180 and age<188) or (age>300 and age<900 and age%120<8) end
    if name=='P1 Right' then on=second and age>=120 and age<128 end
    if name=='P1 B' then on=age>900 and age%30<5 end
    field:set_value(on and 1 or 0)
   end
  end
 end
 if fr==600 or fr==1200 or fr==2400 or fr==3700 then manager.machine.video:snapshot() end
 if fr==2500 then manager.machine:soft_reset();print('SOFT_RESET') end
 if fr==3800 then manager.machine:exit() end
end)
