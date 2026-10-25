/* Main-loop display deadline helpers. The loop commits during VBlank; work
 * that is purely cosmetic may yield a refresh once the current frame is late.
 * Never use these to gate simulation, collision or audio. */
#ifndef FRAME_BUDGET_H
#define FRAME_BUDGET_H
/* Include after <genesis.h> (GET_VCOUNTER). */
/* True while the beam is in active display at or below `line`. VBlank lines
 * (counter >= 224, or PAL's wrapped values) never count as late. */
static inline u8 frame_past_line(u8 line)
{
    u8 v=(u8)GET_VCOUNTER;
    return v<224 && v>=line;
}
#endif
