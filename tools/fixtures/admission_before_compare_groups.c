#include "arcade_native_admission.h"
typedef char admission_input_size[sizeof(ArcadeAdmissionInput)==112?1:-1];
typedef char admission_hits_offset[__builtin_offsetof(ArcadeAdmissionCache,hits)==140?1:-1];
typedef char admission_misses_offset[__builtin_offsetof(ArcadeAdmissionCache,misses)==144?1:-1];
static u8 take(u8 active,u8 *remaining)
{
    if(!active || (active&128) || !*remaining)return 0;
    --*remaining;return 1;
}
static void entity(u8 slot,const u8 active[48],ArcadeNativeAdmission *out)
{
    if(take(active[slot],&out->remaining))
    out->order[out->count++]=slot;
}
void arcade_native_admission_cdef(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],const u8 d[4],const u8 e[2],const u8 f[4],ArcadeNativeAdmission *out)
{
    u8 k,r,head[7],next[24];
    out->remaining=reserved<24?(u8)(24-reserved):0;
    out->large=out->children=out->count=0;
    for(k=0;k<7;k++)head[k]=255;
    /* Build stable legacy-family lists with one reverse traversal. */
    for(k=24;k;)
    {
        --k;r=ranks[k];
        if(r>6 || !entities[24+k] || (entities[24+k]&128))continue;
        next[k]=head[r];head[r]=k;
    }
    /* Specials and players total at most five, so reserving them before shots
     * cannot change shot admission. Their actual drawing priority is separate. */
    for(k=16;k<20;k++)entity(k,entities,out);
    for(k=head[0];k!=255;k=next[k])entity((u8)(24+k),entities,out);
    for(k=0;k<16;k++)if(take(main[k],&out->remaining))
    out->order[out->count++]=(u8)(48+k);
    for(k=0;k<6;k++)if(take(c[k],&out->remaining))out->order[out->count++]=(u8)(64+k);
    for(r=1;r<=4;r++)
    {
        if(r==2)for(k=0;k<4;k++)if(take(d[k],&out->remaining))out->order[out->count++]=(u8)(70+k);
        if(r==3)for(k=0;k<2;k++)if(take(e[k],&out->remaining))out->order[out->count++]=(u8)(74+k);
        if(r==4)for(k=0;k<4;k++)if(take(f[k],&out->remaining))out->order[out->count++]=(u8)(76+k);
        for(k=head[r];k!=255;k=next[k])entity((u8)(24+k),entities,out);
    }
    for(k=0;k<12;k++)entity(k,entities,out); /* C28 G then CE8 H. */
    for(k=20;k<24;k++)entity(k,entities,out); /* DA8 I. */
    for(r=5;r<=6;r++)for(k=head[r];k!=255;k=next[k])entity((u8)(24+k),entities,out);
    for(k=0;k<3;k++)if(large[k] && out->remaining>=4)
    {out->large|=(u8)(1u<<k);out->remaining-=4;}
    for(k=0;k<4;k++)if(take(children[k],&out->remaining))
    {out->children|=(u8)(1u<<k);out->order[out->count++]=(u8)(12+k);}
}

void arcade_native_admission_cde(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],const u8 d[4],const u8 e[2],ArcadeNativeAdmission *out)
{
    static const u8 f[4]={0};
    arcade_native_admission_cdef(entities,ranks,main,reserved,large,children,c,d,e,f,out);
}

void arcade_native_admission_cd(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],const u8 d[4],ArcadeNativeAdmission *out)
{
    static const u8 e[2]={0};
    arcade_native_admission_cde(entities,ranks,main,reserved,large,children,c,d,e,out);
}

void arcade_native_admission_c(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],ArcadeNativeAdmission *out)
{
    static const u8 d[4]={0};
    arcade_native_admission_cd(entities,ranks,main,reserved,large,children,c,d,out);
}

/* Existing no-C callers retain their original contract. */
void arcade_native_admission(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    ArcadeNativeAdmission *out)
{
    static const u8 c[6]={0};
    arcade_native_admission_c(entities,ranks,main,reserved,large,children,c,out);
}

void arcade_native_admission_refresh(const ArcadeAdmissionInput *input,ArcadeAdmissionCache *cache)
{
    u16 k;
    if(cache->valid)
    {
        for(k=0;k<28;k++)if(input->words[k]!=cache->input.words[k])break;
        if(k==28){cache->hits++;return;}
    }
    for(k=0;k<28;k++)cache->input.words[k]=input->words[k];
    arcade_native_admission_cdef(input->state.entities,input->state.ranks,input->state.main,
        input->state.reserved,input->state.large,input->state.children,input->state.c,input->state.d,input->state.e,input->state.f,&cache->result);
    cache->valid=1;cache->misses++;
}
