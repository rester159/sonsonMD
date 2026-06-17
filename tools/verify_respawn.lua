-- verify_respawn.lua : confirm the hero respawns at the RIGHT band edge (arcade
-- B1C2 worldX=scroll+$F8 -> screenX clamped to BAND_RIGHT 208), not BAND_LEFT 8.
-- IDLE so the player dies; when flow enters RESPAWNING (2), read screenX.
local GFC=0xFF0076; local LEVELW=0xFF078A; local PX=0xFF07E0; local CAMX=0xFF0806; local FLOW=0xFF0800
local mem; local em=0; local prevflow=-1; local done=false
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1
  pad("P1 Start",0)
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (em%80)<12 then pad("P1 Start",1) end; return end
  -- IDLE in-game (no input): the player gets hit and dies/respawns
  local flow=mem:read_u8(FLOW)
  if flow~=prevflow then
    local names={[0]="ALIVE",[1]="DYING",[2]="RESPAWNING",[3]="GAMEOVER",[4]="ROUNDCLEAR"}
    print(string.format("flow %s -> %s (em=%d)", names[prevflow] or "?", names[flow] or "?", em))
    if flow==2 and not done then
      local px,cx=rds16(PX),rds16(CAMX); local sx=px-cx
      print(string.format("RESPAWN screenX=%d  (BAND_LEFT=8, BAND_RIGHT=208; arcade B1C2 = RIGHT) -> %s",
            sx, (sx>150) and "RIGHT (faithful)" or ((sx<60) and "LEFT (unfaithful)" or "mid")))
      manager.machine.video:snapshot(); done=true
    end
    prevflow=flow
  end
  if done and (em%4)==0 then  -- after respawn snapshot, exit a few frames later
    if not _t then _t=em end; if em-_t>=8 then manager.machine:exit() end
  end
  if em>2600 then print("no respawn observed by em=2600 (flow="..flow..")"); manager.machine:exit() end
end)
print("verify_respawn.lua running")
