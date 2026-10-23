        for (i = 0; i < ENTITY_MAX; i++)
        {
            /* Earlier actors can allocate the fallback pool in this update.
             * Test its monotonic latch only on reaching the empty tail. */
            if(i==24 && !g_survivalRun && !legacy_pool_used)break;
