#include <genesis.h>
#include "arcade_food_world.h"
#include "arcade_world_object_pool.h"
#include "arcade_food_step.h"
#include "arcade_food_art.h"
#include "arcade_barrier_step.h"
#include "arcade_barrier_upload.h"
#include "arcade_barrier.h"
#include "frame_budget.h"
#include "vram_words.h"
static ArcadeWorldObjectPool pool;
static ArcadeFoodCounter counter;
static u8 writes[20],eaten[20];
static u8 render_pending;
static u8 barrier_cursor[20],barrier_writes[20],barrier_pose[20],barrier_next;
/* Immutable map sources survive deferred DMA and are shared by hardware slot. */
static const u16 barrier_maps[12][4]={
    {1985,1984,1987,1986},{1989,1988,1991,1990},
    {1993,1992,1995,1994},{1997,1996,1999,1998},
    {2001,2000,2003,2002},{2005,2004,2007,2006},
    {2009,2008,2011,2010},{2013,2012,2015,2014},
    {2017,2016,2019,2018},{2021,2020,2023,2022},
    {2025,2024,2027,2026},{2029,2028,2031,2030}
};
#include "arcade_food_columns.h"
static u16 screen;
ArcadeSpecialSpawn arcade_food_spawn;
ArcadePowerup arcade_food_pickup;
/* Ordered supersets of eligible contact records. Recheck flags in each scan:
 * contacts can retire records before the next query in the same source tick. */
