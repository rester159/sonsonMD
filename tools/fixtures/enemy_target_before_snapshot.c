    if(!g_survivalRun)
    {
        arcade_world.camera=(u16)g_game.cameraX;arcade_world.clock=wave_clock;
        arcade_world.stage=(u8)(g_arcadeCombatDifficulty>>8);arcade_world.tracking=g_game.numPlayers>1;
        arcade_world.scroll_stopped=game_arcade_stopped();arcade_world.purge=arcade_food_conversion_pulse;
        arcade_world.shot_budget=arcade_shot_budget;
        arcade_main_pool_update(&arcade_main,arcade_world.camera,wave_clock,
            arcade_world.scroll_stopped,arcade_world.purge,&arcade_bonus_next,arcade_bonus_limit);
        arcade_c_pool_update(&arcade_c,arcade_world.camera,arcade_world.scroll_stopped,
            arcade_world.purge,&arcade_bonus_next,arcade_bonus_limit);
        {
            u8 target=arcade_tracking_player(g_game.numPlayers>1,
                (u16)g_game.players[0].x,game_player_target_flags(0),
                (u16)g_game.players[1].x,game_player_target_flags(1));
            const Player *p=&g_game.players[target];
            arcade_d_pool_update(&arcade_d,arcade_world.camera,wave_clock,
                (u8)(g_arcadeCombatDifficulty>>8),arcade_world.scroll_stopped,arcade_world.purge,
                (u16)p->x,(u8)(239-p->y),target,&arcade_bonus_next,arcade_bonus_limit);
            {
                u16 flags=game_player_target_flags(target);
                u8 eligible=(u8)(((flags>>8)&~flags)&1);
                arcade_e_pool_update(&arcade_e,arcade_world.camera,wave_clock,
                    arcade_world.scroll_stopped,arcade_world.purge,(u16)p->x,(u8)(239-p->y),
                    target,eligible,&arcade_main_ready,&arcade_bonus_next,arcade_bonus_limit);
                arcade_shot_budget=(u8)(arcade_main_ready!=0);
                arcade_world.shot_budget=arcade_shot_budget;
            }
        }
        arcade_f_pool_update(&arcade_f,arcade_world.camera,arcade_world.scroll_stopped,
            arcade_world.purge,&arcade_bonus_next,arcade_bonus_limit);
        for(pk=0;pk<2;pk++)
        {
            const Player *p=&g_game.players[pk];
            ArcadeFoePlayer *q=&arcade_world.players[pk];
            u16 flags=game_player_target_flags(pk);
            q->x=(u16)p->x;q->y=(u8)(239-p->y);
            q->active=(u8)(flags>>8);q->kind=(u8)flags;
        }
    }

