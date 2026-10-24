#!/usr/bin/env python3
"""Old/new production player flags; complete fallback and owned byte domains."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/player-target-flags';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/game.c').read_text();a=s.index('u16 game_player_target_flags(');b=s.index('\nstatic u8 survival_step;',a)
old=(r/'tools/fixtures/player_flags_before_flow_snapshot.c').read_text().replace('game_player_target_flags','before')
c='''#include <assert.h>
#include "arcade_types.h"
#define FLOW_DYING 1
#define FLOW_GAME_OVER 3
typedef struct {u8 flow;} Player;
static struct {Player players[2];u8 numPlayers;} g_game;
static struct {u8 active,kind;} player_life[2];
static u8 life_owned[2],run_started;
static u8 game_player_started(u8 which){return (u8)(run_started&(1<<which));}
'''+old+s[a:b]+'''
int main(void){unsigned k,flow,started,players,active,kind,owned;
for(k=0;k<2;k++)for(players=0;players<256;players++)for(flow=0;flow<256;flow++)for(started=0;started<256;started++){
 g_game.numPlayers=players;g_game.players[k].flow=flow;run_started=started;life_owned[k]=0;
 assert(before(k)==game_player_target_flags(k));
}
for(k=0;k<2;k++)for(owned=1;owned<=2;owned++)for(active=0;active<256;active++)for(kind=0;kind<256;kind++){
 life_owned[k]=owned;player_life[k].active=active;player_life[k].kind=kind;
 assert(before(k)==game_player_target_flags(k));assert(game_player_target_flags(k)==active*256+kind);
}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'fallback_cases':2*256**3,'owned_cases':4*256**2,'scope':'Actual old/new function bodies with valid player indices0/1, all fallback byte combinations and owned active/kind pairs. Host equivalence; target cost and gameplay measured separately.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
