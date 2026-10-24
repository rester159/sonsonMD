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