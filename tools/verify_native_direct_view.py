#!/usr/bin/env python3
"""Compare cached and direct native sprite views in an isolated baseline ROM."""
from pathlib import Path
import shutil,subprocess,re,os,json
r=Path(__file__).resolve().parents[1]; o=r/'out/parity-audit/native-direct-view/view-check';o.mkdir(exist_ok=True)
for name in ('src','inc'):shutil.copytree(r/name,o/name,dirs_exist_ok=True)
(o/'tools').mkdir(exist_ok=True);shutil.copy2(r/'tools/build_native.py',o/'tools/build_native.py')
b=(r/'out/parity-audit/native-direct-view/baseline.c').read_text(); c=(r/'out/parity-audit/native-direct-view/candidate.c').read_text()
start=c.index('        /* Native actors own position and pose.')
end=c.index('\n        if (e->type == ET_POPUP)',start)
view=c[start:end]
b=b.replace('u16 entity_render_all(', 'volatile u32 native_view_checks,native_view_mismatches;\nu16 entity_render_all(')
assert 'volatile u32 native_view_checks' in b
b=b.replace('s16 sx, sy;', 's16 sx, sy;u16 body_tile;u8 body_facing;')
old='        sx = (s16)(e->x - g_game.cameraX);\n        sy = e->y;'
assert b.count(old)==1
b=b.replace(old,view+'''\n        if(!survival && (i<12 || (i>=20 && i<24))){
            native_view_checks++;
            if(sx!=(s16)(e->x-g_game.cameraX) || sy!=e->y || body_tile!=e->tile || body_facing!=e->facing)native_view_mismatches++;
        }
''')
(o/'src/entity.c').write_text(b)
subprocess.run(['python3',str(o/'tools/build_native.py')],check=True,stdout=subprocess.DEVNULL)
syms=dict((n,v) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(o/'out/symbol.txt').read_text(),re.M))
s=(r/'tools/verify_libretro.c').read_text()
s=s.replace('        retro_run();',f'''        retro_run();
        if(r32(ram,0x{syms['native_view_mismatches']})){{fprintf(stderr,"Native view mismatch at refresh %u, checks %u\\n",frame,r32(ram,0x{syms['native_view_checks']}));return 9;}}
''')
s=s.replace('    fclose(f);printf(', f'    fprintf(stderr,"Native view checks: %u\\n",r32(ram,0x{syms["native_view_checks"]}));\n    fclose(f);printf(')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
env=os.environ.copy();env.update(SONSON_SIM_INPUT='1',SONSON_FIXED_TICKS='1')
for players in (1,2):
 cmd=[str(o/'harness'),str(core),str(o/'out/rom.bin'),str(players),'18000',*[syms[n] for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(o/f'{players}p.csv')]
 with (o/f'{players}p.log').open('w') as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
print('Native view equivalence passed both workloads')
