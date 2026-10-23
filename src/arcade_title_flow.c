#include "arcade_title_flow.h"
static void queue(ArcadeTitleEvent *e,u16 command)
{
    /* Maximum: six state commands plus two credit-change commands. */
    e->commands[e->count++]=command;
}
static void clear_screen(ArcadeTitleEvent *e)
{
    queue(e,0x0119);e->flags|=TITLE_SET_BUSY;
}
static void ranking(ArcadeTitleEvent *e,u16 command)
{
    queue(e,0x1000);queue(e,command);queue(e,0x020a);
    queue(e,0x0338);queue(e,0x0700);
}
void arcade_title_step(ArcadeTitleFlow *s,const ArcadeTitleInput *in,ArcadeTitleEvent *e)
{
    e->flags=0;e->count=0;
    switch(s->state)
    {
        case 0: /* 933E */
            e->flags=TITLE_SEED_RNG;++s->state;break;
        case 1: /* 9348 */
            switch(s->phase)
            {
                case 0:
                    if(in->busy)break;
                    e->flags=TITLE_PRELUDE_WORLD;clear_screen(e);++s->phase;break;
                case 1:
                    if(in->busy)break;
                    queue(e,0x1000);queue(e,0x020a);queue(e,0x0338);
                    queue(e,0x0700);queue(e,0x1000);queue(e,0x020b);
                    ++s->phase;break;
                case 2:e->flags=TITLE_PRELUDE_PLAYERS;++s->phase;break;
                case 3:e->flags=TITLE_PRELUDE_DECOR;s->timer=10;++s->phase;break;
                case 4:
                    e->flags=TITLE_PRELUDE_TICK;
                    if(!(in->clock&63) && !--s->timer){++s->state;s->phase=0;}
                    break;
            }
            break;
        case 2: /* 9492 */
            e->flags=TITLE_DEMO_INIT|TITLE_RESET_SUBSTATES;
            s->demo_timer=30;++s->state;s->phase=0;break;
        case 3: /* 94B6: game update precedes timeout and credit change. */
            e->flags=TITLE_DEMO_TICK;
            if(!(in->clock&63) && !--s->demo_timer)
            {
                e->flags|=TITLE_CLEAR_SPRITES|TITLE_CLEAR_E6|TITLE_RESET_SUBSTATES;
                ++s->state;s->phase=0;
            }
            break;
        case 4: /* 94D5 */
            switch(s->phase)
            {
                case 0:
                    if(!in->busy){clear_screen(e);++s->phase;}
                    break;
                case 1:
                    if(!in->busy){ranking(e,0x0900);s->timer=(s->timer&0xff00)|240;++s->phase;}
                    break;
                case 2:
                    /* DEC 07 is byte arithmetic; 06 remains untouched. */
                    s->timer=(s->timer&0xff00)|(u8)(s->timer-1);
                    if(!(s->timer&255)){clear_screen(e);++s->phase;}
                    break;
                case 3:
                    if(!in->busy){ranking(e,0x0a00);s->timer=(s->timer&0xff00)|240;++s->phase;}
                    break;
                case 4:
                    s->timer=(s->timer&0xff00)|(u8)(s->timer-1);
                    if(!(s->timer&255))
                    {
                        s->state=(in->credits||in->free_play)?5:0;s->phase=0;
                        if(s->state)e->flags|=TITLE_RESET_0B;
                    }
                    break;
            }
            break;
        case 5: /* 9574 */
            switch(s->phase)
            {
                case 0:
                    if(in->busy)break;
                    e->flags=TITLE_CLEAR_SPRITES|TITLE_CLEAR_E6|TITLE_CLEAR_8B;
                    clear_screen(e);++s->phase;break;
                case 1:
                    if(in->busy)break;
                    e->flags=TITLE_CLEAR_SPRITES;
                    queue(e,0x020c);queue(e,0x020a);queue(e,0x1000);
                    queue(e,0x0700);queue(e,0x0324);queue(e,0x0e00);
                    s->timer=240;++s->phase;break;
                case 2:
                    if(! --s->timer){s->state=0;s->phase=0;}
                    break;
            }
            break;
    }
    /* 931E runs AFTER the selected handler, even if it waited on busy. */
    if(in->credits!=s->shown_credits)
    {
        s->shown_credits=in->credits;
        queue(e,0x0338);queue(e,0x0700);
        s->state=5;s->phase=0;e->flags|=TITLE_RESET_SUBSTATES;
    }
}
