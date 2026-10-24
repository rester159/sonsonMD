#include "arcade_player.h"
/* Source contract: SonSon ss.02e, main MC6809 $A9A7..$AD0F.
 * ROM motion record $AA16: right $01AA, left $014D, down $02C0, up $0240.
 * The phase transitions below retain source byte wrap and branch boundaries. */
static void add_x(ArcadePlayer *p,s16 delta)
{
    /* Source integer X and fractional byte form a 24-bit unsigned accumulator. */
    int value=(int)p->xf+delta;
    int carry=value>=0?value/256:-((-value+255)/256);
    p->xf=(u8)value;p->x=(u16)(p->x+carry);
}
static void add_y(ArcadePlayer *p,s16 delta)
{
    u16 value=(u16)(((u16)p->y<<8)|p->yf);
    value=(u16)(value+delta);p->y=(u8)(value>>8);p->yf=(u8)value;
}
static void set_state(ArcadePlayer *p,u8 state,u8 phase)
{ p->state=state;p->phase=phase; }
static void climb_command(ArcadePlayer *p,u8 command)
{
    if(command&0x10)set_state(p,2,0);
    else if(p->state!=5 && (command&0x20) && p->y>0x28)set_state(p,3,0);
}
static int supported(const ArcadePlayer *p,const ArcadePlayer *other,s16 *adjust)
{
    u8 dy;
    if(!other || other->active!=1)return 0;
    dy=(u8)(p->y-16-other->y+8);
    if(dy>16 || (u16)(p->x-other->x+11)>=22)return 0;
    *adjust=(s16)(3-dy);return 1;
}
static int rising_into(const ArcadePlayer *p,const ArcadePlayer *other)
{
    return other && other->active==1 && other->state==1 &&
           (u8)(other->y-p->y)<=14 && (u16)(p->x-other->x+14)<=28;
}
static void shift_y(ArcadePlayer *p,ArcadePlayer *other,int delta)
{
    p->y=(u8)(p->y+delta);
    if(p->attached && other)other->y=(u8)(other->y+delta);
}
static void player_anim_tick(ArcadePlayerAnimation *a)
{
    static const u8 codes[8]={0x40,0x41,0x50,0x41,0x48,0x49,0x51,0x49};
    if(!a)return;
    if(a->timer){a->timer--;return;}
    a->code=codes[a->owner*4+a->cursor];a->cursor=(u8)((a->cursor+1)&3);a->timer=4;
}
void arcade_player_animation_step(ArcadePlayerAnimation *a){player_anim_tick(a);}
static void player_anim_face(ArcadePlayerAnimation *a,u8 facing)
{
    if(!a)return;
    a->attr=(u8)(a->owner|(facing==3?0x80:0xc0));a->cursor=a->timer=0;player_anim_tick(a);
}
static void horizontal(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,u8 move,ArcadePlayerAnimation *anim,u8 prelude)
{
    u8 x=(u8)(p->x-camera),allowed=12;
    s16 delta=0;u8 old_face=p->facing;
    if(x<=8)allowed&=(u8)~4;
    if(x>=216)allowed&=(u8)~8;
    input&=allowed;
    if(input&4){p->facing=3;delta=prelude?-0x0100:-0x014D;}
    else if(input&8){p->facing=1;delta=prelude?0x0100:0x01AA;}
    if(old_face!=p->facing)player_anim_face(anim,p->facing);
    if(delta && move){add_x(p,delta);if(p->attached && other)add_x(other,delta);player_anim_tick(anim);}
}
static void step(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,u8 stopped,
                               ArcadeFloor floor,void *context,ArcadeFirePose *fire,ArcadePlayerAnimation *anim,u8 prelude)
{
    s16 adjust;
    if(!p->active){p->previous=input;return;}
    p->attached=(u8)(other && other->active==1 && other->state==5);
    switch(p->state)
    {
    case 0: /* $AA03: normal initialization after reentry, without a move. */
        p->active=1;p->attached=0;p->state++;if(anim)anim->timer=0;player_anim_tick(anim);break;
    case 1: /* $AA32: gravity probe, fresh climb input, horizontal move, seating. */
        p->y--;climb_command(p,(u8)(input & (u8)~p->previous));
        horizontal(p,other,input,camera,1,anim,prelude);
        adjust=floor(p->x,p->y,context);
        if(adjust!=ARCADE_NO_FLOOR)p->y=(u8)(p->y+adjust);
        else set_state(p,supported(p,other,&adjust)?5:3,0);
        break;
    case 2: /* $AB82: ascent, eight-pixel crest, then terrain/held-input recovery. */
        switch(p->phase)
        {
        case 0:p->stamp=p->y;p->phase=1; /* fall through */
        case 1:
            add_y(p,0x0240);if(p->attached && other)add_y(other,0x0240);
            adjust=floor(p->x,p->y,context);
            if(adjust!=ARCADE_NO_FLOOR){p->timer=8;p->phase++;}
            else if(rising_into(p,other)){set_state(p,3,2);p->timer=16;}
            else if((u8)(p->y-p->stamp)>32)set_state(p,3,0);
            break;
        case 2:shift_y(p,other,1);if(--p->timer==0)p->phase++;break;
        case 3:
            shift_y(p,other,-1);
            if(floor(p->x,p->y,context)!=ARCADE_NO_FLOOR)
            {shift_y(p,other,1);set_state(p,1,0);climb_command(p,input&0x30);}
            break;
        default:break;
        }
        horizontal(p,other,input,camera,0,anim,prelude);
        p->attached=(u8)(other && other->active==1 && other->state==5);break;
    case 3: /* $AC38: eight-pixel lift, sixteen-pixel drop, $02C0 falling. */
        switch(p->phase)
        {
        case 0:p->timer=8;p->phase=1; /* fall through */
        case 1:
            shift_y(p,other,1);
            if(--p->timer==0){p->phase++;p->timer=16;}break;
        case 2:shift_y(p,other,-1);if(--p->timer==0)p->phase++;break;
        case 3:
            add_y(p,-0x02C0);if(p->attached && other)add_y(other,-0x02C0);
            adjust=floor(p->x,p->y,context);
            if(adjust!=ARCADE_NO_FLOOR)
            {shift_y(p,other,adjust);set_state(p,1,0);climb_command(p,input&0x30);}
            else if(supported(p,other,&adjust))
            {p->y=(u8)(p->y+adjust);set_state(p,5,0);climb_command(p,input&0x30);}
            break;
        default:break;
        }
        horizontal(p,other,input,camera,0,anim,prelude);
        p->attached=(u8)(other && other->active==1 && other->state==5);break;
    case 4: /* $ACCE/$ACD5: final-life cleanup, two normal updates. */
        if(!p->phase){p->active=255;p->phase=1;}
        else{p->active=0;p->state=p->phase=0;}
        break;
    case 5: /* $ACDC: standing on the other player's shoulders. */
        climb_command(p,(u8)(input & (u8)~p->previous));
        horizontal(p,other,input,camera,1,anim,prelude);
        if(supported(p,other,&adjust))p->y=(u8)(other->y+14);
        else set_state(p,3,0);
        break;
    default:break;
    }
    /* $AB52 precedes $A9C5: clamps keep the fractional byte intact. */
    if((u16)(p->x-camera)<8){p->x=(u16)(camera+8);if(p->state!=5)player_anim_tick(anim);}
    else if((u16)(p->x-camera)>=208)p->x=(u16)(camera+208);
    if(fire){fire->x=p->x;fire->y=(u8)(p->y-(p->state==5?8:0));fire->facing=p->facing;}
    if(!stopped && p->state!=5 && !(input&p->previous&0x3C))
    {add_x(p,0x008D);if(p->attached && other)add_x(other,0x008D);player_anim_tick(anim);}
    p->previous=input;
}

