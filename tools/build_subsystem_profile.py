#!/usr/bin/env python3
"""Build an isolated, coarse SGDK elapsed-time diagnostic (never a release ROM).

getSubTick has scanline quantization and a documented VBlank rollback limitation.
Inclusive totals include interrupts, nested probes and instrumentation overhead;
VBlank includes intentional waiting. These are not cycle counts or FPS proof.
"""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=pathlib.Path,default=root/'out/subsystem-profile')
parser.add_argument('--camera-min',type=int,default=0)
parser.add_argument('--camera-max',type=int,default=65535)
parser.add_argument('--coarse-only',action='store_true',help='Only outer subsystem probes; omit nested per-actor/contact instrumentation')
parser.add_argument('--background-probes',action='store_true',help='Include background renderer probes with --coarse-only')
args=parser.parse_args()
background_probes_enabled=args.background_probes
coarse_only=args.coarse_only
camera_min,camera_max=args.camera_min,args.camera_max
assert 0<=camera_min<=camera_max<=65535
dest=args.output_dir.resolve()
dest.mkdir(parents=True,exist_ok=True)
for directory in ('src', 'inc'):
    shutil.copytree(root / directory, dest / directory, dirs_exist_ok=True)
(dest / 'tools').mkdir(exist_ok=True)
for name in ('build_native.py', 'verify_second_core.py', 'verify_libretro.c'):
    shutil.copy2(root / 'tools' / name, dest / 'tools' / name)

# Each wrapper measures complete calls, including early returns. Public call
# sites retain their original signatures. Production sources are never edited.
probes = [
    ('game.c', 'bool', 'game_update', 'void', '', 'simulation'),
    ('game.c', 'void', 'game_render', 'void', '', 'render'),
    ('entity.c', 'void', 'entity_update_all', 'void', '', 'entities'),
    ('entity.c', 'u16', 'entity_render_all', 'u16 *sat, u16 firstSlot', 'sat, firstSlot', 'sprites'),
    ('entity.c', 'void', 'entity_arcade_prepare_admission', 'void', '', 'catchup_admission'),
]
for index, (file, result, name, params, args, label) in enumerate(probes):
    path = dest / 'src' / file
    source = path.read_text()
    signature = f'{result} {name}({params})'
    if source.count(signature)==0:signature=signature.replace(", ",",")
    assert source.count(signature) == 1, signature
    source = source.replace(signature, f'static {result} profile_body_{name}({params})')
    call = f'profile_body_{name}({args})'
    body = f'{call};' if result == 'void' else f'{result} value={call};'
    ret = '' if result == 'void' else 'return value;'
    source += f'\n{signature}{{u32 start=getSubTick();{body}profile_record({index},start);{ret}}}\n'
    if '#include "subsystem_profile.h"' not in source:
        source = '#include <genesis.h>\n#include "subsystem_profile.h"\n' + source
    path.write_text(source)

path = dest / 'src/game.c'
source = path.read_text().replace('SYS_doVBlankProcess();', 'profile_vblank();')
path.write_text(source)
path = dest / 'src/entity.c'
source = path.read_text()
specialized='void collision_mode(' in source
collision_call='collision_mode(survival)' if specialized else 'collision_pass()'
for slot, name in ((6, collision_call), (7, 'entity_schedule_tick()')):
    call = f'    {name};'
    assert source.count(call) == 1, name
    source = source.replace(call, f'    {{u32 start=getSubTick();{name};profile_record({slot},start);}}')
coarse_entity_source=source
# Section probes preserve the original statement order and existing early skip.
contact_start = ('        /* Food/background displacement is complete.'
                 if '        /* Food/background displacement is complete.' in source
                 else '        /* ECB7: main pool contacts precede K and the formation families. */')
replacements = [
    ('static void collision_pass(void)\n{',
     'static void collision_pass(void)\n{\n    u32 profile_shots_start=getSubTick(),profile_contacts_start,profile_detail_start;'),
    ('player_contacts:\n', 'player_contacts:\n    profile_record(8,profile_shots_start);\n'),
    ('if(!g_survivalRun)game_arcade_food_contact(pk);',
     'if(!g_survivalRun){u32 start=getSubTick();game_arcade_food_contact(pk);profile_record(9,start);}'),
    ('            /* E2E3/E337: both stable shot slots scan the shared background pool. */',
     '            u32 profile_world_start=getSubTick();\n            /* E2E3/E337: both stable shot slots scan the shared background pool. */'),
    ('            large_shot_collisions();',
     '            large_shot_collisions();\n            profile_record(10,profile_world_start);'),
    (contact_start, '        profile_contacts_start=getSubTick();profile_detail_start=getSubTick();\n'+contact_start),
    ('    }   /* end per-player loop */',
     '        profile_record(11,profile_contacts_start);\n    }   /* end per-player loop */'),
]
for before, after in replacements:
    if specialized:
        signature='static inline __attribute__((always_inline)) void collision_mode(const u8 survival)'
        before=before.replace('static void collision_pass(void)',signature).replace('g_survivalRun','survival')
        after=after.replace('static void collision_pass(void)',signature).replace('g_survivalRun','survival')
    assert source.count(before) == 1, before
    source = source.replace(before, after)
