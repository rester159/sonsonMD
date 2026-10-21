#!/usr/bin/env python3
"""Export a fresh source tree with no ROMs, intro assets, logs or Git history."""
from pathlib import Path
import argparse,shutil,json,hashlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('destination',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[1];d=a.destination.resolve()
if d.exists():raise SystemExit('Destination must not exist')
d.mkdir(parents=True)
intro={'sega_splash.c','capcom_splash.c','sega_logo.c','sega_logo.h','sega_logo_shinobi.c','sega_logo_shinobi.h','capcom_logo.c','capcom_logo.h','sega_snap.lua'}
files=[]
for folder in ['src','inc','tools']:
 for f in sorted((r/folder).rglob('*')):
  if not f.is_file() or f.name in intro or '__pycache__' in f.parts:continue
  if f.suffix not in {'.c','.h','.s','.py','.lua','.md'}:continue
  dest=d/f.relative_to(r);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,dest);files.append(str(f.relative_to(r)))
for name in ['build.sh','ROADMAP.md']:
 shutil.copyfile(r/name,d/name);files.append(name)
# The cartridge boot assembly is mandatory SGDK startup, not a logo intro.
p=d/'src/main.c';s=p.read_text();start=s.find('/* ====')
if start>=0:
 end=s.index('int main(',start)
 s=s[:start]+'/* Source-only distribution: boot branding is excluded. */\n\n'+s[end:]
s=s.replace('    sega_splash_run();                    /* SEGA opening (Sonic-1 + chant)    */','    /* Private SEGA/CAPCOM intro modules are omitted from this export. */').replace('    capcom_splash_run();                  /* CAPCOM logo + register-exact FM   */','')
p.write_text(s)
readme=(r/'README.md').read_text().replace('The HUD shows elapsed time, the best survival\ntime and the current pressure level.','The HUD shows elapsed time and the best survival time.')
readme=readme.replace('# SonSon Native (Sega Genesis)','# SonSon MD\n\nPorted by rester159, 2026.\n\nThis repository contains the native game source. **No ROMs or SEGA/CAPCOM intro modules, images or audio are included.** The source build starts at the mode selector; the local full build retains both boot intros. Generated native game graphics and sound tables are included so compiling the game does not require an arcade ROM. Regenerating those tables requires your own source data.\n\nThe owner’s local boot sequence includes about nine seconds of intro screens. Controller-driven emulator fixtures currently use those timings; adapt their input frame offsets for this intro-free source build. The host save tests and gameplay implementation are independent of the boot intros.')
(d/'README.md').write_text(readme)
(d/'.gitignore').write_text('''/out/
/arcade/
/nvram/
/cfg/
/snap/
__pycache__/
*.pyc
*.o
*.bin
*.rom
*.gen
*.smd
*.zip
*.7z
*.sav
*.srm
*.nv
# Markdown documentation is source.
!# Private boot branding must never enter this repository.
/src/sega_splash.c
/src/capcom_splash.c
/src/sega_logo*.c
/src/capcom_logo.c
/inc/sega_logo*.h
/inc/capcom_logo.h
''')
(d/'res').mkdir();(d/'res/.gitkeep').touch()
print(json.dumps({'destination':str(d),'source_files':len(files),'local_rom_sha256':hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()},indent=2))
