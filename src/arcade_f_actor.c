#include "arcade_f_actor.h"
#include "arcade_bonus_art.h"
static void animate(ArcadeFActor *a)
{
 const ArcadeFFrame *f;
 if(a->anim_timer){a->anim_timer--;return;}
 f=&arcade_f_frames[a->animation];a->code=f->code;a->attr=f->attr;
 a->anim_timer=f->timer;a->animation=f->next;
}
static void pose(ArcadeFActor *a,u16 index)
{a->animation=index;a->anim_timer=0;animate(a);}
static void add_x(ArcadeFActor *a,s32 delta)
{
 u32 x=(((u32)a->x<<8)|a->xf)+(u32)delta;
 a->x=(u16)(x>>8);a->xf=(u8)x;
}
static void edge(ArcadeFActor *a,u16 camera)
{if((u8)(a->x-camera)>=248){a->state=1;a->phase=2;}}
static void bonus_animate(ArcadeFActor *a)
{
 const ArcadeBonusFrame *f;
 if(a->anim_timer){a->anim_timer--;return;}
 f=&arcade_bonus_frames[a->animation];a->code=f->code;a->attr=f->attr;
 a->anim_timer=f->timer;a->animation=f->next;
}
void arcade_f_actor_step(ArcadeFActor *a,u16 camera,u8 stopped,u8 purge,
 u8 *live,u8 *next,u8 limit)
{
 if(!a->active)return;
 if(a->state==255)
 {
  if(!a->phase)
  {
   if(*next>10)return;
   a->phase++;a->active=(u8)((a->active&253)|64);
   a->bonus_kind=*next;*next=(u8)(*next+1);if(*next>limit)*next=0;
   a->animation=arcade_bonus_roots[a->bonus_kind];a->anim_timer=0;bonus_animate(a);
  }
  else if(a->phase==2){a->phase++;bonus_animate(a);if(a->owner)a->attr=0xc3;}
  edge(a,camera);
 }
 else if(!a->state)
 {
  if(!a->phase){pose(a,arcade_f_flight[a->direction==1?0:1]);a->half=0;++*live;a->phase++;}
  if(a->phase==1)
  {
   if(!a->delay){a->active=1;a->state=2;a->phase=0;}
   else{a->delay--;if(!stopped)add_x(a,141);}
  }
 }
 else if(a->state==1)
 {
  switch(a->phase)
  {
   case 0:pose(a,arcade_f_death[a->owner]);a->origin_y=32;a->phase++;break;
   case 1:if(--a->origin_y)animate(a);else a->phase++;break;
   case 2:a->active=255;a->phase++;break;
   case 3:a->active=a->state=a->phase=0;--*live;break;
   case 4:pose(a,arcade_f_score[a->owner]);a->origin_y=32;a->phase=1;break;
  }
 }
 else if(a->state==2)
 {
  if(!a->phase){a->delay=16;a->phase++;}
  if(a->phase==1)
  {
   const ArcadeFVector *v=&arcade_f_entry_vectors[a->direction];
   u16 y=(u16)((((u16)a->y<<8)|a->yf)+(u16)v->dy);
   add_x(a,v->dx);a->y=(u8)(y>>8);a->yf=(u8)y;animate(a);
   if(!--a->delay){a->state=3;a->phase=0;}
  }
 }
 else if(a->state==3)
 {
  if(!a->phase){a->amplitude=48;a->wave_phase=0;a->phase++;}
  if(a->phase==1)
  {
   add_x(a,arcade_f_cruise_vectors[a->direction]);
   a->y=arcade_f_wave_y(&a->wave_phase,&a->half,a->origin_y,a->amplitude);
   animate(a);edge(a,camera);
  }
 }
 if(purge && (u16)(a->x-camera-8)<=232){a->state=255;a->phase=0;}
}
