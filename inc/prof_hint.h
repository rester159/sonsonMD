/* Diagnostic stamps for -DPROF_HINT builds (see src/prof_hint.c and
 * tools/frame_profile.py); they compile to nothing otherwise.
 * Slot 0: V-counter when the main loop finished a frame's work. */
#ifndef PROF_HINT_H
#define PROF_HINT_H
#ifdef PROF_HINT
extern volatile u16 prof_cnt[8];
#define PROF_SET(i,n) (prof_cnt[i]=(n))
#else
#define PROF_SET(i,n) ((void)0)
#endif
#endif
