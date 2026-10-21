-- arc_force_class.lua -- force each enemy CLASS to spawn by directly arming its
-- control record ($0638+bit*0x10), then read its pool's real sprite (code,bank).
-- Deterministic: stand still (so the wave consumer doesn't overwrite our arming),
-- arm one class at a time. Record: code_lo@base, attr@base+1, active@base-0x10.
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_force_class.txt","w")
local function set(p,fl,v) local pp=manager.machine.ioport.ports[p]; if pp then local fd=pp.fields[fl]; if fd then fd:set_value(v) end end end
local function clr() for _,pf in ipairs({{":SYSTEM","Coin 1"},{":SYSTEM","1 Player Start"},{":P1","P1 Button 1"}}) do set(pf[1],pf[2],0) end end
-- bit -> {name, controlRec, poolBase, nslots, stride}
local CLS={
  {0,"enemyMain",0x0638,0x0828,6,0x20},{1,"classC",0x0648,0x0A28,6,0x20},
  {2,"classD",0x0658,0x0AE8,6,0x20},  {3,"classE",0x0668,0x0B68,4,0x20},
  {4,"classF",0x0678,0x0BA8,4,0x20},  {5,"classH",0x0688,0x0CE8,6,0x20},
  {6,"classG",0x0698,0x0C28,6,0x20},  {7,"classI",0x06A8,0x0DA8,4,0x20},
  {9,"obj16x16",0x06C8,0x0E48,3,0x30},
}
local fr=0; local everInGame=false; local coinedAt=nil; local H={}
emu.register_frame_done(function()
  fr=fr+1; clr()
  local gs=r8(0x0002)
  if gs==2 then everInGame=true end
  -- DUMP first (always runs), so we never lose output
  if fr>=10500 then
    f:write("=== FORCE-ARMED per-class sprite (code,bank) from pool records ===\n")
    f:write(string.format("everInGame=%s\n",tostring(everInGame)))
    for _,c in ipairs(CLS) do local nm=c[2]
      f:write(string.format("bit%d %s (rec $%04X pool $%04X): ",c[1],nm,c[3],c[4]))
      local ks={}; for k,_ in pairs(H[nm] or {}) do ks[#ks+1]=k end
      table.sort(ks,function(a,b) return H[nm][a].n>H[nm][b].n end)
      if #ks==0 then f:write("(never active)") end
      f:write("\n")
      for i,k in ipairs(ks) do if i<=5 then local e=H[nm][k]
        f:write(string.format("    %s  n=%-5d y=[%d..%d]\n",k,e.n,e.ymin,e.ymax)) end end
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
  w8(0x0073,0xFF)
  -- cycle which class is armed: ~700 frames each
  local idx=((fr//700)% #CLS)+1
  local c=CLS[idx]
  local rec=c[3]
  w8(rec,1); w8(rec+2,0); w8(rec+3,0); w8(rec+0x0E,0)   -- enable + clear state + param E
  if (fr%30)<3 then set(":P1","P1 Button 1",1) end       -- fire to recycle
  -- read THIS class's pool
  local nm=c[2]; H[nm]=H[nm] or {}
  local base,n,stride=c[4],c[5],c[6]
  for s=0,n-1 do local r=base+s*stride
    if r8(r-0x10)~=0 then
      local cl=r8(r); local attr=r8(r+1); local sy=r8(r-8)
      local code=cl+((attr&0x20)<<3); local bank=attr&0x1F
      local k=string.format("code=%3d bank=%2d",code,bank)
      local e=H[nm][k]; if not e then e={n=0,ymin=999,ymax=-1}; H[nm][k]=e end
      e.n=e.n+1; if sy<e.ymin then e.ymin=sy end; if sy>e.ymax then e.ymax=sy end
    end
  end
end)
print("running")
