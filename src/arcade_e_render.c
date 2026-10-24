#include "arcade_e_render.h"
#include "arcade_e_upload.h"
#include "arcade_bonus_art.h"
#include "arcade_cell_sat.h"
typedef char e_actor_stride_check[sizeof(ArcadeEActor)==20?1:-1];
typedef char e_actor_active_check[__builtin_offsetof(ArcadeEActor,active)==7?1:-1];
static u16 tile_for(u8 code,u8 attr)
{
    u16 tile=arcade_e_art(code,attr);u8 k;
    if(tile!=65535)return tile;
    for(k=0;k<22;k++)
    {
        const ArcadeBonusFrame *f=&arcade_bonus_frames[k];
        if(f->code!=code)continue;
        if((f->attr&63)==(attr&63))return f->tile;
        if((attr&63)==3 && f->owner2_tile!=65535)return f->owner2_tile;
    }
    return 65535;
}
u16 arcade_e_render_request(const ArcadeEActor *a)
{
    u16 token;
    if(!a->active || (a->active&128))return 65535;
    token=arcade_e_art(a->code,a->attr);
    return token!=65535 && (token&ARCADE_E_DYNAMIC)?token&32767:65535;
}
u16 arcade_e_render_actor(const ArcadeEActor *a,u8 slot,u16 ready_cell,
    u16 camera,u16 *sat,u16 first,u16 limit)
{
    static const ArcadeSpriteView view={0,1,256,224};
    ArcadeLargeCell cell;u16 tile,n;
    if(slot>=2 || !a->active || (a->active&128))return 0;
    tile=tile_for(a->code,a->attr);if(tile==65535)return 0;
    if(tile&ARCADE_E_DYNAMIC)
    {
        if(ready_cell!=(tile&32767))return 0;
        tile=slot?ARCADE_E_SLOT1:ARCADE_E_SLOT0;
    }
    cell.x=(u8)(a->x-camera);cell.y=(u8)(-a->y-16);
    cell.code=a->code;cell.attr=a->attr;
    n=arcade_cell_sat(&cell,tile,&view,sat,first,limit);
    return n==65535?0:n;
}
