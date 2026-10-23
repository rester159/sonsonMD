#!/usr/bin/env python3
"""Compare actual pre-specialization and current Survival SAT on the target CPU."""
from pathlib import Path
import argparse,subprocess,shutil,re,json
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--entity',type=Path,default=r/'src/entity.c');args=p.parse_args()
o=r/'out/parity-audit/survival-sat';o.mkdir(exist_ok=True)
for n in ('src','inc'):shutil.copytree(r/n,o/n,dirs_exist_ok=True)
(o/'tools').mkdir(exist_ok=True)
for n in ('build_native.py','verify_survival_core.py','verify_libretro.c'):shutil.copy2(r/'tools'/n,o/'tools'/n)
s=args.entity.read_text();needle='u16 entity_render_all(u16 *sat,u16 firstSlot)';assert s.count(needle)==1
s=s.replace(needle,'static u16 survival_render_candidate(u16 *sat,u16 firstSlot)')
s+='\n'+(r/'tools/fixtures/survival_renderer_before_specialization.c').read_text()+'''
volatile u32 survival_sat_checks,survival_sat_errors;
u16 entity_render_all(u16 *sat,u16 firstSlot){
    static u16 reference[320];u16 count,n,k;
    if(!g_survivalRun)return survival_render_candidate(sat,firstSlot);
    n=survival_render_reference(reference,firstSlot);
    count=survival_render_candidate(sat,firstSlot);
    survival_sat_checks++;
    if(n!=count)survival_sat_errors++;
    else for(k=firstSlot*4;k<(firstSlot+count)*4;k++)if(reference[k]!=sat[k])survival_sat_errors++;
    return count;
}
''';(o/'src/entity.c').write_text(s)
subprocess.run(['python3',str(o/'tools/build_native.py')],check=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(o/'out/symbol.txt').read_text(),re.M)}
f=o/'tools/verify_libretro.c';s=f.read_text();s=s.replace('        retro_run();',f'''        retro_run();
        if(r32(ram,0x{syms['survival_sat_errors']})){{fprintf(stderr,"SAT mismatch at refresh %u\\n",frame);return 9;}}
''');s=s.replace('    fclose(f);printf(',f'''    fprintf(stderr,"SAT_CHECKS %u\\n",r32(ram,0x{syms['survival_sat_checks']}));
    fclose(f);printf(''');f.write_text(s)
result=subprocess.run(['python3',str(o/'tools/verify_survival_core.py')],capture_output=True,text=True)
assert result.returncode==0 or 'modes_failing_survival_stress' in result.stderr,(result.stdout,result.stderr)
counts=[]
for players in (1,2):
 log=(o/f'out/survival-core-check/{players}p.txt').read_text();m=re.search(r'SAT_CHECKS (\d+)',log);assert m,log[-1000:];counts.append(int(m[1]));assert counts[-1]>1000
report={'target_sat_comparisons':counts,'mismatches':0,'scope':'Actual original/current Survival rendering compared on identical live state at each draw. Windowless Genesis Plus GX, injected maximum pressure/grace; not normal Survival rules or performance proof.'}
(o/'report.json').write_text(json.dumps(report,indent=2));print(report)
