#include <genesis.h>
#include "save.h"
#include "save_codec.h"
#include "game.h"
#include "frontend.h"
/* Odd-byte SRAM 0x200001..0x20FFFF, two 512-byte logical slots. All game,
 * SDK, constants and sound reside below 2 MiB; SRAM overlays no live resource.
 * Writes occur at confirmed menus/results, never during simulation. */
static SavedData data, decoded;
static uint8_t bytes[512], verify[512];
static uint32_t generation;
static s8 active = -1;
static u8 dirty, locked;
volatile u8 g_saveStatus; /* 0 defaults, 1 loaded, 2 saved, 3 unavailable, 4 newer schema */
static void defaults(void)
{
    u16 i;data.arcade1=data.home1=DSW1_DEFAULT;data.arcade2=data.home2=DSW2_DEFAULT;data.homeCredits=3;
    for(i=0;i<SAVE_RECORDS;i++) {data.records[i].key=0;data.records[i].score=0;data.records[i].ticks=0;}
    arcade_ranking_init(&data.ranking);
}
static void read_slot(u8 slot,uint8_t *p)
{
    u16 i;SRAM_enableRO();for(i=0;i<512;i++)p[i]=SRAM_readByte((u32)slot*512+i);SRAM_disable();
}
void save_init(void)
{
    uint32_t gen=0;u8 i;defaults();active=-1;generation=0;dirty=locked=0;g_saveStatus=0;
    for(i=0;i<2;i++) {
        int valid;read_slot(i,bytes);
        /* Validate into a separate model so an older slot cannot replace current. */
        valid=save_decode(bytes, &decoded,&gen);
        if(valid<0)locked=1;
        if(valid==1 && (active<0 || save_newer(gen,generation))) {
            save_decode(bytes,&data,&gen);generation=gen;active=(s8)i;g_saveStatus=1;
        }
    }
    if(locked)g_saveStatus=4;
}
void save_apply_settings(u8 mode)
{
    g_config.dsw1=mode==MODE_HOME?data.home1:data.arcade1;
    g_config.dsw2=mode==MODE_HOME?data.home2:data.arcade2;
    g_config.homeCredits=data.homeCredits;sync_from_dsw();
}
void save_capture_settings(u8 mode)
{
    u8 *a=mode==MODE_HOME?&data.home1:&data.arcade1;
    u8 *b=mode==MODE_HOME?&data.home2:&data.arcade2;
    if(*a!=g_config.dsw1 || *b!=g_config.dsw2 || data.homeCredits!=g_config.homeCredits) {
        *a=g_config.dsw1;*b=g_config.dsw2;data.homeCredits=g_config.homeCredits;dirty=1;
    }
    save_commit();
}
bool save_commit(void)
{
    u8 slot=active==0?1:0;u16 i;uint32_t gen;
    if(locked)return FALSE;if(!dirty)return TRUE;
    save_encode(bytes,&data,generation+1);
    SRAM_enable();SRAM_writeByte((u32)slot*512+511,0);
    if(SRAM_readByte((u32)slot*512+511)!=0) {SRAM_disable();g_saveStatus=3;return FALSE;}
    for(i=0;i<511;i++)SRAM_writeByte((u32)slot*512+i,bytes[i]);
    for(i=0;i<511;i++)verify[i]=SRAM_readByte((u32)slot*512+i);
    SRAM_disable();
    for(i=0;i<511;i++)if(bytes[i]!=verify[i]) {g_saveStatus=3;return FALSE;}
    /* Check the intended transaction with the mapping closed before publication. */
    verify[511]=SAVE_COMMIT;
    if(save_decode(verify,&decoded,&gen)!=1) {g_saveStatus=3;return FALSE;}
    SRAM_enable();SRAM_writeByte((u32)slot*512+511,SAVE_COMMIT);i=SRAM_readByte((u32)slot*512+511);SRAM_disable();
    if(i!=SAVE_COMMIT) {g_saveStatus=3;return FALSE;}
    active=(s8)slot;generation=gen;dirty=0;g_saveStatus=2;return TRUE;
}
u16 save_run_key(u8 mode,u8 players,u8 survival)
{
    u16 kind=survival?2:(mode==MODE_HOME?1:0);
    u16 rules=survival?0:(g_config.dsw2&0x7B);
    u16 credits=(mode==MODE_HOME && !survival)?g_config.homeCredits:0;
    return (u16)(0x8000|(kind<<12)|((players-1)<<11)|(credits<<7)|rules);
}
static SavedRecord *record(u16 key)
{
    u16 i;for(i=0;i<SAVE_RECORDS;i++)if(data.records[i].key==key)return &data.records[i];return 0;
}
u32 save_best_score(u16 key) {SavedRecord *r=record(key);return r?r->score:0;}
u32 save_best_ticks(u16 key) {SavedRecord *r=record(key);return r?r->ticks:0;}
u32 save_mode_high(u8 mode)
{
    u16 i;u32 high=10000;for(i=0;i<SAVE_RECORDS;i++)if(data.records[i].key && ((data.records[i].key>>12)&7)==mode && data.records[i].score>high)high=data.records[i].score;return high;
}
void save_record_run(u16 key,u32 score,u32 ticks)
{
    SavedRecord *r=record(key);u16 i;
    if(!score && !ticks)return;
    if(!r) {
        for(i=0;i<SAVE_RECORDS;i++)if(!data.records[i].key) {r=&data.records[i];break;}
        if(!r)
        {
            u16 victim=0;
            /* Keep the two survival-time boards while rotating old regular
             * configurations; a full table must not refuse the current run. */
            for(i=0;i<SAVE_RECORDS;i++)if(((data.records[i].key>>12)&7)!=2) {victim=i;break;}
            for(i=victim;i+1<SAVE_RECORDS;i++)data.records[i]=data.records[i+1];
            r=&data.records[SAVE_RECORDS-1];
        }
        r->key=key;r->score=r->ticks=0;
    }
    if(score>r->score) {r->score=score;dirty=1;}
    if(ticks>r->ticks) {r->ticks=ticks;dirty=1;}
}
void save_reset_options(void)
{
    data.arcade1=data.home1=DSW1_DEFAULT;data.arcade2=data.home2=DSW2_DEFAULT;data.homeCredits=3;dirty=1;
    save_apply_settings(g_mode);save_commit();
}
void save_clear_scores(void)
{
    u16 i;for(i=0;i<SAVE_RECORDS;i++) {data.records[i].key=0;data.records[i].score=data.records[i].ticks=0;}
    arcade_ranking_init(&data.ranking);dirty=1;save_commit();
}
void save_get_rankings(ArcadeRanking *out) {*out=data.ranking;}
void save_set_rankings(const ArcadeRanking *ranking)
{
    u16 i;const u8 *a=(const u8 *)ranking,*b=(const u8 *)&data.ranking;
    for(i=0;i<sizeof(*ranking);i++)if(a[i]!=b[i]){data.ranking=*ranking;dirty=1;return;}
}
