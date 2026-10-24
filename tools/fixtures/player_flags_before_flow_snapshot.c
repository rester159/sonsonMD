u16 game_player_target_flags(u8 which)
{
    const Player *p=&g_game.players[which];
    if(life_owned[which])
        return (u16)(((u16)player_life[which].active<<8)|player_life[which].kind);
    return (u16)(((u16)(which<g_game.numPlayers && p->flow!=FLOW_GAME_OVER)<<8)|
                 (u8)(game_player_started(which) &&
                      (p->flow==FLOW_DYING || p->flow==FLOW_GAME_OVER)));
}
