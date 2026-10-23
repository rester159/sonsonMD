#ifndef ARCADE_NATIVE_ADMISSION_H
#define ARCADE_NATIVE_ADMISSION_H
#include "arcade_types.h"
/* Native identities: entity0..47, main48..63, C64..69,D70..73,E74..75,F76..79; large/sharedK use separate masks.
 * Legacy family ranks: A0,C1,D2,E3,F4,orb5,unrecovered6. */
typedef struct {u8 large,children,remaining;u8 order[24],count;} ArcadeNativeAdmission;
void arcade_native_admission(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    ArcadeNativeAdmission *out);
void arcade_native_admission_c(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],ArcadeNativeAdmission *out);
void arcade_native_admission_cd(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],const u8 d[4],ArcadeNativeAdmission *out);
void arcade_native_admission_cde(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],const u8 d[4],const u8 e[2],ArcadeNativeAdmission *out);
void arcade_native_admission_cdef(const u8 entities[48],const u8 ranks[24],
    const u8 main[16],u8 reserved,const u8 large[3],const u8 children[4],
    const u8 c[6],const u8 d[4],const u8 e[2],const u8 f[4],ArcadeNativeAdmission *out);
/* Exactly112 bytes; union alignment permits native word comparisons on68000. */
typedef union {
    struct {u8 entities[48],ranks[24],main[16],large[3],children[4],reserved,c[6],d[4],e[2],f[4];} state;
    u32 words[28];
} ArcadeAdmissionInput;
typedef struct {
    ArcadeAdmissionInput input;
    ArcadeNativeAdmission result;
    u32 hits,misses;
    u8 valid;
} ArcadeAdmissionCache;
/* Cache owns result; callers must not mutate it. Zero-initialize once. */
void arcade_native_admission_refresh(const ArcadeAdmissionInput *input,ArcadeAdmissionCache *cache);
#endif
