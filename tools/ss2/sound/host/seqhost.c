/* Host harness for the native Son Son II sequencer (src/ss2_sound_seq.c).
 *
 * stdin : one line per frame: "<tick 0/1> <hold> <nreq> <id>..."
 *         (requests are passed to $8003 in order, then $8000 runs if tick;
 *         with SS2_RING=1 they go through the 16-entry ring instead:
 *         ss2_snd_seq_queue each, then ss2_snd_seq_frame)
 * stdout: "W <frame> <reg> <sel> <val>" per PSG register write,
 *         "E <frame> <m|s> <channel> <byte>" per note/sfx event,
 *         "F <faults>" at the end.
 */
#include <stdio.h>
#include <stdlib.h>
#include "ss2_sound_seq.h"

static unsigned frame, sel;

void ss2_snd_psg_write(u8 reg, u8 val)
{
    if (reg == 0) sel = val & 7;
    printf("W %u %u %u %u\n", frame, reg, sel, val);
}

void ss2_snd_psg_wave(u16 src) { (void)src; }

void ss2_snd_host_event(char kind, u8 ch, u8 byte)
{
    printf("E %u %c %u %u\n", frame, kind, ch, byte);
}

int main(void)
{
    unsigned tick, hold, n, id, i;
    int ring = getenv("SS2_RING") != NULL;
    ss2_snd_seq_reset();
    while (scanf("%u %u %u", &tick, &hold, &n) == 3)
    {
        ss2_snd_seq_set_hold((u8)hold);
        for (i = 0; i < n; i++)
        {
            if (scanf("%u", &id) != 1) return 2;
            if (ring) ss2_snd_seq_queue((u8)id);
            else ss2_snd_seq_request((u8)id);
        }
        if (ring) ss2_snd_seq_frame();
        else if (tick) ss2_snd_seq_tick();
        frame++;
    }
    printf("F %u\n", ss2_snd_seq_faults);
    return 0;
}
