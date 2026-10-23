#!/usr/bin/env python3
"""Actual 68000 stopped-player movement and animation versus MC6809 observations."""
from pathlib import Path
import hashlib,json,re,subprocess,sys
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/player-stopped-target';o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin')
import csv
rows=list(csv.DictReader((r/'out/parity-audit/player-stopped.csv').open()));assert len(rows)==1281
fields=['x','xf','y','yf','state','phase','timer','stamp','face','held','active','attached']
def actor(row,k):return '{'+','.join(row[f'p{k}_{f}'] for f in fields)+'}'
def anim(row,k):
 v=lambda f:int(row[f'p{k}_{f}'])
 root=(0xafac if k==1 else 0xafd0)+(14 if v('sprite_attr')&64==0 else 0)
 return '{'+','.join(map(str,[k-1,(v('anim_cursor')-root)//3,v('anim_timer'),v('sprite_code'),v('sprite_attr')]))+'}'
source='#include <genesis.h>\n#include "arcade_player.h"\nvolatile u16 test_done,test_error,test_batches,test_words,test_split;\n'
source+='typedef struct {u16 camera;u8 ring,bits[128];ArcadePlayer p[2];ArcadePlayerAnimation a[2];} Fixture;\nstatic const Fixture fixtures[]={\n'
for row in rows:
 source+='{'+row['scroll']+','+row['ring']+',{'+','.join(map(str,bytes.fromhex(row['collision'])))+'},{'+','.join(actor(row,k) for k in (1,2))+'},{'+','.join(anim(row,k) for k in (1,2))+'}},\n'
source+='};\n'
source+='''static s16 floor_probe(u16 x,u8 y,void *ctx){
 const Fixture *f=ctx;u16 col=((((u16)(x+8-f->camera))>>3)+f->ring)&31;
 u8 row=y>>3,low=y&15;
 if(!(f->bits[col*4+(row>>3)]&(128>>(row&7))) || low<2)return ARCADE_NO_FLOOR;
 return 7-low;
}
int main(bool hard){u16 n,k;(void)hard;VDP_setEnable(FALSE);
for(n=0;n<1280;n++){
 const Fixture *f=&fixtures[n],*e=&fixtures[n+1];ArcadePlayer p[2];ArcadePlayerAnimation a[2];
 for(k=0;k<2;k++){p[k]=f->p[k];a[k]=f->a[k];}
 for(k=0;k<2;k++)arcade_player_step_stopped(&p[k],&p[k^1],e->p[k].previous,f->camera,1,floor_probe,(void*)f,0,&a[k]);
 arcade_players_separate(&p[0],&p[1]);
 for(k=0;k<2;k++){
 if(p[k].x!=e->p[k].x || p[k].xf!=e->p[k].xf || p[k].y!=e->p[k].y || p[k].yf!=e->p[k].yf || p[k].state!=e->p[k].state || p[k].phase!=e->p[k].phase || p[k].facing!=e->p[k].facing || p[k].attached!=e->p[k].attached || a[k].cursor!=e->a[k].cursor || a[k].timer!=e->a[k].timer || a[k].code!=e->a[k].code || a[k].attr!=e->a[k].attr){test_error=n+1;break;}
 }if(test_error)break;test_words++;
}
test_batches=3;test_split=1;test_done=1;while(1)SYS_doVBlankProcess();return 0;}
'''
(o/'main.c').write_text(source)
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for name,path in [('main',o/'main.c'),('player',r/'src/arcade_player.c')]:
 obj=o/(name+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
# Execute the same diagnostic ROM without MAME or a frontend window.
from genplus_probe import probe
d=probe(o/'rom.bin',syms,o)
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n')
assert sha(r/'out/rom.bin')==original
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=sha(o/'rom.bin'),native_sha256=sha(r/'src/arcade_player.c'),scope='Actual68000 stopped two-player motion and animation contract versus1280 original CPU observations; live owner remains pending.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['done']==1 and d['error']==0 and d['batches']==3 and d['words']==1280
