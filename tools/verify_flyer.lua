local GLEVELW=0xFF0072; local GGAME=0xFF0808; local ENTITIES=0xFF0834; local EMAX=48; local STRIDE=0x2A
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r8(a) return mem:read_u8(a) end
local function r16(a) return mem:read_u16(a) end
local function wr8(a,v) mem:write_u8(a,v) end
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do sb(n,0) end end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_flyer.txt","w")
local fr=0; local snapped=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if r16(GLEVELW)==0 then if (fr%16)<8 then sb("P1 Start",1) end return end
  wr8(0xFF0826,5); sb("P1 Right",1)
  if (fr%24)<2 then sb("P1 B",1) end
  -- find a live ET_FLYER (type 4) on screen
  local found=nil
  for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE
    if r8(b)~=0 and r8(b+1)==4 then found=i end end
  if found and snapped<3 and (fr%9)==0 then
    manager.machine.video:snapshot()
    f:write(string.format("fr=%d SNAP flyer slot %d\n",fr,found)); f:flush(); snapped=snapped+1
  end
  if fr>=3000 or snapped>=3 then f:write(string.format("done fr=%d snaps=%d\n",fr,snapped)); f:close(); manager.machine:exit() end
end)
print("running")
