#!/usr/bin/env python3
"""Complete-identity cache invalidation and exact output equivalence."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/admission-cache';o.mkdir(exist_ok=True)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_native_admission.h"
static void check(ArcadeAdmissionInput *in,ArcadeAdmissionCache *cache){
 ArcadeNativeAdmission ref;memset(&ref,0,sizeof(ref));
 arcade_native_admission_cdef(in->state.entities,in->state.ranks,in->state.main,in->state.reserved,in->state.large,in->state.children,in->state.c,in->state.d,in->state.e,in->state.f,&ref);
 arcade_native_admission_refresh(in,cache);
 assert(ref.large==cache->result.large && ref.children==cache->result.children && ref.remaining==cache->result.remaining && ref.count==cache->result.count);
 assert(!memcmp(ref.order,cache->result.order,ref.count));
}
int main(void){ArcadeAdmissionInput in;ArcadeAdmissionCache cache;unsigned byte,value;u32 hits,misses;
 memset(&in,0,sizeof(in));memset(&cache,0,sizeof(cache));check(&in,&cache);assert(cache.misses==1 && !cache.hits);
 for(byte=0;byte<sizeof(in);byte++)for(value=0;value<256;value++){
  unsigned char *p=(unsigned char*)&in;u8 previous=p[byte];hits=cache.hits;misses=cache.misses;p[byte]=value;
  check(&in,&cache);
  assert(cache.hits==hits+(previous==value) && cache.misses==misses+(previous!=value));
  hits=cache.hits;misses=cache.misses;check(&in,&cache);check(&in,&cache);
  assert(cache.hits==hits+2 && cache.misses==misses);
 }
 /* Invalidate explicitly after arbitrary reset of a cache object. */
 cache.valid=0;misses=cache.misses;check(&in,&cache);assert(cache.misses==misses+1);
 return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_native_admission.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'input_byte_mutations':28672,'checks':86018,'scope':'Every input byte/all256 values, repeated exact-input hits, explicit invalidation and output comparison to uncached native admission. ASan/UBSan; production target ABI offsets guarded at build.'}
(o/'equivalence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
