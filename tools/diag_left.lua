local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end end
local f=io.open("tools/diag_left.txt","w")
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr>=700 and fr<712 then p1("P1 Down",1) end
  if fr>=770 and fr<782 then p1("P1 Start",1) end
  if fr>=900 and fr<914 then p1("P1 A",1) end
  if fr>=1080 then p1("P1 Left",1) end   -- hold LEFT: player to left edge, enemies to its right
  if fr==1400 then
    local px=mem:read_i16(0xFF07F0); f:write("playerX="..px.."\n")
    for i=0,47 do local b=0xFF084E+i*42
      if mem:read_u8(b)~=0 and mem:read_u8(b+1)>=1 and mem:read_u8(b+1)<=13 then
        f:write(string.format("t=%d x=%d facing=%d side=%s\n",mem:read_u8(b+1),mem:read_i16(b+2),mem:read_u8(b+18), (mem:read_i16(b+2)>px) and "RIGHT" or "left")) end end
    f:flush(); manager.machine.video:snapshot()
  end
  if fr>=1450 then f:close(); manager.machine:exit() end
end)
print("running")
