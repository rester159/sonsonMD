static void entity_sprite_admission(void)
{
    ArcadeAdmissionInput input;u8 k,active;
    for(k=0;k<2;k++)input.state.e[k]=arcade_e.actors[k].active;
    input.state.reserved=0;
    static const u8 rank[ET_POPUP+1]={6,0,6,6,4,6,6,6,1,3,6,6,2,5,255};
    const Entity *e=g_entities;
    active=arcade_food_pickup.active;if(active && !(active&128))input.state.reserved++;
    active=arcade_food_conversion.active;if(active && !(active&128))input.state.reserved++;
    active=arcade_food_carrier.active;if(active && !(active&128))input.state.reserved++;
    for(k=0;k<g_game.numPlayers;k++)
    {active=(u8)(game_player_target_flags(k)>>8);if(active && !(active&128))input.state.reserved++;}
    for(k=0;k<16;k++)input.state.main[k]=arcade_main.actors[k].active;
    for(k=0;k<6;k++)input.state.c[k]=arcade_c.actors[k].active;
    for(k=0;k<4;k++)input.state.d[k]=arcade_d.actors[k].active;
    /* Fixed pools0..23 have no legacy rank. Split the loops to avoid testing
     * that invariant on every inactive slot in the hot readiness mapper. */
    for(k=0;k<24;k++,e++)
    {
        active=e->active;
        input.state.entities[k]=(active&128) || e->type==ET_POPUP?0:active;
    }
    for(k=0;k<24;k++,e++)
    {
        active=e->active;
        if(!active || (active&128))
        {input.state.entities[k+24]=0;input.state.ranks[k]=255;continue;}
        input.state.entities[k+24]=e->type==ET_POPUP?0:active;
        input.state.ranks[k]=e->type<=ET_POPUP?rank[e->type]:6;
    }
    for(k=0;k<3;k++)input.state.large[k]=arcade_large_actors[k].body.active;
    for(k=0;k<4;k++)input.state.children[k]=arcade_weapons[k].active?arcade_weapons[k].active:arcade_k_actors[k].active;
    arcade_native_admission_refresh(&input,&arcade_admission_cache);
    arcade_main_ready=arcade_admission.remaining;
    arcade_shot_budget=(u8)(arcade_main_ready!=0);
}
