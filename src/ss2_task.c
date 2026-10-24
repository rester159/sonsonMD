#ifdef SS2_HOST
#define _XOPEN_SOURCE 600
#endif
/* SON SON II cooperative task scheduler, rebuilt from bank 0 $E1A5-$E220
 * and the VBlank countdown at $E14B.
 *
 * The HuCard runs up to four tasks; slot n keeps state, timer and resume
 * data in zero page $00+4n:
 *   state 0 free, 1 waiting (timer counts VBlanks), 2 running,
 *         4 ready to resume, 8 start at a new entry ($E1E2)
 * The idle loop resumes the first ready slot, from slot 0 each time a task
 * yields. Source routines that wait ($E1FE, "JSR $B9B8") block inside their
 * task while the others keep running; here every slot is a native
 * coroutine so that code keeps its source structure. State and timer bytes
 * live in the RAM image like the source; resume data is native.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

#define STACK_BYTES 2048

#ifdef SS2_HOST
#include <ucontext.h>
static ucontext_t sched_ctx, task_ctx[4];
static char task_stack[4][32768];
#else
static u32 task_stack[4][STACK_BYTES / 4];
static void *sched_sp, *task_sp[4];

/* save d2-d7/a2-a6 on the current stack, store sp, switch to new sp */
void ss2_coro_switch(void **save_sp, void *new_sp);
__asm__(
    "    .globl ss2_coro_switch\n"
    "ss2_coro_switch:\n"
    "    movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
    "    move.l  48(%sp),%a0\n"
    "    move.l  %sp,(%a0)\n"
    "    move.l  52(%sp),%sp\n"
    "    movem.l (%sp)+,%d2-%d7/%a2-%a6\n"
    "    rts\n");
#endif

static ss2_task_fn entry[4];
static s8 current = -1;

#define T_STATE(n)  ZP(0x00 + 4 * (n))
#define T_TIMER(n)  ZP(0x01 + 4 * (n))

static void trampoline(void)
{
    entry[current]();
    ss2_task_exit();
}

static void prepare(u8 n)
{
#ifdef SS2_HOST
    getcontext(&task_ctx[n]);
    task_ctx[n].uc_stack.ss_sp = task_stack[n];
    task_ctx[n].uc_stack.ss_size = sizeof task_stack[n];
    task_ctx[n].uc_link = 0;
    makecontext(&task_ctx[n], trampoline, 0);
#else
    /* first switch pops 11 dummy registers and returns into trampoline */
    u32 *sp = &task_stack[n][STACK_BYTES / 4];
    *--sp = (u32)trampoline;
    sp -= 11;
    task_sp[n] = sp;
#endif
}

static void to_task(u8 n)
{
    current = (s8)n;
#ifdef SS2_HOST
    swapcontext(&sched_ctx, &task_ctx[n]);
#else
    ss2_coro_switch(&sched_sp, task_sp[n]);
#endif
    current = -1;
}

static void to_sched(void)
{
    u8 n = (u8)current;
#ifdef SS2_HOST
    swapcontext(&task_ctx[n], &sched_ctx);
#else
    ss2_coro_switch(&task_sp[n], sched_sp);
#endif
}

/* $E1E2: create a task in slot n starting at fn */
void ss2_task_create(u8 n, ss2_task_fn fn)
{
    entry[n] = fn;
    T_STATE(n) = 8;
    prepare(n);
}

/* $E1FE: wait `frames` VBlanks (the source's timer byte) */
void ss2_wait(u8 frames)
{
    u8 n = (u8)current;
    ZP(0x15) = n;
    T_STATE(n) = 1;
    T_TIMER(n) = frames;
    to_sched();
}

/* $E212: the current task ends; its slot is freed */
void ss2_task_exit(void)
{
    T_STATE((u8)current) = 0;
    to_sched();
    for (;;) { }                    /* never resumed */
}

/* "LDX #$xx / TXS" restarts: abandon the current call chain and continue
 * the same slot at fn */
void ss2_task_restart(ss2_task_fn fn)
{
    u8 n = (u8)current;
    entry[n] = fn;
    T_STATE(n) = 4;
    prepare(n);
    to_sched();
    for (;;) { }
}

u8 ss2_task_current(void)
{
    return (u8)current;
}

/* $E14B (VBlank IRQ): count down waiting tasks, slots 3..0 */
void ss2_sched_vblank(void)
{
    s8 n;
    ZP(0x13) = 0x58;
    for (n = 3; n >= 0; n--)
        if (T_STATE(n) == 1 && !--T_TIMER(n)) T_STATE(n) = 4;
}

/* $E1A5: resume ready tasks (state >= 4) from slot 0 until all wait */
void ss2_sched_run(void)
{
    u8 n;
    ZP(0x13) = 0;
    for (n = 0; n < 4; )
    {
        if (T_STATE(n) >= 4)
        {
            ZP(0x15) = n;
            T_STATE(n) = 2;
            T_TIMER(n) = 0;
            to_task(n);
            n = 0;                  /* restart the scan after every yield */
            continue;
        }
        n++;
    }
}
