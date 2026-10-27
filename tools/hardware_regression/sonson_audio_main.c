/* Linked with production sound.c/data in the audio regression fixture. */
#include <genesis.h>
#include "game.h"
#include "sound.h"
volatile u8 g_attract;
GameConfig g_config;
volatile u16 probe_case;
int main(bool hardReset)
{
    u16 i,j;
    const u8 events[]={4,8,12,3,15,0};
    (void)hardReset;
    snd_init();
    for(i=0;i<6;i++) {
        probe_case=i;
        snd_stop();snd_command(events[i]);
        for(j=0;j<360;j++) SYS_doVBlankProcess();
    }
    probe_case=6;snd_stop();snd_command(15);
    for(j=0;j<1200;j++) {if(j%60==0)snd_command(4);if(j%12==0)snd_command(8);SYS_doVBlankProcess();}
    probe_case=7;snd_stop();
    for(;;)SYS_doVBlankProcess();
}
