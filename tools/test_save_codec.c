#include "save_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static SavedData base, changed, decoded;
static unsigned char oldslot[512], newslot[512], trial[512];
int main(void)
{
    uint32_t gen;unsigned i,n,checks=0;
    assert(save_crc32((const uint8_t*)"123456789",9)==0xCBF43926u);checks++;
    base.arcade1=base.home1=0xDF;base.arcade2=base.home2=0xCB;base.homeCredits=3;
    base.records[0].key=0x804B;base.records[0].score=10000;
    arcade_ranking_init(&base.ranking);
    save_encode(oldslot,&base,0xFFFFFFFFu);
    assert(save_decode(oldslot,&decoded,&gen)==1 && gen==0xFFFFFFFFu && decoded.records[0].score==10000);checks++;
    assert(!memcmp(&decoded.ranking,&base.ranking,sizeof(base.ranking)));checks++;
    /* Reconstruct a genuine v1 payload: absent rankings, original length/CRC. */
    memcpy(trial,oldslot,512);trial[5]=1;trial[6]=1;trial[7]=69;
    memset(trial+337,0,170);
    {uint32_t crc=save_crc32(trial,507);for(i=0;i<4;i++)trial[507+i]=(uint8_t)(crc>>(24-i*8));}
    memset(&decoded,0x77,sizeof decoded);
    assert(save_decode(trial,&decoded,&gen)==1 && decoded.records[0].score==10000);
    assert(!memcmp(&decoded.ranking,&base.ranking,sizeof(base.ranking)));checks++;
    assert(save_newer(0,0xFFFFFFFFu) && !save_newer(5,5) && !save_newer(0x80000000u,0));checks++;
    changed=base;changed.homeCredits=9;changed.home2=0xA9;changed.records[0].score=123456;
    changed.records[1].key=0xA800;changed.records[1].ticks=36000;
    for(i=0;i<sizeof(changed.ranking);i++)((uint8_t *)&changed.ranking)[i]=(uint8_t)i;
    save_encode(newslot,&changed,0);
    assert(save_decode(newslot,&decoded,&gen)==1 && decoded.homeCredits==9 && decoded.records[1].ticks==36000);checks++;
    assert(!memcmp(&decoded.ranking,&changed.ranking,sizeof(changed.ranking)));checks++;
    /* Every interruption boundary: invalidate, 511 payload bytes, commit last.
     * The previous slot remains readable; the new one is never accepted early. */
    for(n=0;n<=513;n++) {
        memset(trial,0xFF,512);
        if(n)trial[511]=0;
        for(i=0;i<511 && i+1<n;i++)trial[i]=newslot[i];
        if(n==513)trial[511]=SAVE_COMMIT;
        int result=save_decode(trial,&decoded,&gen);
        assert((n==513 && result==1) || (n!=513 && result!=1));
        assert(save_decode(oldslot,&decoded,&gen)==1 && decoded.records[0].score==10000);checks++;
    }
    for(i=0;i<512;i++) {memcpy(trial,newslot,512);trial[i]^=1;assert(save_decode(trial,&decoded,&gen)!=1);checks++;}
    memset(trial,0,512);assert(!save_decode(trial,&decoded,&gen));checks++;
    memset(trial,255,512);assert(!save_decode(trial,&decoded,&gen));checks++;
    memcpy(trial,newslot,512);trial[5]=3;assert(save_decode(trial,&decoded,&gen)==-1);checks++;
    puts("CRC known vector, round trips, generation wrap, 514 torn writes, corruption and newer-schema preservation passed");
    printf("checks=%u\n",checks);return 0;
}
