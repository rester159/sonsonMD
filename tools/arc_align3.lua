-- arc_align3.lua : arcade ground-truth. Set Coinage=1C/1C, coin, start, drive,
-- read hero Y ($0488), snapshot. Hero base $0490; Y@-8=$0488, X int@-B=$0485.
local MD=0x0002
local HX=0x0485
local HY=0x0488
local SCR=0x0090
local mem
local function rd8(a) return mem:read_u8(a) end
local function setf(port,field,v)
  local p=manager.machine.ioport.ports[port]
  if p then local fd=p.fields[field]; if fd then fd:set_value(v); return true end end
  return false
end
local fr=0
local mdprev=-1
local phase=0
local t0
local snaps=0
local diddip=false
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1
  if not diddip then
    -- Coinage is an enum; print its allowed values once, set to lowest-cost (1C/1C).
    local p=manager.machine.ioport.ports[":DSW1"]
    local fd=p and p.fields["Coinage"]
    if fd then
      -- default value usually 1C/1C already (0x0F per memory). Force a known setting.
      fd:set_value(fd.defvalue)
    end
    diddip=true
  end
  setf(":SYSTEM","Coin 1",0); setf(":SYSTEM","1 Player Start",0)
  local md=rd8(MD)
  if md~=mdprev then print(string.format("F%d md %d->%d",fr,mdprev,md)); mdprev=md end

  if phase==0 and md==1 and fr>200 then t0=fr; phase=1; print("F"..fr.." attract -> insert coin") end
  if phase==1 then
    -- hold coin 30 frames
    if fr>=t0 and fr<t0+30 then setf(":SYSTEM","Coin 1",1) end
    if fr==t0+60 then phase=2; t0=fr; print("F"..fr.." -> press start") end
  elseif phase==2 then
    if fr>=t0 and fr<t0+30 then setf(":SYSTEM","1 Player Start",1) end
    if fr==t0+60 then phase=3; t0=fr; print("F"..fr.." -> waiting for md==2 (else retry)") end
  elseif phase==3 then
    -- if still not in game, retry coin+start once more
    if md~=2 and fr>t0+120 then phase=0 end
  end

  if md==2 then
    -- idle a beat then snapshot standing pose
    if (fr%40)==0 and snaps<6 then
      print(string.format("INGAME F%d heroX=%d heroY=%d scroll=%d",fr,rd8(HX),rd8(HY),rd8(SCR)))
      manager.machine.video:snapshot(); snaps=snaps+1
    end
    if snaps>=6 then manager.machine:exit() end
  end
  if fr>=5000 then print("TIMEOUT md="..md); manager.machine:exit() end
end)
print("arc_align3 running")
