#include "arcade_name_entry.h"
/* Original A214, all selectable characters before BACK at52. */
static const u8 characters[82]={
0x2f,0x10,0x11,0x10,0x12,0x10,0x13,0x10,0x14,0x10,0x15,0x10,0x16,0x10,0x17,0x10,
0x18,0x19,0x10,0x1a,0x10,0x1b,0x10,0x1c,0x10,0x1d,0x10,0x1e,0x10,0x1f,0x10,0x20,
0x10,0x21,0x22,0x10,0x23,0x10,0x24,0x10,0x25,0x10,0x26,0x10,0x27,0x10,0x28,0x10,
0x29,0x10,0x2a,0x00,0x10,0x01,0x10,0x02,0x10,0x03,0x10,0x04,0x10,0x0b,0x10,0x0d,
0x10,0x2c,0x10,0x3f,0x05,0x10,0x06,0x10,0x07,0x10,0x08,0x10,0x09,0x10,0x0e,0x10,
0x2b,0x10};
void arcade_name_init(ArcadeNameEntry *s)
{
    u8 i;
    s->raw[0]=s->raw[1]=s->raw[2]=s->raw[3]=s->raw[7]=s->raw[8]=0;
    s->raw[6]=0x54;
    for(i=0;i<4;i++)s->name[i]=0x3f;
}
static void move(ArcadeNameEntry *s,u8 direction)
{
    u8 n=s->raw[6];
    if(direction==8){n+=2;if(n>84)n=0;}
    else if(direction==4){n-=2;if(n&128)n=84;}
    else if(direction==32){u8 next=n+17;if(next<=84)n=next;}
    else {u8 next=n-17;if(!(next&128))n=next;}
    s->raw[6]=n;
}
static void directions(ArcadeNameEntry *s,u8 current,u8 previous)
{
    static const u8 bits[4]={8,4,32,16};
    static const u8 phases[4]={3,0,7,8},timers[4]={5,4,9,10};
    u8 i;
    for(i=0;i<4;i++)
    {
        u8 bit=bits[i],phase=phases[i],timer=timers[i];
        if(!s->raw[phase])
        {
            if((current&~previous)&bit)move(s,bit);
            else if((current&previous)&bit){s->raw[phase]=1;s->raw[timer]=20;}
        }
        else if((current&previous)&bit)
        {
            if(! --s->raw[timer]){move(s,bit);s->raw[timer]=10;}
        }
        else --s->raw[phase];
    }
}
u8 arcade_name_step(ArcadeNameEntry *s,u8 current,u8 previous)
{
    u8 paint=0;
    if(!s->raw[2])
    {
        if((current&~previous)&1)
        {
            u8 pos=s->raw[1],choice=s->raw[6];
            if(choice<82)
            {
                s->name[pos]=characters[choice];paint=(u8)(1<<pos);
                if(pos==3)s->raw[2]=1;
                else {s->raw[1]=pos+1;directions(s,current,previous);}
            }
            else
            {
                s->name[pos]=0x3f;
                if(choice!=82)s->raw[2]=1;
                else
                {
                    if(pos)--s->raw[1];
                    directions(s,current,previous);paint=(u8)(1<<s->raw[1]);
                }
            }
        }
        else directions(s,current,previous);
    }
    /* 9FF9 adjusts the selector as a side effect of cursor positioning.
     * B is remainder; the second branch compares after DECB, i.e. remainder1. */
    if(s->raw[6]%17==15)++s->raw[6];
    else if(s->raw[6]%17==1)--s->raw[6];
    return paint;
}
u8 arcade_name_idle_step(ArcadeNameIdle *s,u16 clock,u8 p1,u8 p2,u8 two)
{
    u8 moving=(p1|(two?p2:0))&0x3c;
    if(s->phase==0)
    {
        if(!moving){s->deadline=clock+0x0d48;s->phase=1;}
    }
    else if(s->phase==1)
    {
        if(moving)s->phase=0;
        else if(clock==s->deadline)s->phase=2;
    }
    else {s->phase=0;return 1;}
    return 0;
}
