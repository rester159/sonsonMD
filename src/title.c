/* title.c -- the arcade SonSon TITLE screen + ATTRACT loop (native).
 *
 * Reproduces the arcade attract sequence (RE'd from the real machine, mame
 * sonson, tools/arc_attract.lua): the TITLE screen ("SONSON" logo + score/HIGH
 * header + "PLEASE DEPOSIT COIN" + flanking characters + "(c) CAPCOM 1984")
 * alternating with a self-playing DEMO. The title BG + sprites are baked
 * byte-faithfully from the arcade title frame (tools/gen_title.py -> ss_title.*).
 *
 * The arcade screen is 256x224 (H32), so the title runs in the game's H32 video
 * mode (the 32-col baked nametable fills the screen exactly). Returns when the
 * player presses START (= the arcade "deposit coin" / home "press start"); the
 * caller then starts a real game. While idle it runs the attract demo.
 */
#include <genesis.h>
#include "frontend.h"
#include "game.h"
#include "ss_title.h"

/* Build a raw SAT word set from the baked title sprites and DMA it to the VDP
 * sprite list. Each SSSprite is a 16x16 (SPRITE_SIZE(2,2)); screen coords add the
 * +0x80 SAT bias. Parked arcade sprites (raw y == 248) resolve off-screen. */
static void title_sprites(void)
{
    u16 i;
    u16 sat[SS_TITLE_SPRITE_COUNT * 4];
    u16 n = 0;
    for (i = 0; i < SS_TITLE_SPRITE_COUNT; i++)
    {
        const SSSprite *s = &ss_title_sprites[i];
        /* title sprite y: undo the gameplay -24 playfield bias (+24) then apply the
         * same 1-row (8px) up-shift as the BG below -> +16. Skip parked slots. */
        s16 ty = (s16)(s->y + 16);
        if (s->y >= 200 || s->y < -16) continue;
        sat[n * 4 + 0] = (u16)((0x80 + ty) & 0x3FF);                  /* y        */
        sat[n * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (n + 1));   /* size+link*/
        sat[n * 4 + 2] = (u16)(TILE_ATTR_FULL(PAL1, 1, s->flipV, s->flipH, s->tile));
        sat[n * 4 + 3] = (u16)((0x80 + s->x) & 0x1FF);               /* x        */
        n++;
    }
    if (n == 0) { VDP_setSprite(0, 0, 0, SPRITE_SIZE(1,1), 0); return; }
    sat[(n - 1) * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | 0);       /* link end */
    /* engine off in the front-end -> the DMA queue is never flushed; do it now. */
    DMA_doDma(DMA_VRAM, sat, VDP_getSpriteListAddress(), (u16)(n * 4), 2);
}

/* Render the baked title BG (logo + text + score header) to BG_A. */
static void title_draw(void)
{
    u16 c, r;
    u16 colbuf[32];

    PAL_setColors(PAL0 * 16, ss_pal_char,   16, CPU);
    PAL_setColors(PAL1 * 16, ss_pal_sprite, 16, CPU);
    VDP_setBackgroundColor(0);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_clearPlane(BG_B, TRUE);

    VDP_loadTileData(ss_title_tiles, SS_TITLE_TILE_BASE, SS_TITLE_TILE_COUNT, DMA);

    /* 32x32 row-major nametable -> BG_A via column writes (the proven game path).
     * Shift UP by one row: the arcade title spans rows 1..30 (header..CREDIT) but
     * the native H32 shows only 28 rows, so map source row r+1 -> dest row r. That
     * fits the header (src 1-2) through the "(c) CAPCOM 1984" line (src 28) into the
     * visible area (CREDIT at src 30 falls just below the bottom edge). */
    for (c = 0; c < 32; c++)
    {
        for (r = 0; r < 32; r++)
            colbuf[r] = ss_title_nametable[(((r + 1) & 31)) * 32 + c];
        VDP_setTileMapDataColumnFast(BG_A, colbuf, c, 0, 32, DMA);
    }

    title_sprites();
}

/* Show the title screen and wait for START. Returns when the player starts
 * (caller then runs a real game). Idle timeout -> run the attract demo, then
 * redraw the title. mode selects the arcade/home flavor (prompt text TODO). */
void title_attract_run(u8 mode)
{
    u16 prevDown = 1;           /* swallow a button held over from the prior screen */
    u16 settle = 0;
    const u16 GO = (u16)(BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C);
    (void)mode;

    /* 3-button mode (kill the 6-button TH phantom presses, as the selector does). */
    JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);

    title_draw();

    for (;;)
    {
        u16 joy, downNow;
        VDP_waitVSync();
        JOY_update();           /* engine vint is OFF in the front-end -> refresh */
        joy = (u16)(JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2));
        downNow = (joy & GO) ? 1 : 0;
        if (settle < 20) { settle++; prevDown = downNow; continue; }
        if (downNow && !prevDown) return;       /* fresh press -> begin the game */
        prevDown = downNow;
    }
}
