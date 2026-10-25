local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right","P1 A","P1 B","P1 C"}) do p1(n,0) end end
local TYPE={[1]="ENEMY_A",[2]="SHOT",[3]="ITEM",[4]="FLYER",[5]="THROWER",[6]="ENEMY_SHOT",[7]="CREATURE_G",[8]="CREATURE_C",[9]="COMPOSITE_E",[10]="HOPPER",[11]="LARGE",[12]="EMITTER",[13]="ORB",[14]="POPUP"}
local f=io.open("tools/diag_probe.txt","w")
local fr=0
local function dump(tag)
  local px=mem:read_i16(0xFF07F0)
  f:write(string.format("=== %s  playerX=%d ===\n",tag,px))
  for i=0,47 do
    local b=0xFF084E+i*42
    if mem:read_u8(b)~=0 then
      local t=mem:read_u8(b+1); local x=mem:read_i16(b+2); local fc=mem:read_u8(b+18)
      local want = (px<x) and 1 or 0   -- toward player: face left(1) if player is left
      f:write(string.format("slot%2d %-10s x=%5d facing=%d  want=%d %s  side=%s\n",
        i, TYPE[t] or ("?"..t), x, fc, want, (fc==want) and "OK" or "MISMATCH",
        (x>px) and "right-of-player" or "left-of-player"))
    end
  end
  f:flush()
end
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr>=700 and fr<712 then p1("P1 Down",1) end
  if fr>=770 and fr<782 then p1("P1 Start",1) end
  if fr>=900 and fr<914 then p1("P1 A",1) end
  if fr>=1080 then p1("P1 Right",1); if (fr%24)<4 then p1("P1 B",1) end end
  if fr==1400 then dump("fr1400"); manager.machine.video:snapshot() end
  if fr==1500 then dump("fr1500"); manager.machine.video:snapshot() end
  if fr>=1540 then f:close(); manager.machine:exit() end
end)
print("running")