# Contiguous contact regions, preserving every original branch/early exit.
for slot, marker in [
    (21, '        /* E2F8 precedes E2FE/E301/E304:'),
    (22, '        if(!g_survivalRun)game_arcade_special_contact(pk);'),
    (24, '        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)\n        {\n            if(arcade_large_contact_target'),
    (25, '    if(g_survivalRun || legacy_pool_used)'),
    (26, '        profile_record(11,profile_contacts_start);')]:
    if specialized:marker=marker.replace('g_survivalRun','survival')
    assert source.count(marker)==1, marker
    source=source.replace(marker, f'        profile_record({slot},profile_detail_start);profile_detail_start=getSubTick();\n'+marker)
marker='        if(!g_survivalRun)game_arcade_special_contact(pk); /* E2FB/E34F, after K. */'
if specialized:marker=marker.replace('g_survivalRun','survival')
assert source.count(marker)==1
source=source.replace(marker,marker+'\n        profile_record(23,profile_detail_start);profile_detail_start=getSubTick();')
entry = ('static inline __attribute__((always_inline)) void entity_update_mode(const u8 survival)\n{' if specialized else 'static void profile_body_entity_update_all(void)\n{')
assert source.count(entry) == 1
source = source.replace(entry, entry+'\n    u32 profile_actor_start=getSubTick();')
for slot, marker in [
    (12, '    /* Enemy AI tracks a LIVE player:'),
    (13, '    __asm__ volatile(".globl profile_large_update_begin'),
    (14, '    __asm__ volatile(".globl profile_collision_begin'),
    (15, '    {u32 start=getSubTick();'+collision_call+';profile_record(6,start);}')]:
    assert source.count(marker) == 1, marker
    source = source.replace(marker, f'    profile_record({slot},profile_actor_start);profile_actor_start=getSubTick();\n'+marker)
actor_call = 'if (e->active) entity_update_one(e,track,i);'
native_call = """if(e->active) {
                if(!g_survivalRun && i<12)native_foe_update(e,i);
                else if(!g_survivalRun && i>=20 && i<24)native_i_update(e,i);
                else entity_update_one(e,track,i);
            }"""
if 'entity_update_one(e,track,survival)' in source:
    native_call=native_call.replace('g_survivalRun','survival').replace('entity_update_one(e,track,i)','entity_update_one(e,track,survival)')
if native_call in source:
    assert source.count(native_call)==1
    wrapped=native_call.replace('if(e->active) {', 'if(e->active) {\n                u16 probe=i<6?16:i<12?17:(i>=16 && i<20)?18:(i>=20 && i<24)?19:20;\n                u32 start=getSubTick();')
    wrapped=wrapped.rsplit('\n            }',1)[0]+'\n                profile_record(probe,start);\n            }'
    source=source.replace(native_call,wrapped)
elif 'native_foe_update(&g_entities[i],i);' in source:
    # Split loops retain the same per-family diagnostic slots.
    for call,probe,count in [
        ('native_foe_update(&g_entities[i],i);','i<6?16:17',1),
        ('native_i_update(&g_entities[i],i);','19',1),
        ('entity_update_one(&g_entities[i],track,0);','i<20?18:20',2),
        ('entity_update_one(&g_entities[i],track,1);','i<6?16:i<12?17:(i>=16 && i<20)?18:(i>=20 && i<24)?19:20',1)]:
        assert source.count(call)==count,call
        source=source.replace(call,'{u32 start=getSubTick();'+call+'profile_record('+probe+',start);}')
else:
    assert source.count(actor_call)==1
    source=source.replace(actor_call, """if (e->active) {
                u16 probe=i<6?16:i<12?17:(i>=16 && i<20)?18:(i>=20 && i<24)?19:20;
                u32 start=getSubTick();entity_update_one(e,track,i);profile_record(probe,start);
            }""")
