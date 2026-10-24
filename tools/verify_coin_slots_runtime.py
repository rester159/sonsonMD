#!/usr/bin/env python3
"""Windowless target coin-slot check; DIP injection, controller-only coin edges."""
from pathlib import Path
import subprocess,re,json,hashlib,os,csv
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/coin-slots-runtime';o.mkdir(parents=True,exist_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text());assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text();a=s.index('static int16_t input(');b=s.index('#define FN',a)
s=s[:a]+'''static unsigned selected,initial,ratio;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;
 if(frame<740)return port==0&&id==3&&frame%24<12;
 if(id!=8)return 0;
 return (port==0&&((frame>=1000&&frame<1012)||(frame>=1080&&frame<1092))) ||
        (port==1&&((frame>=1040&&frame<1052)||(frame>=1120&&frame<1132)));
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;', 'if(argc!=7)return 2;selected=atoi(argv[4]);initial=atoi(argv[5]);ratio=atoi(argv[6]);').replace('frame<10000','frame<1200')
s=s.replace('  retro_run();',f'  if(frame==900)w8(ram,{syms["g_config"]},(r8(ram,{syms["g_config"]})&~31u)|ratio|(selected==0?16u:0u));\n  retro_run();')
s=s.replace('  retro_run();',f'  if(frame==900){{w8(ram,{syms["g_arcadeCredits"]},initial>>8);w8(ram,{syms["g_arcadeCredits"]+1},initial&255);}}\n  retro_run();')
s=s.replace('delay0,delay1\\n','delay0,delay1,credits\\n').replace("  fputc('\\n',f);",f"  fprintf(f,\",%u\",r16(ram,{syms['g_arcadeCredits']}));\n  fputc('\\n',f);")
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')));cases=[]
for selected,initial,ratio in ((0,0,8),(1,0,8),(0,99,8),(1,255,8),(0,65534,8),(1,65534,8),(0,65530,9),(1,65530,9)):
 with (o/f'log-{selected}-{initial}-{ratio}.txt').open('w') as log:subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/f'trace-{selected}-{initial}-{ratio}.csv'),str(selected),str(initial),str(ratio)],check=True,stdout=log,stderr=subprocess.STDOUT)
 rows=[{k:int(v) for k,v in x.items()} for x in csv.DictReader((o/f'trace-{selected}-{initial}-{ratio}.csv').open())]
 if ratio==8:expected=tuple(min(65535,initial+n) for n in ((0,1,2,3) if selected==0 else (1,1,2,3)))
 else:expected=(65530,65531,65531,65532) if selected==0 else (65531,65531,65532,65532)
 for frame,credit in zip((1030,1070,1110,1150),expected):assert rows[frame]['credits']==credit and rows[frame]['attract']==0,(selected,frame,rows[frame])
 cases.append({'selected_slot':selected,'injected_initial_credits':initial,'ratio_raw':ratio,'credits_after_pulses':expected,'trace_sha256':hashlib.sha256((o/f'trace-{selected}-{initial}-{ratio}.csv').read_bytes()).hexdigest()})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'cases':cases,'hashes':prior['hashes'],'harness_sha256':hashlib.sha256((o/'harness.c').read_bytes()).hexdigest(),'scope':'Cold boot to arcade title, inject 2C/1C or 1C/7C DIP and slot selector, then four held C pulses alternating pads. Other slot stays 1C/1C; overflow awards are discarded. Initial credit word injected at the same setup boundary; no fractional writes. Cases include crossing99/255 and reaching65535 and rejecting overflowing seven-credit awards. Does not test DIP menu interaction, coin audio or all coin ratios on target.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
