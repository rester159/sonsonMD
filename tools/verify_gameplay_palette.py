#!/usr/bin/env python3
"""Read the actual gameplay CRAM after boot in the windowless pinned core."""
from pathlib import Path
import argparse,subprocess,re,os,json,hashlib,sys
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build-root',type=Path);a=p.parse_args()
r=a.build_root.resolve() if a.build_root else Path(__file__).resolve().parents[1]
o=r/'out/gameplay-palette';o.mkdir(exist_ok=True)
s=(r/'tools/verify_libretro.c').read_text();needle='    fclose(f);printf('
assert s.count(needle)==1
s=s.replace(needle,'''    {const void*c=dlsym(core,"cram");if(!c)return 3;
     FILE*d=fopen(getenv("SONSON_CRAM_DUMP"),"wb");if(!d)return 3;
     if(fwrite(c,1,128,d)!=128)return 3;fclose(d);}
'''+needle)
(o/'check.c').write_text(s);subprocess.run(['cc','-O2',str(o/'check.c'),'-o',str(o/'check')],check=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
e=os.environ.copy();e['SONSON_CRAM_DUMP']=str(o/'cram.bin')
cmd=[str(o/'check'),str(core),str(r/'out/rom.bin'),'2','1000',*[syms[n]for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(o/'trace.csv')]
with (o/'run.log').open('w') as log:subprocess.run(cmd,env=e,check=True,stdout=log,stderr=subprocess.STDOUT)
b=(o/'cram.bin').read_bytes();actual=[int.from_bytes(b[i:i+2],sys.byteorder) for i in range(96,128,2)]
# Genesis Plus GX stores each CRAM word in packed nine-bit RGB format.
expected=[((i*14//15)>>1)*0x49 for i in range(16)]
report={'rom_sha256':hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest(),'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'actual_packed_cram':actual,'expected_packed_cram':expected,'pass':actual==expected,'scope':'Actual PAL3 after 1000 active gameplay refreshes; controller boot, refilled lives. Verifies palette initialization survives optimized SGDK CPU copy.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n')
assert report['pass'],report
print('Gameplay palette: all16 colors match')
