#include "arcade_d_render.h"
#include "arcade_d_art.h"
#include "arcade_bonus_art.h"
#include "arcade_cell_sat.h"
/* Runtime instrumentation uses these target ABI offsets. */
typedef char d_actor_stride_check[sizeof(ArcadeDActor)==22?1:-1];
typedef char d_actor_active_check[__builtin_offsetof(ArcadeDActor,active)==9?1:-1];
static u16 tile_for(u8 code,u8 attr)
{
    u16 tile=arcade_d_tile(code,attr);u8 k;
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
u16 arcade_d_render_actor(const ArcadeDActor *a,u16 camera,u16 *sat,u16 first,u16 limit)
{
    static const ArcadeSpriteView view={0,1,256,224};
    ArcadeLargeCell cell;u16 tile,n;
    if(!a->active || (a->active&128))return 0;
    tile=tile_for(a->code,a->attr);if(tile==65535)return 0;
    cell.x=(u8)(a->x-camera);cell.y=(u8)(-a->y-16);
    cell.code=a->code;cell.attr=a->attr;
    n=arcade_cell_sat(&cell,tile,&view,sat,first,limit);
    return n==65535?0:n;
}
