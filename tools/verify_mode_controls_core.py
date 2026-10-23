#!/usr/bin/env python3
"""Windowless controller-only Arcade controls and Home options/pause/continue."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/mode-controls-core';o.mkdir(parents=True,exist_ok=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
base=(r/'tools/verify_libretro.c').read_text();a=base.index('static int16_t input(');b=base.index('#define FN',a)
s=base[:a]+'''static unsigned home_case,continue_at,failures,checks;
static bool held(unsigned a,unsigned b){return frame>=a&&frame<b;}
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;
 if(home_case){
  if(port)return id==3&&held(1820,1832);
  if(continue_at)return id==3&&held(continue_at,continue_at+30);
  return(id==5&&(held(920,932)||held(1200,1212)||held(1240,1252)||held(1280,1292)||held(1320,1332)||held(1600,1608)||held(1620,1632)))||
   (id==4&&held(1040,1052))||(id==7&&(held(1360,1372)||held(1400,1412)||held(1440,1452)))||
   (id==6&&home_case!=2&&(held(1480,1492)||held(1520,1532)))||
   (id==3&&(held(950,965)||held(1120,1132)||held(1560,1575)||held(1640,1680)||held(1740,1770)))||
   (id==8&&held(1880,1900));
 }
 if(frame<700)return id==3&&frame%24<12&&port==0;
 if(port==0)return(id==3&&(held(850,865)||held(1000,1012)||held(1130,1150)))||(id==8&&(held(940,970)||held(1040,1060)));
 return(id==3&&held(1080,1100))||(id==8&&held(1160,1180));
}
static void check(bool value,const char*name){checks++;printf("%s %s\\n",value?"PASS":"FAIL",name);if(!value)failures++;}
'''+base[b:base.index('int main(int argc,char**argv)')]
init=base[base.index('    void*core=dlopen'):base.index('    FN(void,retro_get_system_av_info')]
s+='\nint main(int argc,char**argv){if(argc!=4)return 2;home_case=atoi(argv[3]);\n'+init+'''
 uint8_t*ram=retro_get_memory_data(2);if(!ram)return 2;unsigned frozen=0,camera=0,chosen=home_case==2?0:5;bool finished=false;
 for(frame=0;frame<9000;frame++){
  retro_run();
  if(!home_case){
   if(frame==900)check(r16(ram,LW)==0,"unpaid Start stays out of gameplay");
   if(frame==985)check(r16(ram,AC)==1&&r16(ram,LW)==0,"held C inserts exactly one coin");
   if(frame==1030)check(r16(ram,LW)>0&&r16(ram,AC)==0&&r8(ram,GG+32)>0&&r8(ram,GG+74)==0,"P1 paid Start admits only P1");
   if(frame==1070){unsigned shots=0;for(unsigned i=16;i<20;i++)shots+=r8(ram,ENT+i*42)!=0;check(r16(ram,AC)==1&&shots==0,"gameplay C coins without shooting");}
   if(frame==1110)check(r8(ram,GG+32)>0&&r8(ram,GG+74)>0&&r16(ram,AC)==0&&!r8(ram,PAUSE),"P2 Start joins without pausing");
   if(frame==1155)check(!r8(ram,PAUSE)&&r16(ram,AC)==0,"live-player held Start neither pauses nor spends credits");
   if(frame==1190){check(r16(ram,AC)==1&&!r8(ram,PAUSE),"controller2 held C inserts one coin");finished=true;break;}
  }else{
   if(frame==1390)check(r8(ram,CONFIG+7)==5,"options credits3 to5");
   if(frame==1430)check(r8(ram,CONFIG+7)==9,"options credits5 to9");
   if(frame==1470)check(r8(ram,CONFIG+7)==0,"options credits9 to0");
   if(frame==1550)check(r8(ram,CONFIG+7)==chosen,"selected final option credits");
   if(frame==1700)check(r8(ram,MODE)==1&&r16(ram,LW)>0&&r8(ram,GG+84)==2&&r8(ram,HC)==chosen&&!r8(ram,PAUSE),"Home2P uses selected credits and ignores held menu Start");
   if(frame==1760){frozen=r32(ram,FC);camera=r16(ram,GG+88);check(r8(ram,PAUSE)==1,"Home fresh Start pauses");}
   if(frame==1790)check(r32(ram,FC)==frozen&&r16(ram,GG+88)==camera,"held Start leaves simulation and camera frozen");
   if(frame==1860)check(!r8(ram,PAUSE)&&r32(ram,FC)>frozen,"controller2 Start resumes Home");
   if(frame==1920)check(r8(ram,HC)==chosen&&r16(ram,AC)==0,"Home C cannot mint credits");
   if(frame>2000&&!continue_at&&r8(ram,GG+34)==3&&r8(ram,GG+76)==3)continue_at=frame+2;
   if(continue_at&&frame==continue_at+45){
    if(home_case==2){
     check(r8(ram,HC)==0&&r8(ram,GG+32)==0&&r8(ram,GG+74)==0&&r8(ram,GG+34)==3&&r8(ram,GG+76)==3,"zero Home credits prevent continuing either player");
     check(!r8(ram,PAUSE),"unpaid game-over Start does not pause");
    }else{
    check(r8(ram,HC)==4&&r8(ram,GG+32)==3&&r8(ram,GG+74)==3,"Home natural game-over continue spends one shared credit");
    check(!r8(ram,PAUSE)&&r8(ram,OWN)==1&&r8(ram,OWN+1)==1,"Home continue admits both source reentry owners without held-Start pause");
    }
    finished=true;break;
   }
  }
 }
 check(finished,"scenario reached its final checkpoint");printf("CHECKS %u FAILURES %u\\n",checks,failures);
 retro_unload_game();retro_deinit();return failures?1:0;
}
'''
# Optional cleanup exports are not present in the shared harness declarations.
s=s.replace(' retro_unload_game();retro_deinit();','')
for key,name in {'LW':'g_levelW','AC':'g_arcadeCredits','HC':'g_homeCredits','GG':'g_game','PAUSE':'g_paused','CONFIG':'g_config','MODE':'g_mode','FC':'g_frameCounter','ENT':'g_entities','OWN':'life_owned'}.items():s=re.sub(r'\b'+key+r'\b','0x'+syms[name],s)
(o/'run.c').write_text(s);subprocess.run(['cc','-O2',str(o/'run.c'),'-o',str(o/'run')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest();results=[];(o/'report.json').unlink(missing_ok=True)
for mode in (0,1,2):
 log=o/f'{mode}.log'
 with log.open('w')as f:result=subprocess.run([str(o/'run'),str(core),str(r/'out/rom.bin'),str(mode)],stdout=f,stderr=subprocess.STDOUT)
 text=log.read_text();print(text,flush=True);assert result.returncode==0
 results.append({'mode':('Home zero credits' if mode==2 else 'Home paid')if mode else'Arcade','checks':text.splitlines()})
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
(o/'report.json').write_text(json.dumps({'passed':True,'rom_sha256':rh,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'results':results,'scope':'Real menus/options and controllers only; no RAM injection, no refilled lives. Default Arcade coinage and Home selected5 and0 credits, including natural game-over and rejected unpaid continue. Does not cover all DIP ratios or SRAM restoration.'},indent=2)+'\n')
