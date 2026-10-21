-- arc_align2.lua : robust arcade ground-truth capture for player standing-Y.
-- Sets DSW1 Coinage to a 1-coin setting, waits for md($0002)==1 (attract settled),
-- then pulses Coin 1, waits, pulses 1 Player Start, waits for md==2 (in-game).
-- Logs hero Y ($0488 = -$8 of base $0490) + snapshots. Also tracks md transitions.
local MD  = 0x0002
local HX  = 0x0485
local HY  = 0x0488
local SCR = 0x0090
local mem
local function rd8(a) return mem:read_u8(a) end
local function setf(port,field,v)
  local p=manager.machine.ioport.ports[port]
  if p then local fd=p.fields[field]; if fd then fd:set_value(v); return true end end
  return false
end
-- set a DIP field by name to a value
local function setdip(field,v)
  for pn,p in pairs(manager.machine.ioport.ports) do
    local fd=p.fields[field]; if fd then fd:set_value(v); return pn end
  end
  return nil
end
local fr=0
local mdprev=-1
local phase=0   -- 0 wait attract, 1 coined, 2 started, 3 ingame
local coin_fr, start_fr
local snaps=0
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1
  setf(":SYSTEM","Coin 1",0); setf(":SYSTEM","1 Player Start",0)
  local md=rd8(MD)
  if md~=mdprev then print(string.format("F%d md %d->%d",fr,mdprev,md)); mdprev=md end

  if phase==0 and md==1 and fr>200 then
    -- attract reached: coin now
    coin_fr=fr; phase=1
    print("F"..fr.." -> coining")
  elseif phase==1 then
    if fr>=coin_fr and fr<coin_fr+10 then setf(":SYSTEM","Coin 1",1) end
    if fr==coin_fr+40 then start_fr=fr; phase=2; print("F"..fr.." -> starting") end
  elseif phase==2 then
    if fr>=start_fr and fr<start_fr+10 then setf(":SYSTEM","1 Player Start",1) end
    if fr==start_fr+40 then phase=3 end
  end

  if md==2 then
    if (fr%45)==0 and snaps<5 then
      print(string.format("INGAME F%d heroX=%d heroY=%d scroll=%d",fr,rd8(HX),rd8(HY),rd8(SCR)))
      manager.machine.video:snapshot(); snaps=snaps+1
    end
    if snaps>=5 then manager.machine:exit() end
  end
  if fr>=4000 then print("TIMEOUT md="..md); manager.machine:exit() end
end)
print("arc_align2 running")
