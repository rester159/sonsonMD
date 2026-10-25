#include "arcade_prelude_render.h"
#include "arcade_life.h"
u16 arcade_prelude_sat(const ArcadePrelude *s,const ArcadeSpriteView *view,u16 *sat,u16 first,u16 limit)
{
    u16 scratch[48],count=0,i,n,tile;ArcadeLargeCell cell;
    if(first>limit)return 65535;
    /* At most three wrap copies per cell. Stage all four source cells so a
     * capacity failure cannot leave a partially replaced scene. */
    for(i=0;i<4;i++)
    {
        if(i<2)
        {
            const ArcadePlayer *p=&s->players[i];
            const ArcadePlayerAnimation *a=&s->animation[i];
            if(!p->active || (p->active&128))continue;
            cell.x=(u8)p->x;cell.y=(u8)(-p->y-16);cell.code=a->code;cell.attr=a->attr;
            tile=arcade_life_tile(a->code,a->attr);if(!tile)return 65535;
        }
        else
        {
            const ArcadePreludeDecor *d=&s->decor[i-2];const ArcadeBonusFrame *f;
            if(!d->active || (d->active&128))continue;
            u16 pose;
            for(pose=0;pose<22;pose++)if(arcade_bonus_frames[pose].code==d->code)break;
            if(pose==22)return 65535;
            f=&arcade_bonus_frames[pose];
            cell.x=(u8)d->x;cell.y=(u8)(-d->y-16);cell.code=d->code;cell.attr=d->attr;
            tile=(d->attr==0xc3 && f->owner2_tile!=65535)?f->owner2_tile:f->tile;
        }
        n=arcade_cell_sat(&cell,tile,view,scratch,count,12);
        if(n==65535)return 65535;
        count+=n;
    }
    if(count>limit-first)return 65535;
    for(i=0;i<count;i++)
    {
        u16 at=(first+i)*4;
        sat[at]=scratch[i*4];sat[at+1]=(scratch[i*4+1]&0xff00)|(first+i+1);
        sat[at+2]=scratch[i*4+2];sat[at+3]=scratch[i*4+3];
    }
    return count;
}
