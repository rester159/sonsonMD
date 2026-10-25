void game_run(void)
{
    u16 demoT = 0;
    game_init();
    mode_input_rearm();
    while (TRUE)
    {
        u8 starts = mode_input_poll();
        bool done;
        if (!g_attract) game_start_buttons(starts);
        if (g_paused)
        {
            SYS_doVBlankProcess();
            continue;
        }
        done = game_update();
        game_render();
        g_presentCounter++;   /* complete SAT/scroll prepared for this blank */
        SYS_doVBlankProcess();
        if (done) break;           /* GAME OVER finished -> back to the selector */
        if (g_attract)             /* attract demo: abort on any button, else time-cap */
        {
            if (JOY_readJoypad(JOY_1) & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C) ||
                (JOY_readJoypad(JOY_2) & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C)))
                { g_attract_started = 1; break; }
            if (++demoT >= ATTRACT_MAX_FRAMES) break;
        }
    }