/* 940A replaces AA16 during the title prelude: both horizontal magnitudes
 * are 0100; vertical 02C0/0240 remain the same. 9D=1 suppresses idle drift. */
void arcade_player_step_prelude(ArcadePlayer *p,ArcadePlayer *other,u8 input,
    ArcadeFloor floor,void *context,ArcadeFirePose *fire,ArcadePlayerAnimation *anim)
{step(p,other,input,0,1,floor,context,fire,anim,1);}

void arcade_player_step_stopped(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,u8 stopped,
    ArcadeFloor floor,void *context,ArcadeFirePose *fire,ArcadePlayerAnimation *anim)
{step(p,other,input,camera,stopped,floor,context,fire,anim,0);}

/* Original $A98A separation, preserving each fractional X byte. */
void arcade_players_separate(ArcadePlayer *p,ArcadePlayer *q)
{
    s16 dx,half;ArcadePlayer *hi,*lo;
    if(!p->active || !q->active || (u8)(p->y-q->y+8)>=16)return;
    if((u16)(p->x-q->x+12)>=24)return;
    dx=(s16)(p->x-q->x);
    if(dx<0){dx=(s16)-dx;hi=q;lo=p;}else{hi=p;lo=q;}
    half=(s16)((12-dx)/2);hi->x=(u16)(hi->x+half);lo->x=(u16)(lo->x-half);
}

void arcade_player_step(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,
                        ArcadeFloor floor,void *context)
{
    arcade_player_step_events(p,other,input,camera,floor,context,0);
}

void arcade_player_step_events(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,
    ArcadeFloor floor,void *context,ArcadeFirePose *fire)
{arcade_player_step_animated(p,other,input,camera,floor,context,fire,0);}

/* Running-scroll convenience entry retained for isolated motion callers. */
void arcade_player_step_animated(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,
    ArcadeFloor floor,void *context,ArcadeFirePose *fire,ArcadePlayerAnimation *anim)
{arcade_player_step_stopped(p,other,input,camera,0,floor,context,fire,anim);}