static u8 food_slots[20],barrier_slots[20],food_count,barrier_count,candidates_dirty;
static void ensure_candidates(void)
{
    u8 i;
    if(!candidates_dirty)return;
    food_count=barrier_count=0;
    for(i=0;i<20;i++)
    {
        const ArcadeWorldObject *o=&pool.objects[i];
        if(!o->kind && (o->active&1))food_slots[food_count++]=i;
        else if(o->kind==2 && (o->active&3))barrier_slots[barrier_count++]=i;
    }
    candidates_dirty=0;
}
static void remember(u16 i,const ArcadeFoodDraw *draw)
{
    if(!draw->write)return;
    /* A second column may arrive while a first-column transfer is pending. */
    writes[i]|=draw->write;render_pending=1;
    eaten[i]=(pool.objects[i].phase==5);
}
void arcade_food_world_begin(void)
{
    u16 i;ArcadeFoodDraw draw;
    memset(&pool,0,sizeof(pool));
    memset(&counter,0,sizeof(counter));
    memset(&arcade_food_spawn,0,sizeof(arcade_food_spawn));
    memset(&arcade_food_pickup,0,sizeof(arcade_food_pickup));
    render_pending=0;candidates_dirty=1;
    memset(writes,0,sizeof(writes));memset(eaten,0,sizeof(eaten));screen=0;
    memset(barrier_cursor,0,sizeof(barrier_cursor));
    memset(barrier_writes,0,sizeof(barrier_writes));
    memset(barrier_pose,0,sizeof(barrier_pose));arcade_barrier_upload_reset();barrier_next=0;
    arcade_world_object_begin(&pool);
    for(i=0;i<20;i++)
    {
        arcade_food_redraw(&pool.objects[i],0,&draw);
        remember(i,&draw);
    }
}
void arcade_food_world_step(u16 camera,u8 advance)
{
    arcade_food_world_step_clock(camera,advance,0);
}
void arcade_food_world_step_clock(u16 camera,u8 advance,u8 clock)
{
    u16 i,edge=(u16)(camera+256);ArcadeFoodDraw draw;
    while(screen<(camera>>8))
    {
        u16 first=(u16)pool.quarter*4;
        if(arcade_world_object_load(&pool))
        {
            candidates_dirty=1;
            for(i=first;i<first+4;i++)
                writes[i]=eaten[i]=barrier_cursor[i]=barrier_writes[i]=barrier_pose[i]=0;
        }
        screen++;
    }
    for(i=0;i<20;i++)
    {
        ArcadeWorldObject *o=&pool.objects[i];
        if(o->kind==2)
        {
            u8 flash,old_active=o->active;
            arcade_barrier_step(o,&barrier_cursor[i],camera,advance,clock,&draw,&flash);
            if((old_active&3)!=(o->active&3))candidates_dirty=1;
            if(draw.write && o->food>=8 && o->food<44)
            {
                u8 pose=(u8)((o->food-8)*5+(draw.write==4?1+((0x4d-flash)>>2):0));
                /* Flash repeats four ticks; preserve pending work but do not
                 * requeue the unchanged immutable map every simulation tick. */
                if(draw.write!=4 || pose!=barrier_pose[i])
                    barrier_writes[i]|=draw.write==4?3:draw.write;
                if(barrier_writes[i])render_pending=1;
                barrier_pose[i]=pose;
            }
            continue;
        }
        /* Stable visible/eaten phases have no work until contact or recycling. */
        if(!o->active || o->kind || o->phase==3 || o->phase==5)continue;
        /* Phase0 can only enter once its object crosses the right edge.
         * Phases1/2 write columns only on the source column-advance tick. */
        if(o->phase==0 && (!o->y || edge<=o->x))continue;
        if((o->phase==1 || o->phase==2) && !advance)continue;
        {u8 old_active=o->active;
        arcade_food_step(o,camera,advance,&draw);
        if((old_active&1)!=(o->active&1))candidates_dirty=1;}
        remember(i,&draw);
    }
}
u8 arcade_food_world_player(ArcadePlayer *player,u16 camera)
{
    ensure_candidates();
    return arcade_barrier_player_indexed(&pool,player,camera,barrier_slots,barrier_count);
}
u16 arcade_food_world_shot(ArcadePlayerShot *shot)
{
    u8 next[20],candidate=0;u16 i,k;
    if(shot->active!=1)return 0;
    ensure_candidates();
    /* No object or shot changes during this broad-phase pass. Only geometric
     * hits consume next[], so unvisited entries are never read by the scan.
     * Preserve its full slot order and multiple-hit behavior once a hit exists. */
    for(k=0;k<barrier_count;k++)
    {
        const ArcadeWorldObject *o;
        i=barrier_slots[k];o=&pool.objects[i];
        if(!(o->active&2) || o->kind!=2 ||
           (u8)(o->y-shot->y+10)>20 || (u16)(o->x-shot->x+14)>22)continue;
        next[i]=arcade_barrier_next_phase(barrier_cursor[i]);candidate=1;
    }
    if(!candidate)return 0;
    /* Event042F resolves through4D84 to BCD00001000 at4EA6. */
    return (u16)arcade_barrier_shots_indexed(&pool,next,shot,barrier_slots,barrier_count)*1000;
}
u16 arcade_food_world_contact(u16 camera,u16 x,u8 y,u8 active,u8 owner,u8 grade)
{
    u16 i,k,points=0;
    if(active!=1)return 0;
    ensure_candidates();
    for(k=0;k<food_count;k++)
    {
        ArcadeWorldObject *o;
        i=food_slots[k];o=&pool.objects[i];
        ArcadeFoodTile tile;
        ArcadeFoodAward award;
        /* Reject dormant/non-food records before assembling a contact value. */
        if(!(o->active&1) || o->kind || (u16)(o->x-camera)>=256)continue;
        /* Exact E569 byte/word overlap windows, before constructing a contact
         * record. Rejected candidates have no effects in arcade_food_contact. */
        if((u8)(y-o->y+6)>12 || (u16)(x-o->x+6)>12)continue;
        tile.x=o->x;tile.active=o->active;tile.phase=o->phase;
        tile.y=o->y;tile.food=o->food;tile.kind=o->kind;
        if(arcade_food_contact(&tile,camera,x,y,active,owner,&award))
        {
            o->active=tile.active;o->phase=tile.phase;candidates_dirty=1;
            points+=award.points;
            arcade_food_collect(&counter,o->food,o->y,grade,&arcade_food_spawn,&arcade_food_pickup);
        }
    }
    return points;
}
/* Immutable pairs run from the upper map row to the lower map row.
 * A row-zero wrap crosses the plane boundary and must remain two transfers. */
