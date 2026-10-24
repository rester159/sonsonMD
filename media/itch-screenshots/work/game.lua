local mem=manager.machine.devices[":maincpu"].spaces["program"]
local pc=16714156;local fc=16714182;local lw=16714194;local gg=16714196;local np=16711688;local attr=16714191
local f=io.open("/Users/rester159/genesis ports/_capcom/son son/itch-screenshots/work/frames.csv","w");f:write("refresh,tick,players,camera,entities,p1x,p2x,present,cpu_load_sum,round,stage\n")
local frame=0;local samples=0;local started=false
local function pad(k,n,v) local p=manager.machine.ioport.ports[":ctrl"..k..":mdpad:PAD"];if p and p.fields["P"..k.." "..n] then p.fields["P"..k.." "..n]:set_value(v) end end
emu.register_frame_done(function()
 frame=frame+1
 for k=1,2 do for _,n in ipairs({"Start","Right","Left","Up","Down","B","C"}) do pad(k,n,0) end end
 if not started then
  mem:write_u8(np,2)
  if frame<740 then
   if frame%24<12 then pad(1,"Start",1) end
  else
   for k=1,2 do
    if frame%48<8 then pad(k,"C",1) end
    if frame%48>=24 and frame%48<36 then pad(k,"Start",1) end
   end
  end
  if mem:read_u16(lw)>0 and mem:read_u8(attr)==0 and mem:read_u32(fc)>10 then started=true end
 else
  samples=samples+1
  for k=1,2 do
   mem:write_u8(gg+(k-1)*42+32,3)
   pad(k,"Right",1);if samples%16<3 then pad(k,"B",1) end
   if samples%200<24 then pad(k,"Up",1) elseif samples%200<48 then pad(k,"Down",1) end
  end
  local count=0;for i=0,47 do if mem:read_u8(16711832+i*42)~=0 then count=count+1 end end
  f:write(string.format("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",frame,mem:read_u32(fc),mem:read_u8(gg+84),mem:read_u16(gg+88),count,mem:read_u16(gg),mem:read_u16(gg+42),mem:read_u32(pc),mem:read_u16(16715790),mem:read_u8(gg+85),mem:read_u8(gg+86)))
  if samples%500==0 then manager.machine.video:snapshot() end
  if samples>=19500 then f:close();manager.machine:exit() end
 end
 if frame>19500+6000 then f:close();manager.machine:exit() end
end)
