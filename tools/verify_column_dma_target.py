#!/usr/bin/env python3
"""Existing food VRAM assertions plus paired-column wrap checks, windowless."""
from pathlib import Path
import re,subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/column-dma/target';o.mkdir(parents=True,exist_ok=True)
candidate=(r/'out/parity-audit/column-dma/src/arcade_food_world.c').read_text()
candidate+='''\nvoid diagnostic_column(u16 row,u16 col,u8 cold){static const u16 pair[2]={123,456};food_column_dma(pair,row,col,cold);}\n''';(o/'world.c').write_text(candidate)
# Reuse the saved test's build/VRAM assertions only; stop before its MAME runner.
s=(r/'tools/verify_food_world_target.py').read_text().split('# Pixel bank may be discarded:')[0]
s=s.replace("r/'out/parity-audit/food-world-target'",'Path('+repr(str(o))+')').replace("r/'src/arcade_food_world.c'",'Path('+repr(str(o/'world.c'))+')')
needle='test_split=1;test_batches=3;test_done=1;'
extra='''for(u16 row=0;row<32;row+=31)for(u16 col=0;col<64;col+=63)for(u16 cold=0;cold<2;cold++){
 diagnostic_column(row,col,cold);SYS_doVBlankProcess();
 if(readtile(row,col)!=456 || readtile((row+31)&31,col)!=123)test_error=30;
}
'''
# This is the literal source within the archived Python writer.
s=s.replace('int main(bool hard)', 'void diagnostic_column(u16 row,u16 col,u8 cold);\\nint main(bool hard)').replace(needle,extra.replace('\n','\\n')+needle)
exec(compile(s,'food-vram-build','exec'),{'__file__':str(r/'tools/verify_food_world_target.py')})
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
h=(r/'tools/verify_libretro.c').read_text();h=h[:h.index('    unsigned f_addr=')].replace('if(argc!=12)', 'if(argc!=3)')
h+=f'''
 for(frame=0;frame<1000;frame++){{retro_run();if(r16(ram,0x{syms['test_done']})==1){{
 unsigned error=r16(ram,0x{syms['test_error']}),words=r16(ram,0x{syms['test_words']});
 printf("VRAM_TARGET words=%u error=%u\\n",words,error);return error || words!=12;
 }}}}return 3;
}}
''';(o/'harness.c').write_text(h);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
a=subprocess.run([str(o/'harness'),str(core),str(o/'rom.bin')],capture_output=True,text=True,check=True);print(a.stdout)
(o/'report.json').write_text(json.dumps({'passed':True,'rom_sha256':hashlib.sha256((o/'rom.bin').read_bytes()).hexdigest(),'scope':'Real target VRAM readback: initial food, collection, queued eaten maps, horizontal seam, row0/31 and column0/63 paired transfers, immediate and queued. No frontend, no complete-scene pixel claim.'},indent=2))
