-- arc_pshot.lua -- resolve the PLAYER bullet sprite from the arcade. $B25A fires
-- $0E28 only when player screenX>=0xF8 -> drive HARD RIGHT + fire. Read $0E28
-- (active@$0E18, code@$0E28, attr@$0E29) and classI $0DA8 (enemy lob). Also dump
-- spriteram fire-correlated sprites. code=code_lo+((attr&0x20)<<3); bank=attr&0x1F.
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local spr=manager.machine.memory.shares[":spriteram"]
local function r8(a) return mem:read_u8(a) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_pshot.txt","w")
local function set(p,fl,v) local pp=manager.machine.ioport.ports[p]; if pp then local fd=pp.fields[fl]; if fd then fd:set_value(v) end end end
local function clr() for _,pf in ipairs({{":SYSTEM","Coin 1"},{":SYSTEM","1 Player Start"},{":P1","P1 Button 1"},{":P1","P1 Right"},{":P1","P1 Up"},{":P1","P1 Down"}}) do set(pf[1],pf[2],0) end end
local fr=0; local everInGame=false; local coinedAt=nil
local e28={}     -- $0E28 player-shot (code/bank) histogram
local i_da8={}   -- classI $0DA8
local function rec(h,cl,attr,sy) local code=cl+((attr&0x20)<<3); local bank=attr&0x1F
  local k=string.format("code=%3d bank=%2d",code,bank); local e=h[k]; if not e then e={n=0,ymin=999,ymax=-1}; h[k]=e end
  e.n=e.n+1; if sy<e.ymin then e.ymin=sy end; if sy>e.ymax then e.ymax=sy end end
emu.register_frame_done(function()
  fr=fr+1; clr()
  local gs=r8(0x0002)
  if gs==2 then everInGame=true end
  if fr>=9000 then
    f:write("=== PLAYER SHOT pool $0E28 (drive-right+fire) ===\n")
    local ks={}; for k,_ in pairs(e28) do ks[#ks+1]=k end; table.sort(ks,function(a,b) return e28[a].n>e28[b].n end)
    if #ks==0 then f:write("  (never active -- player never reached the fire gate)\n") end
    for _,k in ipairs(ks) do local e=e28[k]; f:write(string.format("  %s n=%-5d y=[%d..%d]\n",k,e.n,e.ymin,e.ymax)) end
    f:write("=== classI $0DA8 (enemy lobbed shot) ===\n")
    ks={}; for k,_ in pairs(i_da8) do ks[#ks+1]=k end; table.sort(ks,function(a,b) return i_da8[a].n>i_da8[b].n end)
    if #ks==0 then f:write("  (never active)\n") end
    for _,k in ipairs(ks) do local e=i_da8[k]; f:write(string.format("  %s n=%-5d y=[%d..%d]\n",k,e.n,e.ymin,e.ymax)) end
    f:close(); manager.machine:exit(); return
  end
  if gs~=2 then
    if gs==1 then if coinedAt==nil then coinedAt=fr end
      local c=(fr-coinedAt)%140
      if c>=10 and c<35 then set(":SYSTEM","Coin 1",1) elseif c>=60 and c<90 then set(":SYSTEM","1 Player Start",1) end end
    return
  end
  coinedAt=nil
  mem:write_u8(0x0073,0xFF)
  set(":P1","P1 Right",1)                      -- push to the right edge (trigger $B25A)
  if (fr%12)<4 then set(":P1","P1 Button 1",1) end
  -- $0E28 player shot
  if r8(0x0E18)~=0 then rec(e28, r8(0x0E28), r8(0x0E29), r8(0x0E20)) end
  -- classI $0DA8 (4 slots, stride 0x20)
  for s=0,3 do local base=0x0DA8+s*0x20
    if r8(base-0x10)~=0 then rec(i_da8, r8(base), r8(base+1), r8(base-8)) end end
end)
print("running")
