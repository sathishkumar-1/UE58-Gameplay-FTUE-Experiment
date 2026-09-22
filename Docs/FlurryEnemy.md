# Red flurry enemy

The original `BP_CombatEnemy` remains the basic enemy and FTUE target. The new
`BP_FlurryEnemy` uses the same damage, montage-notify, death, and spawn systems,
with **Flurry Enemy > Flurry Enemy** enabled in its defaults.

## Encounter

Approach -> wait -> choose a normal punch or flurry windup.

- Normal punch: existing attack and recovery, then choose again.
- Flurry: 0.45 s warning -> 1.5 s of accelerated repeating combo punches.
- Exhaustion: 0.75 s counter window. Any successful player hit kills it.
- Recovery: 0.35 s, then approach/choose again.

Each choice has a 50% flurry chance, with a flurry guaranteed after two normal
attacks. The opening choice can be either attack. The enemy ignores damage
during its flurry warning and burst, preventing attack spam from interrupting
the challenge. Outside those phases it takes normal damage; exhaustion makes
the next player hit lethal regardless of remaining health.

Damage is produced by existing montage hit notifies and spatial melee sweeps,
not a timer that damages the player at any distance. The accelerated combo is
the prototype flurry animation. The existing mannequin heavy hit reaction is
reused for exhaustion and can be replaced via `ExhaustedAnimation`.

## Player evade

Hold **F / gamepad Left Shoulder** to evade attacks from enemies in front.
Release to attack. Evading cancels an ongoing attack/charge and prevents new
attacks or back dodges while held. Successful evades take no damage or knockback.
The HUD indicates evading, evaded hits, flurry warning, and the counter window.
Evading is enabled during normal gameplay and disabled in menus/FTUE/death.
Existing light, heavy, and dodge bindings are preserved.

The player's full-body animation chains random center/left/right motions using
the user-retargeted sequences in `Anims/MIxamo/Retargeted`. All three already
reference `SK_Mannequin`, shared by the player's Quinn mesh and Manny animation
Blueprint. The original Mixamo Y-Bot assets and retarget setup are untouched.
Retargeting transfers the motion to the mannequin's bones; no runtime Y-Bot
mesh or additional retarget pass is needed.

`EvadeAnimations`, `EvadePlayRate` (1.0), and `EvadeBlendTime` (0.12 s) are
editable player Blueprint defaults. Playback uses the existing full-body
montage slot, avoids consecutive repeats, and stops on release/death/menu.
There is no timer to fire a delayed evade after release. Protection is continuous
while held, independent of animation phase, and still only covers frontal enemy
hits. Space remains the separate back dodge. Input is now `IA_Evade`; core
redirects preserve old serialized C++ property/function references. The imported
animation filenames retain their original `*_Block` names.

## Spawn integration

`BP_CombatEnemySpawner.FlurryEnemyClass` selects the new Blueprint. Spawners use
the basic enemy until `ACombatGameMode::IsRunActive()` is true. The first eligible
normal-gameplay spawn is red; subsequent spawns use `FlurrySpawnChance` (50%).
An existing tutorial dummy stays alive until defeated. The one-living-enemy
rule and stop-on-player-death behavior are preserved. Set the flurry class to
None or its chance to zero to disable this variant on an individual spawner.

## Tuning and implementation

- `AI/CombatEnemy.h`: flurry settings and visible state.
- `AI/CombatEnemyFlurry.cpp`: phase transitions and animation playback.
- `AI/CombatEnemy.cpp`: attack selection, combo chaining, and damage rules.
- `CombatCharacterEvade.cpp`: evade control and randomized animation chaining.
- `CombatCharacter.cpp`: Enhanced Input binding and damage prevention.
- `UI/CombatRunWidget.cpp`: combat cues and binding-aware evade hint.
- `Materials/MI_FlurryEnemy_01` and `_02`: red overrides for both mannequin slots.

## Playtest checklist

