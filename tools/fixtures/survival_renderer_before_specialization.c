static __attribute__((noinline)) u16 survival_render_reference(u16 *sat, u16 firstSlot)
{
    /* Mode changes only in game_init, never in rendering or the sound ISR. */
    const u8 survival=g_survivalRun;
    u16 count  = 0;            /* entity BODY sprites (capped at ENTITY_SAT_MAX) */
    u16 glyphs = 0;            /* popup glyph sprites (capped at POPUP_GLYPH_MAX)*/
    u16 slot   = firstSlot;
    u16 i,visit;u8 players_written=0;
    if(!survival)
    {
        entity_sprite_admission();
        slot+=arcade_food_bonus_sat((u16)g_game.cameraX,sat,slot,
            MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX);
        count=(u16)(slot-firstSlot); /* Wrapped cells share the body budget. */
        /* Hidden/inactive actors cannot request dynamic art. Empty requests
         * leave both upload residency and ready-cell identities unchanged. */
        if((arcade_e.actors[0].active && arcade_e.actors[0].active<128) ||
           (arcade_e.actors[1].active && arcade_e.actors[1].active<128))
        {
            u16 cells[2]={65535,65535};u8 k;
            for(k=0;k<arcade_admission.count;k++)
            {
                u8 id=arcade_admission.order[k];
                if(id>=74 && id<76)cells[id-74]=arcade_e_render_request(&arcade_e.actors[id-74]);
            }
            if(arcade_e_upload(cells,704,2))
            {
                for(k=0;k<2;k++)if(cells[k]!=65535)arcade_e_ready[k]=cells[k];
            }
            else arcade_e_ready[0]=arcade_e_ready[1]=65535;
        }
    }

    /* ONE pass over the pool: entity bodies (16x16) AND floating "+N" score-popup
     * glyphs (1x1 font-tile sprites), built into one linked SAT chain.
     *   - Bodies use PAL1 + the entity's baked tile (capped at ENTITY_SAT_MAX).
     *   - Popups carry NO body tile; they emit cached glyph sprites pointing at
     *     the SGDK font (TILE_FONT_INDEX + (ch-32), PAL3 -- the font palette
     *     game.c set up). The "+N" text was formatted ONCE at spawn (e->popupStr),
     *     so this does zero per-frame intToStr/strlen. Riding the same SAT DMA,
     *     the popups cost no active-display VRAM writes (the arcade's classK
     *     $0ED8 score-child, sub_CBC8 anim, re-expressed cheaply).
     * A single loop avoids a second 48-slot scan. */
    for (visit=0;visit<(survival?ENTITY_MAX:arcade_admission.count);visit++)
    {
        Entity *e;
        s16 sx, sy;u16 body_tile;u8 body_facing;
        i=survival?visit:arcade_admission.order[visit];
        if(!survival && !players_written && (i<16 || i>=20))
        {
            slot+=game_render_players(sat,slot,MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX);
            players_written=1;
        }
        if(!survival && i>=12 && i<16)continue; /* Shared K follows large. */
        if(!survival && i>=76)
        {
            u16 n=arcade_f_render_actor(&arcade_f.actors[i-76],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=74)
        {
            u16 n=arcade_e_render_actor(&arcade_e.actors[i-74],(u8)(i-74),arcade_e_ready[i-74],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=70)
        {
            u16 n=arcade_d_render_actor(&arcade_d.actors[i-70],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=64)
        {
            u16 n=arcade_c_render_actor(&arcade_c.actors[i-64],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=48)
        {
            u16 n=arcade_main_render_actor(&arcade_main.actors[i-48],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        e=&g_entities[i];
        /* Source active=2 remains visible throughout the hit/death sequence. */
        if(!e->active)continue;
        if(e->active>2 && (survival || i>=12 ||
           !(arcade_foes[i].pose&ARCADE_BONUS_POSE) || (e->active&128)))continue;

        /* Native actors own position and pose. Materialize the view only when
         * a sprite is actually drawn, not after every simulation update. */
        if(!survival && i<12) {
            const ArcadeFoe *a=&arcade_foes[i];const ArcadeFoeFrame *pose;
            sx=(s16)(a->body.x-g_game.cameraX);
            sy=(u8)(239-a->body.y);if(sy>=240)sy-=256;
        if(a->pose&ARCADE_BONUS_POSE)
        {
            const ArcadeBonusFrame *bonus=&arcade_bonus_frames[a->pose&0x3fff];
            u8 alternate=(u8)((a->pose&ARCADE_BONUS_OWNER2)!=0);
            body_tile=alternate?bonus->owner2_tile:bonus->tile;
            body_facing=(u8)(((alternate?0xc3:bonus->attr)&64)!=0);
        }
        else
        {
            pose=&arcade_foe_frames[a->pose];body_tile=pose->tile;
            body_facing=(u8)((pose->attr&64)!=0);
        }
        } else if(!survival && i>=20 && i<24) {
            const ArcadeIActor *a=&arcade_i_actors[i-20];
            sx=(s16)(a->body.x-g_game.cameraX);
            sy=(u8)(239-a->body.y);if(sy>=240)sy-=256;
            body_tile=arcade_i_tile(a->code,a->attr);body_facing=(a->attr&64)!=0;
        } else {
            sx=(s16)(e->x-g_game.cameraX);sy=e->y;
            body_tile=e->tile;body_facing=e->facing;
        }

        if (e->type == ET_POPUP)
        {
            u8 k;
            if (glyphs >= POPUP_GLYPH_MAX) continue;
            if (sy <= -8 || sy >= SCREEN_H) continue;
            for (k = 0; k < e->popupLen && glyphs < POPUP_GLYPH_MAX; k++)
            {
                s16 gx = (s16)(sx + k * 8);
                u16 ftile = (u16)(TILE_FONT_INDEX + ((u8)e->popupStr[k] - 32));
                if (gx <= -8 || gx >= SCREEN_W) continue;
                sat[slot * 4 + 0] = (u16)((0x80 + sy) & 0x3FF);
                sat[slot * 4 + 1] = (u16)((SPRITE_SIZE(1, 1) << 8) | (slot + 1));
                sat[slot * 4 + 2] = (u16)TILE_ATTR_FULL(PAL3, 1, 0, 0, ftile);
                sat[slot * 4 + 3] = (u16)((0x80 + gx) & 0x1FF);
                slot++; glyphs++;
            }
            continue;
        }

        /* entity body (16x16). Cap the BODY sprites; popups have their own cap. */
        if (count >= ENTITY_SAT_MAX) continue;
        /* cull off-screen sprites (with a small margin) so the SAT stays lean */
        if (sx <= -BODY_W || sx >= SCREEN_W || sy <= -BODY_H || sy >= SCREEN_H)
            continue;

        /* (dying enemies no longer blink: the baked ENEMY_DEATH animation -- the
         * 307->308->309->310 collapse, driven in entity_update_one -- IS the
         * death feedback now, so we draw every frame of it.) */
        sat[slot * 4 + 0] = (u16)((0x80 + sy) & 0x3FF);
        /* link to the NEXT slot; the loop tail patches the final link to 0 */
        sat[slot * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (slot + 1));
        /* flipH = !facing: the arcade ENEMY/projectile sprite art is drawn facing
         * LEFT at flip0 (opposite the PLAYER art, which faces right at flip0 -- the
         * player render in game.c uses flipH=facing and is correct). The AI sets
         * e->facing toward the player CORRECTLY (verified live: facing values track
         * the player's side), so only the sprite orientation was inverted -- enemies
         * faced AWAY from the player, and throwers visually faced/"shot" backward
         * toward other enemies. Inverting the flip here makes every enemy face the
         * player, matching the arcade (white skulls face the hero, tools/arcdemo). */
        sat[slot * 4 + 2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, (u8)((!survival && e->type==ET_SHOT)?0:!body_facing), body_tile);
        sat[slot * 4 + 3] = (u16)((0x80 + sx) & 0x1FF);

        slot++;
        count++;
    }

    if(!survival)
    {
        ArcadeLargeCell cells[16];u16 visible=0,n,k,j;
        u16 resident_tiles[4]={65535,65535,65535,65535};
        u8 remaining,resident_mask=0;
        if(!players_written)slot+=game_render_players(sat,slot,
            MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX);
        /* Existing world-to-screen convention maps source Y to Y-1.
         * Whole-game viewport parity remains a separate integration concern. */
        static const ArcadeSpriteView view={0,1,256,224};
        for(k=0;k<3;k++)if(arcade_admission.large&(1u<<k))
        {
            remaining=4;
            if(arcade_large_cells(&arcade_large_actors[k],(u16)g_game.cameraX,
                &remaining,cells+k*4))visible|=(u16)(15u<<(k*4));
        }
        for(k=0;k<4;k++)
        {
            const ArcadeKProjectile *a=&arcade_k_actors[k];
            if(arcade_admission.children&(1u<<k))
            {
                ArcadeLargeCell *c=&cells[12+k];
                const ArcadeFoeWeapon *w=&arcade_weapons[k];
                if(w->active)
                {
                    const ArcadeFoeFrame *f=&arcade_foe_frames[w->pose];
                    c->x=(u8)(w->x-g_game.cameraX);c->y=(u8)(-w->y-16);
                    c->code=f->code;c->attr=f->attr;
                    resident_tiles[k]=f->tile;resident_mask|=(u8)(1u<<k);
                }
                else
                {c->x=(u8)(a->x-g_game.cameraX);c->y=(u8)(-a->y-16);c->code=a->code;c->attr=a->attr;}
                visible|=(u16)(1u<<(12+k));
            }
        }
        n=arcade_large_present_mixed(cells,visible,resident_tiles,&view,sat,slot,
            MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX,512,1);
        if(n==65535)
        {
            n=(visible==large_last_visible && resident_mask==large_last_resident)?large_last_count:0;
            if(slot+n>MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX)n=0;
            for(k=0;k<n;k++)for(j=0;j<4;j++)sat[(slot+k)*4+j]=large_last_sat[k*4+j];
        }
        else
        {
            large_last_count=n;large_last_visible=visible;large_last_resident=resident_mask;
            for(k=0;k<n;k++)for(j=0;j<4;j++)large_last_sat[k*4+j]=sat[(slot+k)*4+j];
        }
        if(n && slot>firstSlot)sat[(slot-1)*4+1]=(u16)((sat[(slot-1)*4+1]&0xff00)|slot);
        for(k=0;k<n;k++)sat[(slot+k)*4+1]=(u16)(0x500|(slot+k+1));
        slot+=n;
    }

    /* terminate the chain: the last appended sprite links to 0 */
    if (slot != firstSlot) sat[(slot - 1) * 4 + 1] =
        (u16)((sat[(slot - 1) * 4 + 1] & 0xFF00) | 0);

    return (u16)(slot - firstSlot);   /* total sprites appended (bodies + glyphs) */
}
