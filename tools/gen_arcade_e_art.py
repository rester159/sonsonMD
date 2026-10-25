#!/usr/bin/env python3
"""E ROM cells and exact resident aliases. Dynamic tokens are not VRAM tiles."""
import audit_arcade_e_art as A
r=A.r
(r/'inc/arcade_e_art.h').write_text('''#ifndef ARCADE_E_ART_H
#define ARCADE_E_ART_H
#include "arcade_types.h"
#define ARCADE_E_ART_CELLS 12
#define ARCADE_E_DYNAMIC 0x8000
/* Resident tile, ARCADE_E_DYNAMIC|cell, or65535 for unknown. Flip bits ignored. */
u16 arcade_e_art(u8 code,u8 attr);
extern const u32 arcade_e_pixels[ARCADE_E_ART_CELLS][32];
#endif
''')
s='#include "arcade_e_art.h"\nconst u32 arcade_e_pixels[ARCADE_E_ART_CELLS][32]={\n'
for pixels in A.missing:s+='{'+','.join('0x%08x'%v for v in pixels)+'},\n'
s+='};\nu16 arcade_e_art(u8 code,u8 attr)\n{\n switch(((u16)(attr&31)<<9)|code|((u16)(attr&32)<<3))\n {\n'
for p in A.mapping:
 token=p['resident_tile']if p['resident_tile']is not None else 0x8000|p['new_cell']
 s+='  case %d:return %d;\n'%((p['bank']<<9)|p['code'],token)
s+=' }\n return 65535;\n}\n';(r/'src/arcade_e_art.c').write_text(s)