# No probe may separate this unbraced guard from its loop.
assert ('if('+('survival' if specialized else 'g_survivalRun')+' || legacy_pool_used)\n    /* Ordinary slots0..15 were unconditionally excluded below. */\n    for (') in source
path.write_text(coarse_entity_source if coarse_only else source)
(dest / 'inc/subsystem_profile.h').write_text('''#include <genesis.h>
void profile_record(u16 slot,u32 start);
void profile_vblank(void);
''')
# Keep historical slot numbers stable; append late-course background owners.
background_probes=[('arcade_food_world.c','arcade_food_world_render','u8 phase,u8 cold','phase,cold'),
 ('arcade_release_background.c','arcade_release_background_render','void',''),
 ('arcade_finish_background.c','arcade_finish_background_render','void',''),
 ('arcade_food_bonus.c','arcade_food_bonus_background','u8 plane_phase','plane_phase')]
for slot,(file,name,params,callargs) in enumerate([] if coarse_only and not background_probes_enabled else background_probes,27):
 path=dest/'src'/file;source=path.read_text();signature=f'void {name}({params})'
 assert source.count(signature)==1,signature
 source='#include "subsystem_profile.h"\n'+source.replace(signature,f'static void profile_body_{name}({params})')
 source+=f'\n{signature}{{u32 start=getSubTick();profile_body_{name}({callargs});profile_record({slot},start);}}\n'
 path.write_text(source)
(dest / 'src/subsystem_profile.c').write_text('''#include <genesis.h>
/* Three u32 words per slot: total subticks, completed calls, maximum. */
volatile u32 subsystem_profile[31][3];
void profile_record(u16 slot,u32 start)
{
    u32 elapsed=getSubTick()-start;
    subsystem_profile[slot][0]+=elapsed;
    subsystem_profile[slot][1]++;
    if(elapsed>subsystem_profile[slot][2])subsystem_profile[slot][2]=elapsed;
}
void profile_vblank(void)
{u32 start=getSubTick();SYS_doVBlankProcess();profile_record(5,start);}
''')
profile_path=dest/'src/subsystem_profile.c'
profile_source=profile_path.read_text().replace('#include <genesis.h>','#include <genesis.h>\n#include "game.h"')
profile_source=profile_source.replace('    subsystem_profile[slot][0]+=elapsed;',f'    if((u16)g_game.cameraX<{camera_min} || (u16)g_game.cameraX>{camera_max})return;\n    subsystem_profile[slot][0]+=elapsed;')
profile_path.write_text(profile_source)
# Extend only this private harness to print completed-call counters at exit.
path = dest / 'tools/verify_libretro.c'
source = path.read_text()
needle = '    if(getenv("SONSON_VRAM_DUMP"))'
assert source.count(needle) == 1
source = source.replace(needle, '''    if(getenv("SONSON_PROFILE")) {
        unsigned addr=strtoul(getenv("SONSON_PROFILE"),NULL,16);
        for(unsigned k=0;k<31;k++)printf("PROFILE %u %u %u %u\\n",k,r32(ram,addr+k*12),r32(ram,addr+k*12+4),r32(ram,addr+k*12+8));
    }
''' + needle)
path.write_text(source)
path = dest / 'tools/verify_second_core.py'
source = path.read_text().replace('env=os.environ.copy();', 'env=os.environ.copy();env["SONSON_PROFILE"]=syms["subsystem_profile"];')
path.write_text(source)
subprocess.run(['python3', str(dest / 'tools/build_native.py')], check=True)
(dest / 'profile-scope.json').write_text(json.dumps({
    'slots': [p[-1] for p in probes] + ['vblank_wait', 'collision', 'scheduling',
        'shot_families', 'player_food_background', 'shot_background_large', 'player_body_special_legacy',
        'native_pool_context', 'entity_actor_loop', 'large_actor_loop', 'food_carrier',
        'g_actor_adapter', 'h_actor_adapter', 'player_shot_adapter', 'i_actor_adapter', 'other_actor_adapter',
        'contact_main_c_d_e_f', 'contact_k', 'contact_special', 'contact_h_g_i',
        'contact_large', 'contact_legacy', 'food_world_render', 'release_background', 'finish_background', 'food_bonus_background'],
    'camera_range':[camera_min,camera_max],
    'coarse_only':coarse_only,
    'background_probes':background_probes_enabled,
    'scope': __doc__,
    'production_rom_sha256': hashlib.sha256((root/'out/rom.bin').read_bytes()).hexdigest(),
    'counters': 'Inclusive completed calls ending inside the selected camera range; may include boot/attract when their camera qualifies.',
}, indent=2)+'\n')
print('Diagnostic only:', dest / 'out/rom.bin')
