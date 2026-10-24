/* SON SON II dialogue window and HUD item icons, rebuilt from HuCard bank 2
 * $6984-$6E19 and bank 4 $BADE.
 *
 * The window is drawn into the PCE background map at world-tile positions
 * ($8F,$91 = top-left, set by the caller relative to the camera): it grows
 * open in 4 steps, types text from bank 8 ($93/$94) one 8x8 character per
 * frame (any button skips), pages on $FE, offers a 2-way ($FD) or 3-way
 * ($FC) choice whose continuation address follows in the text, and
 * shrinks closed, the map redrawn underneath. Map writes are presentation:
 * they go to ss2_bat_queue for the renderer; the logic keeps the source's
 * waits and RAM.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

Ss2BatOp ss2_bat_queue[SS2_BATQ];
u16 ss2_bat_head, ss2_bat_tail;

static void bat_op(u8 kind, u8 x, u8 y, u16 w)
{
    Ss2BatOp *o = &ss2_bat_queue[ss2_bat_head & (SS2_BATQ - 1)];
    o->kind = kind; o->x = x; o->y = y; o->w = w;
    ss2_bat_head++;
    ZP(0x8B) = 0x02;                         /* $BA4C: VWR selected last */
}

/* $6DBA: BAT word ($26/$27) at world tile ($90,$92) */
static void put(u16 w)
{
    ZP(0x26) = (u8)w; ZP(0x27) = (u8)(w >> 8);
    bat_op(SS2_BAT_PUT, ZP(0x90), ZP(0x92), w);
}

/* $6E0A: one frame, keeping the loop counters $36/$37 */
static void frame(void)
{
    u8 a = ZP(0x36), b = ZP(0x37);
    ss2_wait(1);                             /* $B9B8 */
    ZP(0x37) = b;
    ZP(0x36) = a;
}

static void frames3(void)
{
    frame(); frame(); frame();
}

/* $6C4E: the window frame, $36 cells wide and $36-4 rows tall */
static void draw_frame(void)
{
    u8 x, y;
    ZP(0x90) = ZP(0x8F);
    ZP(0x92) = ZP(0x91);
    put(0x11FC);
    ZP(0x90)++;
    for (x = (u8)(ZP(0x36) - 2); x; x--) { put(0x11FD); ZP(0x90)++; }
    put(0x11FE);
    for (y = (u8)(ZP(0x36) - 6); y; y--)
    {
        ZP(0x90) = ZP(0x8F);
        ZP(0x92)++;
        put(0x1200);
        ZP(0x90)++;
        for (x = (u8)(ZP(0x36) - 2); x; x--) { put(0x1202); ZP(0x90)++; }
        put(0x11FF);
    }
    ZP(0x90) = ZP(0x8F);
    ZP(0x92)++;
    put(0x1210);
    ZP(0x90)++;
    for (x = (u8)(ZP(0x36) - 2); x; x--) { put(0x1211); ZP(0x90)++; }
    put(0x1201);
}

/* $6CF4: redraw the map under the window ($36 wide, $36-4 tall) */
static void restore(void)
{
    ZP(0x92) = (u8)(ZP(0x91) + ZP(0x36) - 4);
    ZP(0x90) = (u8)(ZP(0x8F) + ZP(0x36));
    bat_op(SS2_BAT_RESTORE, ZP(0x8F), ZP(0x91), (u16)((ZP(0x36) << 8) | (u8)(ZP(0x36) - 4)));
    ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
    ss2_mpr6 = ZP(0x8E);
}

/* $6997: place the window relative to the camera and grow it open */
static void open_window(void)
{
    u8 y;
    ZP(0x8F) = (u8)(ZP(0x8F) + (u8)((((Z_CAMX_HI << 8) | Z_CAMX_LO)) >> 3));
    ZP(0x91) = (u8)(ZP(0x91) + (u8)((((Z_CAMY_HI << 8) | Z_CAMY_LO)) >> 3));
    ZP(0x8F) = (u8)(ZP(0x8F) + 3);
    ZP(0x91) = (u8)(ZP(0x91) + 3);
    ZP(0x36) = 6;
    for (y = 4; y; y--)
    {
        frames3();
        ZP(0x8F)--;
        ZP(0x91)--;
        draw_frame();
        ZP(0x36) = (u8)(ZP(0x36) + 2);
    }
}

/* $6C1F: wait for I/II, then shrink the window closed */
static void close_window(void)
{
    u8 y;
    do frame(); while (!(Z_PRESS & 0x03));
    ZP(0x36) = 0x0C;
    for (y = 4; ; )
    {
        frames3();
        restore();
        if (!--y) return;
        ZP(0x8F)++;
        ZP(0x91)++;
        ZP(0x36) = (u8)(ZP(0x36) - 2);
        draw_frame();
    }
}

static u8 text(u8 y)
{
    return ss2_rom_byte((u16)(((ZP(0x94) << 8) | ZP(0x93)) + y));
}

static u16 cursor(void)
{
    return (Z_FRAME & 0x08) ? 0x1202 : 0x11AF;   /* BBS3 <$12: blink */
}

/* $6C12: the continuation address after a choice; Y indexes the text */
static void cont(u8 y)
{
    u16 a;
    y++;
    ZP(0x28) = text(y);
    y++;
    ZP(0x29) = text(y);
    a = (u16)((ZP(0x29) << 8) | ZP(0x28));
    ss2_msg_continue(a, y);
}

