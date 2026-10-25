#include <genesis.h>
#include "arcade_postgame.h"
#include "arcade_name_entry.h"
#include "arcade_name_map.h"
#include "arcade_name_art.h"
#include "arcade_title_map.h"
#include "arcade_title_art.h"
#include "arcade_bonus_art.h"
#include "arcade_pacing.h"
#include "entity.h"
#include "game.h"
#include "mode_controls.h"
#include "save.h"
#include "sound.h"
#include "frontend.h"
#include "ss_title.h"
#include "arcade_prelude_hud.h"
#include "arcade_prelude_hud_art.h"
#define BASE ARCADE_BONUS_TILE_BASE
/* Current game layout reserves the HUD font from tile1376. SGDK's
 * TILE_FONT_INDEX expands to runtime maps_addr and cannot be used in #if. */
#if BASE + ARCADE_TITLE_ART_TILES > 1376
#error Ranking art overlaps retained HUD font
#endif
u8 arcade_postgame_start;
/* Visible to diagnostics; zero outside this scene. */
volatile u8 arcade_postgame_phase;
static u16 map[1024];
static void row_publish(u8 row)
{
    u16 words[32],i;
    for(i=0;i<32;i++)words[i]=BASE+map[row*32+i];
    DMA_doDma(DMA_VRAM,words,VDP_getBGAAddress()+row*128,32,2);
}
static void background_publish(void)
{
    u8 row;for(row=5;row<31;row++)row_publish(row);
}
static void sprite_publish(const ArcadeNameEntry names[2],u8 players)
{
    static const ArcadeSpriteView view={0,1,256,224};
    u16 sat[8]={0},count=0,n;u8 p;
    for(p=0;p<2;p++)if(players&(1<<p))
    {
        n=arcade_name_cursor_sat(&names[p],p,BASE,&view,sat,count,2);
        if(n!=65535)count+=n;
    }
    if(count)sat[(count-1)*4+1]&=0xff00;
    DMA_doDma(DMA_VRAM,sat,VDP_getSpriteListAddress(),count?count*4:4,2);
}
static void scene_begin(void)
{
    VDP_setEnable(FALSE);DMA_flushQueue();DMA_waitCompletion();
    VDP_setHorizontalScroll(BG_A,0);VDP_setVerticalScroll(BG_A,0);
    VDP_clearTextAreaBG(BG_B,0,5,32,26);
}
static void ranking_draw(const ArcadeRanking *ranking,u8 total,u8 load)
{
    ArcadeTitleRankEntry entries[5];u8 row,i;u16 cell;
    scene_begin();
    if(load)VDP_loadTileData(arcade_title_pixels,BASE,ARCADE_TITLE_ART_TILES,DMA);
    for(cell=160;cell<992;cell++)map[cell]=ARCADE_TITLE_BLANK;
    for(row=0;row<5;row++)
    {
        const u8 *entry=total?ranking->total[row]:ranking->individual[row];
        for(i=0;i<4;i++)entries[row].score[i]=entry[i];
        for(i=0;i<8;i++)entries[row].name[i]=i<(total?8:4)?entry[4+i]:16;
    }
    arcade_title_map_ranking(map,total,entries);background_publish();
    {u16 empty[4]={0};DMA_doDma(DMA_VRAM,empty,VDP_getSpriteListAddress(),4,2);}
    VDP_setEnable(TRUE);
}
static void pack_score(u32 score,u8 out[4])
{
    s16 i;for(i=3;i>=0;i--){u8 low=score%10;score/=10;out[i]=(u8)((score%10)*16+low);score/=10;}
}
static u8 pad(u16 value)
{
    return ((value&(BUTTON_A|BUTTON_B))?1:0)|((value&BUTTON_LEFT)?4:0)|
           ((value&BUTTON_RIGHT)?8:0)|((value&BUTTON_UP)?16:0)|((value&BUTTON_DOWN)?32:0);
}
void arcade_postgame_run(u32 p1,u32 p2,u8 two)
{
    ArcadeRanking ranking;ArcadeRankingResult result;ArcadeNameEntry names[2]={{0}};
    ArcadeNameIdle idle={0};ArcadePacing *pacing;u8 scores[3][4],previous[2]={0},players,p,stage=0;
    u16 timer=0,shown=g_arcadeCredits;
    arcade_postgame_start=0;save_get_rankings(&ranking);
    pack_score(p1,scores[0]);pack_score(p2,scores[1]);pack_score(p1+p2,scores[2]);
    arcade_ranking_submit(&ranking,scores,two,&result);
    if(!(result.qualified[0]|result.qualified[1]|result.qualified[2]))return;
    players=(result.qualified[0]||result.qualified[2]?1:0)|(result.qualified[1]||result.qualified[2]?2:0);
    for(p=0;p<2;p++)
    {
        if(players&(1<<p))arcade_name_init(&names[p]);
        else names[p].raw[2]=1;
        previous[p]=pad(JOY_readJoypad(p?JOY_2:JOY_1));
    }
    scene_begin();VDP_loadTileData(arcade_name_pixels,BASE,ARCADE_NAME_TILES,DMA);
    arcade_name_map_begin(map,players);background_publish();sprite_publish(names,players);
    VDP_setEnable(TRUE);snd_init();snd_command(0x13);
    pacing=arcade_pacing_enter(SYS_isPAL(),(u16)vtimer);
    arcade_postgame_phase=1;
    for(;;)
    {
        u8 current[2],done=0;
        SYS_doVBlankProcess();JOY_update();(void)mode_input_poll();
        current[0]=pad(JOY_readJoypad(JOY_1));current[1]=pad(JOY_readJoypad(JOY_2));
        if(shown!=g_arcadeCredits)
        {
            char text[7];shown=g_arcadeCredits;arcade_credit_text(shown,text);VDP_drawText(text,19,2);
        }
        arcade_pacing_observe(pacing,(u16)vtimer,0);
        while(arcade_pacing_take(pacing))
        {
            entity_arcade_clock_advance();
            if(!(*(volatile u8 *)&g_config.dsw2&128))continue;
            if(stage==0)
            {
                u8 paint;
                /* 9E01's participation order, including the no-total branch. */
                if(result.qualified[0]||result.qualified[2])
                {
                    paint=arcade_name_step(&names[0],current[0],previous[0]);
                    arcade_name_map_update(map,0,&names[0],paint);
                    if(!result.qualified[2])names[1].raw[2]=1;
                }
                else names[0].raw[2]=1;
                paint=arcade_name_step(&names[1],current[1],previous[1]);
                arcade_name_map_update(map,1,&names[1],paint);
                if(names[0].raw[2] && names[1].raw[2])stage=1;
                if(arcade_name_idle_step(&idle,entity_arcade_clock_full(),current[0],current[1],two))stage=1;
                row_publish(23);sprite_publish(names,players);
            }
            else if(stage==1)
            {
                u8 entered[2][4];for(p=0;p<2;p++){u8 i;for(i=0;i<4;i++)entered[p][i]=names[p].name[i];}
                arcade_ranking_names(&ranking,&result,entered);stage=2;
            }
            else if(stage==2){ranking_draw(&ranking,0,1);snd_command(0x15);timer=180;stage=3;arcade_postgame_phase=2;}
            else if(stage==3){if(! --timer)stage=4;}
            else if(stage==4){ranking_draw(&ranking,1,0);timer=180;stage=5;arcade_postgame_phase=3;}
            else if(! --timer){done=1;break;}
            previous[0]=current[0];previous[1]=current[1];
            /* 9D60 invokes Start purchase after the name/ranking handler.
             * Hand off to the existing requested controller-specific owner. */
            if(mode_input_starts_held() && (g_arcadeCredits || !(g_config.dsw1&15)))
            {arcade_postgame_start=mode_input_starts_held();done=1;break;}
        }
        if(done)break;
    }
    save_set_rankings(&ranking);snd_shutdown();arcade_postgame_phase=0;
}

