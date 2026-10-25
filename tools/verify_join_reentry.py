#!/usr/bin/env python3
"""Controller-only join/continue into source reentry; windowless GPGX."""
from pathlib import Path
import hashlib,json,re,subprocess,os
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/join-reentry';o.mkdir(parents=True,exist_ok=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'tools/verify_libretro.c').read_text();a=s.index('        if(!started)w8(ram,np,players);');b=s.index('        retro_run();',a);s=s[:a]+s[b:]
s=s.replace('r8(ram,gg+84)==players','r8(ram,gg+84)>=1')
a=s.index('static int16_t input(');b=s.index('#define FN',a)
input_c='''static unsigned first_owner,join_shift,continued_at,admissions,observed_gg;
static uint8_t r8(uint8_t*r,unsigned a);
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;
 if(!started){
  if(frame<740)return id==3&&frame%24<12&&port==0;
  if(port!=first_owner)return 0;
  return(id==8&&frame%48<8)||(id==3&&frame%48>=24&&frame%48<36);
 }
 if(port==first_owner)return 0;
 if(admissions && !continued_at && sample>1000 && r8(input_ram,observed_gg+port*42+34)==3)continued_at=sample;
 unsigned t=continued_at?sample-continued_at:sample+join_shift;
 if(continued_at)return(id==8&&t<8)||(id==3&&t>=24&&t<48);
 return(id==8&&t>=120&&t<128)||(id==3&&t>=144&&t<168);
}
'''
s=s[:a]+input_c+s[b:];s=s.replace('    input_ram=ram;input_fc=fc;','    input_ram=ram;input_fc=fc;observed_gg=gg;first_owner=atoi(getenv("FIRST_OWNER"));join_shift=atoi(getenv("JOIN_SHIFT"));')
mark='        else {\n            unsigned entities='
probe='''        else {
            static unsigned previous_flow=3,last_tick;unsigned owner=1-first_owner,p=gg+owner*42,l=LIFE+owner*18;
            unsigned flow=r8(ram,p+34),owned=r8(ram,OWN+owner),state=r8(ram,l+6),tick=r32(ram,fc);
            if(previous_flow==3&&flow!=3){
             admissions++;
             printf("ADMIT %u %u %u %u %u %u %u %u %u %u %u\\n",admissions,owner,r8(ram,p+32),flow,owned,state,r16(ram,p),r16(ram,gg+88),r16(ram,p+2),r8(ram,l+8),r8(ram,l+12));
            }
            previous_flow=flow;
            if(tick!=last_tick){printf("STATE %u %u %u %u %u %u %u\\n",tick,admissions,flow,r8(ram,p+32),owned,state,r8(ram,l+8));last_tick=tick;}
            if(admissions==2&&flow==0&&owned==0)break;
            unsigned entities='''
for key,name in [('LIFE','player_life'),('OWN','life_owned')]:probe=probe.replace(key+'+','0x'+syms[name]+'+')
s=s.replace(mark,probe).replace('return (fixed_ticks?r32(ram,fc)-start_tick>=limit:sample==limit)?0:1;','return admissions==2?0:1;')
(o/'run.c').write_text(s);subprocess.run(['cc','-O2',str(o/'run.c'),'-o',str(o/'run')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest();runs=[];(o/'report.json').unlink(missing_ok=True)
for first,shift in ((0,0),(1,0),(0,11),(0,12),(0,13)):
 env=os.environ.copy();env['FIRST_OWNER']=str(first);env['JOIN_SHIFT']=str(shift);log=o/f'first-{first}-shift-{shift}.log'
 with log.open('w')as f:
  subprocess.run([str(o/'run'),str(core),str(r/'out/rom.bin'),'2','12000',*[syms[n]for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(o/f'first-{first}-shift-{shift}.csv')],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
 rows=[list(map(int,line.split()[1:]))for line in log.read_text().splitlines()if line.startswith('ADMIT ')]
 assert len(rows)==2,rows
 for row in rows:
  n,owner,lives,flow,owned,state,x,camera,y,kind,lock=row
  assert owner==1-first and lives==3 and owned==1 and kind==1,row
  assert state in (6,7) and flow in (1,2),row
  if state==6:assert y==240,('pending pose must stay hidden',row)
  if state==7:assert -10<=((x-camera+32768)%65536)-32768<=-4 and y==135 and lock<=16,row
 states=[list(map(int,line.split()[1:]))for line in log.read_text().splitlines()if line.startswith('STATE ')]
 assert states[-1][1]==2 and states[-1][2]==0 and states[-1][4]==0,states[-1]
 runs.append({'first_owner':first,'start_refresh_shift':shift,'admissions':rows,'final_state':states[-1]});print(runs[-1],flush=True)
assert any(row[5]==6 for run in runs for row in run['admissions']), 'No admission sampled before its first simulation step'
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
(o/'report.json').write_text(json.dumps({'passed':True,'rom_sha256':rh,'runs':runs,'scope':'Controller-only cold start, other-controller coin+held Start join, natural final death, coin+held Start continue, and return to ordinary play. Both joining orders. No injected state/refills; verifies source reentry owner and initial pose/reserve preservation, not source-equivalent join behavior (a console extension).'},indent=2)+'\n')
