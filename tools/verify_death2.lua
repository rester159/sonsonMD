-- verify_death2.lua : (a) measure steady-state FPS in busy gameplay (per-second
-- windows) to confirm 60fps with the new death anims, and (b) kill a live ENEMY of
-- whatever type is on screen and log its tile stepping through the death burst.
-- Drives the hero to RUN+FIRE (real play) so enemies spawn, get shot, and die.
local GGAME=0xFF0840; local DEADREQ=0xFF083D; local ENT=0xFF0866; local FCTR=0xFF0076
local ESTRIDE=0x2A
local mem
local function r8(a) return mem:read_u8(a) end
local function r16(a) return mem:read_u16(a) end
local function r32(a) return mem:read_u32(a) end
local function w8(a,v) mem:write_u8(a,v) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end

-- death-state per type: walkers/creatures/large/emitter/orb=1; flyer/thrower/hopper=2
local DSTATE={[1]=1,[4]=2,[5]=2,[7]=1,[8]=1,[9]=1,[10]=2,[11]=1,[12]=1,[13]=1}
local TNAME={[1]="ENEMY_A",[4]="FLYER",[5]="THROWER",[7]="CREG",[8]="CREC",[9]="CMPE",[10]="HOPPER",[11]="LARGE",[12]="EMIT",[13]="ORB"}

local fr=0
local F_SEL=720
local presses={{at=F_SEL},{at=F_SEL+90}}
local F_INGAME=1000
local lastG, lastFr
local killed=nil; local kill_fr=nil
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; rel()
  for _,p in ipairs(presses) do if fr>=p.at and fr<p.at+10 then setbtn("P1 Start",1) end end
  if fr<F_INGAME then return end

  -- real play: run right + fire so enemies come and die
  setbtn("P1 Right",1)
  if (fr%4)<2 then setbtn("P1 C",1) end
  local ph=(fr//40)%4; if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end

  -- per-second FPS windows
  if not lastG then lastG=r32(FCTR); lastFr=fr end
  if (fr-lastFr)>=60 then
    local g=r32(FCTR); local s=60.0*(g-lastG)/(fr-lastFr)
    -- count live entity bodies
    local n=0; for k=0,47 do if r8(ENT+k*ESTRIDE)~=0 then n=n+1 end end
    print(string.format("F%-5d FPS=%.2f  liveEnt=%d", fr, s, n))
    lastG=g; lastFr=fr
  end

  -- at fr 1500, kill a live enemy of any type and trace its death tiles
  if fr==1500 and not killed then
    for k=0,47 do
      local b=ENT+k*ESTRIDE
      local ty=r8(b+1)
      if r8(b)~=0 and DSTATE[ty] then
        w8(b+17,1)                    -- hp=1
        w8(b+15,DSTATE[ty])           -- death state
        w8(b+16,60)                   -- timer (unused for despawn now, but set)
        killed=b; kill_fr=fr
        print(string.format("F%d KILL ent%d type=%s state->%d", fr, k, TNAME[ty], DSTATE[ty]))
        break
      end
    end
    if not killed then print("F1500 no killable enemy on screen") end
  end
  if killed and (fr-kill_fr)%2==0 and fr<=kill_fr+70 then
    print(string.format("  F%-5d death: active=%d state=%d tile=%d",
      fr, r8(killed), r8(killed+15), r16(killed+40)))
  end

  if fr>=3200 then manager.machine:exit() end
end)
print("verify_death2 running")
