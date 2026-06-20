# SonSon NATIVE — 2-Player Simultaneous Co-op (RE + implementation plan)

SonSon is a **2-player SIMULTANEOUS co-op** game (NOT alternating): SonSon (P1) and
TonTon (P2) are both on screen at once, each with their own controls, score, and
lives, sharing ONE auto-scrolling world. This doc captures the arcade ground truth
(RE'd from the 6809, sibling oracle `..\sonson\port\src`) and the native build plan.

## 1. Arcade ground truth (cited)

- **Both players run every frame.** `sub_A98A` ($A98A): set P1 velocity from input ZP
  `$44/$46`, run P1 physics (`sub_A9A7`); if `$0089` (single-player flag) is SET, return;
  else set P2 velocity from `$45/$47` and run P2 physics too. So in 2P BOTH update each
  frame. `$0089`=1 → only P1 (single player).
- **Player overlap resolve** (`sub_A98A` $A945): if both players active and within ~12px
  horizontal and ~16px vertical, push them apart horizontally (each moves (0xC-|dx|)/2).
- **Player entities:** P1 `$0490`, P2 `$04C0` (stride `$30`). Field offsets (signed):
  `-0xD` = active/inactive byte (nonzero = inactive), `-0x8` = Y byte, `-0xB` = world-X
  word, `+8` = velocity word. (`sub_9094`, `sub_A98A`.)
- **Spawn (`sub_9094`):** inits BOTH records. P1 (`$0490`): Y-field `$68`, sprite-ptr
  `$AFAC`, shot-pool `$04E0`. P2 (`$04C0`): Y-field `$88`, sprite-ptr `$AFD0`, shot-pool
  `$0520`. Each record links to the other at `+0xE`. (P1/P2 start on different lanes.)
- **Scroll is a GLOBAL constant auto-scroll, independent of players** (`sub_A697` $A697):
  `$0091 (scroll frac) += $008D` each frame; on carry `$0090 (scroll int) += 1`. ≈0.55
  px/frame perpetual forward creep. NOT driven by any player.
- **Each player is clamped to the visible screen band [8..0xD0]=[8..208]** in screen space
  (screenX = worldX − scroll), WITHOUT moving the camera (`sub_AB52` $AB52). A player at
  the left edge is carried by the scroll; at the right edge they can't advance on screen
  but the world keeps scrolling. Players roam freely between the edges.
- **Per-player score:** the header reads `SONSON · HIGH · TONTON` (confirmed on the real
  arcade, `mame sonson`): P1=SonSon score (left), HIGH (center), P2=TonTon score (right).
  Separate per-player scores, not shared.
- **Per-player lives** (each player gets the configured life count).

### Native vs arcade camera (the one model change)
The native engine currently folds the creep into the single player's velocity and moves
the camera to keep that player band-pinned. The arcade auto-scrolls globally and lets the
player roam [8..208]. We switch to the arcade model (global creep + per-player band clamp),
which supports co-op naturally and is more faithful for 1P too.

## 2. RE still open (resolve during the phase that needs it)
- **TonTon (P2) sprite tiles + palette.** P1 sprite-ptr `$AFAC` vs P2 `$AFD0` differ;
  determine whether TonTon is a palette-swap of SonSon (same tiles, different sprite
  palette) or distinct sprite data. Bake/wire accordingly.
- **Per-player death/respawn in co-op:** a dead player respawns (costing a life) while the
  other keeps playing; GAME OVER only when BOTH are out (1P: when the one player is out).
- **Scoring attribution:** kill/fruit credited to the player whose shot/contact scored it.

## 3. Native implementation phases
- **P1 — camera + data model.** Adopt arcade global auto-scroll (`$008D` creep) + per-player
  band clamp [8..208]. Refactor `g_game.player` → `g_game.players[2]` + `g_numPlayers`
  (default 1). With numPlayers=1, P1 behavior must be unchanged (verify no regression).
- **P2 — second player.** Enable P2 from JOY_2, TonTon sprite/palette, overlap push-apart,
  render both in the SAT.
- **P3 — per-player game state.** Move score/lives/flow/flowTimer/bonusNextIdx into the
  per-player struct; independent death→dying→respawn; GAME OVER when all active players out.
- **P4 — collision / shots / scoring.** Per-player shot pools; enemies can hit either
  player; score credited to the scoring player.
- **P5 — HUD.** `SONSON · HIGH · TONTON` header + per-player lives.
- **P6 — front-end wiring.** Home "2 PLAYERS" and arcade 2P-start set `g_numPlayers=2`.
- **P7 — verify.** MAME `genesis` snapshots: both players moving, independent deaths,
  game-over when both out; 1P regression check.