static u8 food_column_dma(const u16 *pair,u16 row,u16 col,u8 cold)
{
    u16 base=VDP_getBGAAddress();
    if(row) {
        u16 address=base+2*((row-1)*64+col);
        if(cold){DMA_doDma(DMA_VRAM,(void*)pair,address,2,128);return 1;}
        return vram_words_queue(pair,address,2,128);
    }
    if(cold){
        DMA_doDma(DMA_VRAM,(void*)(pair+1),base+2*col,1,2);
        DMA_doDma(DMA_VRAM,(void*)pair,base+2*(31*64+col),1,2);return 1;
    }
    if(!vram_words_queue(pair+1,base+2*col,1,2))return 0;
    return vram_words_queue(pair,base+2*(31*64+col),1,2);
}

/* Barrier flashes are synchronized: every visible barrier can request a new
 * pose (one tile upload plus two map columns) on the same tick, which made
 * those refreshes the most frequent overrun in two-player play. They are
 * cosmetic and coalesce to the latest pose, so once the frame is late (main
 * loop already past this active line, with SAT construction still to come)
 * publish just one per refresh, rotating so no barrier starves. */
#define ARCADE_FOOD_LATE_LINE 150
void arcade_food_world_render(u8 phase,u8 cold)
{
    u16 i,n;u8 published=0;
    if(!render_pending)return;
    render_pending=0;
    for(n=0,i=barrier_next;n<20;n++,i=i==19?0:i+1)if(barrier_writes[i])
    {
        ArcadeWorldObject *o=&pool.objects[i];u16 k;
        u8 slot=arcade_barrier_slot[i],mask=barrier_writes[i];
        if(published && !cold && frame_past_line(ARCADE_FOOD_LATE_LINE)){render_pending=1;break;}
        /* Reserve all four map writes plus later frame work before changing art. */
        if((u32)DMA_getQueueSize()+8>DMA_getMaxQueueSize() ||
           !DMA_canQueue(DMA_VRAM,260)){render_pending=1;continue;}
        if(!arcade_barrier_upload(slot,barrier_pose[i],520,8)){render_pending=1;continue;}
        for(k=0;k<4;k+=2)if(mask&(k<2?1:2))
        {
            u16 col=((o->x>>3)+phase+(k>>1))&63;
            u16 row=(31-(o->y>>3))&31;
            if(!food_column_dma(&barrier_maps[slot][k],row,col,0))break;
        }
        if(k==4)barrier_writes[i]=0;else render_pending=1;
        published=1;barrier_next=(u8)(i==19?0:i+1);
    }
    for(i=0;i<20;i++)if(writes[i])
    {
        ArcadeWorldObject *o=&pool.objects[i];
        const u16 *tiles=food_columns[eaten[i]][(u8)(o->food+12)];
        u16 k,words=writes[i]==3?4:2;
        if(!cold && ((u16)(DMA_getQueueSize()+words)>DMA_getMaxQueueSize() ||
                     !DMA_canQueue(DMA_VRAM,words))){render_pending=1;continue;}
        for(k=0;k<4;k+=2)if(writes[i]&(k<2?1:2))
        {
            u16 col=((o->x>>3)+phase+(k>>1))&63;
            u16 row=(31-(o->y>>3))&31;
            if(!food_column_dma(&tiles[k],row,col,cold))break;
        }
        if(k==4)writes[i]=0;else render_pending=1;
    }
}

void arcade_food_world_stop(ArcadeStop *stop,u16 camera,u8 clock,ArcadeStopEvent *event)
{arcade_stop_step(stop,&pool,camera,clock,event);}