/* Title rankings own all patterns below the plane tables. Keep the source
 * header, ranking art and dynamic HUD in disjoint ranges (1501 tiles). */
#if ARCADE_TITLE_ART_TILES + SS_TITLE_CHAR_TILE_COUNT + ARCADE_PRELUDE_HUD_TILES > 1536
#error Title ranking patterns overlap plane tables
#endif
void arcade_title_ranking_draw(u8 total)
{
    enum { HEADER_BASE=ARCADE_TITLE_ART_TILES,
           HUD_BASE=HEADER_BASE+SS_TITLE_CHAR_TILE_COUNT };
    ArcadeRanking ranking;ArcadeTitleRankEntry entries[5];
    u16 row,col,words[32],score[8],credit[32],prompt[32],empty[4]={0};u8 i;
    save_get_rankings(&ranking);
    VDP_setEnable(FALSE);DMA_flushQueue();DMA_waitCompletion();
    VDP_clearPlane(BG_A,TRUE);VDP_clearPlane(BG_B,TRUE);
    VDP_setHorizontalScroll(BG_A,0);VDP_setVerticalScroll(BG_A,0);
    PAL_setColors(0,ss_pal_char,16,CPU);
    VDP_loadTileData(arcade_title_pixels,0,ARCADE_TITLE_ART_TILES,DMA);
    VDP_loadTileData(ss_title_tiles,HEADER_BASE,SS_TITLE_CHAR_TILE_COUNT,DMA);
    VDP_loadTileData(arcade_prelude_hud_pixels,HUD_BASE,ARCADE_PRELUDE_HUD_TILES,DMA);
    for(col=0;col<1024;col++)map[col]=ARCADE_TITLE_BLANK;
    for(row=0;row<5;row++) {
        const u8 *entry=total?ranking.total[row]:ranking.individual[row];
        for(i=0;i<4;i++)entries[row].score[i]=entry[i];
        for(i=0;i<8;i++)entries[row].name[i]=i<(total?8:4)?entry[4+i]:16;
    }
    arcade_title_map_ranking(map,total,entries);
    for(row=0;row<28;row++) {
        for(col=0;col<32;col++) {
            u16 source=(row+1)*32+col;
            words[col]=row<3?(ss_title_nametable[source]&0xf800)|
                ((ss_title_nametable[source]&0x7ff)+HEADER_BASE):map[source];
        }
        DMA_doDma(DMA_VRAM,words,VDP_getBGAAddress()+row*128,32,2);
    }
    arcade_prelude_hud(save_mode_high(MODE_ARCADE),g_arcadeCredits,
                       !(g_config.dsw1&15),0,score,credit,prompt);
    for(col=0;col<8;col++)score[col]+=HUD_BASE-ARCADE_PRELUDE_HUD_BASE;
    for(col=0;col<32;col++)credit[col]+=HUD_BASE-ARCADE_PRELUDE_HUD_BASE;
    DMA_doDma(DMA_VRAM,score,VDP_getBGAAddress()+128+26,8,2);
    DMA_doDma(DMA_VRAM,credit,VDP_getBGAAddress()+27*128,32,2);
    DMA_doDma(DMA_VRAM,empty,VDP_getSpriteListAddress(),4,2);
    VDP_setEnable(TRUE);
}
