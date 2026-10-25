#!/usr/bin/env python3
"""Actual68000 session RNG retention across entity new-game initialization."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
from genplus_probe import probe
p=argparse.ArgumentParser();p.add_argument('--baseline',action='store_true');args=p.parse_args()
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/rng-handoff'/('before' if args.baseline else 'after');o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk')
entity=r/('out/parity-audit/prelude-motion/build/src/entity.c' if args.baseline else 'src/entity.c')
(o/'main.c').write_text('#include '+json.dumps(str(entity))+'''
volatile u16 test_done,test_error,test_batches,test_words,test_split;
int main(bool hard){(void)hard;VDP_setEnable(FALSE);g_survivalRun=0;g_game.numPlayers=2;
 for(u16 i=0;i<256;i++){
  u16 seed=(u16)(0x87cd+i*257);u8 last=(u8)(255-i);
  arcade_rng.seed=seed;arcade_rng.last=last;
  entity_init();
  if(arcade_rng.seed!=seed || arcade_rng.last!=last){test_error=i+1;break;}
  test_words++;
 }
 test_batches=1;test_split=1;test_done=1;while(1)SYS_doVBlankProcess();return 0;
}
''')
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(r/'src'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
subprocess.run(['m68k-elf-gcc',*flags,'-c',str(o/'main.c'),'-o',str(o/'main.o')],check=True)
objects=[p for p in (r/'out/native').glob('*.o') if p.stem not in ('main','entity')]
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),str(o/'main.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:int(v,16)for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
d=probe(o/'rom.bin',syms,o)
d.update(passed=d['done']==1 and d['error']==0 and d['words']==256,entity_sha256=hashlib.sha256(entity.read_bytes()).hexdigest(),scope='Isolated68000 entity_init retention for256seed/sample patterns; not original full RNG call-order or title lifecycle comparison.')
(o/'report.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))
assert d['done']==1
assert (d['error']==1 and d['words']==0) if args.baseline else d['passed']
