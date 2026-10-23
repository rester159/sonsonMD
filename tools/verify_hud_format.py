#!/usr/bin/env python3
"""Source eight-digit formatting plus actual 2P HUD tilemap in windowless GPGX."""
from pathlib import Path
import subprocess,hashlib,re,json,os
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/hud-format';o.mkdir(parents=True,exist_ok=True)
rom=(r/'arcade/ss.01e').read_bytes()
assert rom[0xebe:0xecc].hex()=='3414c608d7f48610a7800af426fa'
assert rom[0xedc:0xeea].hex()=='c61086076d842605e7804a26f739'
for offset,template in ((0x86f,b'\x10\x2e\x28HIGH?'),(0x877,b'\x10\x6b\x28TOTAL@SCORE?'),(0x886,b'\x10\x21\x0cSONSON?'),(0x890,b'\x10\x39\x0cTONTON?'),(0x89a,b'\x10\x22\x0cSONSON?')):
 assert rom[offset:offset+len(template)]==template
assert rom[0xd16:0xd19]==bytes.fromhex('8e104d')
assert rom[0xd1c:0xd1f]==bytes.fromhex('8e1042')
assert rom[0xd3e:0xd41]==bytes.fromhex('8e1057')
assert rom[0xd4c:0xd4f]==bytes.fromhex('8e108d')
assert rom[0xd5a:0xd84].hex()=='34408e0070ce0074108e0078a682abc219a7a2a682a9c219a7a2a682a9c219a7a2a682a9c219a7a235c0'
s=(r/'src/game.c').read_text();a=s.index('static void hud_score_string(');b=s.index('/* Append players',a)
c='''#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
static void uintToStr(u16 n,char*s,u16 w){sprintf(s,"%0*u",w,n);}
static u32 divmodu(u32 n,u16 d){assert(n/d<=65535);return ((n%d)<<16)|(n/d);}
'''+s[a:b]+'''
int main(void){u32 seed=901;for(unsigned i=0;i<200000;i++){
 seed=seed*1664525u+1013904223u;u32 n=i<10001?i:seed;
 char actual[16],expected[16];memset(actual,0xa5,sizeof(actual));
 hud_score_string(n,actual);sprintf(expected,"%8u",n%100000000u);
 assert(!strcmp(actual,expected));for(unsigned j=9;j<16;j++)assert((unsigned char)actual[j]==0xa5);
}return 0;}
''';(o/'host.c').write_text(c)
subprocess.run(['cc','-O2',str(o/'host.c'),'-o',str(o/'host')],check=True);subprocess.run([str(o/'host')],check=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'tools/verify_libretro.c').read_text();needle='        retro_run();'
s=s.replace(needle,'''        if(started && sample==900){
          unsigned values[2]={12345678,98765432};
          for(unsigned k=0;k<2;k++)for(unsigned j=0;j<4;j++)w8(ram,gg+k*42+28+j,values[k]>>(24-j*8));
        }
'''+needle)
s=s.replace('    fclose(f);printf(','''    {const uint16_t*c=dlsym(core,"cram");if(!c)return 3;printf("SCORE_INK %u\\n",c[47]);}
    printf("FINAL_SCORES %u %u FONT %u\\n",r32(ram,gg+28),r32(ram,gg+70),r16(ram,MAPS)/32-96);
    fclose(f);printf('''.replace('MAPS','0x'+syms['maps_addr']))
(o/'target.c').write_text(s);subprocess.run(['cc','-O2',str(o/'target.c'),'-o',str(o/'target')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';e=os.environ.copy();e['SONSON_VRAM_DUMP']=str(o/'vram.bin')
cmd=[str(o/'target'),str(core),str(r/'out/rom.bin'),'2','1000',*[syms[n]for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(o/'trace.csv')]
p=subprocess.run(cmd,env=e,capture_output=True,text=True,check=True);(o/'target.log').write_text(p.stdout)
p1,p2,font=map(int,re.search(r'FINAL_SCORES (\d+) (\d+) FONT (\d+)',p.stdout).groups());assert p1>=12345678 and p2>=98765432
v=(o/'vram.bin').read_bytes();regs=v[65536:];base=(regs[4]&7)*8192
word=lambda a:v[a^1]*256+v[(a+1)^1]
assert int(re.search(r'SCORE_INK (\d+)',p.stdout).group(1))==0x16d
source_gfx=G.Roms(str(r/'arcade'))
for i,ch in enumerate('SONTHIGTOALSCRE'):
 expected=G.bake_char_tile(source_gfx,ord(ch)-ord('A')+17,3 if i<4 else 10)
 addr=(font+ord('a')-32+i)*32
 actual=[(word(addr+y*4)<<16)|word(addr+y*4+2) for y in range(8)]
 assert tuple(actual)==tuple(expected),(ch,actual,expected)
# Original reserve icons: color attributes include the character-bank bits.
assert rom[0x1250:0x1260].hex()=='99999595f4f5e4e59f9f9f9f7c7d7a7b'
for owner in range(2):
 pattern=rom[0x1250+owner*8:0x1258+owner*8]
 for slot,i in enumerate((2,3,0,1)):
  attr=pattern[i];code=pattern[i+4]+((attr&3)<<8)
  expected=G.bake_char_tile(source_gfx,code,attr>>2)
  addr=(font+ord('p')-32+owner*4+slot)*32
  actual=[(word(addr+y*4)<<16)|word(addr+y*4+2) for y in range(8)]
  assert tuple(actual)==tuple(expected),('reserve pixels',owner,slot)
# This workload refills lives to99, so both players show the source cap of4.
for col,top,bottom in ((2,'pq'*4,'rs'*4),(23,'tu'*4,'vw'*4)):
 for row,label in ((2,top),(3,bottom)):
  actual=[word(base+128*row+(col+i)*2)for i in range(8)]
  expected=[0x8000|font+ord(c)-32 for c in label]
  assert actual==expected,('reserve map',col,row,actual,expected)
checks=[]
for row,col,score in ((1,2,p1),(1,13,max(p1,p2)),(1,23,p2),(3,13,(p1+p2)%100000000)):
 expected=[0xc000|font+ord(ch)-32 for ch in f'{score:8d}'];actual=[word(base+128*row+(col+i)*2) for i in range(8)]
 assert actual==expected,(col,actual,expected)
 checks.append({'row':row,'column':col,'score':score,'words':actual})
for row,col,label in ((0,2,'abcabc'),(0,14,'efge'),(0,25,'dbcdbc'),(2,11,'hihjk lmino')):
 expected=[0x8000|font+ord(ch)-32 for ch in label];actual=[word(base+128*row+(col+i)*2) for i in range(len(label))]
 assert actual==expected,(col,actual,expected)
report={'passed':True,'host_cases':200000,'source':'4EBE clears8 cells;4EDC blanks up to7 leading zeroes;4D16/4D1C/4D3E/4D4C fields104D/1042/1057/108D;4D5A packed-BCD total discards carry','rom_sha256':hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest(),'score_fields':checks,'label_words':'all27 match source-colored lowercase-slot glyph references','label_pixels':'all960 pixels match source glyphs in banks3 and10','reserve_icons':'512 source pixels and32 map words match; live99 fixture caps at4 icons per player','score_ink':'PAL2 index15 matches source white0xAAA','scope':'Production formatter host cases and actual compiled2P HUD after score fixtures. No MAME or frontend. Does not prove full gameplay HUD/lives/credit parity.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
