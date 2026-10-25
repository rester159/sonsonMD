static void collision_pass(void)
{
    u16 i, j;
    const u16 shot_first=g_survivalRun?0:(u16)(16+(wave_clock&1)*2);
    const u16 shot_end=g_survivalRun?ENTITY_MAX:(u16)(shot_first+2);
    u8 pk,pass;
    /* E2BF..E2CB: source pools0828,0A28,0AE8,0B68,0BA8.
     * Each family scans both shots before advancing to the next family.
     * Legacy/direct-spawn types retain a fallback pass after I/H/G. */
    u8 heads[6]={ENTITY_MAX,ENTITY_MAX,ENTITY_MAX,ENTITY_MAX,ENTITY_MAX,ENTITY_MAX};
    u8 next[ENTITY_MAX-24];
    static const u8 family_rank[ET_POPUP+1]={
        5,5,5,5,4,0,5,5,1,3,5,5,2,5,5
    };
    /* Every ordinary shot handler requires active==1. Skip only their
     * side-effect-free empty scans; player contact still runs below. */
    if(!g_survivalRun && g_entities[shot_first].active!=1 &&
       g_entities[shot_first+1].active!=1)goto player_contacts;
    if(!g_survivalRun)
    {
        main_shot_collisions();
        c_shot_collisions();
        d_shot_collisions();
        e_shot_collisions();
        f_shot_collisions();
        /* Prepend in reverse slot order: stable family lists in one pool walk.
         * This avoids six full rescans for each of the player's shots. */
        for(i=legacy_pool_used?ENTITY_MAX:24;i>24;)
        {
            const Entity *en=&g_entities[--i];u8 rank;
            if(en->active!=1 || en->owner==128)continue;
            rank=en->type<=ET_POPUP?family_rank[en->type]:5;
            next[i-24]=heads[rank];heads[rank]=(u8)i;
        }
    }
    for(pass=0;pass<(g_survivalRun?1:6);pass++)
    {
    if(!g_survivalRun && pass==5)gh_shot_collisions();

    /* SHOT vs ENEMY: a hit defeats the enemy and consumes the shot, awarding
     * score (arcade: a shot poking an enemy triggers its death sequence
     * sub_DD78). One loop handles every killable archetype -- the ground walker,
     * the flyer, and the thrower -- each with its own death-state index + score.
     * (The flyer/thrower use state 2 = dying; the walker uses state 1.) */
    for (i = shot_first; i < shot_end; i++)
    {
        Entity *s = &g_entities[i];
        if (s->type != ET_SHOT)continue;
        if(g_survivalRun?(!s->active || s->state):(s->active!=1))continue;
        /* Original $E2B3 selects P1 on even phases, P2 on odd phases. */
        if(!g_survivalRun && s->owner!=(wave_clock&1))continue;

        /* Slots 0..11 were handled above; 12..15 are enemy weapons, not targets. */
        for (j=g_survivalRun?0:heads[pass]; j<ENTITY_MAX;
             j=g_survivalRun?j+1:next[j-24])
        {
            Entity *en = &g_entities[j];
            u8  deathState;
            u32 award;
            /* Reject empty/reserved slots before decoding an enemy type. This
             * scan repeats per player shot; the type tests have no side effects. */
            if(en->active!=1 || en->owner==128)continue;

            /* Same type metadata and scan order as the former comparison chain. */
            {
                static const u8 death_states[ET_POPUP+1]={
                    [ET_ENEMY_A]=1,[ET_FLYER]=2,[ET_THROWER]=2,[ET_CREATURE_G]=1,
                    [ET_CREATURE_C]=1,[ET_COMPOSITE_E]=1,[ET_HOPPER]=2,
                    [ET_LARGE]=1,[ET_EMITTER]=1,[ET_ORB]=1
                };
                static const u16 awards[ET_POPUP+1]={
                    [ET_ENEMY_A]=SCORE_ENEMY,[ET_FLYER]=SCORE_FLYER,
                    [ET_THROWER]=SCORE_THROWER,[ET_CREATURE_G]=SCORE_CREATURE_G,
                    [ET_CREATURE_C]=SCORE_CREATURE_C,[ET_COMPOSITE_E]=SCORE_COMPOSITE_E,
                    [ET_HOPPER]=SCORE_HOPPER,[ET_LARGE]=SCORE_LARGE,
                    [ET_EMITTER]=SCORE_EMITTER,[ET_ORB]=SCORE_ORB
                };
                u16 type=en->type;
                if(type>ET_POPUP)continue;
                deathState=death_states[type];
                if(!deathState)continue;
                award=awards[type];
            }

            if (en->state == deathState) continue;   /* dead/dying */

            if (aabb(s->x, s->y, SHOT_HALF_W * 2, SHOT_HALF_H * 2,
                     en->x, en->y, BODY_W, BODY_H))
            {
                if (en->hp > 1)
                {
                    en->hp--;                       /* tough enemy: another hit    */
                    snd_sfx_enemy();                /* hit-feedback SFX            */
                }
                else
                {
                    if (en->type == ET_LARGE) en->hp = 0; /* $EEBE: 1 -> 0 */
                    en->state = deathState;        /* enter death sequence      */
                    en->timer = ENEMY_DEATH_FRAMES;
                    en->vx    = 0;
                    /* credit the player whose shot scored the kill (co-op). */
                    g_game.players[s->owner & 1].score += award;
                    /* NOTE: the arcade shows a score child on enemy kills too
                     * (classK $0ED8), but we DON'T pop a "+N" per kill here -- in a
                     * busy fire-fight that would spawn many concurrent popup-glyph
                     * sprites and shave the frame budget. The iconic SonSon popup is
                     * the FRUIT one (low-frequency), which we DO show (see ET_ITEM
                     * collect below). Enemy kills still score + play the defeat SFX. */
                    snd_sfx_enemy();
                }
                if(!g_survivalRun){s->active=2;s->phase=2;s->fireTimer=0;}
                s->state=1;s->timer=5; /* Impact retains its slot through the end puff. */
                if(g_survivalRun)anim_restart(&s->av,SS_ANIM_SHOT_END);
                break;
            }
        }
    }

    } /* source shot-family order */

player_contacts:
    /* PLAYER vs ENEMY/PROJECTILE (contact-hurt, arcade DCAC) + PLAYER vs ITEM
     * (collect), for EACH active co-op player. Every hostile body -- walker, flyer,
     * thrower, AND the lobbed projectile -- costs that player a life on overlap; the
     * lethal request is gated per-player to one life per touch (lethal_contact).
     * A player who is OUT (flow GAME_OVER) is skipped entirely. Fruit is credited
     * to whichever player collects it. */
    for (pk = 0; pk < g_game.numPlayers; pk++)
    {
        Player *p = &g_game.players[pk];
        if (p->flow == FLOW_GAME_OVER) continue;   /* out player: no contact/collect */
        if(!g_survivalRun && pk!=(wave_clock&1))continue;
        if(!g_survivalRun)game_arcade_food_contact(pk); /* E2E0/E334. */
        if(!g_survivalRun)
        {
            u8 k;
            /* E2E3/E337: both stable shot slots scan the shared background pool. */
            for(k=0;k<2;k++)
            {
                Entity *s=&g_entities[16+pk*2+k];ArcadePlayerShot shot;
                if(s->active!=1 || s->type!=ET_SHOT || s->owner!=pk)continue;
                shot.x=(u16)s->x;shot.y=(u8)(239-s->y);shot.active=s->active;
                shot.state=s->phase;shot.phase=s->fireTimer;
                p->score+=arcade_food_world_shot(&shot);
                s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;
                if(shot.active!=1)s->state=1;
            }
            large_shot_collisions();
        }
        /* ECB7: main pool contacts precede K and the formation families. */
        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 slot=arcade_main_contact_target(arcade_main.actors,(u16)p->x,
                (u8)(239-p->y),(u8)(game_player_target_flags(pk)>>8));
            if(slot!=255)
            {
                ArcadeMainActor *a=&arcade_main.actors[slot];
                if(a->active&64)
                {
                    ArcadeBonusAward award;
                    if(arcade_main_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                    {p->score+=award.points;g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                }
                else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
            }
        }
        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 slot=arcade_c_contact_target(arcade_c.actors,(u16)p->x,
                (u8)(239-p->y),(u8)(game_player_target_flags(pk)>>8));
            if(slot!=255)
            {
                ArcadeCActor *a=&arcade_c.actors[slot];
                if(a->active&64)
                {
                    ArcadeBonusAward award;
                    if(arcade_c_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                    {p->score+=award.points;g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                }
                else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
            }
        }
        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 slot=arcade_d_contact_target(arcade_d.actors,(u16)p->x,
                (u8)(239-p->y),(u8)(game_player_target_flags(pk)>>8));
            if(slot!=255)
            {
                ArcadeDActor *a=&arcade_d.actors[slot];
                if(a->active&64)
                {
                    ArcadeBonusAward award;
                    if(arcade_d_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                    {p->score+=award.points;g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                }
                else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
            }
        }
        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 slot=arcade_e_contact_target(arcade_e.actors,(u16)p->x,
                (u8)(239-p->y),(u8)(game_player_target_flags(pk)>>8));
            if(slot!=255)
            {
                ArcadeEActor *a=&arcade_e.actors[slot];
                if(a->active&64)
                {
                    ArcadeBonusAward award;
                    if(arcade_e_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                    {p->score+=award.points;g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                }
                else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
            }
        }
        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 slot=arcade_f_contact_target(arcade_f.actors,(u16)p->x,
                (u8)(239-p->y),(u8)(game_player_target_flags(pk)>>8));
            if(slot!=255)
            {
                ArcadeFActor *a=&arcade_f.actors[slot];
                if(a->active&64)
                {
                    ArcadeBonusAward award;
                    if(arcade_f_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                    {p->score+=award.points;g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                }
                else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
            }
        }
        /* E2F8 precedes E2FE/E301/E304: consume a touching K projectile
         * before an overlapping H/G/I body can raise the death request. */
        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 slot=shared_k_target((u16)p->x,(u8)(239-p->y),0);
            if(slot!=255)
            {
                if(arcade_weapons[slot].active)
                {arcade_weapons[slot].state=3;arcade_weapons[slot].phase=2;}
                else
                {arcade_k_actors[slot].state=3;arcade_k_actors[slot].phase=2;}
                p->deadReq=1;
                if(!p->hurt)p->hurt=HURT_FRAMES;
            }
        }
        if(!g_survivalRun)game_arcade_special_contact(pk); /* E2FB/E34F, after K. */
        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 k;
            /* E2FE/E301/E304: H, G, I. Ordinary contact does not test AI state. */
            for(k=0;k<16;k++)
            {
                u16 index=k<6?k+6:k<12?k-6:k+8;
                Entity *en=&g_entities[index];
                if(!en->active || (en->active&2))continue;
                if(en->active&64)
                {
                    if(index<12 && arcade_body_contact((u16)p->x,(u8)(239-p->y),
                        (u16)en->x,(u8)(239-en->y),1,1))
                    {
                        ArcadeBonusAward award;
                        if(arcade_gh_bonus_collect(&arcade_foes[index],pk,g_arcadeCombatDifficulty,&award))
                        {
                            en->active=2;p->score+=award.points;
                            g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);
                            /* ECE9 returns after one hit, then the dispatcher
                             * advances to the next family, not the next slot. */
                            k=k<6?5:11;
                        }
                    }
                    continue;
                }
                if(arcade_body_contact((u16)p->x,(u8)(239-p->y),
                     (u16)en->x,(u8)(239-en->y),1,en->active))
                {
                    p->deadReq=1;
                    if(!p->hurt)p->hurt=HURT_FRAMES;
                    break;
                }
            }
        }

        if(!g_survivalRun && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            if(arcade_large_contact_target(arcade_large_actors,(u16)p->x,(u8)(239-p->y),1)!=255)p->deadReq=1;
            if(p->deadReq && !p->hurt)p->hurt=HURT_FRAMES;
        }

    /* Ordinary slots0..15 were unconditionally excluded below. */
    for (j = g_survivalRun?0:16; j < ENTITY_MAX; j++)
    {
        Entity *en = &g_entities[j];
        if(!g_survivalRun && (j<16 || (j>=20 && j<24)))continue;
        if (en->active != 1 || en->owner==128) continue;

        switch (en->type)
        {
            /* hostile bodies: lethal on contact unless already in their death
             * sequence (a dying/corpse enemy is harmless). Death-state index
             * differs by archetype (1 for the walkers/creatures/large/emitter/orb,
             * 2 for the flyer/thrower/hopper) -- match each so a corpse can't hurt. */
            case ET_ENEMY_A:
            case ET_CREATURE_G:
            case ET_CREATURE_C:
            case ET_COMPOSITE_E:
            case ET_LARGE:
            case ET_EMITTER:
            case ET_ORB:
                if (en->state != 1) lethal_contact(p, en);
                break;
            case ET_FLYER:
            case ET_THROWER:
            case ET_HOPPER:
                if (en->state != 2) lethal_contact(p, en);
                break;

            /* the lobbed projectile hurts the player, then is consumed */
            case ET_ENEMY_SHOT:
                if (aabb(p->x, p->y, PLAYER_W, PLAYER_H,
                         en->x, en->y, ESHOT_HALF_W * 2, ESHOT_HALF_H * 2))
                {
                    if (lethal_contact(p, en))
                    {
                        en->active=0;
                        if(!g_survivalRun && j>=12 && j<16)
                            arcade_weapons[j-12].active=0;
                    }  /* spent on a hit */
                }
                break;

            /* FRUIT collect: SonSon's signature scoring. Award by fruit TYPE
             * (escalating values), play the item SFX, pop a floating "+N", and
             * vanish. (The arcade spawns a classK $0ED8 score-child here -- the
             * popup; sub_CBC8 animates it.) */
            case ET_ITEM:
                if (aabb(p->x, p->y, PLAYER_W, PLAYER_H,
                         en->x, en->y, BODY_W, BODY_H))
                {
                    u16 val = k_fruit_score[en->fruit < FRUIT_TYPE_COUNT
                                            ? en->fruit : 0];
                    p->score += val;                 /* credit the collector (co-op) */
                    spawn_popup((s16)(en->x), (s16)(en->y), val);
                    /* Two arcade pickup tiers (latch-verified): 0x02 fires on the
                     * small +10 pickup, 0x0C on the high +500 pickup (a distinct,
                     * higher cue). Map the native fruit economy onto them by value:
                     * the high-value fruit (ORANGE 500 / APPLE 1000 / GOLD 2000)
                     * gets the bonus SFX, the low CHERRY (300) the plain item SFX. */
                    if (val >= 500) snd_sfx_bonus();  /* high-value pickup (cmd 0x0C) */
                    else            snd_sfx_item();    /* low-value pickup  (cmd 0x02) */
                    en->active = 0;                  /* fruit eaten                */
                }
                break;

            default:
                break;
        }
    }
    }   /* end per-player loop */
}
