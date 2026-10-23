#!/usr/bin/env python3
"""Production game_run loop with real cadence/input helpers and fake platform."""
from pathlib import Path
import subprocess,json,argparse,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/game-pacing-loop';o.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=r/'src/game.c');args=parser.parse_args()
s=args.source.read_text();a=s.index('static ArcadePacing home_pacing;');b=s.index('/* Returns true when results own',a);helpers=s[a:b]
a=s.index('void game_run(void)');b=s.index('    /* AUDIO teardown',a);loop=s[a:b]+'}\n'
c=r'''
#include <assert.h>
#include <stdint.h>
#include "arcade_pacing.h"
typedef u8 bool;
#define TRUE 1
#define MODE_ARCADE 0
#define MODE_HOME 1
static u8 g_mode,g_attract_startPlayers,arcade_title_demo_owned;
static u8 arcade_title_demo_update(u8 done){return done;}
static u16 g_arcadeCredits;

#define FALSE 0
#define JOY_1 0
#define JOY_2 1
#define BUTTON_START 128
#define BUTTON_A 64
#define BUTTON_B 16
#define BUTTON_C 32
#define ATTRACT_MAX_FRAMES 3000
static u8 g_survivalRun,g_paused,g_attract,g_attract_started,g_resultsActive,boundary;
static unsigned admitted;
static u32 vtimer,g_presentCounter;
static unsigned frames,updates,region,stalls,last_updates,rendered,clock_phase;
static uint64_t elapsed;
static u16 previous_refresh;
static u16 JOY_readJoypad(u8 k){return g_attract?0:(u16)((frames+k)%7==0?16:0);}
static u8 entity_arcade_clock(void){return (u8)(updates+clock_phase);}
HELPERS
static void game_init(void){vtimer=65520;frames=updates=last_updates=rendered=0;elapsed=0;previous_refresh=(u16)vtimer;g_presentCounter=0;g_paused=0;g_resultsActive=boundary=0;admitted=0;}
static void mode_input_rearm(void){}
static u8 SYS_isPAL(void){return region;}
static void snd_play_coin(void){}
u8 mode_input_coins(void){return 0;}
u8 mode_input_poll(void){
 g_paused=!g_attract && ((frames>=100 && frames<150)||(frames>=500 && frames<575));
 if(!g_paused && !g_survivalRun)elapsed+=(u16)((u16)vtimer-previous_refresh);
 previous_refresh=(u16)vtimer;return 0;
}
static u8 game_arcade_frozen(void){return 0;}
static void game_start_buttons(u8 starts){assert(!starts);}
static void entity_arcade_prepare_admission(void){admitted=updates;}
static bool game_update(void){
 assert(gameplay_input_ready && !g_paused && !boundary);
 if(!g_survivalRun && !g_resultsActive)assert(admitted==updates);
 boundary=g_resultsActive;updates++;
 g_resultsActive=(updates>=101 && updates<201);boundary|=g_resultsActive;
 return updates==2000;
}
static void game_render(void){
 const uint64_t num=277ULL*3420*(region?313:262),den=5ULL*(region?53203424:53693175);
 assert(!g_paused && !gameplay_input_ready);
 if(!g_survivalRun){assert(gameplay_pacing->pending+updates==elapsed*num/den);assert(gameplay_pacing->phase==elapsed*num%den);}
 else assert(!gameplay_pacing->pending);
 entity_arcade_prepare_admission();rendered++;
}
static void SYS_doVBlankProcess(void){
 assert(updates-last_updates<=(g_survivalRun?1u:2u));if(g_paused)assert(updates==last_updates);
 boundary=0;last_updates=updates;frames++;assert(frames<10000);
 vtimer+=(stalls && frames%31==0)?100:1;
}
static void game_present(void){SYS_doVBlankProcess();}
LOOP
int main(void){unsigned cases=0;g_mode=MODE_HOME;for(region=0;region<2;region++)for(stalls=0;stalls<2;stalls++)for(g_survivalRun=0;g_survivalRun<2;g_survivalRun++){
 g_attract=0;game_run();assert(updates==2000 && g_presentCounter==rendered);cases++;
 if(!g_survivalRun && stalls)assert(gameplay_pacing->pending>0);
}
assert(cases==8);
g_survivalRun=0;g_attract=1;region=0;
for(stalls=0;stalls<2;stalls++)for(clock_phase=0;clock_phase<256;clock_phase++) {
 game_run();assert(updates==1920-(clock_phase&63));assert(!g_attract_started);
}
return 0;
}
'''.replace('HELPERS',helpers).replace('LOOP',loop)
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_pacing.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'source_sha256':hashlib.sha256(args.source.read_bytes()).hexdigest(),'scenarios':8,'updates_each':2000,'attract_timeout_scenarios':512,'scope':'Production Home loop/cadence/input with platform/gameplay shims (Arcade shared handoffs tested separately); source-time debt conservation, pause bypass/resume, host wrap, normal/Survival, NTSC/PAL and stalls. Attract timeout across every clock phase with/without stalls. Configured per-display cap retains debt; intermediate admission and results boundaries checked, extreme stalls demonstrate backlog. Not gameplay/VDP parity.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
