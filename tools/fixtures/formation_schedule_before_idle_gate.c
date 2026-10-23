static void formation_schedule(u8 family,u16 tracked_x)
{
    const u8 bit=family?SS_WAVE_BIT_CLASSH:SS_WAVE_BIT_CLASSG;
    const u16 first=family?6:0;
    u8 i,live=0;
    ArcadeBirth births[6];
    for(i=0;i<6;i++)if(g_entities[first+i].active)live++;
    if(!arcade_formation_step(&arcade_waves.controls[bit],family,live,wave_clock,
       (u8)(g_arcadeCombatDifficulty>>8),(u16)g_game.cameraX,tracked_x,&arcade_rng,births,
       &formation_reservation[family]))return;
    for(i=0;i<6;i++)
    {
        Entity *e=entity_spawn_range(family?ET_ENEMY_A:ET_CREATURE_G,
                       (s16)births[i].x,(s16)(239-births[i].y),first+i,first+i+1);
        ArcadeFormationActor *a=&arcade_foes[first+i].body;
        /* Dedicated slots are free when this controller's live count is zero. */
        if(!e)continue;
        e->active=128;e->fromSched=1;e->fruit=(u8)(bit+1);e->owner=128;
        e->facing=births[i].facing==3;
        a->x=births[i].x;a->y=births[i].y;
        a->active=128;a->state=a->phase=0;a->delay=births[i].delay;
        a->facing=births[i].facing;
        if(!family)arcade_foes[first+i].kind=0;
    }
}