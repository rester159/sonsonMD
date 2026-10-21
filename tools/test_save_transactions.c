#include "genesis.h"
#include "game.h"
#include "save.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
GameConfig g_config;u8 g_mode=1;
static u8 ram[1024],baseline[1024];static int budget=-1,gate,absent;static unsigned writes;
void SRAM_enable(void){gate=1;}void SRAM_enableRO(void){gate=2;}void SRAM_disable(void){gate=0;}
u8 SRAM_readByte(u32 offset){assert(offset<1024 && gate);return absent?255:ram[offset];}
void SRAM_writeByte(u32 offset,u8 value){assert(offset<1024 && gate==1);writes++;if(!absent && budget!=0){ram[offset]=value;if(budget>0)budget--;}}
void sync_from_dsw(void){}
int main(void)
{
 unsigned phase,n,checks=0;
 for(phase=0;phase<2;phase++) {
  memset(ram,255,sizeof ram);absent=0;budget=-1;save_init();save_apply_settings(1);
  g_config.homeCredits=5;save_capture_settings(1);assert(g_saveStatus==2);
  if(phase){g_config.homeCredits=3;save_capture_settings(1);assert(g_saveStatus==2);}
  memcpy(baseline,ram,1024);
  for(n=0;n<=513;n++) {
   memcpy(ram,baseline,1024);budget=-1;save_init();save_apply_settings(1);assert(g_config.homeCredits==(phase?3:5));
   budget=(int)n;g_config.homeCredits=9;save_capture_settings(1);
   budget=-1;save_init();save_apply_settings(1);
   assert(g_config.homeCredits==(n==513?9:(phase?3:5)));assert(gate==0);checks++;
  }
 }
 absent=1;save_init();save_apply_settings(1);g_config.homeCredits=9;save_capture_settings(1);assert(g_saveStatus==3 && gate==0);checks++;
 absent=0;memset(ram,255,1024);budget=-1;save_init();save_apply_settings(1);
 g_config.homeCredits=5;save_capture_settings(1);save_record_run(0x8001,123456,0);assert(save_commit());
 save_reset_options();assert(save_best_score(0x8001)==123456 && g_config.homeCredits==3);checks++;
 save_clear_scores();assert(save_best_score(0x8001)==0 && g_config.homeCredits==3);checks++;
 save_record_run(0xA000,0,12345);assert(save_commit());
 for(n=0;n<80;n++){save_record_run((u16)(0x8100+n),100+n,0);assert(save_commit());assert(save_best_score((u16)(0x8100+n))==100+n);}
 assert(save_best_ticks(0xA000)==12345);checks++;
 /* Future save schemas are not overwritten automatically. */
 ram[5]=2;ram[511]=0xA5;save_init();writes=0;g_config.homeCredits=9;save_capture_settings(1);assert(g_saveStatus==4 && writes==0);checks++;
 printf("%u transaction checks: every torn-write boundary on both slots, absent/read-only storage, reset isolation and newer-schema preservation passed\n",checks);return 0;
}
