#ifndef ARCADE_E_UPLOAD_H
#define ARCADE_E_UPLOAD_H
#include "arcade_e_art.h"
/* Manual arenas F320..F39F and F680..F6FF. Plane scrollF000; SATF400. */
#define ARCADE_E_SLOT0 1945
#define ARCADE_E_SLOT1 1972
extern const u16 arcade_e_upload_tiles[2];
void arcade_e_upload_reset(void);
/* Call after previous queue flush. Cells0..11 or65535 for unused/shared art.
 * Reserve all later transfers; publish new poses only on successful admission.
 * Main-thread-only producer, immutable ROM data, same-frame queue flush required. */
u8 arcade_e_upload(const u16 cells[2],u16 reserve_bytes,u16 reserve_ops);
#endif
