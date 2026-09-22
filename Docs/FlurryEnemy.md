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

## Player block

Hold **F / gamepad Left Shoulder** to block attacks from enemies in front.
Release to attack. Blocking cancels an ongoing attack/charge and prevents new
attacks or dodges while held. Successful blocks take no damage or knockback.
The HUD indicates guard, blocked hits, flurry warning, and the counter window.
Blocking is enabled during normal gameplay and disabled in menus/FTUE/death.
Existing light, heavy, and dodge bindings are preserved.

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
- `CombatCharacterBlock.cpp`: guard control.
- `CombatCharacter.cpp`: Enhanced Input binding and damage prevention.
- `UI/CombatRunWidget.cpp`: combat cues and binding-aware block hint.
- `Materials/MI_FlurryEnemy_01` and `_02`: red overrides for both mannequin slots.

## Playtest checklist

- Fresh-profile FTUE still uses the basic enemy; block does not bypass lessons.
- Returning players and completed FTUE runs encounter the red enemy.
- Both normal-first and flurry-first choices occur; flurries repeat after recovery.
- Holding F or Left Shoulder prevents frontal hits throughout the burst.
- Releasing guard during a burst permits damage; attacks cannot occur while guarding.
- A light punch during exhaustion kills a full-health red enemy.
- Missing the window allows recovery and further attacks.
- Dummy enemies still take their usual number of hits.
- Death/restart clears guard, animations, cues, and pending spawns.

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
- No full cold editor build or packaged build was run. Perform a normal C++
  build before packaging assets that use the newly added reflected properties.

The tutorial playthrough persisted completion in the local test profile. Use
`FTUE.Reset` when a fresh-profile test is needed. Block feedback currently uses
the HUD; a dedicated raised-arm guard animation is a subsequent visual pass.
