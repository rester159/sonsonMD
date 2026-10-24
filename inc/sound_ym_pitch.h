#ifndef SOUND_YM_PITCH_H
#define SOUND_YM_PITCH_H
#include "arcade_types.h"
/* High byte is YM A4 data (block/Fnum high), low byte is A0 data.
 * Index is the masked 12-bit AY period; period zero is silent. */
extern const u16 sound_ym_pitch[4096];
#endif
