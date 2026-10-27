#include <genesis.h>
#include "ss2_sound.h"
volatile u16 probe_case, probe_frame;
int main(bool hardReset) {
    u16 id, f;
    (void)hardReset;
    ss2_sound_init();
    for (id=0; id<43; id++) {
        probe_case=id;
        ss2_sound_request(SS2_SND_STOP_ALL);
        ss2_sound_tick(); SYS_doVBlankProcess();
        ss2_sound_request((u8)id);
        for(f=0;f<360;f++) {
            probe_frame=f;
            ss2_sound_tick(); SYS_doVBlankProcess();
        }
    }
    probe_case=43; ss2_sound_request(3);
    for(f=0;f<3600;f++) {
        probe_frame=f;
        if(f%60==0)ss2_sound_request(0x19);
        if(f%120==15)ss2_sound_request(0x16);
        if(f%180==30)ss2_sound_request(0x15);
        ss2_sound_tick(); SYS_doVBlankProcess();
    }
    ss2_sound_request(SS2_SND_STOP_ALL);
    for(f=0;f<180;f++){ss2_sound_tick();SYS_doVBlankProcess();}
    probe_case=44;
    while(TRUE)SYS_doVBlankProcess();
}
