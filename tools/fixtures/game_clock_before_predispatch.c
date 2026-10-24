static u8 game_results_update(void)
{
    ArcadeResultsInput input;ArcadeResultsEvent event;u8 k;
    input.clock=entity_arcade_clock();input.screen_busy=g_resultsDisplay && arcade_results_display_pending();
    input.p1_out=g_game.players[0].flow==FLOW_GAME_OVER;
    input.p2_out=g_game.players[1].flow==FLOW_GAME_OVER;
    input.two_players=g_game.numPlayers>1;input.marker=0;
    arcade_results_step(&g_results,&input,&event);
    if(event.flags&ARCADE_RESULTS_PLAY)return 0;
    if(event.flags&ARCADE_RESULTS_CLEAR)
    {
        VDP_setEnable(FALSE);DMA_flushQueue();
        arcade_results_display_begin();g_resultsDisplay=1;g_resultsPlayers=0;
        VDP_setHorizontalScroll(BG_A,0);VDP_setEnable(TRUE);
    }
    if(event.flags&ARCADE_RESULTS_PLAYER)g_resultsPlayers|=event.player_mask;
    if(event.flags&ARCADE_RESULTS_ANIMATE)
        for(k=0;k<2;k++)if(g_resultsPlayers&(1<<k))
        {
            if(life_owned[k]==1)arcade_life_animation_step(&player_life[k]);
            else arcade_player_animation_step(&player_animation[k]);
        }
    if(g_resultsDisplay)arcade_results_display_event(&event);
    if((event.flags&ARCADE_RESULTS_AWARD) && !g_attract)
        for(k=0;k<2;k++)if(event.points[k])
        {g_game.players[k].score+=event.points[k];check_extra_life(&g_game.players[k]);}
    if(event.flags&ARCADE_RESULTS_RESTART)game_course_restart();
    entity_arcade_clock_advance();
    return 1;
}
bool game_update(void)
{
    g_frameCounter++;          /* headless FPS probe tick (see g_frameCounter) */
    if(g_resultsActive && game_results_update())return FALSE;
