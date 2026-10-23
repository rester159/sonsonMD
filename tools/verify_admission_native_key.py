#!/usr/bin/env python3
"""Check the compact admission key against the full cache across mode/reset transitions."""
from pathlib import Path
import subprocess,json,argparse
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--entity',type=Path,required=True);args=p.parse_args()
s=args.entity.read_text();a=s.index('static u8 admission_native_key;');b=s.index('/* Map real pool owners',a)
o=r/'out/parity-audit/admission-native-key/host';o.mkdir(parents=True,exist_ok=True)
c='''#include <assert.h>
#include <string.h>
#include "arcade_native_admission.h"
static ArcadeAdmissionCache arcade_admission_cache;
'''+s[a:b]+'''
int main(void){
 ArcadeAdmissionCache ref={0};ArcadeAdmissionInput in,expected;u32 seed=1;unsigned trial,k;
 memset(&in,0,sizeof in);
 for(trial=0;trial<100000;trial++){
  unsigned legacy=(trial%113)<13;
  if(trial%7==0)for(k=0;k<112;k++){seed=seed*1664525u+1013904223u;((u8*)&in)[k]=(u8)(seed>>24);}
  if(trial%19==0)admission_native_key=0; /* game reset retains old cache */
  expected=in;
  if(!legacy)for(k=0;k<6;k++){expected.words[6+k]=0;expected.words[12+k]=0xffffffffUL;}
  arcade_native_admission_refresh(&expected,&ref);
  if(legacy){admission_native_key=0;arcade_native_admission_refresh(&in,&arcade_admission_cache);}
  else entity_admission_native(&in);
  assert(!memcmp(&ref.input,&arcade_admission_cache.input,sizeof ref.input));
  assert(!memcmp(&ref.result,&arcade_admission_cache.result,sizeof ref.result));
  assert(ref.hits==arcade_admission_cache.hits && ref.misses==arcade_admission_cache.misses && ref.valid==arcade_admission_cache.valid);
 }
 return 0;
}
''';(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_native_admission.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cache_calls':100000,'differences':0,'scope':'Actual compact key vs full cache, all input/result bytes and hit/miss counters; reset, fallback/native transitions, cache hits, and random active values. Host ASan/UBSan; target replay separate.'}
(o/'report.json').write_text(json.dumps(report,indent=2));print(report)
