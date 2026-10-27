#!/usr/bin/env python3
"""Build and run the 68000 terrain/motion oracle fixture in Genesis Plus GX."""
from pathlib import Path
import hashlib,json,os,re,shutil,subprocess
ROOT=Path(__file__).resolve().parents[3]
out=ROOT/'out/ss2/perf-native-check'
for directory in ('src/boot','tools','out'): (out/directory).mkdir(parents=True,exist_ok=True)
shutil.copytree(ROOT/'inc',out/'inc',dirs_exist_ok=True)
shutil.copytree(ROOT/'src/boot',out/'src/boot',dirs_exist_ok=True)
for name in ('ss2_logic.c','ss2_objects.c','ss2_rom.c','ss2_probe.c'):
    shutil.copy2(ROOT/'src'/name,out/'src'/name)
shutil.copy2(Path(__file__).with_name('probe_check_main.c'),out/'src/main.c')
for name in ('build_native.py','check_autoinc_hazard.py'):
    shutil.copy2(ROOT/'tools'/name,out/'tools'/name)
subprocess.run(['python3',str(out/'tools/build_native.py'),'-D','SS2_PROBECHECK'],check=True)
exe=out/'capture'
subprocess.run(['cc','-O2',str(ROOT/'tools/ss2/gpgx_shot.c'),'-o',str(exe)],check=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
env=dict(os.environ,SS2_RAMDUMP=str(out/'ram.bin'))
subprocess.run([str(exe),str(core),str(out/'out/rom.bin'),'6000',str(out),'',''],env=env,check=True)
symbols={n:int(a,16)&65535 for a,n in re.findall(r'^([0-9a-f]+)\s+\w\s+(\S+)$',(out/'out/symbol.txt').read_text(),re.M)}
ram=(out/'ram.bin').read_bytes()
expected={'ss2_probe_checks':(4,65536),'ss2_probe_failures':(4,0),'probe_done':(2,0x600d),
          'probe_guard_failures':(2,0),'probe_abi_failures':(2,0),'motion_failures':(4,0)}
results={}
for name,(size,want) in expected.items():
    address=next(a for n,a in symbols.items() if n==name or n.startswith(name+'.'))
    value=int.from_bytes(bytes(ram[((address+i)&65535)^1] for i in range(size)),'big')
    results[name]=value
    assert value==want,(name,value,want)
results['rom_sha256']=hashlib.sha256((out/'out/rom.bin').read_bytes()).hexdigest()
results['core_sha256']=hashlib.sha256(core.read_bytes()).hexdigest()
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
