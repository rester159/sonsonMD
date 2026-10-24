#!/usr/bin/env python3
"""Check actual schedule reset retains recovered actor bytes on course entry."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/course-actor-retention';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert rom[0x9793-0x4000:0x979d-0x4000]==bytes.fromhex('4f a7 10 30 85 31 3f 26 f8 39')
s=(r/'src/entity.c').read_text();a=s.index('static void entity_schedule_reset(');b=s.index('\nvoid entity_schedule_init',a)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_i.h"
#include "arcade_large.h"
#include "arcade_k.h"
#include "ss_wave_sched.h"
static u8 survival_delay,survival_cursor,arcade_bonus_next,arcade_bonus_limit,wave_clock;
static ArcadeWaves arcade_waves;static ArcadeRng arcade_rng;
static ArcadeIActor arcade_i_actors[4];static ArcadeLargeActor arcade_large_actors[3];
static ArcadeKProjectile arcade_k_actors[4];static ArcadeFoe arcade_foes[12];static ArcadeFoeWeapon arcade_weapons[4];
static ArcadeIWave arcade_i_wave;static u8 arcade_i_live,arcade_i_reservation,arcade_shot_budget,formation_reservation[2],s_sched_gcursor;
static u16 large_last_count,large_last_visible;static u8 large_last_resident,arcade_main_ready;static u16 arcade_e_ready[2];
#include "arcade_main_pool.h"
static ArcadeMainPool arcade_main;
void arcade_main_pool_reset(ArcadeMainPool*p,u8 cold){(void)p;(void)cold;}
#include "arcade_c_pool.h"
static ArcadeCPool arcade_c;
void arcade_c_pool_reset(ArcadeCPool*p,u8 cold){(void)p;(void)cold;}
#include "arcade_d_pool.h"
static ArcadeDPool arcade_d;
void arcade_d_pool_reset(ArcadeDPool*p,u8 cold){(void)p;(void)cold;}
#include "arcade_e_pool.h"
static ArcadeEPool arcade_e;
void arcade_e_pool_reset(ArcadeEPool*p,u8 cold){(void)p;(void)cold;}
#include "arcade_f_pool.h"
static ArcadeFPool arcade_f;
void arcade_f_pool_reset(ArcadeFPool*p,u8 cold){(void)p;(void)cold;}
void arcade_e_upload_reset(void){}
void arcade_waves_reset(ArcadeWaves*w,u8 c){(void)w;(void)c;}
void arcade_waves_course_begin(ArcadeWaves*w){(void)w;}
static void arcade_large_upload_reset(void){}
'''+s[a:b]+r'''
int main(void){unsigned seed,i;
for(seed=0;seed<256;seed++){
 ArcadeIActor ei[4];ArcadeLargeActor el[3];ArcadeKProjectile ek[4];ArcadeFoe ef[12];ArcadeFoeWeapon ew[4];
#define INIT(pool,expected) memset(pool,seed,sizeof(pool));memcpy(expected,pool,sizeof(pool))
 INIT(arcade_i_actors,ei);INIT(arcade_large_actors,el);INIT(arcade_k_actors,ek);INIT(arcade_foes,ef);INIT(arcade_weapons,ew);
 for(i=0;i<4;i++){ei[i].body.active=0;ek[i].active=0;ew[i].active=0;}
 for(i=0;i<3;i++)el[i].body.active=0;
 for(i=0;i<12;i++)ef[i].body.active=0;
 memset(&arcade_i_wave,seed,sizeof(arcade_i_wave));
 memset(&arcade_waves,255-seed,sizeof(arcade_waves));
 wave_clock=173;entity_schedule_reset(0);assert(wave_clock==173);
 assert(arcade_i_wave.cursor==seed && arcade_i_wave.remaining==seed);
 assert(!memcmp(&arcade_i_wave.control,&arcade_waves.controls[SS_WAVE_BIT_CLASSI],sizeof(ArcadeWaveControl)));
 assert(!memcmp(ei,arcade_i_actors,sizeof(ei)));assert(!memcmp(el,arcade_large_actors,sizeof(el)));
 assert(!memcmp(ek,arcade_k_actors,sizeof(ek)));assert(!memcmp(ef,arcade_foes,sizeof(ef)));
 assert(!memcmp(ew,arcade_weapons,sizeof(ew)));
 wave_clock=219;entity_schedule_reset(1);assert(wave_clock==219);
 memset(ei,0,sizeof(ei));memset(el,0,sizeof(el));memset(ek,0,sizeof(ek));
 assert(!memcmp(ei,arcade_i_actors,sizeof(ei)));assert(!memcmp(el,arcade_large_actors,sizeof(el)));assert(!memcmp(ek,arcade_k_actors,sizeof(ek)));
 for(i=0;i<12;i++)assert(!arcade_foes[i].body.active && !arcade_foes[i].body.xf && !arcade_foes[i].body.yf && !arcade_foes[i].target);
 for(i=0;i<4;i++)assert(!arcade_weapons[i].active && !arcade_weapons[i].xf && !arcade_weapons[i].yf);
}return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':256,'actor_records_per_case':27,'scope':'Actual schedule reset with scheduler/renderer stubs. Source9793 byte guard; all retained actor bytes compared for I, large, K, G/H and weapons; cold reset regression. Controller-field retention and full runtime behavior not proven.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
