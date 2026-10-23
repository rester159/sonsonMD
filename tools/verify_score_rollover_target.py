#!/usr/bin/env python3
"""Execute the production inline score helper on 68000, without a frontend."""
from pathlib import Path
import shutil,subprocess,re,json,hashlib,random
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/score-rollover-target';o.mkdir(exist_ok=True)
shutil.copytree(r/'inc',o/'inc',dirs_exist_ok=True);shutil.copytree(r/'src/boot',o/'src/boot',dirs_exist_ok=True)
(o/'tools').mkdir(exist_ok=True);shutil.copy2(r/'tools/build_native.py',o/'tools/build_native.py')
values=[0,1,30,20000,99979999,99999990,99999999,100000000,0xffffffff];cases=[]
for a in values:
 for b in values:
  for demo in [0,1,255]:cases.append((a,b,demo,a if demo else (a+b)%100000000))
s='''#include <genesis.h>
#include "game.h"
volatile u8 g_attract;
volatile u32 score_test_done,score_test_errors,score_test_cases;
static const volatile u32 cases[][4]={
'''+''.join('{'+','.join(str(x)+'UL' for x in row)+'},\n' for row in cases)+'''};
int main(bool hard){
 static Player p;u16 k;(void)hard;VDP_setEnable(FALSE);
 for(k=0;k<sizeof(cases)/sizeof(cases[0]);k++){
  p.score=cases[k][0];g_attract=(u8)cases[k][2];game_score_add(&p,cases[k][1]);
  if(p.score!=cases[k][3])score_test_errors++;
  score_test_cases++;
 }
 score_test_done=0x1234;while(1)SYS_doVBlankProcess();return 0;
}
''';(o/'src/main.c').write_text(s)
subprocess.run(['python3',str(o/'tools/build_native.py')],check=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(o/'out/symbol.txt').read_text(),re.M)}
s=(r/'tools/verify_libretro.c').read_text();s=s[:s.index('    unsigned f_addr=')];s=s.replace('if(argc!=12)', 'if(argc!=3)')
s+=f'''
 for(frame=0;frame<1000;frame++){{retro_run();
  if(r32(ram,0x{syms['score_test_done']})==0x1234){{
   unsigned errors=r32(ram,0x{syms['score_test_errors']}),count=r32(ram,0x{syms['score_test_cases']});
   printf("SCORE_TARGET cases=%u errors=%u\\n",count,errors);return errors || count!={len(cases)};
  }}
 }}return 3;
}}
''';(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
run=subprocess.run([str(o/'harness'),str(core),str(o/'out/rom.bin')],check=True,capture_output=True,text=True);print(run.stdout)
report={'cases':len(cases),'errors':0,'rom_sha256':hashlib.sha256((o/'out/rom.bin').read_bytes()).hexdigest(),'scope':'Real 68000 execution of production score helper, -O3/LTO; volatile boundary operands prevent folding. Includes live/attract, eight-digit rollover, external u32 normalization. Not complete score-event timing proof.'}
(o/'report.json').write_text(json.dumps(report,indent=2))
