#include "arcade_collision.h"
/* E8EA/E993: inclusive unsigned bounds, with byte Y and word X wrap. */
u8 arcade_gh_shot_overlap(u16 enemy_x,u8 enemy_y,u16 shot_x,u8 shot_y)
{
    return (u8)(enemy_y-shot_y+11)<=16 &&
           (u16)(enemy_x-shot_x+8)<=16;
}

/* Source family scan: first eligible overlapping record, in fixed slot order. */
u8 arcade_gh_shot_target(const ArcadeFoe *foes,u8 family,u16 shot_x,u8 shot_y)
{
    u8 k;
    const ArcadeFoe *a=foes+(family?6:0);
    for(k=0;k<6;k++,a++)
        if(a->body.active==1 && !(a->body.state&128) &&
           arcade_gh_shot_overlap(a->body.x,a->body.y,shot_x,shot_y))return k;
    return 255;
}

/* ECE9..ED2D, ordinary hostile contact (bit6 bonus objects are separate).
 * Entry requires player active1; enemy major state does not gate contact. */
u8 arcade_body_contact(u16 player_x,u8 player_y,u16 enemy_x,u8 enemy_y,
                       u8 player_active,u8 enemy_active)
{
    return player_active==1 && enemy_active && !(enemy_active&0x42) &&
           (u8)(player_y-enemy_y+7)<=14 &&
           (u16)(player_x-enemy_x+7)<=14;
}
