#!/usr/bin/env python3
"""Build with native m68k-elf tools; GDK selects an SGDK 2.11 installation."""
import os, pathlib, subprocess, hashlib, json, argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--clean",action="store_true",help="recompile every native source file")
group=parser.add_mutually_exclusive_group()
group.add_argument("--lto",dest="lto",action="store_true",help="Optimize native C across modules (default)")
group.add_argument("--no-lto",dest="lto",action="store_false",help="Build the comparison baseline without cross-module optimization")
parser.set_defaults(lto=True)
parser.add_argument("-D",dest="defines",action="append",default=[],help="Extra preprocessor define (diagnostic builds)")
parser.add_argument("--out",default="out",help="Output directory")
parser.add_argument("--cflag",action="append",default=[],help="Extra compiler flag, also applied at the LTO link (experiments)")
parser.add_argument("--opt",choices=("2","3","s"),default="3",help="Native optimization level")
args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
os.chdir(root)
# SGDK 2.11 is required by this project's PCM splash APIs. Prefer an
# explicit SONSON_GDK, then a compatible GDK, then the existing portForge SDK.
candidates=[os.environ.get('SONSON_GDK'),os.environ.get('GDK'),str(pathlib.Path.home()/'My_apps/portForge/tools/sgdk')]
sdk=None
for candidate in filter(None,candidates):
    path=pathlib.Path(candidate)
    header=path/'inc/snd/pcm/snd_pcm.h'
    if header.exists() and 'SND_PCM_unloadDriver' in header.read_text():
        sdk=path;break
if sdk is None:
    raise SystemExit('Set SONSON_GDK to SGDK 2.11 (including its lib/libmd.a).')
out=root/args.out; out.mkdir(parents=True,exist_ok=True)
compiler=subprocess.check_output(['m68k-elf-gcc','--version'],text=True).splitlines()[0]
headers=b''.join(p.read_bytes() for d in [root/'inc',sdk/'inc'] for p in sorted(d.rglob('*.h')))
headers+= (sdk/'lib/libmd.a').read_bytes()
header_hash=hashlib.sha256(headers).hexdigest()
def run(args): subprocess.run([str(x) for x in args],check=True)
# SGDK CPU copies read u16 buffers through u32 pointers. With GCC16/LTO,
# strict aliasing removes stack palette stores; keep type-punning defined
# for optimization purposes in both compilation and the final LTO link.
flags=['-m68000','-DSGDK_GCC','-O'+args.opt,'-fno-builtin','-fno-strict-aliasing','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions',*(['-g'] if 'PROF_HINT' in args.defines else []),'-Iinc','-Isrc','-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]+['-D'+d for d in args.defines]+args.cflag
run(['m68k-elf-gcc',*flags,'-c','src/boot/rom_head.c','-o',out/'rom_head.o'])
run(['m68k-elf-objcopy','-O','binary',out/'rom_head.o',out/'rom_head.bin'])
run(['m68k-elf-gcc',*flags,'-x','assembler-with-cpp','-Wa,--register-prefix-optional,--bitwise-or','-c','src/boot/sega.s','-o',out/'sega.o'])
if args.lto:flags.append('-flto')
objects=[]
for source in sorted((root/'src').glob('*.c')):
    obj=out/'native'/ (source.stem+'.o'); obj.parent.mkdir(exist_ok=True)
    stamp=obj.with_suffix('.sha256')
    identity=hashlib.sha256(source.read_bytes()+header_hash.encode()+compiler.encode()+repr(flags).encode()).hexdigest()
    if args.clean or not obj.exists() or not stamp.exists() or stamp.read_text()!=identity:
        run(['m68k-elf-gcc',*flags,'-c',source,'-o',obj]);stamp.write_text(identity)
    objects.append(obj)
run(['m68k-elf-gcc',*(['-flto','-fno-strict-aliasing','-O'+args.opt,*args.cflag] if args.lto else []),'-m68000','-n','-T',sdk/'md.ld','-nostdlib',out/'sega.o',*objects,sdk/'lib/libmd.a','-lgcc','-Wl,--gc-sections','-o',out/'rom.out'])
run(['m68k-elf-objcopy','-O','binary',out/'rom.out',out/'rom.bin'])
(out/'symbol.txt').write_bytes(subprocess.check_output(['m68k-elf-nm','-n',out/'rom.out']))
b=bytearray((out/'rom.bin').read_bytes());
assert len(b)<=0x100000, 'Flat 1 MiB cartridge capacity exceeded; review ROM hardware profile'
b.extend(bytes((-len(b))%131072)); b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big') for i in range(0x200,len(b),2))&65535).to_bytes(2,'big'); (out/'rom.bin').write_bytes(b)
(out/'build-manifest.json').write_text(json.dumps({'sdk':str(sdk),'compiler':compiler,'sdk_library_sha256':hashlib.sha256((sdk/'lib/libmd.a').read_bytes()).hexdigest(),'symbols_sha256':hashlib.sha256((out/'symbol.txt').read_bytes()).hexdigest(),'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for d in ['src','inc'] for p in sorted((root/d).rglob('*')) if p.is_file()},'flags':flags,'rom_sha256':hashlib.sha256(b).hexdigest()},indent=2)+'\n')
print('Built',out/'rom.bin',len(b),'bytes')
