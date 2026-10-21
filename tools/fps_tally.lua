-- fps_tally.lua : drive identical real play (run+fire+weave) and tally per-second
-- FPS into buckets so two builds can be compared apples-to-apples. Deterministic
-- input. Prints min/avg + bucket counts at the end.
local GGAME=0xFF0840; local ENT=0xFF0866; local FCTR=0xFF0076; local ESTRIDE=0x2A
local mem
local function r8(a) return mem:read_u8(a) end
local function r32(a) return mem:read_u32(a) end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local fr=0; local F_SEL=720; local F_INGAME=1000
local lastG,lastFr; local samples={}
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; rel()
  if fr>=F_SEL and fr<F_SEL+10 then setbtn("P1 Start",1) end
  if fr>=F_SEL+90 and fr<F_SEL+100 then setbtn("P1 Start",1) end
  if fr<F_INGAME then return end
  setbtn("P1 Right",1)
  if (fr%4)<2 then setbtn("P1 C",1) end
  local ph=(fr//40)%4; if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  if not lastG then lastG=r32(FCTR); lastFr=fr end
  if (fr-lastFr)>=60 then
    local g=r32(FCTR); local s=60.0*(g-lastG)/(fr-lastFr)
    samples[#samples+1]=s; lastG=g; lastFr=fr
  end
  if fr>=7000 then
    local mn=999; local sum=0; local b60,b55,b45,b35,blo=0,0,0,0,0
    for _,s in ipairs(samples) do
      sum=sum+s; if s<mn then mn=s end
      if s>=58 then b60=b60+1 elseif s>=50 then b55=b55+1 elseif s>=40 then b45=b45+1 elseif s>=33 then b35=b35+1 else blo=blo+1 end
    end
    print(string.format("TALLY n=%d min=%.1f avg=%.2f | >=58:%d 50-57:%d 40-49:%d 33-39:%d <33:%d",
      #samples, mn, sum/#samples, b60,b55,b45,b35,blo))
    manager.machine:exit()
  end
end)
print("fps_tally running")
