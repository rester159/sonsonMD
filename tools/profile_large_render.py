#!/usr/bin/env python3
"""Measure nested large-render functions on the existing matched-input fixture."""
from pathlib import Path
import re,json,hashlib,subprocess,csv
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/large-render-profile';o.mkdir(exist_ok=True)
work=r/'out/parity-audit/workload-comparison';report=json.loads((work/'results.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rh=sha(r/'out/rom.bin');name='after' if rh==report['candidate_rom_sha256'] else 'before'
assert rh==report['candidate_rom_sha256' if name=='after' else 'baseline_rom_sha256']
syms={n:int(a,16)&0xffffff for a,n in re.findall(r'^([0-9a-fA-F]+)\s+[tT]\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
dis=subprocess.check_output(['m68k-elf-objdump','-d',str(r/'out/rom.out')],text=True)
functions=['entity_render_all','arcade_large_cells','arcade_large_present','arcade_cell_sat','arcade_large_art_frame','arcade_large_upload']
mark='local taps={};local starts={};local elapsed={};local calls={}\nlocal function now()return manager.machine.time:as_double()end\nlocal function install_taps()\n'
for i,fn in enumerate(functions,1):
 start=syms[fn];end=min(a for a in syms.values() if a>start)
 returns=[int(a,16) for a in re.findall(r'^\s*([0-9a-f]+):\s+4e75\s+rts',dis,re.M) if start<=int(a,16)<end]
 assert returns,fn
 mark+=f'taps[#taps+1]=m:install_read_tap({start},{start+1},"begin-{i}",function()starts[{i}]=now()end)\n'
 for pc in returns:
  mark+=f'taps[#taps+1]=m:install_read_tap({pc},{pc+1},"end-{i}-{pc}",function()if starts[{i}] then elapsed[{i}]=(elapsed[{i}]or 0)+now()-starts[{i}];calls[{i}]=(calls[{i}]or 0)+1;starts[{i}]=nil end end)\n'
mark+='end\n'
base=(work/(name+'.lua')).read_text().replace(str(work/(name+'.csv')),str(o/'times.csv'))
base=base.replace('emu.register_frame_done(function()',mark+'emu.register_frame_done(function()',1).replace(' frame=frame+1',' frame=frame+1;if frame==60 then install_taps()end',1)
a=base.index('   f:write(tostring(index)');b=base.index('\n',a)
base=base[:a]+'''   f:write(tostring(index));for k=1,6 do f:write(string.format(",%.9f,%d",elapsed[k]or 0,calls[k]or 0))end;f:write("\\n");elapsed={};calls={}'''+base[b:]
(o/'run.lua').write_text(base)
with (o/'mame.log').open('w') as f:
 subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
assert sha(r/'out/rom.bin')==rh
rows=list(csv.reader((o/'times.csv').open()));assert len(rows)==report['frames']
# Select actual source-cell conversion calls, rather than inactive large slots.
busy=[row for row in rows if int(row[8])>0]
assert busy
result={'rom_sha256':rh,'updates':len(rows),'updates_with_cells':len(busy),'scope':'Nested function elapsed time includes callees and interrupts; do not sum inclusive durations','functions':{fn:{'mean_ms_when_cells':sum(float(row[1+2*i]) for row in busy)*1000/len(busy),'peak_ms_when_cells':max(float(row[1+2*i]) for row in busy)*1000,'calls':sum(int(row[2+2*i]) for row in busy)} for i,fn in enumerate(functions)}}
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
