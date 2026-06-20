local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r16(a) return mem:read_u16(a) end
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 A","P1 B","P1 C"}) do sb(n,0) end end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_attract.txt","w")
local fr=0; local shot=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- hold Start 6 frames at two points to clear selector + config (multi-frame
  -- press required by the front-end input filter), then NO input -> title idles.
  if (fr>=700 and fr<706) or (fr>=950 and fr<956) then sb("P1 Start",1) end
  if fr>=1100 and (fr%300)==0 and shot<14 then
    manager.machine.video:snapshot(); f:write(string.format("snap %02d fr=%d lw=%d\n",shot,fr,r16(0xFF0072))); f:flush(); shot=shot+1
  end
  if fr>=4200 then f:close(); manager.machine:exit() end
end)
print("running")
