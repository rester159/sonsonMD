#ifndef ARCADE_FOES_H
#define ARCADE_FOES_H
#include "arcade_player.h"
#include "arcade_waves.h"
typedef struct {u16 next,tile;u8 code,attr,duration;} ArcadeFoeFrame;
typedef struct {s16 dx,dy;} ArcadeFoeVector;
typedef struct {
    ArcadeFormationActor body;
    u16 cursor,pose;
    u8 kind,anim_timer,stamp,target,hit_style,death_timer;
} ArcadeFoe;
typedef struct {u16 x;u8 y,active,kind;} ArcadeFoePlayer;
typedef struct {
    u16 camera;
    u8 clock,stage,tracking,scroll_stopped,shot_budget,purge;
    ArcadeFoePlayer players[2];
} ArcadeFoeWorld;
typedef struct {
    u16 x,origin,cursor,pose;
    u8 xf,y,yf,active,state,phase,facing,timer,anim_timer,parent;
} ArcadeFoeWeapon;
extern const ArcadeFoeFrame arcade_foe_frames[];
extern const ArcadeFoeVector arcade_foe_vectors[3][5];
extern const u16 arcade_foe_g_walk[4],arcade_foe_g_unarmed[4],arcade_foe_g_throw[4],arcade_foe_g_recover[4],arcade_foe_h_walk[4];
extern const u16 arcade_foe_g_death[10],arcade_foe_g_exit[10],arcade_foe_h_death[10],arcade_foe_h_exit[10];
extern const u16 arcade_foe_weapon_init[4],arcade_foe_weapon_fly[4],arcade_foe_weapon_end[2];
extern const ArcadeFoePlayer arcade_foe_empty_target;
void arcade_foe_step(ArcadeFoe *foe,u8 family,u8 slot,const ArcadeFoeWorld *world,
                     ArcadeFloor floor,void *context,ArcadeFoeWeapon weapons[4]);
/* Other kinds occupying the same four CBC8 child slots. */
void arcade_foe_step_shared(ArcadeFoe *foe,u8 family,u8 slot,const ArcadeFoeWorld *world,
                     ArcadeFloor floor,void *context,ArcadeFoeWeapon weapons[4],u8 reserved);
void arcade_foe_step_g(ArcadeFoe *foe,u8 slot,const ArcadeFoeWorld *world,
                     ArcadeFloor floor,void *context,ArcadeFoeWeapon weapons[4],u8 reserved);
void arcade_foe_step_h(ArcadeFoe *foe,u8 slot,const ArcadeFoeWorld *world,
                     ArcadeFloor floor,void *context,ArcadeFoeWeapon weapons[4]);
void arcade_foe_weapon_step(ArcadeFoeWeapon *weapon,const ArcadeFoeWorld *world,
                            const ArcadeFoe parents[6]);
#endif
