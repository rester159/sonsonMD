#include "arcade_attract.h"

/* Original 9A6E..9BD1, including the adjacent prelude scripts. The main demo
 * crosses 9B14/9BB6 naturally; those addresses are not end markers. */
extern const u8 arcade_attract_data[356];

void arcade_attract_init(ArcadeAttract *state)
{
    state->cursor[0] = 0;
    state->cursor[1] = 0x9B14 - 0x9A6E;
    state->input[0] = state->input[1] = 0;
    /* 9492 clears 44:45 but retains 46:47 until the next 9A2C step. */
    state->delay[0] = state->delay[1] = 0;
}

void arcade_attract_prelude_init(ArcadeAttract *state)
{
    /* 93EB..93F7 changes pointers and delays only. 44..47 are retained;
     * the next 9A2C copies the current inputs before loading the first pair. */
    state->cursor[0] = 0x9BB6 - 0x9A6E;
    state->cursor[1] = 0x9BC4 - 0x9A6E;
    state->delay[0] = state->delay[1] = 0;
}

u8 arcade_attract_step(ArcadeAttract *state, u8 clock_low)
{
    u16 i;
    for (i = 0; i < 2; ++i)
        if (!state->delay[i] &&
            (state->cursor[i] > 354 || (state->cursor[i] & 1)))
            return 0;
    /* 9A2C copies both old inputs before 9A4D updates either stream. */
    for (i = 0; i < 2; ++i) {
        state->previous[i] = state->input[i];
        if (state->delay[i]) --state->delay[i];
        else {
            state->input[i] = arcade_attract_data[state->cursor[i]++];
            state->delay[i] = arcade_attract_data[state->cursor[i]++];
        }
    }
    /* 9A5F: fire remains set until the next pair replaces the input byte. */
    if (!(clock_low & 7)) {
        state->input[0] |= 1;
        state->input[1] |= 1;
    }
    return 1;
}
