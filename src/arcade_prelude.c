#include "arcade_prelude.h"
void arcade_prelude_players_begin(ArcadePrelude *s,ArcadeFloor floor,void *ctx,ArcadeFirePose fire[2])
{
    u8 i;
    for(i=0;i<2;i++)
    {
        ArcadePlayer *p=&s->players[i];
        p->active=1;p->state=p->phase=0;
        p->x=i?0x00d0:0x001c;p->y=0x88;
        /* A98A receives both current and previous source input bytes. */
        p->previous=s->input.previous[i];
    }
    arcade_attract_prelude_init(&s->input);
    for(i=0;i<2;i++)
        arcade_player_step_stopped(&s->players[i],&s->players[i^1],s->input.input[i],0,1,
                                  floor,ctx,fire?&fire[i]:0,&s->animation[i]);
    arcade_players_separate(&s->players[0],&s->players[1]);
}
void arcade_prelude_decor_begin(ArcadePrelude *s)
{
    u8 i;
    for(i=0;i<2;i++)
    {
        ArcadePreludeDecor *d=&s->decor[i];
        d->active=1;d->kind=8+i;d->state=d->phase=0;
        (void)arcade_pickup_step(d,0);
        d->state=2;d->phase=0;d->x=i?0x0090:0x005c;d->y=0xb4;
        /* B1DA/9617 executes the first pose;941A then selects state2.
         * B23D only animates negative kinds, so these poses remain fixed. */

    }
}
u8 arcade_prelude_step(ArcadePrelude *s,u8 clock,ArcadeFloor floor,void *ctx,ArcadeFirePose fire[2])
{
    u8 i;
    if(!arcade_attract_step(&s->input,clock))return 0;
    for(i=0;i<2;i++)
    {
        s->players[i].previous=s->input.previous[i];
        arcade_player_step_prelude(&s->players[i],&s->players[i^1],s->input.input[i],
                                  floor,ctx,fire?&fire[i]:0,&s->animation[i]);
    }
    arcade_players_separate(&s->players[0],&s->players[1]);
    for(i=0;i<2;i++)(void)arcade_pickup_step(&s->decor[i],0);
    return 1;
}

u8 arcade_prelude_collect(ArcadePrelude *s,ArcadePreludeRewards *r,ArcadeBonusAward events[2])
{
    u8 player,slot,count=0;
    /* E7D6/E7DF accepts any nonzero player-active value. Each object is
     * collected only at active1, so the first overlapping player owns it. */
    for(player=0;player<2;player++)if(s->players[player].active)
        for(slot=0;slot<2;slot++)
            if(arcade_pickup_collect(&s->decor[slot],s->players[player].x,s->players[player].y,
                player,r->combat,&r->collected,&r->spawn,&r->conversion,&events[count]))
            {r->combat=events[count].combat;++count;}
    return count;
}
