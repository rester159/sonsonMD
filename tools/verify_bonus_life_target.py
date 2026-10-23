#!/usr/bin/env python3
"""Run extracted production bonus bridge checks on a real 68000 core."""
from pathlib import Path
import shutil,subprocess,re,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/bonus-life-target';o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/verify_bonus_life_binding.py')],check=True)
shutil.copytree(r/'inc',o/'inc',dirs_exist_ok=True);shutil.copytree(r/'src/boot',o/'src/boot',dirs_exist_ok=True)
(o/'tools').mkdir(exist_ok=True);shutil.copy2(r/'tools/build_native.py',o/'tools/build_native.py');shutil.copy2(r/'src/arcade_bonus_life.c',o/'src/arcade_bonus_life.c')
s=(r/'out/parity-audit/bonus-life-binding/test.c').read_text().replace('#include <assert.h>','''#include <genesis.h>
volatile u32 bonus_test_done,bonus_test_errors,bonus_test_checks;
#define assert(value) do{bonus_test_checks++;if(!(value))bonus_test_errors++;}while(0)
''').replace('int main(void){','int main(bool hard){\n (void)hard;VDP_setEnable(FALSE);')
s=s.replace('assert(cases==520);return 0;', 'assert(cases==520);bonus_test_done=0x1234;while(1)SYS_doVBlankProcess();return 0;')
(o/'src/main.c').write_text(s);subprocess.run(['python3',str(o/'tools/build_native.py')],check=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(o/'out/symbol.txt').read_text(),re.M)}
s=(r/'tools/verify_libretro.c').read_text();s=s[:s.index('    unsigned f_addr=')].replace('if(argc!=12)','if(argc!=3)')
s+=f'''
 for(frame=0;frame<1000;frame++){{retro_run();if(r32(ram,0x{syms['bonus_test_done']})==0x1234){{
 unsigned errors=r32(ram,0x{syms['bonus_test_errors']}),checks=r32(ram,0x{syms['bonus_test_checks']});
 printf("BONUS_TARGET checks=%u errors=%u\\n",checks,errors);return errors || checks!=1668;
 }}}}return 3;
}}
'''
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
run=subprocess.run([str(o/'harness'),str(core),str(o/'out/rom.bin')],capture_output=True,text=True);print(run.stdout);assert run.returncode==0,run.stderr
(o/'report.json').write_text(json.dumps({'passed':True,'rom_sha256':hashlib.sha256((o/'out/rom.bin').read_bytes()).hexdigest(),'scope':'68000 execution of extracted production bonus bridge and reserve codec; 512 roundtrips, eight mode/player scenarios and reserve rollover. Isolated -O3/LTO ROM, not normal-play validation.'},indent=2)+'\n')
