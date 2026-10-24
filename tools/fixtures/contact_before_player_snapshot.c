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
