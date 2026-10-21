-- verify_death_snap.lua : force-kill a WALKER + (if present) a FLYER, trace their
-- death tiles, and snapshot the screen mid-burst so the death anim can be eyeballed.
-- Also snapshot the player death pose.
local GGAME=0xFF0840; local DEADREQ=0xFF083D; local ENT=0xFF0866; local ESTRIDE=0x2A
local mem
local function r8(a) return mem:read_u8(a) end
local function r16(a) return mem:read_u16(a) end
local function w8(a,v) mem:write_u8(a,v) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local DSTATE={[1]=1,[4]=2,[5]=2,[7]=1,[8]=1,[9]=1,[10]=2,[11]=1,[12]=1,[13]=1}
local TNAME={[1]="ENEMY_A",[4]="FLYER",[5]="THROWER",[7]="CREG",[8]="CREC",[9]="CMPE",[10]="HOPPER",[11]="LARGE",[12]="EMIT",[13]="ORB"}
local fr=0; local F_SEL=720; local F_INGAME=1000
local killW,killWfr,killF,killFfr
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; rel()
  if fr>=F_SEL and fr<F_SEL+10 then setbtn("P1 Start",1) end
  if fr>=F_SEL+90 and fr<F_SEL+100 then setbtn("P1 Start",1) end
  if fr<F_INGAME then return end
  setbtn("P1 Right",1)
  if (fr%8)<3 then setbtn("P1 C",1) end
  local ph=(fr//40)%4; if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end

  -- kill a walker at 1500
  if fr==1500 then
    for k=0,47 do local b=ENT+k*ESTRIDE
      if r8(b)~=0 and r8(b+1)==1 then w8(b+17,1); w8(b+15,1); killW=b; killWfr=fr
        print(string.format("F%d KILL walker ent%d",fr,k)); break end end
  end
  if killW and (fr-killWfr)%4==0 and fr<=killWfr+64 then
    print(string.format("  walkerF%-5d active=%d tile=%d",fr,r8(killW),r16(killW+40))) end
  if killW and fr==killWfr+14 then manager.machine.video:snapshot(); print("SNAP walker_death F"..fr) end

  -- look for a flyer to kill from 1600..2600
  if not killF and fr>=1600 and fr<=2600 then
    for k=0,47 do local b=ENT+k*ESTRIDE
      if r8(b)~=0 and r8(b+1)==4 then w8(b+17,1); w8(b+15,2); killF=b; killFfr=fr
        print(string.format("F%d KILL flyer ent%d",fr,k)); break end end
  end
  if killF and (fr-killFfr)%4==0 and fr<=killFfr+50 then
    print(string.format("  flyerF%-5d active=%d tile=%d",fr,r8(killF),r16(killF+40))) end
  if killF and fr==killFfr+12 then manager.machine.video:snapshot(); print("SNAP flyer_death F"..fr) end

  -- player death snapshot at 2800
  if fr==2800 then w8(DEADREQ,1); print("F2800 player dead_request") end
  if fr==2812 then manager.machine.video:snapshot(); print("SNAP player_death F"..fr) end

  if fr>=2900 then manager.machine:exit() end
end)
print("verify_death_snap running")
