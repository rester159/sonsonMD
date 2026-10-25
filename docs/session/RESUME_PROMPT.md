# Prompt to paste into the new session

Copy everything in the code block below into the new session as your first message.

---

```
We're continuing an in-progress project: a from-scratch NATIVE Genesis re-implementation of
Capcom's arcade SonSon (1984), in C on SGDK. It is already DONE and PLAYABLE at 60fps; the
remaining work is incremental arcade-faithfulness refinement.

FIRST, read these resume docs in order (they are self-contained):
  D:\genesis ports\_capcom\son son\session_resume_docs\START_HERE.md
  D:\genesis ports\_capcom\son son\session_resume_docs\PROJECT_STATE.md
  D:\genesis ports\_capcom\son son\session_resume_docs\NEXT_WORK.md
There is also a full chronological log in your auto-memory (sonson-native-project.md via MEMORY.md).

The repo is D:\genesis ports\_capcom\son son\sonson_native (git, branch main). Build with
`bash build.sh` -> out/rom.bin. Verify headlessly with MAME's `genesis` driver (NTSC) at
D:/portForge/tools/mame/mame.exe and the deterministic g_frameCounter fps ratio. Play it in BlastEm.

NON-NEGOTIABLE DOCTRINE: per bug, decide the track up front. BEHAVIOR bugs (position, animation,
HP, collision, scripts, spawns, sequencing) -> START IN THE REVERSE ENGINEERING: decode the actual
arcade 6809 routine as ground truth (unidasm on arcade/ss.0{1,2,3}e; ../sonson port src as oracle),
find where the native deviates, fix the native to match, and CITE the disasm $address. No symptom-
first, no guessing. COLOR & FRAMERATE have no RE ground truth -> judge by perceived quality on real
hardware. And the Iron Law: no fix without a root cause confirmed by evidence.

After reading the docs, start on the TOP ITEM in NEXT_WORK.md: wire the native spawn scheduler to the
exact arcade $FCD9 wave schedule (data already extracted to inc/ss_wave_sched.h) so the enemy mix
follows the real arcade progression, replacing the current hand-authored approximation. Confirm the
root-cause/mechanism from the doc, make the change carefully (the current spawn system works — keep
the on-screen cap and watch fps), and verify it holds 60fps and matches the arcade progression.

Tell me your understanding of the current state and your plan before making changes.
```

---

## Optional: if you'd rather have it self-pace autonomously
The previous session ran on a `/loop`. To resume that mode instead, paste the block above first
(so it reads the docs), then send:

```
/loop continue the SonSon-native faithfulness refinement: work NEXT_WORK.md top-to-bottom, RE-first, verify 60fps, commit each item
```