- Fresh-profile FTUE still uses the basic enemy; evade does not bypass lessons.
- Returning players and completed FTUE runs encounter the red enemy.
- Both normal-first and flurry-first choices occur; flurries repeat after recovery.
- Holding F or Left Shoulder prevents frontal hits throughout the burst.
- Releasing evade during a burst permits damage; attacks cannot occur while evading.
- A light punch during exhaustion kills a full-health red enemy.
- Missing the window allows recovery and further attacks.
- Dummy enemies still take their usual number of hits.
- Death/restart clears evade, animations, cues, and pending spawns.
- Holding evade chains all three retargeted motions without adjacent repeats.
- Release midway through a motion blends out and permits an immediate counter.

## Validation (2026-09-22)

- UE 5.8.1 Live Coding builds succeeded, including the HUD layout correction.
- Changed player, enemy, and spawner Blueprints compiled successfully.
- PIE tutorial progressed through light, heavy, and dodge to completion. The
  tutorial target was repositioned within the disposable PIE session to check
  hit progression; its full approach behavior was not revalidated.
- A returning-player run spawned the red variant. Repeated natural flurries,
  exhaustion, and recovery were observed; logged intervals matched 1.5/0.75 s.
- Holding F maintained 5/5 player HP through repeated enemy attacks. Unguarded
  hits reduced HP and reached Game Over.
- A real input counter test held F until exhaustion, released it, and sent one
  light punch: the full-health red enemy reached 0 HP, with player HP still 5/5.
- Reloading Lvl_Combat retained the endless spawner's new class and 50% mix.
- Controller binding is configured but was not tested with physical hardware.
- At that earlier stage, no normal editor build or packaged build had been run.
  The normal editor build has since passed; see animated validation below.

The tutorial playthrough persisted completion in the local test profile. Use
`FTUE.Reset` when a fresh-profile test is needed. The checks above describe the
original guard implementation, before the animated evade update.

## Animated evade validation

- Verified all three supplied retargeted sequences reference `SK_Mannequin`,
  matching the player's mesh/animation Blueprint; mannequin previews render.
- The first evade Live Coding compile compiled all six actions, but patch linking
  failed with LNK2001/LNK1120: missing definition of `LogCombatCharacter`.
  Added `DEFINE_LOG_CATEGORY(LogCombatCharacter)` in `CombatCharacter.cpp`.
  The user subsequently built GSGREditor Win64 Development and reopened UE.
  Verified the normal build log includes the DLL link and `Result: Succeeded`.
- After restart, player defaults contain IA_Evade and all three animations at
  play rate 1.0 / blend time 0.12. F / Left Shoulder mappings are correct.
  Player, game-mode, flurry-enemy, and animation Blueprints compile with warnings
  treated as errors. No Evade-specific missing-reference errors were observed.
- Floating PIE recorded 101 animation starts (39 center, 31 left, 31 right),
  with zero adjacent repeats. Repeated natural frontal attacks/flurries left
  the player at 5/5 HP; EVADING and EVADED! cues were captured.
- LMB/RMB/Space while held did not interrupt Evade or damage the enemy. Release
  during exhaustion plus one LMB click killed a 3-HP red enemy; player stayed
  at 5 HP and Evade cleared. This is a new animated-Evade counter test.
- F interrupted a light attack and heavy charge. Release returned to idle with
  no new evade starts for over 20 seconds; repress resumed correctly. Space
  after release visibly performed the separate back dodge.
- Sampled player transforms remained at the stationary anchor; retargeted poses
  rendered grounded. Fine foot sliding and blend polish still need human motion
  review. Source animation and retarget settings were not changed.
- A rear-positioned enemy damaged/killed the player while F remained held;
  death cleared Evade. Restart with F released yielded 5 HP and Evade off.
  Rear placement and isolated attack tests modified only disposable PIE actors.
- Physical gamepad, packaging, and a fresh-profile FTUE replay remain untested
  for this update. Existing completed local FTUE profile was preserved.
- PIE stopped after testing. Evidence is in `Saved/Logs/GSGR.log` and
  `Saved/Screenshots/Evade*.png`; detailed handoff is `Docs/CurrentWork.md`.
- Startup also reports an unrelated missing `SM_DoorFrame_Edge` mesh. It was
  left for a separate asset cleanup task.

Retargeting reference: [Epic's Auto Retargeting documentation](https://dev.epicgames.com/documentation/unreal-engine/auto-retargeting-in-unreal-engine).
