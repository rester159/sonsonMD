#!/usr/bin/env python3
"""Extract F0C9's sixteen twelve-pair placement sequences."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
ptrs=[int.from_bytes(rom[a-0x4000:a-0x4000+2],'big')for a in range(0xf0c9,0xf0e9,2)]
assert ptrs==list(range(0xf0e9,0xf269,24))
lines=['/* Generated from ss.03e F0C9. */','#include "arcade_main_wave.h"','const u8 arcade_main_placements[16][12][2]={']
for a in ptrs:
 b=rom[a-0x4000:a-0x4000+24];assert len(b)==24
 lines.append('    {'+','.join('{%d,%d}'%(b[i],b[i+1])for i in range(0,24,2))+'},')
lines.append('};');(r/'src/arcade_main_wave_data.c').write_text('\n'.join(lines)+'\n')
o=r/'out/parity-audit/main-wave';o.mkdir(parents=True,exist_ok=True)
(o/'manifest.json').write_text(json.dumps({'schema':1,'program_sha256':hashlib.sha256(rom).hexdigest(),'pointer_table':0xf0c9,'sequences':ptrs,'pairs_per_sequence':12,'units':'source upward Y byte, actor subtype byte','scope':'Lossless table extraction; runtime binding pending'},indent=2)+'\n')
print('Extracted 16 sequences / 192 placement pairs')