/* $6ACA: two-way choice (Left/Right), cursor at columns 2 and 6 */
static void choice2(u8 y)
{
    ss2_sfx(0xF1);                           /* $BBEF */
    ZP(0x96) = 0;
    for (;;)
    {
        u16 c;
        u8 a;
        frame();
        a = Z_PRESS;
        if (a & 0x03) { cont(y); return; }
        if (a & 0x80) ZP(0x96) = 0;
        else if (a & 0x20) ZP(0x96) = 1;
        c = cursor();
        ZP(0x92) = (u8)(ZP(0x91) + 5);
        if (!ZP(0x96))
        {
            ZP(0x90) = (u8)(ZP(0x8F) + 2); put(c);
            ZP(0x90) = (u8)(ZP(0x8F) + 6); put(0x1202);
        }
        else
        {
            ZP(0x90) = (u8)(ZP(0x8F) + 6); put(c);
            ZP(0x90) = (u8)(ZP(0x8F) + 2); put(0x1202);
        }
    }
}

/* $6B4A: three-way choice (Up/Down), cursor at rows 1, 3, 5 */
static void choice3(u8 y)
{
    /* write order per cursor row (the last write leaves $92) */
    static const u8 order[3][3] = { { 1, 3, 5 }, { 3, 1, 5 }, { 5, 3, 1 } };
    ss2_sfx(0xF1);
    ZP(0x96) = 0;
    for (;;)
    {
        u16 c;
        u8 a, i;
        frame();
        a = Z_PRESS;
        if (a & 0x03) { cont(y); return; }
        if (a & 0x10) { if (ZP(0x96)) ZP(0x96)--; }
        else if (a & 0x40) { if (ZP(0x96) != 2) ZP(0x96)++; }
        c = cursor();
        ZP(0x90) = (u8)(ZP(0x8F) + 2);
        for (i = 0; i < 3; i++)
        {
            ZP(0x92) = (u8)(ZP(0x91) + order[ZP(0x96)][i]);
            put(i ? 0x1202 : c);
        }
    }
}

/* $69EE: type the text at ($93) into the open window */
void ss2_msg_text(void)
{
    u8 y = 0;
    for (;;)                                 /* $69EF: a page */
    {
        u8 r, c;
        ZP(0x26) = 0x02; ZP(0x27) = 0x12;
        ZP(0x92) = (u8)(ZP(0x91) + 1);
        for (r = 6; r; r--)
        {
            ZP(0x90) = (u8)(ZP(0x8F) + 1);
            for (c = 10; c; c--) { put(0x1202); ZP(0x90)++; }
            ZP(0x92)++;
        }
        ss2_map6(8);
        if (text(y) == 0xFF) return;
        ss2_sfx(0x1C);
        ZP(0x90) = (u8)(ZP(0x8F) + 1);
        ZP(0x92) = (u8)(ZP(0x91) + 1);
        ZP(0x36) = 0;
        ZP(0x37) = 0;
        for (;;)                             /* $6A33: a character */
        {
            u8 ch;
            if (!ZP(0x36))
            {
                frame();
                if ((ZP(0x97) && (Z_PRESS & 0xF0)) || (Z_PRESS & 0x03))
                    ZP(0x36)++;              /* skip the typing */
            }
            ss2_map6(8);
            ch = text(y);
            if (ch == 0xFE)                  /* line / page break */
            {
                if (++ZP(0x37) != 3)
                {
                    ZP(0x90) = (u8)(ZP(0x8F) + 1);
                    ZP(0x92) = (u8)(ZP(0x92) + 2);
                    y++;
                    continue;
                }
                ss2_sfx(0xF1);               /* $6A97: wait for a button */
                ZP(0x90) = (u8)(ZP(0x8F) + 9);
                ZP(0x92)++;
                for (;;)
                {
                    frame();
                    if (Z_PRESS & 0x03) break;
                    put(cursor());
                }
                y++;
                break;                       /* JMP $69EF */
            }
            if (ch == 0xFD) { choice2(y); return; }
            if (ch == 0xFC) { choice3(y); return; }
            if (ch == 0xFF) { ss2_sfx(0xF1); return; }
            put((u16)(0x1180 + ch));         /* tile $180+ch, palette 1 */
            ZP(0x90)++;
            y++;
        }
    }
}

void ss2_msg_open(void)  { open_window(); }      /* $6997 */
void ss2_msg_close(void) { close_window(); }     /* $6C1F */

/* $6984: a whole message: open, type, close; MPR6 back to the caller's */
void ss2_message(void)
{
    u8 b = ZP(0x8E);
    open_window();
    ss2_msg_text();
    close_window();
    ss2_map6(b);
}

/* $BADE: HUD text/icon table at ($26) (absolute BAT address, palette,
 * characters, '/' new row, '\\' end) */
void ss2_hud_text(u16 ptr)
{
    ZP(0x26) = (u8)ptr; ZP(0x27) = (u8)(ptr >> 8);
    bat_op(SS2_BAT_HUD, 0, 0, ptr);
}

/* Continuations named in message text after a choice. The NPC and shop
 * code (bank 3 $997F-) owns most of them. */
void ss2_msg_continue(u16 addr, u8 y)
{
    if (!ss2_npc_continue(addr, y)) ss2_unported(addr);
}
