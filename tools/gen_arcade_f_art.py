#!/usr/bin/env python3
"""Generate F's exact resident aliases; reject any unallocated pose."""
from pathlib import Path
import runpy,json
r=Path(__file__).resolve().parents[1]
runpy.run_path(str(r/'tools/audit_arcade_f_art.py'))
m=json.loads((r/'out/parity-audit/f-art/audit.json').read_text())
assert m['animation_frames']==28 and len(m['poses'])==11 and m['new_cells']==0
(r/'inc/arcade_f_art.h').write_text('''#ifndef ARCADE_F_ART_H
#define ARCADE_F_ART_H
#include "arcade_types.h"
/* Exact resident aliases; no additional VRAM or upload ownership. */
u16 arcade_f_tile(u8 code,u8 attr);
#endif
''')
s='#include "arcade_f_art.h"\nu16 arcade_f_tile(u8 code,u8 attr)\n{\n    switch(((u16)(attr&31)<<9)|code|((u16)(attr&32)<<3))\n    {\n'
for p in m['poses']:s+='        case %d:return %d;\n'%((p['bank']<<9)|p['code'],p['resident_tile'])
s+='    }\n    return 65535;\n}\n'
(r/'src/arcade_f_art.c').write_text(s)
