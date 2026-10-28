#!/usr/bin/env python3
"""Exercise the actual cell decoder on 68000 against an independent pixel oracle."""
from pathlib import Path
import os,re,shutil,subprocess,json
root=Path.cwd(); out=root/'out/ss2/cell-check'
for d in ['src/boot','tools','out']: (out/d).mkdir(parents=True,exist_ok=True)
shutil.copytree(root/'inc',out/'inc',dirs_exist_ok=True)
shutil.copytree(root/'src/boot',out/'src/boot',dirs_exist_ok=True)
for f in ['build_native.py','check_autoinc_hazard.py']:shutil.copy2(root/'tools'/f,out/'tools'/f)
s=(root/'src/ss2_main.c').read_text()
decoder=s[s.index('static const u8 *conv_map;'):s.index('/* a scene',s.index('static const u8 *conv_map;'))]
pre='''#include <genesis.h>
const u8 *ss2_banks[32];
static u8 bank[8192], maps[16][8];
static u32 actual[34], expected[32];
volatile u32 cell_checks, cell_failures;
volatile u16 cell_done;
'''
post='''
int main(bool reset) {
 u16 n,i,r,x; u32 seed=17; (void)reset;
 for(i=0;i<32;i++)ss2_banks[i]=bank;
 for(i=0;i<128;i++)maps[i/8][i%8]=(u8)(i*13+7);
 for(n=0;n<1024;n++) {
  u16 src=(n<1000)?(u16)((n*71u)&0x1f9f):(u16)(0x1f90+(n-1000)*5);
  const u8 *cm=maps[(n/8)&15];
  for(i=0;i<96;i++){seed=seed*1664525UL+1013904223UL;bank[(src+i)&8191]=(u8)(seed>>24);}
  memset(expected,0,sizeof expected);
  if((src&8191)<=8192-96)for(r=0;r<16;r++)for(x=0;x<16;x++) {
   u16 bit=15-x, byte=(u16)(r*2+bit/8);u8 c=0,plane;
   for(plane=0;plane<3;plane++) c|=((bank[(src&8191)+byte+32*plane]>>(bit%8))&1)<<plane;
   expected[(x/8)*16+r]=(expected[(x/8)*16+r]<<4)|(c?(cm[c]&15):0);
  }
  actual[0]=actual[33]=0x12345678;
  conv_cell((u8)n,src,cm,actual+1);
  for(i=0;i<32;i++)if(actual[i+1]!=expected[i])cell_failures++;
  if(actual[0]!=0x12345678||actual[33]!=0x12345678)cell_failures++;
  cell_checks++;
  SYS_doVBlankProcess();
 }
 cell_done=0x600d;
 while(TRUE)SYS_doVBlankProcess();
}
'''
(out/'src/main.c').write_text(pre+decoder+post)
(out/'src/ss2_rom.c').write_text('/* This oracle uses generated pixels, no ROM data. */\n')
subprocess.run(['python3',str(out/'tools/build_native.py'),'--out','out'],check=True)
env=dict(os.environ,SS2_RAMDUMP=str(out/'ram.bin'))
subprocess.run([str(root/'out/ss2/gpgx_shot'),str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'),str(out/'out/rom.bin'),'6000',str(out),'',''],env=env,check=True,timeout=120)
sym={n:int(a,16)&65535 for a,n in re.findall(r'^([0-9a-f]+)\s+\w\s+(\S+)$',(out/'out/symbol.txt').read_text(),re.M)}
ram=(out/'ram.bin').read_bytes();result={}
for name,size,want in [('cell_checks',4,1024),('cell_failures',4,0),('cell_done',2,0x600d)]:
 a=sym[name];v=int.from_bytes(bytes(ram[(a+i)^1] for i in range(size)),'big');result[name]=v;assert v==want,result
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
