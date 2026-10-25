#include <genesis.h>
#include "vram_words.h"
#define VRAM_WORDS_MAX 64
typedef struct {const u16 *src;u16 addr,count,stride;} VramWords;
static VramWords list[VRAM_WORDS_MAX];
static u16 used;
u8 vram_words_queue(const u16 *src,u16 addr,u16 count,u16 stride)
{
    VramWords *w;
    if(used>=VRAM_WORDS_MAX)return 0;
    w=&list[used++];w->src=src;w->addr=addr;w->count=count;w->stride=stride;
    return 1;
}
u8 vram_words_room(u16 n){return (u8)(used+n<=VRAM_WORDS_MAX);}
void vram_words_flush(void)
{
    vu32 *ctrl=(vu32*)VDP_CTRL_PORT;vu16 *data=(vu16*)VDP_DATA_PORT;
    const VramWords *w=list,*end=list+used;
    for(;w<end;w++)
    {
        const u16 *src=w->src;u16 addr=w->addr,n=w->count;
        while(n--){*ctrl=VDP_WRITE_VRAM_ADDR((u32)addr);*data=*src++;addr+=w->stride;}
    }
    used=0;
}
