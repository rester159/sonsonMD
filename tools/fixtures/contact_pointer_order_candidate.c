            u8 k;
            static Entity *const contact_order[16] = {
                &g_entities[6],&g_entities[7],&g_entities[8],&g_entities[9],&g_entities[10],&g_entities[11],
                &g_entities[0],&g_entities[1],&g_entities[2],&g_entities[3],&g_entities[4],&g_entities[5],
                &g_entities[20],&g_entities[21],&g_entities[22],&g_entities[23]
            };
            /* E2FE/E301/E304: H, G, I. Ordinary contact does not test AI state. */
            for(k=0;k<16;k++)
            {
                Entity *en=contact_order[k];
                if(!en->active || (en->active&2))continue;
                if(en->active&64)
                {
                    u16 index=k<6?k+6:k<12?k-6:k+8;
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