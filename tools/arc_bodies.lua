-- arc_bodies.lua -- read each class pool's ACTUAL rendered body sprite during long
-- natural-scroll play (reaches deep waves where classF/G/D/E spawn). Dump is BEFORE
-- the in-game guard so output always writes. code=code_lo+((attr&0x20)<<3); bank=attr&0x1F.
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r8(a) return mem:read_u8(a) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_bodies.txt","w")
local function set(p,fl,v) local pp=manager.machine.ioport.ports[p]; if pp then local fd=pp.fields[fl]; if fd then fd:set_value(v) end end end
local function clr() for _,pf in ipairs({{":SYSTEM","Coin 1"},{":SYSTEM","1 Player Start"},{":P1","P1 Button 1"},{":P1","P1 Right"},{":P1","P1 Up"},{":P1","P1 Down"}}) do set(pf[1],pf[2],0) end end
local POOLS={ {"enemyMain",0x0828,6,0x20},{"classC",0x0A28,6,0x20},{"classD",0x0AE8,6,0x20},{"classE",0x0B68,4,0x20},
  {"classF",0x0BA8,4,0x20},{"classG",0x0C28,6,0x20},{"classH",0x0CE8,6,0x20},{"classI",0x0DA8,4,0x20},
  {"obj16x16",0x0E48,3,0x30},{"objSingle",0x0E28,1,0x20} }
local fr=0; local everInGame=false; local coinedAt=nil; local H={}
emu.register_frame_done(function()
  fr=fr+1; clr()
  local gs=r8(0x0002)
  if gs==2 then everInGame=true end
  if fr>=20000 then
    f:write("=== PER-CLASS body sprite (actual pool render, natural deep scroll) ===\n")
    for _,p in ipairs(POOLS) do local cls=p[1]
      f:write(string.format("%s ($%04X): ",cls,p[2]))
      local ks={}; for k,_ in pairs(H[cls] or {}) do ks[#ks+1]=k end
      table.sort(ks,function(a,b) return H[cls][a].n>H[cls][b].n end)
      if #ks==0 then f:write("(never active)") end; f:write("\n")
      for i,k in ipairs(ks) do if i<=6 then local e=H[cls][k]
        f:write(string.format("    %s n=%-5d y=[%d..%d]\n",k,e.n,e.ymin,e.ymax)) end end
    end
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
  set(":P1","P1 Right",1)
  if (fr%50)<3 then set(":P1","P1 Button 1",1) end
  local ph=(fr//40)%4
  if ph==1 then set(":P1","P1 Up",1) elseif ph==3 then set(":P1","P1 Down",1) end
  for _,p in ipairs(POOLS) do local cls,base,n,stride=p[1],p[2],p[3],p[4]; H[cls]=H[cls] or {}
    for s=0,n-1 do local rec=base+s*stride
      if r8(rec-0x10)~=0 then
        local code=r8(rec)+((r8(rec+1)&0x20)<<3); local bank=r8(rec+1)&0x1F; local sy=r8(rec-8)
        local k=string.format("code=%3d bank=%2d",code,bank)
        local e=H[cls][k]; if not e then e={n=0,ymin=999,ymax=-1}; H[cls][k]=e end
        e.n=e.n+1; if sy<e.ymin then e.ymin=sy end; if sy>e.ymax then e.ymax=sy end
      end
    end
  end
end)
print("running")
