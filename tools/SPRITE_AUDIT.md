# SonSon-native SPRITE FIDELITY AUDIT (vs real arcade, in progress)

Method: drive the real arcade (`mame sonson -rompath D:/portForge/tools/mame/roms`),
capture the 96-byte `:spriteram` share during gameplay (`tools/arc_sprite_census.lua`)
and read enemy POOL records directly (`tools/arc_class_sprites.lua`). Sprite decode
(sonson_render.c): slot = `[sy, attr, code_lo, sx]`; `code = code_lo+((attr&0x20)<<3)`;
`bank = attr & 0x1F`. Native baked codes are in `tools/gen_anim.py` (ANIMS).

## CONFIRMED CORRECT (native bake == arcade)
- PLAYER run/climb/fire: codes 64,65,80 / 66-69 / 83, bank 0/1. ✓
- WALKER (the common ground enemy): codes 304,305,306 bank 2. ✓ (arcade's most common)
- WALKER DEATH burst: 307,308,309,310,312,313,314,315 bank 2. ✓
- SHOTS (player bullet + enemy shot): codes 70,71 (and 78,79) bank 2. ✓
  (player-bullet WIRING fixed in commit 9698921.)

## CONFIRMED WRONG -- baked codes the arcade NEVER renders (need correct sprite)
- FLYER baked 296/297 bank2  -> NOT in arcade census.
- THROWER baked 320/321 bank2 -> NOT in arcade census.
- CREATURE_G baked 272/273/274 bank2 -> NOT in arcade census.
- CREATURE_C baked 264/265 bank2 -> NOT in arcade census.
- EMITTER baked 318/319 bank2 -> NOT in arcade census.
- ORB baked 230/231 bank14 -> NOT in arcade census (objSingle pool $0E28 never
  activated in normal play -- ET_ORB's very existence is suspect, may be a mis-ID).

## RIGHT CODE, WRONG BANK (shape ok, colors wrong) -- needs code->archetype confirm
- code 352/353: baked bank2 (CMPE); arcade renders bank 26 (Y 64-169, airborne).
- code 322/323: baked bank2 (LARGE); arcade renders bank 31 (Y 200, ground).
- code 388/389: baked bank2 (HOP);  arcade renders bank 30 (Y 200, ground).

## ARCADE ENEMIES the native may be MISSING / mis-mapped
- A bank-9 VERTICAL multi-segment creature: codes 96-109 bank9, stacked Y 0-253
  (pool $0DA8). The native has no vertical/segmented enemy.
- bank-26 enemy family: codes 352,353,360,361,372,373,374,375,379,380,382,383
  (airborne/mid, Y 64-185) -- multiple frames, likely 1-2 airborne archetypes.
- bank-29 (88-95), bank-30 (387-392), bank-31 (322-325,330) ground/air enemies.

## TAXONOMY PROBLEM (root cause of most errors)
The native's arcade-class -> EntityType mapping is partly wrong. Verified: pool
$0CE8 (the native calls "classH" -> HOPPER) actually renders the WALKER (304-306).
So the wave-schedule bit->archetype table AND several baked sprites inherit a
mis-identified taxonomy. Fixing faithfully requires, per class pool ($0828 enemyMain,
$0A28 C, $0AE8 D, $0B68 E, $0BA8 F, $0C28 G, $0CE8 H, $0DA8 I, $0E48 obj16x16):
read its real sprite (code,bank) + observe its behavior at the waves where it spawns,
then map it to the right native archetype and re-bake.

## NEXT STEPS (loop)
1. Get authoritative per-class sprite table: reach DEEP waves (classF/G/D/E spawn
   there). The $0090 wave-pin crashed gameplay; instead drive natural deep scroll
   with robust survival, or pin $005A/$0096 like composite_re2.lua. Move the dump
   BEFORE the in-game guard so it always writes.
2. For each class: real (code,bank) + behavior -> correct native archetype + re-bake
   in gen_anim.py. Re-verify in-game with a snapshot.
3. Re-examine ET_ORB (objSingle $0E28) -- confirm real vs remove.
4. Front-end screens: the port shell is NOT the arcade title/attract/high-score.
