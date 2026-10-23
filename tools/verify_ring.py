#!/usr/bin/env python3
"""Target VRAM regression: injected boundary fixture, all reachable ring phases for the current ribbon length."""
import pathlib,re,json,subprocess,math
root=pathlib.Path(__file__).resolve().parents[1];out=root/'out/ring-check';out.mkdir(parents=True,exist_ok=True)
header=(root/'inc/ss_level.h').read_text()
cols=int(re.search(r'#define SS_RIBBON_COLS\s+(\d+)',header)[1]);levels=int(re.search(r'#define SS_LEVEL_COUNT\s+(\d+)',header)[1]);phases=64//math.gcd(cols,64)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(root/'out/symbol.txt').read_text(),re.M)}
s='''local mem=manager.machine.devices[":maincpu"].spaces.program
local vram=manager.machine.devices[":gen_vdp"].spaces.videoram
local fr=0;local checks=0;local failures=0;local turns=0;local seeded=false;local age=0
local gg=GG;local phase=0;local previous=0
local function tile(c,r)
 c=c%COLS;local level=0
 for i=0,LEVELCOUNT-1 do if c>=mem:read_u16(PREFIX+i*2) then level=i end end
 local d=LEVELS+level*14;local map=mem:read_u32(d+6);local cols=mem:read_u16(d+10)
 return mem:read_u16(map+(r*cols+c-mem:read_u16(PREFIX+level*2))*2)
end
local function seed()
 local camera=COLS*8-32;local first=camera//8;local base=mem:read_u16(BGA)
 mem:write_u16(gg+88,camera);mem:write_u16(gg+90,first);mem:write_u16(gg+92,first+63)
 for k=0,1 do mem:write_u16(gg+k*42,camera+80+k*24);mem:write_u8(gg+k*42+32,99) end
 for c=first,first+63 do for r=0,31 do vram:write_u16(base+(r*64+(c+phase)%64)*2,tile(c,r)) end end
 seeded=true;age=0;previous=camera
end
emu.register_frame_done(function()
 fr=fr+1
 local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"];p.fields["P1 C"]:set_value((not seeded and fr%48<8) and 1 or 0);p.fields["P1 Start"]:set_value((not seeded and fr%48>=24 and fr%48<36) and 1 or 0)
 if not seeded then if mem:read_u16(LW)>0 and mem:read_u8(ATTR)==0 and mem:read_u32(FC)>20 then seed() end
 else
  age=age+1;local camera=mem:read_u16(gg+88)
  if camera<previous then turns=turns+1;phase=(turns*COLS)%64 end
  previous=camera
  local first=camera//8;local base=mem:read_u16(BGA)
  if age>2 then
   for c=first,first+32 do for r=0,31 do
    local actual=vram:read_u16(base+(r*64+(c+phase)%64)*2);local expected=tile(c,r)
    checks=checks+1;if actual~=expected then failures=failures+1 end
   end end
  end
  if camera<64 and age>100 then
   if turns>=PHASES then
    local f=io.open(RESULT,"w");f:write(string.format('{"wrap_fixtures":%d,"tile_words_checked":%d,"mismatches":%d}',turns,checks,failures));f:close();manager.machine:exit()
   else seed() end
  end
 end
 if fr%500==0 then print("RING",fr,seeded,turns,age,mem:read_u16(gg+88),failures) end
 if fr>7000 then manager.machine:exit() end
end)
'''
for k,v in {'COLS':cols,'LEVELCOUNT':levels,'PHASES':phases,'GG':syms['g_game'],'PREFIX':syms['ss_ribbon_col0'],'LEVELS':syms['ss_levels'],'BGA':syms['bga_addr'],'LW':syms['g_levelW'],'ATTR':syms['g_attract'],'FC':syms['g_frameCounter'],'RESULT':json.dumps(str(out/'results.json'))}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(out/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(root/'out/rom.bin'),'-autoboot_script',str(out/'run.lua'),'-skip_gameinfo','-window','-nomaximize','-resolution','512x448','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-nothrottle','-video','none','-sound','none','-noreadconfig','-nvram_directory',str(out/'nvram')],cwd=root,stdout=(out/'mame.txt').open('w'),stderr=subprocess.STDOUT,check=True)
result=json.loads((out/'results.json').read_text());print(result);assert result['mismatches']==0
