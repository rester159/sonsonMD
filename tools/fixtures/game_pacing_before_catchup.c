        /* Keep one simulation per prepared display until render-owned logical
         * admission is separated. Excess debt remains visible in pacing.pending;
         * this is not a catch-up or PAL performance guarantee. */
        if(g_survivalRun || arcade_pacing_take(&gameplay_pacing))
        {
            gameplay_input_consume();gameplay_input_ready=1;
            done = game_update();
            gameplay_input_ready=0;updated=1;
        }
