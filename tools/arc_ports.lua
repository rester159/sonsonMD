-- arc_ports.lua -- dump all ioport port:field names for the sonson arcade driver
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_ports.txt","w")
local fr=0
emu.register_frame_done(function()
  fr=fr+1
  if fr==2 then
    for pname,port in pairs(manager.machine.ioport.ports) do
      for fname,_ in pairs(port.fields) do
        f:write(string.format("%s  ::  %s\n", pname, fname))
      end
    end
    f:flush(); f:close(); manager.machine:exit()
  end
end)
print("arc_ports running")
