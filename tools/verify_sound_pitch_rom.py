#!/usr/bin/env python3
"""Validate the linked big-endian ROM lookup, not just its host representation."""
from pathlib import Path
import hashlib
import json
import re
root=Path(__file__).resolve().parents[1]
out=root/'out/parity-audit/sound-pitch-lookup'
out.mkdir(parents=True,exist_ok=True)
rom=(root/'out/rom.bin').read_bytes()
manifest=json.loads((root/'out/build-manifest.json').read_text())
digest=hashlib.sha256(rom).hexdigest()
assert manifest['rom_sha256']==digest
for name in ['src/sound.c','src/sound_ym_pitch.c','inc/sound_ym_pitch.h']:
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==manifest['source_sha256'][name], name
symbols=(root/'out/symbol.txt').read_text()
assert hashlib.sha256(symbols.encode()).hexdigest()==manifest['symbols_sha256']
match=re.search(r'^([0-9a-fA-F]+)\s+\w\s+sound_ym_pitch$',symbols,re.M)
assert match
address=int(match.group(1),16)
assert address%2==0 and address+8192<=len(rom)<=0x100000
for period in range(4096):
    expected=0
    if period:
        total=1845281//period
        block=0
        while total>=2048 and block<7:
            total>>=1
            block+=1
        expected=(block<<11)|min(total,2047)
    actual=int.from_bytes(rom[address+period*2:address+period*2+2],'big')
    assert actual==expected, (period,actual,expected)
report={'rom_sha256':digest,'table_address':address,'entries':4096,'scope':'Linked big-endian ROM values, even alignment and flat-cartridge bounds; source/symbol identities checked. Does not execute YM bus writes or validate audible output.'}
(out/'rom-results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
