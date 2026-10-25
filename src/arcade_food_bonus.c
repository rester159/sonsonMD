#include <genesis.h>
#include "arcade_food_bonus.h"
#include "vram_words.h"
#include "arcade_food_world.h"
#include "arcade_food_art.h"
#include "arcade_bonus_art.h"
#include "arcade_cell_sat.h"
#include "arcade_powerup_upload.h"
ArcadeSpecialSpawn arcade_food_conversion_spawn;
ArcadePowerup arcade_food_conversion;
ArcadeOrb arcade_food_carrier;
u8 arcade_food_orb,arcade_food_conversion_pulse;
static u8 collected,dirty,rows[8],section_variant;
static u16 columns[8];
void arcade_food_bonus_reset(void)
{
    collected=dirty=arcade_food_orb=arcade_food_conversion_pulse=section_variant=0;
    arcade_powerup_upload_reset();
    memset(&arcade_food_carrier,0,sizeof(arcade_food_carrier));
    memset(&arcade_food_conversion_spawn,0,sizeof(arcade_food_conversion_spawn));
    memset(&arcade_food_conversion,0,sizeof(arcade_food_conversion));
}
void arcade_food_bonus_section(u8 variant)
{
    /* A7D0 writes only descriptor active/phase/placement and actor kind/grade.
     * Do not clear an existing actor or its owner/exit latch here. */
    arcade_food_spawn.placement=104;
    arcade_food_spawn.active=1;arcade_food_spawn.phase=0;
    arcade_food_pickup.kind=10;section_variant=variant;
}
static void spawn_step(ArcadeSpecialSpawn *spawn,ArcadePowerup *actor,u8 slot,
                       u16 camera,u8 blocked,u8 ready,u8 advance)
{
    ArcadeSpecialSpawnEvent event;u8 phase=spawn->phase;
    arcade_special_spawn_step(spawn,blocked,ready,advance,&event);
    if(event.write)
    {
        u8 index=(u8)(slot*4+phase-1);
        columns[index]=(camera>>3)+32;rows[index]=event.row;
        dirty|=(u8)(1u<<index);
    }
    if(event.spawn)
    {
        actor->active=1;actor->state=actor->phase=0;
        actor->placement=spawn->row;
    }
}
void arcade_food_bonus_step(u16 camera,u8 blocked,u8 ready,u8 advance)
{
    u8 orb;
    /* B0D1 visits0550 before0558; both precede B19A actor updates. */
    spawn_step(&arcade_food_spawn,&arcade_food_pickup,0,camera,blocked,ready,advance);
    spawn_step(&arcade_food_conversion_spawn,&arcade_food_conversion,1,camera,blocked,ready,advance);
    orb=arcade_food_pickup.kind==10?
        arcade_variant_pickup_step(&arcade_food_pickup,camera,section_variant):
        arcade_pickup_step(&arcade_food_pickup,camera);
    if(orb)
    {
        arcade_food_orb=orb;
        arcade_food_carrier.active=1;
        arcade_food_carrier.state=arcade_food_carrier.phase=0;
        arcade_food_carrier.variant=orb-1;
    }
    arcade_powerup_step(&arcade_food_conversion,camera);
}
void arcade_food_carrier_step(u16 camera)
{
    /* A61C follows the enemy actors; the parent is the persistent0708 slot. */
    arcade_orb_step(&arcade_food_carrier,&arcade_food_pickup,camera);
}
u8 arcade_food_bonus_contact(u16 x,u8 y,u8 owner,u16 combat,ArcadeBonusAward *award)
{
    if(arcade_food_pickup.kind==10)
        return arcade_variant_pickup_collect(&arcade_food_pickup,section_variant,
            x,y,owner,combat,&collected,&arcade_food_conversion_spawn,
            &arcade_food_conversion,award);
    return arcade_pickup_collect(&arcade_food_pickup,x,y,owner,combat,
        &collected,&arcade_food_conversion_spawn,&arcade_food_conversion,award);
}
void arcade_food_bonus_background(u8 plane_phase)
{
    u16 i;
    if(!dirty)return;
    for(i=0;i<8;i++)if(dirty&(1u<<i))
    {
        u16 k,col=(columns[i]+plane_phase)&63;
        if(!vram_words_room(2))continue;
        for(k=0;k<2;k++)
        {
            u16 row=(31-rows[i]-k)&31;
            if(!vram_words_queue(&arcade_food_spawn_tiles[i&3][k],
                VDP_getBGAAddress()+2*(row*64+col),1,2))break;
        }
        if(k==2)dirty&=(u8)~(1u<<i);
    }
}
static u16 ordinary_sat(u16 camera,u16 *sat,u16 first,u16 limit)
{
    const ArcadePowerup *a=&arcade_food_pickup;
    static const ArcadeSpriteView view={0,1,256,224};
    ArcadeLargeCell cell;u16 i;
    if(!a->active || (a->active&128))return 0;
    for(i=0;i<22;i++)if(arcade_bonus_frames[i].code==a->code)
    {
        const ArcadeBonusFrame *f=&arcade_bonus_frames[i];
        u16 tile=(a->attr==0xc3 && f->owner2_tile!=65535)?f->owner2_tile:f->tile;
        u16 n;
        cell.x=(u8)(a->x-camera);cell.y=(u8)(-a->y-16);
        cell.code=a->code;cell.attr=a->attr;
        n=arcade_cell_sat(&cell,tile,&view,sat,first,limit);
        return n==65535?0:n;
    }
    return 0;
}

static u16 uploaded_sat(u16 x,u8 y,u8 code,u8 attr,u8 art_slot,
                        u16 camera,u16 *sat,u16 first,u16 limit)
{
    u16 cells[3]={65535,65535,65535},n;
    ArcadeLargeCell cell;static const ArcadeSpriteView view={0,1,256,224};
    cells[art_slot]=arcade_powerup_art_cell(code,attr);
    if(cells[art_slot]==65535 || !arcade_powerup_upload(cells,512,1))return 0;
    cell.x=(u8)(x-camera);cell.y=(u8)(-y-16);cell.code=code;cell.attr=attr;
    n=arcade_cell_sat(&cell,ARCADE_POWERUP_VRAM_BASE+art_slot*4,&view,sat,first,limit);
    return n==65535?0:n;
}
u16 arcade_food_bonus_sat(u16 camera,u16 *sat,u16 first,u16 limit)
{
    u16 count=0;
    const ArcadeOrb *orb=&arcade_food_carrier;
    const ArcadePowerup *a=&arcade_food_conversion;
    /* FF3D visits0708 carrier,0728 pickup,0748 conversion. A deferred
     * art upload for one object must not suppress the other resident objects. */
    if(orb->active && !(orb->active&128))
        count+=uploaded_sat(orb->x,orb->y,orb->code,orb->attr,2,camera,sat,first+count,limit);
    count+=ordinary_sat(camera,sat,first+count,limit);
    if(a->active && !(a->active&128))
        count+=uploaded_sat(a->x,a->y,a->code,a->attr,1,camera,sat,first+count,limit);
    return count;
}
