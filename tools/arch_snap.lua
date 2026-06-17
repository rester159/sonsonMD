-- arch_snap.lua : capture varied faithful-scene snapshots (round 1 creature mix,
-- round 2 flyer mix) from the native port for the report. Saves PNGs to the MAME
-- snap dir; we also log the on-screen census at each snap so the report can name
-- what's visible. Pure hold-RIGHT + fire; snaps at dense moments.
local GGAME=0xFF0840; local ENTITIES=0xFF0866
local EMAX,STRIDE=48,0x2A; local OFF_TYPE,OFF_FROMSCHED,ROUND_OFF=1,0x1C,0x1E
local TYPENAME={[1]="ENEMY_A",[2]="SHOT",[3]="ITEM",[4]="FLYER",[5]="THROWER",
  [6]="ESHOT",[7]="CREATURE_G",[8]="CREATURE_C",[9]="COMPOSITE_E",[10]="HOPPER",
  [11]="LARGE",[12]="EMITTER",[13]="ORB",[14]="POPUP"}
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rd8(a) return mem:read_u8(a) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arch_snap.txt","w")
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function census()
  local byType={}; local total=0
  for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE
    if rd8(b)~=0 then total=total+1; local t=rd8(b+OFF_TYPE); byType[t]=(byType[t] or 0)+1 end end
  local ts={}; for t,n in pairs(byType) do ts[#ts+1]=string.format("%s=%d",TYPENAME[t] or ("?"..t),n) end
  table.sort(ts); return total, table.concat(ts," ")
end
local fr=0; local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local snaps={1380,1500,1620, 2340,2460}   -- r1 creature mix, r2 flyer mix
local si=1
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then setbtn("P1 Right",1); if (fr%8)==0 then setbtn("P1 B",1) end end
  if si<=#snaps and fr==snaps[si] then
    manager.machine.video:snapshot()
    local total,desc=census()
    f:write(string.format("SNAP %d: fr=%d round=%d total=%d [%s]\n", si, fr, rd8(GGAME+ROUND_OFF), total, desc))
    f:flush(); si=si+1
  end
  if fr>=2600 then f:close(); manager.machine:exit() end
end)
print("arch_snap running")
