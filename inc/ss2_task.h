/* ss2_task.h -- SON SON II cooperative tasks (bank 0 $E1A5 scheduler). */
#ifndef SS2_TASK_H
#define SS2_TASK_H

#include <genesis.h>

typedef void (*ss2_task_fn)(void);

void ss2_task_create(u8 slot, ss2_task_fn fn);  /* $E1E2 */
void ss2_wait(u8 frames);                       /* $E1FE / JSR $B9B8 etc. */
void ss2_task_exit(void);                       /* $E212 */
void ss2_busy(u8 frames);                       /* CPU-bound across VBlanks */
void ss2_task_restart(ss2_task_fn fn);          /* LDX #$xx / TXS restarts */
u8   ss2_task_current(void);

void ss2_sched_vblank(void);                    /* $E14B countdown (IRQ) */
void ss2_sched_run(void);                       /* $E1A5 idle loop       */

#endif
