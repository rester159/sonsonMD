-- verify_vertical.lua : prove the FAITHFUL vertical model in the native build.
-- Boots SEGA->CAPCOM->selector->config->game (pulse START until g_levelW!=0).
-- Then: settle, hold UP (climb up tiers), hold DOWN (climb down), hold RIGHT (run+
-- scroll), and observe a FALL by standing over a gap column. Logs player Y/vstate/
-- tier + snapshots. Verifies on-platform (Y == one of the 6 SS_TIER_Y), climb,
-- fall, and the 60fps ratio (g_frameCounter/emu-frame ~1.000).
--
-- Player struct (game.h): x@+0(s16) y@+2(s16) ySub@+6 vstate@+12(u8) tier@+13(u8)
--   climbing@+14 facing@+15. g_game base below (re-derive per build from symbol.txt).
local GAME   = 0xFF0844
local PX     = GAME + 0
local PY     = GAME + 2
local PVST   = GAME + 12
local PTIER  = GAME + 13
local LEVELW = 0xFF07EE
local GFC    = 0xFF0076
local TIER_Y = {200,168,136,104,72,40}   -- SS_TIER_Y (bottom..top) -- the valid stand Ys
local VNAME  = {[0]="RUN",[1]="CLMBUP",[2]="CLMBDN",[3]="FALL"}

local mem
local function rd8(a) return mem:read_u8(a) end
local function rd16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function rd32(a) return mem:read_u32(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end

local function onplatform(y)
  for _,ty in ipairs(TIER_Y) do if y==ty then return true end end
  return false
end

local fr=0
local ig0, gfc0
local last=""
local snaps=0
local seen={RUN=false,CLMBUP=false,CLMBDN=false,FALL=false}
local minY,maxY=999,-999

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1
  release_all()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (fr%80)<12 then setbtn("P1 Start",1) end return end
  if not ig0 then ig0=fr; gfc0=rd32(GFC)
    print(string.format("INGAME F%d Y=%d vstate=%d tier=%d",fr,rd16(PY),rd8(PVST),rd8(PTIER)))
    manager.machine.video:snapshot(); snaps=snaps+1
  end
  local rel=fr-ig0

  -- script: settle[0-60] UP[60-160] settle[160-200] DOWN[200-360] settle[360-400]
  --         RIGHT[400-700] (run+scroll, also crosses gap cols -> may FALL)
  local act="idle"
  if rel>=60 and rel<160 then setbtn("P1 Up",1); act="UP"
  elseif rel>=200 and rel<360 then setbtn("P1 Down",1); act="DOWN"
  elseif rel>=400 and rel<900 then setbtn("P1 Right",1); act="RIGHT" end

  local y=rd16(PY); local vst=rd8(PVST); local ti=rd8(PTIER)
  if y<minY then minY=y end
  if y>maxY then maxY=y end
  local vn=VNAME[vst] or ("?"..vst)
  if seen[vn]~=nil then seen[vn]=true end
  local s=string.format("%s Y=%d v=%s tier=%d",act,y,vn,ti)
  if s~=last then print(string.format("rel%-4d %s X=%d",rel,s,rd16(PX))); last=s end

  -- snapshot at a few moments (one per tier reached while climbing + after fall)
  if (rel==150 or rel==350 or rel==700) and snaps<5 then
    manager.machine.video:snapshot(); snaps=snaps+1
  end

  if rel>=920 then
    local gf=rd32(GFC)-gfc0; local ef=fr-ig0
    print(string.format("DONE: %d emu-frames, gfc+%d => %.3f game-frames/emu-frame (1.000=60fps)",ef,gf,gf/ef))
    print(string.format("Y range while playing: %d..%d  (valid tier Ys: 40,72,104,136,168,200)",minY,maxY))
    print(string.format("states seen: RUN=%s CLMBUP=%s CLMBDN=%s FALL=%s",
      tostring(seen.RUN),tostring(seen.CLMBUP),tostring(seen.CLMBDN),tostring(seen.FALL)))
    manager.machine:exit()
  end
  if fr>=24000 then print("HARD TIMEOUT"); manager.machine:exit() end
end)
print("verify_vertical running")
