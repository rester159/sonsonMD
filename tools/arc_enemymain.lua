-- arc_enemymain.lua -- isolate enemyMain: pin wave 12 (mask 0x0001 = bit0 only) so
-- ONLY enemyMain spawns into $0828; read its real sprite (walker + thrower variants).
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_enemymain.txt","w")
local function set(p,fl,v) local pp=manager.machine.ioport.ports[p]; if pp then local fd=pp.fields[fl]; if fd then fd:set_value(v) end end end
local function clr() for _,pf in ipairs({{":SYSTEM","Coin 1"},{":SYSTEM","1 Player Start"},{":P1","P1 Button 1"}}) do set(pf[1],pf[2],0) end end
local fr=0; local coinedAt=nil; local H={}
local WAVES={12,44,50,74}
emu.register_frame_done(function()
  fr=fr+1; clr()
  local gs=r8(0x0002)
  if fr>=9000 then
    f:write("=== enemyMain $0828 (pinned bit0-only waves) ===\n")
    local ks={}; for k,_ in pairs(H) do ks[#ks+1]=k end
    table.sort(ks,function(a,b) return H[a].n>H[b].n end)
    for _,k in ipairs(ks) do local e=H[k]; f:write(string.format("  %s n=%-5d y=[%d..%d]\n",k,e.n,e.ymin,e.ymax)) end
    f:close(); manager.machine:exit(); return
  end
  if gs~=2 then
    if gs==1 then if coinedAt==nil then coinedAt=fr end
      local c=(fr-coinedAt)%140
      if c>=10 and c<35 then set(":SYSTEM","Coin 1",1) elseif c>=60 and c<90 then set(":SYSTEM","1 Player Start",1) end end
    return
  end
  coinedAt=nil
  w8(0x0073,0xFF)
  w8(0x0090, WAVES[((fr//1200)%4)+1]); w8(0x0091,0x80)   -- cycle bit0-only waves
  if (fr%30)<3 then set(":P1","P1 Button 1",1) end
  for s=0,5 do local rec=0x0828+s*0x20
    if r8(rec-0x10)~=0 then
      local code=r8(rec)+((r8(rec+1)&0x20)<<3); local bank=r8(rec+1)&0x1F; local sy=r8(rec-8)
      if bank~=22 and not (bank<=1 and code<=85) then  -- skip HUD(b22) + player(b0/1)
        local k=string.format("code=%3d bank=%2d",code,bank)
        local e=H[k]; if not e then e={n=0,ymin=999,ymax=-1}; H[k]=e end
        e.n=e.n+1; if sy<e.ymin then e.ymin=sy end; if sy>e.ymax then e.ymax=sy end
      end
    end
  end
end)
print("running")
